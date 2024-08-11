#include "con4m.h"

typedef struct {
    c4m_alloc_hdr *new_hdr;
    c4m_alloc_hdr *old_hdr;
    int            ix;
} worklist_item_t;

typedef struct {
    uint64_t        write_ix;
    uint64_t        read_ix;
    worklist_item_t items[];
} worklist_t;

typedef struct {
    c4m_arena_t   *from_space;
    c4m_arena_t   *to_space;
    void          *fromspc_start;
    void          *fromspc_end;
    c4m_alloc_hdr *next_to_record;
    worklist_t    *worklist;
    int            reached_allocs;
    int            copied_allocs;
    bool           stack_scanning;
} c4m_collection_ctx;

#define GC_OP_FW   0
#define GC_OP_COPY 1

// In gcbase.c, but not directly exported.
extern c4m_arena_t *c4m_current_heap;
extern uint64_t     c4m_page_bytes;
extern uint64_t     c4m_page_modulus;
extern uint64_t     c4m_modulus_mask;

static c4m_system_finalizer_fn system_finalizer = NULL;

void
c4m_gc_set_finalize_callback(c4m_system_finalizer_fn fn)
{
    system_finalizer = fn;
}

#ifdef C4M_GC_STATS
int                          c4m_gc_show_heap_stats_on = C4M_SHOW_GC_DEFAULT;
static thread_local uint32_t c4m_total_collects        = 0;
static thread_local uint64_t c4m_total_garbage_words   = 0;
static thread_local uint64_t c4m_total_size            = 0;
extern thread_local uint64_t c4m_total_alloced;
extern thread_local uint64_t c4m_total_requested;
extern thread_local uint32_t c4m_total_allocs;

uint64_t
c4m_get_alloc_counter()
{
    return c4m_current_heap->alloc_count;
}
#endif

#ifdef C4M_GC_FULL_TRACE
int c4m_gc_trace_on = C4M_GC_FULL_TRACE_DEFAULT;
#endif

#ifdef C4M_FULL_MEMCHECK
extern uint64_t c4m_end_guard;
#endif

typedef struct hook_record_t {
    struct hook_record_t *next;
    c4m_gc_hook           post_collect;
} hook_record_t;

static hook_record_t *c4m_gc_hooks = NULL;

#if defined(__linux__)
static inline void
c4m_get_stack_scan_region(uint64_t *top, uint64_t *bottom)
{
    pthread_t self = pthread_self();

    pthread_attr_t attrs;
    uint64_t       addr;

    pthread_getattr_np(self, &attrs);

    size_t size;

    pthread_attr_getstack(&attrs, (void **)&addr, &size);

#ifdef C4M_USE_FRAME_INTRINSIC
    *bottom = (uint64_t)__builtin_frame_address(0);
#else
    *bottom = (uint64_t)addr + size;
#endif

    *top = (uint64_t)addr;
}

#elif defined(__APPLE__) || defined(BSD)
// Apple at least has no way to get the thread's attr struct that
// I can find. But it does provide an API to get at the same data.
static inline void
c4m_get_stack_scan_region(uint64_t *top, uint64_t *bottom)
{
    pthread_t self = pthread_self();

    *bottom = (uint64_t)pthread_get_stackaddr_np(self);

#ifdef C4M_USE_FRAME_INTRINSIC
    *top = (uint64_t)__builtin_frame_address(0);
#else
    *top = (uint64_t)&self;
#endif
}
#else
#error "Unsupported platform."
#endif

void
c4m_gc_register_collect_fn(c4m_gc_hook post)
{
    hook_record_t *record = calloc(1, sizeof(hook_record_t));

    record->post_collect = post;
    record->next         = c4m_gc_hooks;
    c4m_gc_hooks         = record;
}

static inline void
run_post_collect_hooks()
{
    hook_record_t *record = c4m_gc_hooks;

    while (record != NULL) {
        if (record->post_collect) {
            (*record->post_collect)();
        }
        record = record->next;
    }
}

static void
migrate_finalizers(c4m_arena_t *old, c4m_arena_t *new)
{
    c4m_finalizer_info_t *cur = old->to_finalize;
    c4m_finalizer_info_t *next;

    // For the moment, we might have issues w/ finalizers.
    return;
    while (cur != NULL) {
        c4m_alloc_hdr *alloc = cur->allocation;
        next                 = cur->next;

        // If it's been forwarded, we migrate the record to the new heap.
        // In the other branch, we'll call the finalizer and delete the
        // record (we do not cache records right now).
        if (alloc->fw_addr) {
            cur->next        = new->to_finalize;
            new->to_finalize = cur;
            // fw_addr is the user-facing address; but the alloc record
            // gets the actual header, which is why we do the -1 index.
            cur->allocation  = &((c4m_alloc_hdr *)alloc->fw_addr)[-1];
        }
        else {
            system_finalizer(alloc->data);
            c4m_rc_free(cur);
        }

        cur = next;
    }
}

static inline bool
value_in_fromspace(c4m_collection_ctx *ctx, void *ptr)
{
    if (ptr >= ctx->fromspc_end || ptr <= ctx->fromspc_start) {
        return false;
    }
    c4m_gc_trace(C4M_GCT_PTR_TEST, "In fromspace (%p) == true", ptr);
    return true;
}

#ifdef C4M_FULL_MEMCHECK
static inline char *
name_alloc(c4m_alloc_hdr *alloc)
{
    if (!alloc->con4m_obj) {
        return "raw alloc";
    }
    if (!alloc->type) {
        return "internal structure";
    }

    return c4m_internal_type_name(alloc->type);
}
#endif

static void *ptr_relocate(c4m_collection_ctx *, void *);

static inline void *
possible_relo(c4m_collection_ctx *ctx, void *p)
{
    // Must be NULL if it's not in the heap.
    if (!c4m_in_heap(p)) {
        return p;
    }

    return ptr_relocate(ctx, p);
}

static worklist_item_t *
next_write_slot(c4m_collection_ctx *ctx)
{
    worklist_item_t *result = &ctx->worklist->items[ctx->worklist->write_ix];

    result->ix = ctx->worklist->write_ix++;

    return result;
}

static inline worklist_item_t *
next_read_slot(c4m_collection_ctx *ctx)
{
    return &ctx->worklist->items[ctx->worklist->read_ix++];
}

static inline void
fill_new_hdr(c4m_collection_ctx *ctx, c4m_alloc_hdr *old, c4m_alloc_hdr *new)
{
    // The old alloc might have debug padding, so we don't strictly
    // use 'alloc_len' because it hides that.

    char *raw = (char *)new;

    ctx->next_to_record = (void *)(raw + old->alloc_len);
    ctx->reached_allocs = ctx->reached_allocs + 1;
    new->guard          = old->guard;
    new->next_addr      = (char *)ctx->next_to_record;
    new->alloc_len      = old->alloc_len;
    new->request_len    = old->request_len;
    new->scan_fn        = old->scan_fn;
    new->finalize       = old->finalize;
    new->con4m_obj      = old->con4m_obj;
    new->cached_hash    = old->cached_hash;
    new->type           = possible_relo(ctx, old->type);

#if defined(C4M_FULL_MEMCHECK)
    new->end_guard_loc = ((uint64_t *)new->next_addr) - 2;
#endif
#if defined(C4M_ADD_ALLOC_LOC_INFO)
    new->alloc_file = old->alloc_file;
    new->alloc_line = old->alloc_line;
#endif
}

static c4m_alloc_hdr *
get_new_header(c4m_collection_ctx *ctx, c4m_alloc_hdr *old)
{
    c4m_alloc_hdr *new    = ctx->next_to_record;
    worklist_item_t *item = next_write_slot(ctx);

    item->new_hdr = new;
    item->old_hdr = old;
    old->fw_addr  = new;

    c4m_gc_trace(C4M_GCT_SCAN_PTR,
                 "Marking header @%p, to move to %p. Write ix = %d",
                 old,
                 old->fw_addr,
                 item->ix);

    fill_new_hdr(ctx, old, new);

    return new;
}

// We perhaps should instead keep a binary range tree or k-d tree to
// not have to keep scanning memory.
//
// My thinking here is:
//
// 1. Keep it simple.
//
// 2. Sequential access is VERY fast relative to jumping around pages
//    when walking a tree.
//
// 3. Get fancier if there's data showing we need to do so.
static void *
ptr_relocate(c4m_collection_ctx *ctx, void *ptr)
{
    // Align the pointer for scanning back to the header.
    // It should be aligned, but people do the craziest things.
    void **p = (void **)(((uint64_t)ptr) & ~0x0000000000000007);
    p        = (void **)(((c4m_alloc_hdr *)p) - 1);

    while ((*(uint64_t *)p) != c4m_gc_guard) {
        p--;
    }

    c4m_gc_trace(C4M_GCT_SCAN_PTR, "Header @%p for ptr @%p", p, ptr);

    c4m_alloc_hdr *hdr = (c4m_alloc_hdr *)p;
    c4m_alloc_hdr *fw  = hdr->fw_addr;

    if (fw == NULL) {
        fw = get_new_header(ctx, hdr);
    }
    else {
        c4m_gc_trace(C4M_GCT_SCAN_PTR, "Existing forward @%p -> %p", hdr, fw);
    }

    ptrdiff_t offset = ((char *)ptr) - (char *)hdr->data;
    char     *result = ((char *)fw->data) + offset;

    assert(result - (char *)fw == ((char *)ptr) - (char *)hdr);

    return result;
}

static void
move_range_unsafely(c4m_collection_ctx *ctx, worklist_item_t *item)
{
    // Unsafely because we aren't being specific about what's a pointer
    // and what isn't, so we might be transforming data into dangling ptrs.

    uint64_t *p   = item->old_hdr->data;
    uint64_t *end = (uint64_t *)item->old_hdr->next_addr;
    uint64_t *dst = item->new_hdr->data;

    c4m_gc_trace(C4M_GCT_WORKLIST,
                 "Move (%ld): %p-%p -> %p",
                 ctx->worklist->read_ix,
                 p,
                 end,
                 dst);

    while (p < end) {
        uint64_t value = *p++;

        if (value_in_fromspace(ctx, (void *)value)) {
            *dst = (uint64_t)ptr_relocate(ctx, (void *)value);
        }
        else {
            *dst = value;
        }
        dst++;
    }
}

static void
one_worklist_item(c4m_collection_ctx *ctx)
{
    worklist_item_t *item = next_read_slot(ctx);
    c4m_alloc_hdr *new    = item->new_hdr;
    c4m_alloc_hdr *old    = item->old_hdr;

    c4m_gc_trace(C4M_GCT_WORKLIST, "process item: %d", ctx->worklist->read_ix);

    if (C4M_GC_SCAN_NONE == (void *)item->new_hdr->scan_fn) {
        size_t copy_len = ((char *)old->next_addr) - (char *)old->data;

        memcpy(new->data, old->data, copy_len);

        ctx->copied_allocs++;
        return;
    }

    if (C4M_GC_SCAN_ALL == (void *)item->new_hdr->scan_fn) {
        move_range_unsafely(ctx, item);
        ctx->copied_allocs++;

        return;
    }

    // Start simple, will get this back up in a bit.
    uint32_t  numwords    = item->new_hdr->alloc_len / 8;
    uint32_t  bf_byte_len = ((numwords / 64) + 1) * 8;
    uint64_t *map         = malloc(bf_byte_len);
    uint64_t *map_copy    = map;

    memset(map, 0, bf_byte_len);

    // TODO: Probably this should point at the OLD location?
    // Tho would be messy w/ marshal.
    (*item->new_hdr->scan_fn)(map, item->old_hdr->data);

    uint32_t  scan_ix = 0;
    uint64_t  value;
    uint64_t *p   = item->old_hdr->data;
    uint64_t *end = (uint64_t *)item->old_hdr->next_addr;
    uint64_t *dst = item->new_hdr->data;

    while (p < end) {
        value = *p++;

        if (((*map) & (1 << scan_ix++)) && value_in_fromspace(ctx, (void *)value)) {
            *dst++ = (uint64_t)ptr_relocate(ctx, (void *)value);
        }
        else {
            *dst++ = value;
        }

        if (c4m_unlikely(scan_ix == 64)) {
            ++map;
            scan_ix = 0;
        }
    }
    ctx->copied_allocs++;
    free(map_copy);
}

static void
process_worklist(c4m_collection_ctx *ctx)
{
    c4m_gc_trace(C4M_GCT_WORKLIST,
                 "list start: %d - %d",
                 ctx->worklist->read_ix,
                 ctx->worklist->write_ix);

    while (ctx->worklist->read_ix != ctx->worklist->write_ix) {
        one_worklist_item(ctx);
    }

    c4m_gc_trace(C4M_GCT_WORKLIST,
                 "@end: %d - dx",
                 ctx->worklist->read_ix,
                 ctx->worklist->write_ix);
}

static void
scan_external_range(c4m_collection_ctx *ctx, uint64_t *p, int num_words)
{
    for (int i = 0; i < num_words; i++) {
        uint64_t value = *p;

#ifdef HAS_ADDRESS_SANITIZER
        if (__asan_addr_is_in_fake_stack(__asan_get_current_fake_stack(),
                                         value)) {
            p++;
            continue;
        }
#endif

        if (value_in_fromspace(ctx, (void *)value)) {
            *p = (uint64_t)ptr_relocate(ctx, (void *)value);
        }
        p++;
    }
}

static void
scan_roots(c4m_collection_ctx *ctx)
{
    c4m_arena_t *old       = ctx->from_space;
    uint32_t     num_roots = hatrack_zarray_len(old->roots);

    for (uint32_t i = 0; i < num_roots; i++) {
        c4m_gc_root_info_t *ri = hatrack_zarray_cell_address(old->roots, i);

        c4m_gc_trace(C4M_GCT_SCAN,
                     "Root scan start: %p (%u item(s)) (%s:%d)",
                     ri->ptr,
                     (uint32_t)ri->num_items,
                     ri->file,
                     ri->line);
        scan_external_range(ctx, ri->ptr, ri->num_items);
        c4m_gc_trace(C4M_GCT_SCAN,
                     "Root scan end: %p (%d item(s)) (%s:%d)",
                     ri->ptr,
                     (uint32_t)ri->num_items,
                     ri->file,
                     ri->line);
        process_worklist(ctx);
    }
}

static void
raw_trace(c4m_collection_ctx *ctx)
{
    c4m_arena_t *cur = ctx->from_space;
    uint64_t    *stack_top;
    uint64_t    *stack_bottom;

    c4m_get_stack_scan_region((uint64_t *)&stack_top,
                              (uint64_t *)&stack_bottom);

    scan_external_range(ctx, (uint64_t *)&c4m_bi_types[2], 1);
    process_worklist(ctx);
    scan_external_range(ctx, (uint64_t *)&c4m_bi_types[0], C4M_NUM_BUILTIN_DTS);
    process_worklist(ctx);

    /*    scan_external_range(ctx,
                            (uint64_t *)&ctx->from_space->history->ring[0],
                            C4M_ALLOC_HISTORY_SIZE);
                            process_worklist(ctx);*/
    scan_roots(ctx);

    c4m_gc_trace(C4M_GCT_SCAN,
                 "Stack scan start: %p to %p (%lu item(s))",
                 stack_top,
                 stack_bottom,
                 stack_bottom - stack_top);

    ctx->stack_scanning = true;
    scan_external_range(ctx, stack_top, stack_bottom - stack_top);
    process_worklist(ctx);

    c4m_gc_trace(C4M_GCT_SCAN,
                 "Stack scan end: %p to %p (%lu item(s))",
                 stack_top,
                 stack_bottom,
                 stack_bottom - stack_top);

    if (system_finalizer != NULL) {
        migrate_finalizers(cur, ctx->to_space);
    }
}

#ifdef C4M_FULL_MEMCHECK

extern uint64_t c4m_end_guard;

void
_c4m_memcheck_raw_alloc(void *a, char *file, int line)
{
    // This uses info int the alloc itself that might be corrupt;
    // It's a TODO to make the external records log n searchable,
    // to remove that potential issue.

    if (!c4m_in_heap(a)) {
        fprintf(stderr,
                "\n%s:%d: Heap pointer %p is corrupt; not in heap.\n",
                file,
                line,
                a);
        abort();
    }

    c4m_alloc_hdr *h = (c4m_alloc_hdr *)(((unsigned char *)a) - sizeof(c4m_alloc_hdr));
    if (h->guard != c4m_gc_guard) {
        fprintf(stderr, "\n%s:%d: ", file, line);
        c4m_alloc_display_front_guard_error(h, a, NULL, 0, true);
    }

    if (*h->end_guard_loc != c4m_end_guard) {
        fprintf(stderr, "\n%s:%d: ", file, line);
        c4m_alloc_display_rear_guard_error(h,
                                           a,
                                           h->request_len,
                                           a,
                                           NULL,
                                           0,
                                           true);
    }
}

void
_c4m_memcheck_object(c4m_obj_t o, char *file, int line)
{
    if (!c4m_in_heap(o)) {
        fprintf(stderr,
                "\n%s:%d: Heap pointer %p is corrupt; not in heap.\n",
                file,
                line,
                o);
        abort();
    }

    c4m_alloc_hdr *h = c4m_object_header(o);

    if (h->guard != c4m_gc_guard) {
        fprintf(stderr, "\n%s:%d: ", file, line);
        c4m_alloc_display_front_guard_error(h, o, NULL, 0, true);
    }

    if (*h->end_guard_loc != c4m_end_guard) {
        fprintf(stderr, "\n%s:%d: ", file, line);
        c4m_alloc_display_rear_guard_error(h,
                                           o,
                                           h->request_len,
                                           h->end_guard_loc,
                                           NULL,
                                           0,
                                           true);
    }
}

// No API here; we use this to fail tests if there's any definitive
// error.  The abort() will do it, but that only fires if the check
// for being recent finds the error, mainly for debugging purposes.

bool c4m_definite_memcheck_error = false;

void
c4m_alloc_display_front_guard_error(c4m_alloc_hdr *hdr,
                                    void          *ptr,
                                    char          *file,
                                    int            line,
                                    bool           bail)
{
    fprintf(stderr,
            "%s @%p is corrupt; its guard has been overwritten.\n"
            "Expected '%llx', but got '%llx'\n"
            "Alloc location: %s:%d\n\n",
            name_alloc(hdr),
            ptr,
            (long long unsigned)c4m_gc_guard,
            (long long unsigned)hdr->guard,
            file ? file : hdr->alloc_file,
            file ? line : hdr->alloc_line);

    c4m_definite_memcheck_error = true;

#ifdef C4M_STRICT_MEMCHECK
    abort();
#else
    if (bail) {
        abort();
    }
#endif
}

void
c4m_alloc_display_rear_guard_error(c4m_alloc_hdr *hdr,
                                   void          *ptr,
                                   int            len,
                                   void          *rear_guard_loc,
                                   char          *file,
                                   int            line,
                                   bool           bail)
{
    fprintf(stderr,
            "%s @%p overflowed. It was allocated to %d bytes, and had its "
            "end guard at %p.\n"
            "End guard should have been '%llx' but was actually '%llx'\n"
            "Alloc location: %s:%d\n\n",
            name_alloc(hdr),
            ptr,
            len,
            rear_guard_loc,
            (long long unsigned int)c4m_end_guard,
            (long long unsigned int)*(uint64_t *)rear_guard_loc,
            file ? file : hdr->alloc_file,
            file ? line : hdr->alloc_line);

    c4m_definite_memcheck_error = true;

#ifdef C4M_STRICT_MEMCHECK
    abort();
#else
    if (bail) {
        abort();
    }
#endif
}

static void
show_next_allocs(c4m_shadow_alloc_t *a)
{
#ifdef C4M_SHOW_NEXT_ALLOCS
    int n = C4M_SHOW_NEXT_ALLOCS;
    fprintf(stderr, "Next allocs:\n");

    while (a && n) {
        fprintf(stderr,
                "%s:%d (@%p; %d bytes)\n",
                a->file,
                a->line,
                a->start,
                a->len);
        a = a->next;
        n -= 1;
    }
#endif
}

static void
memcheck_validate_old_records(c4m_arena_t *from_space)
{
    uint64_t           *low  = (void *)from_space->data;
    uint64_t           *high = (void *)from_space->heap_end;
    c4m_shadow_alloc_t *a    = from_space->shadow_start;

    while (a != NULL) {
        c4m_shadow_alloc_t *next = a->next;

        if (a->start->guard != c4m_gc_guard) {
            c4m_alloc_display_front_guard_error(a->start,
                                                a->start->data,
                                                a->file,
                                                a->line,
                                                false);
        }

        if (*a->end != c4m_end_guard) {
            c4m_alloc_display_rear_guard_error(a->start,
                                               a->start->data,
                                               a->len,
                                               a->end,
                                               a->file,
                                               a->line,
                                               false);
            show_next_allocs(next);
        }

        if (a->start->fw_addr != NULL) {
            uint64_t **p   = (void *)a->start->data;
            uint64_t **end = (void *)a->end;

            while (p < end) {
                uint64_t *v = *p;
                if (v > low && v < high) {
                    void **probe;
                    probe = (void **)(((uint64_t)v) & ~0x0000000000000007);

                    while (*(uint64_t *)probe != c4m_gc_guard) {
                        --probe;
                    }

                    c4m_alloc_hdr *h = (c4m_alloc_hdr *)probe;
                    if (!h->fw_addr) {
                        // We currently don't mark this as a definite
                        // error for testing, because it *can* be a
                        // false positive.  It's reasonably likely in
                        // common situations on a mac.
                        fprintf(stderr,
                                "*****Possible missed allocation*****\n"
                                "At address %p, Found a pointer "
                                " to %p, which was NOT copied.\n"
                                "The pointer was found in a live allocation"
                                " from %s:%d.\n"
                                "That allocation moved to %p.\n"
                                "The allocation's gc bit map: %p\n"
                                "The pointer itself was allocated from %s:%d.\n"
                                "Note that this can be a false positive if "
                                "the memory in the allocation was non-pointer "
                                "data and properly marked as such.\n"
                                "Otherwise, it may be a pointer that was "
                                "marked as data, incorrectly.\n\n",
                                p,
                                *p,
                                a->start->alloc_file,
                                a->start->alloc_line,
                                a->start->fw_addr,
                                a->start->scan_fn,
                                h->alloc_file,
                                h->alloc_line);
#ifdef C4M_STRICT_MEMCHECK
                        exit(-4);
#endif
                    }
                }
                p++;
            }
        }
        a = next;
    }
}

static void
memcheck_delete_old_records(c4m_arena_t *from_space)
{
    c4m_shadow_alloc_t *a = from_space->shadow_start;

    while (a != NULL) {
        c4m_shadow_alloc_t *next = a->next;
        c4m_rc_free(a);
        a = next;
    }
}

#endif

#ifdef C4M_GC_STATS
#define PREP_STATS()                                                    \
    c4m_arena_t *old_arena = (void *)ctx->from_space;                   \
                                                                        \
    uint64_t old_used, old_free, old_total, live, available, new_total; \
                                                                        \
    c4m_gc_heap_stats(&old_used, &old_free, &old_total);                \
                                                                        \
    uint64_t prev_new_allocs  = c4m_total_allocs;                       \
    uint64_t prev_start_bytes = ctx->from_space->start_size;            \
    uint64_t prev_used_mem    = old_used - prev_start_bytes;            \
    uint64_t copy_count       = ctx->from_space->num_transferred;       \
    uint64_t old_num_records  = c4m_total_alloced + copy_count;         \
                                                                        \
    uint64_t num_migrations;                                            \
                                                                        \
    c4m_total_collects++;

#define SHOW_STATS()                                                           \
    const int mb   = 0x100000;                                                 \
    num_migrations = ctx->copied_allocs;                                       \
    c4m_gc_heap_stats(&live, &available, &new_total);                          \
                                                                               \
    c4m_current_heap->legacy_count    = num_migrations;                        \
    c4m_current_heap->num_transferred = num_migrations;                        \
    c4m_current_heap->start_size      = live / 8;                              \
                                                                               \
    c4m_total_garbage_words += (old_used - live) / 8;                          \
    c4m_total_size += (old_total / 8);                                         \
                                                                               \
    if (!c4m_gc_show_heap_stats_on) {                                          \
        return ctx->to_space;                                                  \
    }                                                                          \
                                                                               \
    c4m_printf("\n[h1 u]****Heap Stats****\n");                                \
                                                                               \
    c4m_printf(                                                                \
        "[h2]Pre-collection heap[i] @{:x}:",                                   \
        c4m_box_u64((uint64_t)old_arena));                                     \
                                                                               \
    c4m_printf("[em]{:,}[/] mb used of [em]{:,}[/] mb; ([em]{:,}[/] mb free)", \
               c4m_box_u64(old_used / mb),                                     \
               c4m_box_u64(old_total / mb),                                    \
               c4m_box_u64(old_free / mb));                                    \
                                                                               \
    c4m_printf(                                                                \
        "[em]{:,}[/] records, [em]{:,}[/] "                                    \
        "migrated; [em]{:,}[/] new. ([em]{:,}[/] mb)\n",                       \
        c4m_box_u64(old_num_records),                                          \
        c4m_box_u64(num_migrations),                                           \
        c4m_box_u64(prev_new_allocs),                                          \
        c4m_box_u64(prev_used_mem / mb));                                      \
                                                                               \
    c4m_printf(                                                                \
        "[h2]Post collection heap [i]@{:x}: ",                                 \
        c4m_box_u64((uint64_t)c4m_current_heap));                              \
                                                                               \
    c4m_printf(                                                                \
        "[em]{:,}[/] mb used of [em]{:,}[/] mb; "                              \
        "([b i]{:,}[/] mb free, [b i]{:,}[/] mb collected)",                   \
        c4m_box_u64(live / mb),                                                \
        c4m_box_u64(new_total / mb),                                           \
        c4m_box_u64(available / mb),                                           \
        c4m_box_u64((old_used - live) / mb));                                  \
                                                                               \
    c4m_printf("Copied [em]{:,}[/] records; Trashed [em]{:,}[/]",              \
               c4m_box_u64(num_migrations),                                    \
               c4m_box_u64(old_num_records - num_migrations));                 \
                                                                               \
    c4m_printf("[h2]Totals[/h2]\n[b]Total requests:[/] [em]{:,}[/] mb ",       \
               c4m_box_u64(c4m_total_requested / mb));                         \
                                                                               \
    c4m_printf("[b]Total alloced:[/] [em]{:,}[/] mb",                          \
               c4m_box_u64(c4m_total_alloced / mb));                           \
                                                                               \
    c4m_printf("[b]Total allocs:[/] [em]{:,}[/]",                              \
               c4m_box_u64(c4m_total_allocs));                                 \
                                                                               \
    c4m_printf("[b]Total collects:[/] [em]{:,}",                               \
               c4m_box_u64(c4m_total_collects));                               \
                                                                               \
    double      u = c4m_total_garbage_words * (double)100.0;                   \
    c4m_utf8_t *gstr;                                                          \
                                                                               \
    u    = u / (double)(c4m_total_size);                                       \
    gstr = c4m_cstr_format("{}", c4m_box_double(u));                           \
    gstr = c4m_str_slice(gstr, 0, 5);                                          \
                                                                               \
    c4m_printf("[b]Collect utilization[/]: [em]{}%[/] [i]garbage",             \
               gstr);                                                          \
                                                                               \
    c4m_printf("[b]Average allocation size:[/] [em]{:,}[/] bytes",             \
               c4m_box_u64(c4m_total_alloced / c4m_total_allocs))
#else
#define PREP_STATS()
#define SHOW_STATS()
#endif

#if defined(C4M_GC_FULL_TRACE) && C4M_GCT_COLLECT != 0
#define RUN_CORE_TRACE(x)                                \
    c4m_gc_trace(C4M_GCT_COLLECT,                        \
                 "=========== COLLECT START; arena @%p", \
                 c4m_current_heap);                      \
    raw_trace(x);                                        \
    c4m_gc_trace(C4M_GCT_COLLECT,                        \
                 "=========== COLLECT END; arena @%p\n", \
                 c4m_current_heap)
#else
#define RUN_CORE_TRACE(x) raw_trace(x)
#endif

#ifdef C4M_FULL_MEMCHECK
#define MEMCHECK_OLD_HEAP(x)          \
    memcheck_validate_old_records(x); \
    memcheck_delete_old_records(x)
#else
#define MEMCHECK_OLD_HEAP(x)
#endif

c4m_arena_t *
c4m_collect_arena(c4m_arena_t *from_space)
{
#ifdef C4M_GC_SHOW_COLLECT_STACK_TRACES
    printf(
        "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
        "Initiating garbage collection.\n Current stack: \n\n");
    c4m_static_c_backtrace();
    printf(
        "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n");
#endif

    c4m_collection_ctx *ctx;
    void               *worklist_end;
    worklist_t         *worklist;
    const int           hdr_sz = sizeof(c4m_alloc_hdr) / 8;
    uint64_t            len    = from_space->heap_end - (uint64_t *)from_space;

    worklist = c4m_raw_arena_alloc(len * 8, &worklist_end, (void **)&ctx);

    if (from_space->grow_next) {
        len <<= 1;
    }

    ctx->from_space     = from_space;
    ctx->to_space       = c4m_new_arena(len, from_space->roots);
    ctx->worklist       = worklist;
    ctx->next_to_record = ctx->to_space->next_alloc;
    ctx->fromspc_start  = ctx->from_space->data;
    ctx->fromspc_end    = ctx->from_space->heap_end;
    ctx->fromspc_start += hdr_sz;
    ctx->fromspc_end -= hdr_sz;

    PREP_STATS();

    RUN_CORE_TRACE(ctx);

    ctx->to_space->next_alloc  = ctx->next_to_record;
    ctx->to_space->alloc_count = ctx->copied_allocs;
    assert(ctx->copied_allocs == ctx->reached_allocs);

    MEMCHECK_OLD_HEAP(ctx->from_space);

    uint64_t start = (uint64_t)ctx->to_space;
    uint64_t end   = (uint64_t)ctx->to_space->heap_end;
    uint64_t total = end - start;
    uint64_t where = (uint64_t)ctx->to_space->next_alloc;
    uint64_t inuse = where - start;

    if (((total + (total >> 1)) >> 4) < inuse) {
        ctx->to_space->grow_next = true;
    }

    run_post_collect_hooks();

    // Free the worklist.
    c4m_gc_trace(C4M_GCT_MUNMAP,
                 "worklist: del @%p (used %lld items)",
                 ctx->worklist,
                 ctx->worklist->write_ix);

    if (ctx->from_space == c4m_current_heap) {
        c4m_current_heap = ctx->to_space;
    }

    c4m_arena_t *result = ctx->to_space;
    SHOW_STATS();

    pthread_mutex_unlock(&ctx->from_space->lock);
    c4m_delete_arena(ctx->from_space);
    c4m_delete_arena((void *)ctx->worklist);

    return result;
}

void
c4m_gc_thread_collect()
{
    c4m_current_heap = c4m_collect_arena(c4m_current_heap);
}

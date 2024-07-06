#include "con4m.h"

typedef struct {
    c4m_arena_t *from_space;
    c4m_arena_t *to_space;
    void       **worklist;
    void       **worklist_start;
    void       **worklist_end;
    void       **next_item;
    void        *fromspc_start;
    void        *fromspc_end;
    int          reached_allocs;
    int          copied_allocs;
} c4m_collection_ctx;

// In gcbase.c, but not directly exported.
extern thread_local c4m_arena_t *c4m_current_heap;
extern uint64_t                  c4m_page_bytes;
extern uint64_t                  c4m_page_modulus;
extern uint64_t                  c4m_modulus_mask;

static c4m_system_finalizer_fn system_finalizer = NULL;

void
c4m_gc_set_finalize_callback(c4m_system_finalizer_fn fn)
{
    system_finalizer = fn;
}

#ifdef C4M_GC_STATS
int                          c4m_gc_show_heap_stats_on = 0;
static thread_local uint32_t c4m_total_collects        = 0;
static thread_local uint64_t c4m_total_garbage_words   = 0;
static thread_local uint64_t c4m_total_size            = 0;
extern thread_local uint64_t c4m_total_words;
extern thread_local uint64_t c4m_words_requested;
extern thread_local uint32_t c4m_total_allocs;

uint64_t
c4m_get_alloc_counter()
{
    return c4m_total_allocs - c4m_current_heap->starting_counter;
}
#endif

#ifdef C4M_GC_FULL_TRACE
int c4m_gc_trace_on = 1;
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

#ifdef C4M_USE_FRAME_INTRINSIC
    pthread_attr_getstackaddr(&attrs, (void **)&addr);
    *bottom = (uint64_t)__builtin_frame_address(0);
#else
    size_t size;
    pthread_attr_getstack(&attrs, (void **)&addr, &size);
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

static c4m_alloc_hdr *
prep_allocation(c4m_alloc_hdr *old, c4m_arena_t *new_arena)
{
    c4m_alloc_hdr *res;

#if defined(C4M_GC_STATS) || defined(C4M_DEBUG)
#define TRACE_DEBUG_ARGS , debug_file, debug_ln

    char *debug_file = old->alloc_file;
    int   debug_ln   = old->alloc_line;
#else
#define TRACE_DEBUG_ARGS
#endif

    res = c4m_alloc_from_arena(&new_arena,
                               old->alloc_len,
                               old->scan_fn,
                               (bool)old->finalize
                                   TRACE_DEBUG_ARGS);

    res->finalize  = old->finalize;
    res->con4m_obj = old->con4m_obj;

    return &res[-1];
}

static void
migrate_finalizers(c4m_arena_t *old, c4m_arena_t *new)
{
    c4m_finalizer_info_t *cur = old->to_finalize;
    c4m_finalizer_info_t *next;

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

static inline bool
value_in_allocation(c4m_alloc_hdr *hdr, void *ptr)
{
    if (ptr > (void *)hdr && ptr < (void *)hdr->next_addr) {
        c4m_gc_trace(C4M_GCT_PTR_TEST,
                     "In alloc (ptr @%p alloc @%p) == true",
                     ptr,
                     hdr);
        return true;
    }

    c4m_gc_trace(C4M_GCT_PTR_TEST,
                 "In alloc (ptr @%p alloc @%p) == false",
                 ptr,
                 hdr);
    return false;
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

static inline c4m_alloc_hdr *
get_header(c4m_collection_ctx *ctx, void *ptr)
{
    // This assumes we've already checked that the pointer is in the heap,
    // so we don't check the front. Worst case with a corrupted heap, we'll
    // hit our guard page.

    // Align the pointer.
    void **p = (void **)(((uint64_t)ptr) & ~0x0000000000000007);

    while (*(uint64_t *)p != c4m_gc_guard) {
        --p;
    }

    c4m_alloc_hdr *result = (c4m_alloc_hdr *)p;

#if defined(C4M_GC_STATS) && (C4M_GCT_OBJ != 0)
    if (result->con4m_obj) {
        c4m_base_obj_t *obj = (c4m_base_obj_t *)result->data;

        c4m_gc_trace(C4M_GCT_OBJ,
                     "object identified @%p (hdr @%p); type=%s (%s:%d)",
                     obj,
                     result,
                     obj->base_data_type->name,
                     result->alloc_file,
                     result->alloc_line);
    }
    else {
        c4m_gc_trace(C4M_GCT_ALLOC_FOUND,
                     "found alloc @%p, end @%p, obj len = %d, total len = %d",
                     result,
                     result->next_addr,
                     (int)(((char *)result->next_addr) - (char *)result->data),
                     (int)(((char *)result->next_addr) - (char *)result));
    }
#endif

    return result;
}

static void process_worklist(c4m_collection_ctx *);

static inline void
add_copy_to_worklist(c4m_collection_ctx *ctx, c4m_alloc_hdr *hdr)
{
    c4m_gc_trace(C4M_GCT_SCAN_PTR,
                 "Added copy instruction to worklist for %p (wl item @%p).",
                 hdr,
                 ctx->next_item);

    *ctx->next_item++ = hdr;
    *ctx->next_item++ = (void *)~0;

    if (!(((uint64_t)ctx->next_item) & c4m_page_modulus)) {
        char *p = (char *)ctx->next_item;
        p -= c4m_page_bytes;

        if (ctx->worklist < (void **)p || ctx->worklist > ctx->next_item) {
            madvise(p, c4m_page_bytes, MADV_FREE);
        }
    }

    if (ctx->next_item >= ctx->worklist_end) {
        ctx->next_item = ctx->worklist_start;
    }

    if (ctx->next_item == ctx->worklist) {
        process_worklist(ctx);
    }
}

static inline void
add_forward_to_worklist(c4m_collection_ctx *ctx, void **addr)
{
    c4m_gc_trace(C4M_GCT_SCAN_PTR,
                 "Added pointer %p to worklist (wl item @%p).",
                 addr,
                 ctx->next_item);

    *ctx->next_item++ = addr;
    *ctx->next_item++ = NULL;

    if (!(((uint64_t)ctx->next_item) & c4m_page_modulus)) {
        char *p = (char *)ctx->next_item;
        p -= c4m_page_bytes;

        if (ctx->worklist < (void **)p || ctx->worklist > ctx->next_item) {
            madvise(p, c4m_page_bytes, MADV_FREE);
        }
    }

    if (ctx->next_item >= ctx->worklist_end) {
        ctx->next_item = ctx->worklist_start;
    }

    if (ctx->next_item == ctx->worklist) {
        process_worklist(ctx);
    }
}

static inline void
update_pointer(c4m_alloc_hdr *oldalloc, void *oldptr, void **loc_to_update)
{
    c4m_alloc_hdr *newalloc = oldalloc->fw_addr;
    ptrdiff_t      diff     = ((char *)oldptr) - ((char *)oldalloc);
    char          *newval   = ((char *)newalloc) + diff;

    *loc_to_update = newval;

    c4m_gc_trace(C4M_GCT_MOVE,
                 "replace pointer @%p: %p->%p (%ld vs %ld)",
                 loc_to_update,
                 oldptr,
                 newval,
                 ((char *)oldptr) - (char *)oldalloc->data,
                 newval - (char *)newalloc->data);
}

static inline void
scan_allocation(c4m_collection_ctx *ctx, c4m_alloc_hdr *hdr)
{
    void          **p       = (void **)hdr->data;
    void          **end     = (void **)hdr->next_addr;
    c4m_mem_scan_fn scanner = hdr->scan_fn;
    void           *contents;

    if ((void *)scanner == C4M_GC_SCAN_ALL) {
        while (p < end) {
            ASAN_UNPOISON_MEMORY_REGION(p, 8);
            contents = *p;

            // We haven't copied anything yet.
            //
            // This allocation cannot copy until any pointers it contains
            // are scanned, and have *their* initial allocations set up.
            //
            // So we go through looking for pointers into the old heap.
            // If the pointers are self-referential, we can just update
            // them.
            //
            // Otherwise, if the allocation add the location that the
            // pointer lives in the old heap to the worklist.

            if (value_in_fromspace(ctx, contents)) {
                if (value_in_allocation(hdr, contents)) {
                    update_pointer(hdr, contents, p);
                }
                else {
                    add_forward_to_worklist(ctx, p);
                }
            }

            p++;
        }

        return;
    }

    if ((void *)scanner == C4M_GC_SCAN_NONE) {
        return;
    }

    uint32_t  numwords    = hdr->alloc_len;
    uint32_t  bf_byte_len = ((numwords / 64) + 1) * 8;
    uint64_t *map         = alloca(bf_byte_len);

    memset(map, 0, bf_byte_len);
    (*scanner)(map, numwords);

    int last_cell = numwords / 64;

    for (int i = 0; i <= last_cell; i++) {
        uint64_t w = map[i];
        while (w) {
            int ix = 63 - __builtin_clzll(w);
            w &= ~(1ULL << ix);
            void **loc = &p[ix];
            contents   = *loc;

            if (value_in_fromspace(ctx, contents)) {
                if (value_in_allocation(hdr, contents)) {
                    update_pointer(hdr, contents, loc);
                }
                else {
                    add_forward_to_worklist(ctx, loc);
                }
            }
        }
        p += 64;
    }

    return;
}

static void
forward_one_allocation(c4m_collection_ctx *ctx, void **ptr_loc)
{
    void          *ptr = *ptr_loc;
    c4m_alloc_hdr *hdr = get_header(ctx, ptr);

    if (hdr->fw_addr) {
        c4m_gc_trace(C4M_GCT_PTR_TO_MOVE,
                     "ptr %p (@%p; %s:%d) needs to move; tospace record @%p",
                     ptr,
                     ptr_loc,
                     hdr->alloc_file,
                     hdr->alloc_line,
                     hdr->fw_addr);

        update_pointer(hdr, ptr, ptr_loc);
        return;
    }

    hdr->fw_addr = prep_allocation(hdr, ctx->to_space);
    ctx->reached_allocs++;
    c4m_gc_trace(C4M_GCT_PTR_TO_MOVE,
                 "ptr %p (@%p; %s:%d) needs to move; NEW RECORD ADDED (@%p)",
                 ptr,
                 ptr_loc,
                 hdr->alloc_file,
                 hdr->alloc_line,
                 hdr->fw_addr);

    update_pointer(hdr, ptr, ptr_loc);
    scan_allocation(ctx, hdr);
    // We can copy this item once any pointers inside it can be relocated.
    add_copy_to_worklist(ctx, hdr);
}

static void
process_worklist(c4m_collection_ctx *ctx)
{
    // The worklist contains one of two things:
    //
    // 1. Pointers to cells in allocations we've already started to
    //    copy.  Those cells contain a pointer somewhere else into the
    //    heap, but not it's own allocation.
    //
    // 2. An instruction to COPY an allocation's data to its new home.
    //    This only gets issued after any pointers inside our
    //    allocation are guaranteed to have been updated, so that
    //    we don't copy pointers into the old heap on accident.
    //
    // For #1, the allocation in question may already be forwarded,
    // but if not, it will get added to the worklist immediately,
    // causing the to-space memory to be allocated, allowing us to
    // update the pointer.

    while (ctx->worklist < ctx->next_item) {
        void **p    = (void *)*ctx->worklist++;
        void  *copy = (void *)*ctx->worklist++;

        // We take the complement of the pointer if we're supposed to
        // copy. So if it's not a pointer into the fromspace, we
        // invert it and copy.

        if (!copy) {
            forward_one_allocation(ctx, p);
        }
        else {
            c4m_alloc_hdr *src = (c4m_alloc_hdr *)p;
            c4m_alloc_hdr *dst = src->fw_addr;

            memcpy(dst->data, src->data, src->alloc_len * 8);

#ifdef C4M_FULL_MEMCHECK
            // We just wiped the back guard with the memcpy.
            *dst->end_guard_loc = c4m_end_guard;
#endif
            ctx->copied_allocs++;
            c4m_gc_trace(C4M_GCT_MOVED,
                         "%d words moved from %p to %p (%s:%d)\n",
                         src->alloc_len,
                         src->data,
                         dst->data,
                         dst->alloc_file,
                         dst->alloc_line);
        }
    }

    ctx->worklist  = ctx->worklist_start;
    ctx->next_item = ctx->worklist_start;
}

// This is only used for roots, not for memory allocations.
//
// That's because memory allocations have a hook to help us figure out which cells
// of the allocation may contain pointers.
static void
scan_range_for_allocs(c4m_collection_ctx *ctx, void **start, int num)
{
    for (int i = 0; i < num; i++) {
#ifdef HAS_ADDRESS_SANITIZER
        if (__asan_addr_is_in_fake_stack(__asan_get_current_fake_stack(), start)) {
            start++;
            continue;
        }
#endif
        if (value_in_fromspace(ctx, *start)) {
            forward_one_allocation(ctx, start);
        }
        start++;
    }
    process_worklist(ctx);
}

#ifdef C4M_PARANOID_STACK_SCAN
static void
scan_stack_for_allocs(c4m_collection_ctx *ctx, void **start, int num)
{
    char *p = (char *)start;

    for (int i = 0; i < num; i++) {
        if (value_in_fromspace(ctx, *start)) {
            forward_one_allocation(ctx, start);
        }
        p++;
        start = (void **)p;
    }
    process_worklist(ctx);
}
#endif

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
        scan_range_for_allocs(ctx, ri->ptr, ri->num_items);
        c4m_gc_trace(C4M_GCT_SCAN,
                     "Root scan end: %p (%d item(s)) (%s:%d)",
                     ri->ptr,
                     (uint32_t)ri->num_items,
                     ri->file,
                     ri->line);
    }
}

static inline void
raw_trace(c4m_collection_ctx *ctx)
{
    c4m_arena_t      *cur   = ctx->from_space;
    c4m_arena_t      *stash = (void *)~(uint64_t)cur;
    uint64_t          len   = cur->heap_end - (uint64_t *)cur;
    hatrack_zarray_t *r     = cur->roots;
    uint64_t         *stack_top;
    uint64_t         *stack_bottom;

    if (cur->grow_next) {
        len <<= 1;
    }

    ctx->to_space = c4m_new_arena((size_t)len, r);

    ASAN_UNPOISON_MEMORY_REGION(
        ctx->to_space,
        (((char *)ctx->to_space->heap_end) - (char *)ctx->to_space->data));

    uint64_t alloc_len = cur->largest_alloc * 32;
    if (alloc_len & c4m_page_modulus) {
        alloc_len = (alloc_len & c4m_modulus_mask) + c4m_page_bytes;
    }

    ctx->worklist_start = mmap(NULL,
                               alloc_len,
                               PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANON,
                               0,
                               0);

    ctx->worklist     = (void **)ctx->worklist_start;
    ctx->next_item    = ctx->worklist;
    ctx->worklist_end = (void **)&ctx->worklist_start[alloc_len / 8];

    c4m_get_stack_scan_region((uint64_t *)&stack_top,
                              (uint64_t *)&stack_bottom);

    ctx->fromspc_start = ctx->from_space->data;
    ctx->fromspc_end   = ctx->from_space->heap_end;

    scan_roots(ctx);
    c4m_gc_trace(C4M_GCT_SCAN,
                 "Stack scan start: %p to %p (%lu item(s))",
                 stack_top,
                 stack_bottom,
                 stack_bottom - stack_top);

#ifdef C4M_PARANOID_STACK_SCAN
    scan_stack_for_allocs(ctx, (void **)stack_top, stack_bottom - stack_top);
#else
    scan_range_for_allocs(ctx, (void **)stack_top, stack_bottom - stack_top);
#endif

    ctx->from_space = (void *)~(uint64_t)stash;

    c4m_gc_trace(C4M_GCT_SCAN,
                 "Stack scan end: %p to %p (%lu item(s))",
                 stack_top,
                 stack_bottom,
                 stack_bottom - stack_top);

    if (system_finalizer != NULL) {
        migrate_finalizers(cur, ctx->to_space);
    }

    ASAN_UNPOISON_MEMORY_REGION(
        ctx->to_space->next_alloc,
        (((char *)ctx->to_space->heap_end) - (char *)ctx->to_space->next_alloc));
}

#ifdef C4M_FULL_MEMCHECK

extern uint64_t c4m_end_guard;

static inline char *
name_alloc(c4m_alloc_hdr *alloc)
{
    if (!alloc->con4m_obj) {
        return "raw alloc";
    }
    c4m_base_obj_t *base = (c4m_base_obj_t *)alloc->data;
    return (char *)base->base_data_type->name;
}

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
        return;
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

    c4m_base_obj_t *b = &((c4m_base_obj_t *)o)[-1];
    c4m_alloc_hdr  *h = &((c4m_alloc_hdr *)b)[-1];

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

    if (bail) {
        abort();
    }
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
    if (hdr->con4m_obj) {
        len -= sizeof(c4m_base_obj_t);
    }

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

    if (bail) {
        abort();
    }
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
                        fprintf(stderr,
                                "Possible missed allocation. Found a pointer "
                                " to %p, which was NOT copied. The pointer "
                                " was found in a live allocation "
                                " from %s:%d, now residing at %p.\n",
                                *p,
                                h->alloc_file,
                                h->alloc_line,
                                h->fw_addr);
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

c4m_arena_t *
c4m_collect_arena(c4m_arena_t *from_space)
{
    c4m_collection_ctx ctx = {
        .from_space     = from_space,
        .reached_allocs = 0,
        .copied_allocs  = 0,
    };

#ifdef C4M_GC_STATS
    c4m_arena_t *old_arena = ctx.from_space;

    uint64_t old_used, old_free, old_total, live, available, new_total;

    c4m_gc_heap_stats(&old_used, &old_free, &old_total);

    uint64_t stashed_counter  = c4m_total_allocs;
    uint64_t stashed_words    = c4m_total_words;
    uint64_t stashed_requests = c4m_words_requested;
    uint64_t start_counter    = c4m_current_heap->starting_counter;
    uint64_t start_records    = c4m_current_heap->legacy_count;
    uint64_t prev_new_allocs  = stashed_counter - start_counter;
    uint64_t prev_start_bytes = c4m_current_heap->start_size;
    uint64_t prev_used_mem    = old_used - prev_start_bytes;
    uint64_t old_num_records  = start_records + prev_new_allocs;

    uint64_t num_migrations;

    c4m_total_allocs = 0;
    c4m_total_words  = 0;
    c4m_total_collects++;
#endif

    ASAN_UNPOISON_MEMORY_REGION(
        from_space,
        (((char *)ctx.from_space->heap_end) - (char *)ctx.from_space->data));

#ifdef C4M_FULL_MEMCHECK
    memcheck_validate_old_records(old_arena);
#endif

#if defined(C4M_GC_FULL_TRACE) && C4M_GCT_COLLECT != 0
    c4m_gc_trace(C4M_GCT_COLLECT,
                 "=========== COLLECT START; arena @%p",
                 c4m_current_heap);
    raw_trace(&ctx);
    c4m_gc_trace(C4M_GCT_COLLECT,
                 "=========== COLLECT END; arena @%p\n",
                 c4m_current_heap);
#else
    raw_trace(&ctx);
#endif

#ifdef C4M_FULL_MEMCHECK
    memcheck_delete_old_records(old_arena);
#endif

    uint64_t start = (uint64_t)ctx.to_space;
    uint64_t end   = (uint64_t)ctx.to_space->heap_end;
    uint64_t where = (uint64_t)ctx.to_space->next_alloc;
    uint64_t total = end - start;
    uint64_t inuse = where - start;

    if (((total + (total >> 1)) >> 4) < inuse) {
        ctx.to_space->grow_next = true;
    }

    run_post_collect_hooks();

    c4m_delete_arena(ctx.from_space);

    // Free the worklist.
    char *unmap_s = ((char *)ctx.worklist_start);
    char *unmap_e = ((char *)ctx.worklist_end);
    int   n       = unmap_e - unmap_s;
    munmap(unmap_s, n);
    c4m_gc_trace(C4M_GCT_MUNMAP, "worklist: del @%p (%d items)", unmap_s, n);

#ifdef C4M_GC_STATS
    const int mb        = 0x100000;
    num_migrations      = c4m_total_allocs;
    c4m_total_allocs    = stashed_counter;
    c4m_total_words     = stashed_words;
    c4m_words_requested = stashed_requests;

    c4m_current_heap = ctx.to_space;

    c4m_gc_heap_stats(&live, &available, &new_total);

    c4m_current_heap->legacy_count     = num_migrations;
    c4m_current_heap->starting_counter = stashed_counter;
    c4m_current_heap->start_size       = live / 8;

    c4m_total_garbage_words += (old_used - live) / 8;
    c4m_total_size += (old_total / 8);

    if (!c4m_gc_show_heap_stats_on) {
        return ctx.to_space;
    }

    c4m_printf("\n[h1 u]****Heap Stats****\n");

    c4m_printf(
        "[h2]Pre-collection heap[i] @{:x}:",
        c4m_box_u64((uint64_t)old_arena));

    c4m_printf("[em]{:,}[/] mb used of [em]{:,}[/] mb; ([em]{:,}[/] mb free)",
               c4m_box_u64(old_used / mb),
               c4m_box_u64(old_total / mb),
               c4m_box_u64(old_free / mb));

    c4m_printf(
        "[em]{:,}[/] records, [em]{:,}[/] "
        "migrated ([em]{:,}[/] mb); [em]{:,}[/] new. ([em]{:,}[/] mb)\n",
        c4m_box_u64(old_num_records),
        c4m_box_u64(start_records),
        c4m_box_u64(prev_start_bytes / mb),
        c4m_box_u64(prev_new_allocs),
        c4m_box_u64(prev_used_mem / mb));

    c4m_printf(
        "[h2]Post collection heap [i]@{:x}: ",
        c4m_box_u64((uint64_t)c4m_current_heap));

    c4m_printf(
        "[em]{:,}[/] mb used of [em]{:,}[/] mb; "
        "([b i]{:,}[/] mb free, [b i]{:,}[/] mb collected)",
        c4m_box_u64(live / mb),
        c4m_box_u64(new_total / mb),
        c4m_box_u64(available / mb),
        c4m_box_u64((old_used - live) / mb));

    c4m_printf("[b][i]Copied [em]{:,}[/] records; Trashed [em]{:,}[/]",
               c4m_box_u64(num_migrations),
               c4m_box_u64(old_num_records - num_migrations));

    c4m_printf("[h2]Totals[/h2]\n[b]Total requests:[/] [em]{:,}[/] mb ",
               c4m_box_u64((c4m_words_requested * 8) / mb));

    c4m_printf("[b]Total alloced:[/] [em]{:,}[/] mb",
               c4m_box_u64((c4m_total_words * 8) / mb));

    c4m_printf("[b]Total allocs:[/] [em]{:,}[/]",
               c4m_box_u64(c4m_total_allocs));

    c4m_printf("[b]Total collects:[/] [em]{:,}",
               c4m_box_u64(c4m_total_collects));

    double      u = c4m_total_garbage_words * (double)100.0;
    c4m_utf8_t *gstr;

    // Precision isn't implemented on floats yet.
    u    = u / (double)(c4m_total_size);
    gstr = c4m_cstr_format("{}", c4m_box_double(u));
    gstr = c4m_str_slice(gstr, 0, 5);

    c4m_printf("[b]Collect utilization[/]: [em]{}%[/] [i]garbage",
               gstr);

    c4m_printf("[b]Average allocation size:[/] [em]{:,}[/] bytes",
               c4m_box_u64((c4m_total_words * 8) / c4m_total_allocs));

#endif
    return ctx.to_space;
}

void
c4m_gc_thread_collect()
{
    c4m_current_heap = c4m_collect_arena(c4m_current_heap);
}

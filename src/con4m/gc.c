#include "con4m.h"

// The lock-free dictionary for roots ensures that threads can add
// roots in parallel. However, we currently make an implicit
// assumption that, roots are not going to be added when some thread
// is collecting.
//
// That assumption will get violated when we support dynamic loading,
// and need to register roots dynamically. We will need to implement a
// global lock around this, which probably will be the same as the
// shutdown lock we need to add, that stops all threads to marshal
// state.

uint64_t                         c4m_gc_guard     = 0;
static thread_local c4m_arena_t *current_heap     = NULL;
static c4m_set_t                *external_holds   = NULL;
static c4m_system_finalizer_fn   system_finalizer = NULL;
static uint64_t                  page_bytes;
static uint64_t                  page_modulus;
static uint64_t                  modulus_mask;

#ifdef C4M_GC_FULL_TRACE

int c4m_gc_trace_on = 1;

#endif

typedef struct hook_record_t {
    c4m_gc_hook           post_collect;
    struct hook_record_t *next;
} hook_record_t;

static hook_record_t *c4m_gc_hooks = NULL;

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

#ifdef C4M_GC_STATS
int                   c4m_gc_show_heap_stats_on = 0;
thread_local uint32_t c4m_total_allocs          = 0;
thread_local uint32_t c4m_total_collects        = 0;
thread_local uint64_t c4m_total_words           = 0;
thread_local uint64_t c4m_words_requested       = 0;

uint64_t
c4m_get_alloc_counter()
{
    return c4m_total_allocs - current_heap->starting_counter;
}
#endif

bool
c4m_in_heap(void *p)
{
    return p > ((void *)current_heap) && p < (void *)current_heap->heap_end;
}

void
c4m_get_heap_bounds(uint64_t *start, uint64_t *next, uint64_t *end)
{
    *start = (uint64_t)current_heap;
    *next  = (uint64_t)current_heap->next_alloc;
    *end   = (uint64_t)current_heap->heap_end;
}

void
c4m_get_stack_bounds(uint64_t *top, uint64_t *bottom)
{
    pthread_t self = pthread_self();

#if defined(__linux__)
    pthread_attr_t attrs;
    size_t         size;
    uint64_t       addr;

    pthread_getattr_np(self, &attrs);
    pthread_attr_getstack(&attrs, (void **)&addr, &size);

    *bottom = (uint64_t)addr + size;
    *top    = (uint64_t)addr;
#elif defined(__APPLE__) || defined(BSD)
    // Apple at least has no way to get the thread's attr struct that
    // I can find. But it does provide an API to get at the same data.
    *bottom = (uint64_t)pthread_get_stackaddr_np(self);
    *top    = *bottom - pthread_get_stacksize_np(self);
#endif
}

// This puts a junk call frame on we scan, which on my mac seems
// to be 256 bytes. Playing it safe and not subtracting it out, though.
void
c4m_get_stack_scan_region(uint64_t *top, uint64_t *bottom)
{
    uint64_t local = 0;
    c4m_get_stack_bounds(top, bottom);
    *top = (uint64_t)(&local);
}

void
c4m_gc_heap_stats(uint64_t *used, uint64_t *available, uint64_t *total)
{
    uint64_t start = (uint64_t)current_heap;
    uint64_t end   = (uint64_t)current_heap->heap_end;
    uint64_t cur   = (uint64_t)current_heap->next_alloc;

    if (used != NULL) {
        *used = cur - start;
    }

    if (available != NULL) {
        *available = end - cur;
    }

    if (total != NULL) {
        *total = end - start;
    }
}

static void *
c4m_gc_malloc_wrapper(size_t size, void *arg)
{
    // Hatrack wants a 16-byte aligned pointer. The con4m gc allocator will
    // always produce a 16-byte aligned pointer. The raw allocation header is
    // 48 bytes and its base pointer is always 16-byte aligned.
    return c4m_gc_raw_alloc(size, GC_SCAN_ALL);
}

static void
c4m_gc_free_wrapper(void *oldptr, size_t size, void *arg)
{
    // do nothing; memory is garbage collected
}

static void *
c4m_gc_realloc_wrapper(void *oldptr, size_t oldsize, size_t newsize, void *arg)
{
    return c4m_gc_resize(oldptr, newsize);
}

static pthread_key_t c4m_thread_key;

struct c4m_pthread {
    size_t size;
    char   data[];
};

static void
c4m_thread_release_pthread(void *arg)
{
    pthread_setspecific(c4m_thread_key, NULL);

    struct c4m_pthread *pt = arg;
    mmm_thread_release((mmm_thread_t *)pt->data);
}

static void
c4m_thread_acquire_init_pthread(void)
{
    pthread_key_create(&c4m_thread_key, c4m_thread_release_pthread);
}

static mmm_thread_t *
c4m_thread_acquire(void *aux, size_t size)
{
    static pthread_once_t init = PTHREAD_ONCE_INIT;
    pthread_once(&init, c4m_thread_acquire_init_pthread);

    struct c4m_pthread *pt = pthread_getspecific(c4m_thread_key);
    if (NULL == pt) {
        int len  = sizeof(struct c4m_pthread) + size;
        pt       = calloc(1, len);
        pt->size = size;
        pthread_setspecific(c4m_thread_key, pt);
        mmm_thread_t *r = (mmm_thread_t *)pt->data;
        c4m_gc_register_root(&(r->retire_list), 1);
        return r;
    }

    return (mmm_thread_t *)pt->data;
}

void
c4m_initialize_gc()
{
    static bool once = false;

    if (!once) {
        hatrack_zarray_t     *initial_roots;
        hatrack_mem_manager_t hatrack_manager = {
            .mallocfn  = c4m_gc_malloc_wrapper,
            .zallocfn  = c4m_gc_malloc_wrapper,
            .reallocfn = c4m_gc_realloc_wrapper,
            .freefn    = c4m_gc_free_wrapper,
            .arg       = NULL,
        };

        c4m_gc_guard   = c4m_rand64();
        initial_roots  = hatrack_zarray_new(C4M_MAX_GC_ROOTS,
                                           sizeof(c4m_gc_root_info_t));
        external_holds = c4m_rc_alloc(sizeof(c4m_set_t));
        once           = true;
        page_bytes     = getpagesize();
        page_modulus   = page_bytes - 1; // Page size is always a power of 2.
        modulus_mask   = ~page_modulus;

        int initial_len = C4M_DEFAULT_ARENA_SIZE;
        current_heap    = c4m_new_arena(initial_len, initial_roots);
        c4m_arena_register_root(current_heap, &external_holds, 1);

        mmm_setthreadfns(c4m_thread_acquire, NULL);
        hatrack_setmallocfns(&hatrack_manager);
        c4m_gc_trace(C4M_GCT_INIT, "init:set_guard:%llx", c4m_gc_guard);

        hatrack_set_init(external_holds, HATRACK_DICT_KEY_TYPE_PTR);
    }
}

void
c4m_gc_add_hold(c4m_obj_t obj)
{
    hatrack_set_add(external_holds, obj);
}

void
c4m_gc_remove_hold(c4m_obj_t obj)
{
    hatrack_set_remove(external_holds, obj);
}

// The idea here is once the object unmarshals the object file and
// const objects, it can make the heap up till that point read-only.
// We definitely won't want to allocate anything that will need
// to be writable at runtime...
static void
lock_existing_heap()
{
    uint64_t to_lock       = (uint64_t)current_heap;
    uint64_t words_to_lock = ((uint64_t)(current_heap->next_alloc)) - to_lock;
    int      b_to_lock     = words_to_lock * 8;
    b_to_lock              = c4m_round_up_to_given_power_of_2(getpagesize(),
                                                 b_to_lock);

// This doesn't seem to be working on mac; disallows reads.
#ifdef __linux__
//    mprotect((void *)to_lock, b_to_lock, PROT_READ);
#endif
}

static thread_local c4m_arena_t *stashed_heap;

c4m_arena_t *
c4m_internal_stash_heap()
{
    // This assumes the stashed heap isn't going to be used for allocations
    // until it's returneed.
    stashed_heap = current_heap;
    current_heap = c4m_new_arena(
        C4M_DEFAULT_ARENA_SIZE,
        hatrack_zarray_unsafe_copy(current_heap->roots));

    uint64_t *s = (uint64_t *)stashed_heap;
    uint64_t *e = (uint64_t *)stashed_heap->next_alloc;

    c4m_arena_register_root(current_heap, stashed_heap, e - s);

    return stashed_heap;
}

void
c4m_internal_lock_then_unstash_heap()
{
    lock_existing_heap();
    current_heap = stashed_heap;
}

void
c4m_internal_unstash_heap()
{
    c4m_arena_t *popping = current_heap;

    c4m_arena_remove_root(popping, stashed_heap);
    current_heap = stashed_heap;

    uint64_t *s = (uint64_t *)popping;
    uint64_t *e = (uint64_t *)popping->next_alloc;

    c4m_arena_register_root(current_heap, popping, e - s);
}

void
c4m_internal_set_heap(c4m_arena_t *heap)
{
    current_heap = heap;
}

static void *
raw_arena_alloc(uint64_t len, void **end)
{
    // Add two guard pages to sandwich the alloc.
    size_t total_len  = (size_t)(page_bytes * 2 + len);
    char  *full_alloc = mmap(NULL,
                            total_len,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANON,
                            0,
                            0);

    char *ret   = full_alloc + page_bytes;
    char *guard = full_alloc + total_len - page_bytes;

    mprotect(full_alloc, page_bytes, PROT_NONE);
    mprotect(guard, page_bytes, PROT_NONE);

    *end = guard;

    c4m_gc_trace(C4M_GCT_MMAP,
                 "arena:mmap:@%p-@%p (%p):%llu",
                 full_alloc,
                 full_alloc + total_len,
                 guard,
                 len);

    return ret;
}

c4m_arena_t *
c4m_new_arena(size_t num_words, hatrack_zarray_t *roots)
{
    // Convert words to bytes.
    uint64_t allocation = ((uint64_t)num_words) * 8;

    // We're okay to over-allocate here. We round up to the nearest
    // power of 2 that is a multiple of the page size.

    if (allocation & page_modulus) {
        allocation = (allocation & modulus_mask) + page_bytes;
        num_words  = allocation >> 3;
    }

    void        *arena_end;
    c4m_arena_t *new_arena = raw_arena_alloc(allocation, &arena_end);

    new_arena->next_alloc = (c4m_alloc_hdr *)new_arena->data;
    new_arena->heap_end   = arena_end;

    // new_arena->late_mutations = calloc(sizeof(queue_t), 1);

    // c4m_gc_trace("******** alloc late mutations dict: %p\n",
    //              new_arena->late_mutations);

    // queue_init(new_arena->late_mutations);

    new_arena->roots = roots;

    return new_arena;
}

#if defined(C4M_GC_STATS) || defined(C4M_DEBUG)
#define TRACE_DEBUG_ARGS , debug_file, debug_ln

void *
_c4m_gc_raw_alloc(size_t len, uint64_t *ptr_map, char *debug_file, int debug_ln)

#else
#define TRACE_DEBUG_ARGS

void *
_c4m_gc_raw_alloc(size_t len, uint64_t *ptr_map)

#endif
{
#ifdef C4M_ALLOW_POINTER_MAPS
    return c4m_alloc_from_arena(&current_heap,
                                len,
                                ptr_map,
                                false TRACE_DEBUG_ARGS);
#else
    return c4m_alloc_from_arena(&current_heap,
                                len,
                                GC_SCAN_ALL,
                                false TRACE_DEBUG_ARGS);
#endif
}

#if defined(C4M_GC_STATS) || defined(C4M_DEBUG)
void *
_c4m_gc_raw_alloc_with_finalizer(size_t    len,
                                 uint64_t *ptr_map,
                                 char     *debug_file,
                                 int       debug_ln)
#else
void *
_c4m_gc_raw_alloc_with_finalizer(size_t len, uint64_t *ptr_map)
#endif
{
#ifdef C4M_ALLOW_POINTER_MAPS
    return c4m_alloc_from_arena(&current_heap,
                                len,
                                ptr_map,
                                true TRACE_DEBUG_ARGS);
#else
    return c4m_alloc_from_arena(&current_heap,
                                len,
                                GC_SCAN_ALL,
                                true TRACE_DEBUG_ARGS);
#endif
}

void *
c4m_gc_resize(void *ptr, size_t len)
{
    // We'd like external C code to be able to use our GC. Some things
    // (i.e., openssl) will call realloc(NULL, ...) to get memory
    // for whatever reason.
    if (ptr == NULL) {
        return c4m_gc_raw_alloc(len, GC_SCAN_ALL);
    }
    c4m_alloc_hdr *hdr = &((c4m_alloc_hdr *)ptr)[-1];

    assert(hdr->guard = c4m_gc_guard);

#if defined(C4M_GC_STATS) || defined(C4M_DEBUG)
    char *debug_file = hdr->alloc_file;
    int   debug_ln   = hdr->alloc_line;
#endif

#ifdef C4M_ALLOW_POINTER_MAPS
    void *result = c4m_alloc_from_arena(&current_heap,
                                        len,
                                        hdr->ptr_map,
                                        (bool)hdr->finalize TRACE_DEBUG_ARGS);
#else
    void *result = c4m_alloc_from_arena(&current_heap,
                                        len,
                                        GC_SCAN_ALL,
                                        (bool)hdr->finalize TRACE_DEBUG_ARGS);
#endif
    if (len > 0) {
        size_t bytes = ((size_t)(hdr->next_addr - hdr->data)) * 8;
        memcpy(result, ptr, c4m_min(len, bytes));
    }

    if (hdr->finalize == 1) {
        c4m_alloc_hdr *newhdr = &((c4m_alloc_hdr *)result)[-1];
        newhdr->finalize      = 1;

        c4m_finalizer_info_t *p = current_heap->to_finalize;

        while (p != NULL) {
            if (p->allocation == hdr) {
                p->allocation = newhdr;
                return result;
            }
            p = p->next;
        }
        c4m_unreachable();
    }

    assert(!(hdr->flags & GC_FLAG_REACHED));

    return result;
}

void
c4m_gc_set_finalize_callback(c4m_system_finalizer_fn fn)
{
    system_finalizer = fn;
}

void
c4m_delete_arena(c4m_arena_t *arena)
{
    // TODO-- allocations need to have an arena pointer or thread id
    // for cross-thread to work.
    //
    // c4m_gc_trace("******** delete late mutations dict: %p\n",
    // arena->late_mutations);
    // free(arena->late_mutations);

    char *start = ((char *)arena) - page_bytes;
    char *end   = (char *)arena->heap_end + page_bytes;

    c4m_gc_trace(C4M_GCT_MUNMAP, "arena:delete:%p:%p", start, end);

#if defined(C4M_MADV_ZERO)
    madvise(start, end - start, MADV_ZERO);
    mprotect((void *)start, end - start, PROT_NONE);
#endif
    munmap(start, end - start);

    return;
}

#ifdef C4M_GC_STATS
void
_c4m_arena_register_root(c4m_arena_t *arena,
                         void        *ptr,
                         uint64_t     len,
                         char        *file,
                         int          line)
#else
void
_c4m_arena_register_root(c4m_arena_t *arena, void *ptr, uint64_t len)
#endif
{
    // Len is measured in 64 bit words and must be at least 1.

    c4m_gc_root_info_t *ri;
    hatrack_zarray_new_cell(arena->roots, (void *)&ri);
    ri->num_items = len;
    ri->ptr       = ptr;
#ifdef C4M_GC_STATS
    ri->file = file;
    ri->line = line;
#endif
}

void
c4m_arena_remove_root(c4m_arena_t *arena, void *ptr)
{
    int32_t max = atomic_load(&arena->roots->length);

    for (int i = 0; i < max; i++) {
        c4m_gc_root_info_t *ri = hatrack_zarray_cell_address(arena->roots, i);
        if (ri->ptr == ptr) {
            ri->num_items = 0;
            ri->ptr       = NULL;
        }
    }
}

static inline c4m_alloc_hdr *
header_scan(uint64_t *ptr, uint64_t *stop_location, uint64_t *offset)
{
    uint64_t *p = (uint64_t *)(((uint64_t)ptr) & ~0x0000000000000007);

    while (p > stop_location) {
        if (*p == c4m_gc_guard) {
            c4m_alloc_hdr *result = (c4m_alloc_hdr *)p;

            c4m_gc_trace(
                C4M_GCT_ALLOC_FOUND,
                "find_alloc:%p-%p:start:%p:data:%p:len:%d:total:%d",
                p,
                result->next_addr,
                ptr,
                result->data,
                (int)(((char *)result->next_addr) - (char *)result->data),
                (int)(((char *)result->next_addr) - (char *)result));

            *offset = ((uint64_t)ptr) - ((uint64_t)p);
            return result;
        }
        p -= 1;
    }

    fprintf(stderr,
            "Corrupted con4m heap; could not find an allocation record for "
            "the memory address: %p\n",
            ptr);
    abort();
}

c4m_alloc_hdr *
c4m_find_alloc(void *ptr)
{
    uint64_t offset;

    return header_scan((uint64_t *)ptr, 0, &offset);
}

#if defined(C4M_GC_STATS) || defined(C4M_DEBUG)
c4m_utf8_t *
c4m_gc_alloc_info(void *addr, int *line)
{
    if (!c4m_in_heap(addr)) {
        if (line != NULL) {
            *line = 0;
        }
        return NULL;
    }

    c4m_alloc_hdr *h = c4m_find_alloc(addr);
    if (line != NULL) {
        *line = h->alloc_line;
    }

    return c4m_new_utf8(h->alloc_file);
}
#endif

static c4m_alloc_hdr *
prep_allocation(c4m_alloc_hdr *old, c4m_arena_t *new_arena)
{
    c4m_alloc_hdr *res;

#if defined(C4M_GC_STATS) || defined(C4M_DEBUG)
    char *debug_file = old->alloc_file;
    int   debug_ln   = old->alloc_line;
#endif

#ifdef C4M_ALLOW_POINTER_MAPS
    res = c4m_alloc_from_arena(&new_arena,
                               old->alloc_len,
                               old->ptr_map,
                               (bool)old->finalize
                                   TRACE_DEBUG_ARGS);
#else
    res = c4m_alloc_from_arena(&new_arena,
                               old->alloc_len,
                               GC_SCAN_ALL,
                               (bool)old->finalize
                                   TRACE_DEBUG_ARGS);
#endif

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

// ------
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

static inline bool
value_in_fromspace(c4m_collection_ctx *ctx, void *ptr)
{
    if (ptr >= ctx->fromspc_end || ptr < ctx->fromspc_start) {
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

static inline void
add_copy_to_worklist(c4m_collection_ctx *ctx, c4m_alloc_hdr *hdr)
{
    c4m_gc_trace(C4M_GCT_SCAN_PTR,
                 "Added copy instruction to worklist for %p (wl item @%p).",
                 hdr,
                 ctx->next_item);

    *ctx->next_item++ = hdr;
    *ctx->next_item++ = (void *)~0;

    if (!(((uint64_t)ctx->next_item) & page_modulus)) {
        char *p = (char *)ctx->next_item;
        p -= page_bytes;

        madvise(p, page_bytes, MADV_FREE);
    }

    if (ctx->next_item > ctx->worklist_end) {
        ctx->next_item = ctx->worklist_start;
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

    if (!(((uint64_t)ctx->next_item) & page_modulus)) {
        char *p = (char *)ctx->next_item;
        p -= page_bytes;

        madvise(p, page_bytes, MADV_FREE);
    }

    if (ctx->next_item > ctx->worklist_end) {
        ctx->next_item = ctx->worklist_start;
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
    void **p   = (void **)hdr->data;
    void **end = (void **)hdr->next_addr;
    void  *contents;

    while (p < end) {
        contents = *p;

        // We haven't coppied anything yet.
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

    while (ctx->worklist != ctx->next_item) {
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
}

// This is only used for roots, not for memory allocations.
//
// That's because memory allocations have a hook to help us figure out which cells
// of the allocation may contain pointers.
static void
scan_range_for_allocs(c4m_collection_ctx *ctx, void **start, int num)
{
    for (int i = 0; i < num; i++) {
        if (value_in_fromspace(ctx, *start)) {
            forward_one_allocation(ctx, start);
        }
        start++;
    }
    process_worklist(ctx);
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

    ctx->to_space      = c4m_new_arena((size_t)len, r);
    uint64_t alloc_len = cur->largest_alloc * 32;
    if (alloc_len & page_modulus) {
        alloc_len = (alloc_len & modulus_mask) + page_bytes;
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

    scan_range_for_allocs(ctx, (void **)stack_top, stack_bottom - stack_top);
    ctx->from_space = (void *)~(uint64_t)stash;

    c4m_gc_trace(C4M_GCT_SCAN,
                 "Stack scan end: %p to %p (%lu item(s))",
                 stack_top,
                 stack_bottom,
                 stack_bottom - stack_top);

    if (system_finalizer != NULL) {
        migrate_finalizers(cur, ctx->to_space);
    }
}

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
    uint64_t start_counter    = current_heap->starting_counter;
    uint64_t start_records    = current_heap->legacy_count;
    uint64_t prev_new_allocs  = stashed_counter - start_counter;
    uint64_t prev_start_bytes = current_heap->start_size;
    uint64_t prev_used_mem    = old_used - prev_start_bytes;
    uint64_t old_num_records  = start_records + prev_new_allocs;

    uint64_t num_migrations;

    c4m_total_allocs = 0;
    c4m_total_words  = 0;
    c4m_total_collects++;
#endif

#if defined(C4M_GC_FULL_TRACE) && C4M_GCT_COLLECT != 0
    c4m_gc_trace(C4M_GCT_COLLECT,
                 "=========== COLLECT START; arena @%p",
                 current_heap);
    raw_trace(&ctx);
    c4m_gc_trace(C4M_GCT_COLLECT,
                 "=========== COLLECT END; arena @%p\n",
                 current_heap);
#else
    raw_trace(&ctx);
#endif

    run_post_collect_hooks();

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

    current_heap = ctx.to_space;

    c4m_gc_heap_stats(&live, &available, &new_total);

    current_heap->legacy_count     = num_migrations;
    current_heap->starting_counter = stashed_counter;
    current_heap->start_size       = live / 8;

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
        c4m_box_u64((uint64_t)current_heap));

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

    c4m_printf("[h2]Totals[/h2]\n[b]Total requests:[/][em]{:,}[/] mb ",
               c4m_box_u64((c4m_words_requested * 8) / mb));

    c4m_printf("[b]Total alloced:[/] [em]{:,}[/] mb",
               c4m_box_u64((c4m_total_words * 8) / mb));

    c4m_printf("[b]Total allocs:[/] [em]{:,}[/]",
               c4m_box_u64(c4m_total_allocs));

    c4m_printf("[b]Total collects:[/] [em]{:,}",
               c4m_box_u64(c4m_total_collects));

    c4m_printf("[b]Average allocation size:[/] [em]{:,} bytes",
               c4m_box_u64((c4m_total_words * 8) / c4m_total_allocs));

#endif
    return ctx.to_space;
}

void
c4m_gc_thread_collect()
{
    current_heap = c4m_collect_arena(current_heap);
}

#ifdef C4M_GC_STATS
void
_c4m_gc_register_root(void *ptr, uint64_t num_words, char *f, int l)
{
    c4m_gc_trace(C4M_GCT_REGISTER,
                 "root_register:@%p-%p (%s:%d)",
                 ptr,
                 ptr + num_words,
                 f,
                 l);
    _c4m_arena_register_root(current_heap, ptr, num_words, f, l);
}
#else
void
_c4m_gc_register_root(void *ptr, uint64_t num_words)
{
    c4m_arena_register_root(current_heap, ptr, num_words);
}
#endif

void
c4m_gcm_remove_root(void *ptr)
{
    c4m_arena_remove_root(current_heap, ptr);
}

#if 0 // not used anymore.

thread_local int  ro_test_pipe_fds[2] = {0, 0};
thread_local bool ro_test_pipe_inited = false;

bool
c4m_is_read_only_memory(volatile void *address)
{
    // This works by creating a pipe we can always read from,
    // reading one byte from memory and writing it to the pipe,
    // then reading the byte back out of the pipe to write to
    // the same location.
    //
    // If it's read-only memory, the pipe read() will fail because
    // it cannot store the byte back (EFAULT).
    //
    // This is the only way I know how to do this test without risking
    // crashing the process. However, this is NOT a threadsafe test
    // when the underlying memory is mutable; when re-writing the byte
    // we could be in a race with other threads.
    //
    // Still, that seems like an OK compromise for me; this is used to
    // help validate that, when people are using C keyword parameters,
    // the keyword parameter isn't forgotten (since they must be
    // static string constants). This test FAILING at all will
    // indicate a programmer error.

    char c = ((volatile char *)(address))[0];

    if (!ro_test_pipe_inited) {
        pipe(ro_test_pipe_fds);
        ro_test_pipe_inited = true;
    }

    if (write(ro_test_pipe_fds[1], &c, 1) <= 0) {
        fprintf(stderr, "Memory address %p is invalid.\n", address);
        abort();
    }

    if (read(ro_test_pipe_fds[0], (void *)address, 1) <= 0) {
        if (errno == EFAULT) {
            return true;
        }
    }


    return false;
}
#endif

#if defined(C4M_GC_STATS) || defined(C4M_DEBUG)
void *
c4m_alloc_from_arena(c4m_arena_t   **arena_ptr,
                     size_t          len,
                     const uint64_t *ptr_map,
                     bool            finalize,
                     char           *file,
                     int             line)
#else
// This currently assumes ptr_map doesn't need more than 64 entries.
// Note that len is measured in WORDS not bytes.
void *
c4m_alloc_from_arena(c4m_arena_t   **arena_ptr,
                     size_t          len,
                     const uint64_t *ptr_map,
                     bool            finalize)
#endif
{
#ifdef C4M_DEBUG
    _c4m_watch_scan(file, line);
#endif

    c4m_arena_t *arena = *arena_ptr;

    // Round up to aligned length.
    size_t wordlen = c4m_round_up_to_given_power_of_2(C4M_FORCED_ALIGNMENT,
                                                      len);

// Come back here if, when we trigger the collector, the resulting
// free space isn't enough, in which case we do a second collect.
// There are better ways to handle this like to just grab enough extra
// zero- mapped pages to ensure we get the allocation, but ideally
// people won't ask for such large allocs relative to the arena size
// without just asking for a new arena, so I'm not going to bother
// right now; maybe someday.
try_again:;
    c4m_alloc_hdr *raw  = arena->next_alloc;
    c4m_alloc_hdr *next = (c4m_alloc_hdr *)&(raw->data[wordlen]);

    if (((uint64_t *)next) > arena->heap_end) {
        arena      = c4m_collect_arena(arena);
        *arena_ptr = arena;

        raw  = arena->next_alloc;
        next = (c4m_alloc_hdr *)&(raw->data[wordlen]);
        if (((uint64_t *)next) > arena->heap_end) {
            arena->grow_next = true;
            arena            = c4m_collect_arena(arena);
            *arena_ptr       = arena;
            goto try_again;
        }
    }

    if (len > arena->largest_alloc) {
        arena->largest_alloc = len;
    }

    arena->alloc_count++;
    arena->next_alloc = next;
    raw->guard        = c4m_gc_guard;
    raw->arena        = arena;
    raw->next_addr    = (uint64_t *)arena->next_alloc;
    raw->alloc_len    = wordlen;
    raw->ptr_map      = (uint64_t *)ptr_map;

#ifdef C4M_GC_STATS
    c4m_words_requested += len;
    c4m_total_words += wordlen;

    raw->alloc_file = file;
    raw->alloc_line = line;

    c4m_gc_trace(C4M_GCT_ALLOC,
                 "new_record:%p-%p:data:%p:len:%zu:arena:%p-%p (%s:%d)",
                 raw,
                 raw->next_addr,
                 raw->data,
                 len,
                 arena,
                 arena->heap_end,
                 raw->alloc_file,
                 raw->alloc_line);

    c4m_total_allocs++;
#endif

    if (finalize) {
        c4m_finalizer_info_t *record = c4m_rc_alloc(sizeof(c4m_finalizer_info_t));
        record->allocation           = raw;
        record->next                 = arena->to_finalize;
        arena->to_finalize           = record;
    }

    return (void *)(raw->data);
}

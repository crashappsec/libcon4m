#include "con4m.h"

uint64_t                  c4m_gc_guard     = 0;
thread_local c4m_arena_t *c4m_current_heap = NULL;
uint64_t                  c4m_page_bytes;
uint64_t                  c4m_page_modulus;
uint64_t                  c4m_modulus_mask;

#ifdef C4M_GC_STATS
thread_local uint64_t c4m_total_requested = 0;
thread_local uint64_t c4m_total_alloced   = 0;
thread_local uint32_t c4m_total_allocs    = 0;
#endif

#ifdef C4M_FULL_MEMCHECK
#ifndef C4M_MEMCHECK_RING_SZ
// Must be a power of 2.
#define C4M_MEMCHECK_RING_SZ 64
#endif
#if C4M_MEMCHECK_RING_SZ != 0
#define C4M_USE_RING
#else
#undef C4M_USE_RING
#endif

uint64_t c4m_end_guard;

#ifdef C4M_USE_RING
static thread_local c4m_shadow_alloc_t *memcheck_ring[C4M_MEMCHECK_RING_SZ] = {
    0,
};
static thread_local unsigned int ring_head = 0;
static thread_local unsigned int ring_tail = 0;
#endif

#endif // C4M_FULL_MEMCHECK

static c4m_set_t    *external_holds = NULL;
static pthread_key_t c4m_thread_key;

struct c4m_pthread {
    size_t size;
    char   data[];
};

bool
c4m_in_heap(void *p)
{
    return p > ((void *)c4m_current_heap) && p < (void *)c4m_current_heap->heap_end;
}

void
c4m_get_heap_bounds(uint64_t *start, uint64_t *next, uint64_t *end)
{
    *start = (uint64_t)c4m_current_heap;
    *next  = (uint64_t)c4m_current_heap->next_alloc;
    *end   = (uint64_t)c4m_current_heap->heap_end;
}

void
c4m_get_stack_scan_region(uint64_t *top, uint64_t *bottom)
{
    pthread_t self = pthread_self();

    *bottom = (uint64_t)__builtin_frame_address(0);

#if defined(__linux__)
    pthread_attr_t attrs;
    size_t         size;
    uint64_t       addr;

    pthread_getattr_np(self, &attrs);
    pthread_attr_getstack(&attrs, (void **)&addr, &size);

    *top = (uint64_t)addr;

#elif defined(__APPLE__) || defined(BSD)
    // Apple at least has no way to get the thread's attr struct that
    // I can find. But it does provide an API to get at the same data.
    *top = *bottom - pthread_get_stacksize_np(self);
#endif
}

void
c4m_gc_heap_stats(uint64_t *used, uint64_t *available, uint64_t *total)
{
    uint64_t start = (uint64_t)c4m_current_heap;
    uint64_t end   = (uint64_t)c4m_current_heap->heap_end;
    uint64_t cur   = (uint64_t)c4m_current_heap->next_alloc;

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

#ifdef HATRACK_ALLOC_PASS_LOCATION
void *
c4m_gc_malloc_wrapper(size_t size, void *arg, char *file, int line)
{
    return _c4m_gc_raw_alloc(size, arg, file, line);
}

static void
c4m_gc_free_wrapper(void *oldptr, size_t size, void *arg, char *file, int line)
{
    // do nothing; memory is garbage collected
}

static void *
c4m_gc_realloc_wrapper(void  *oldptr,
                       size_t oldsize,
                       size_t newsize,
                       void  *arg,
                       char  *file,
                       int    line)
{
    return c4m_gc_resize(oldptr, newsize);
}

#else
static void *
c4m_gc_malloc_wrapper(size_t size, void *arg)
{
    // Hatrack wants a 16-byte aligned pointer. The con4m gc allocator will
    // always produce a 16-byte aligned pointer. The raw allocation header is
    // 48 bytes and its base pointer is always 16-byte aligned.
    return c4m_gc_raw_alloc(size * 2, arg);
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

#endif

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

#ifdef C4M_USE_RING
        for (int i = 0; i < C4M_MEMCHECK_RING_SZ; i++) {
            memcheck_ring[i] = NULL;
        }
#endif
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

#ifdef C4M_FULL_MEMCHECK
        c4m_end_guard = c4m_rand64();
#endif
        c4m_gc_guard     = c4m_rand64();
        initial_roots    = hatrack_zarray_new(C4M_MAX_GC_ROOTS,
                                           sizeof(c4m_gc_root_info_t));
        external_holds   = c4m_rc_alloc(sizeof(c4m_set_t));
        once             = true;
        c4m_page_bytes   = getpagesize();
        c4m_page_modulus = c4m_page_bytes - 1; // Page size is always a power of 2.
        c4m_modulus_mask = ~c4m_page_modulus;

        int initial_len  = C4M_DEFAULT_ARENA_SIZE;
        c4m_current_heap = c4m_new_arena(initial_len, initial_roots);
        c4m_arena_register_root(c4m_current_heap, &external_holds, 1);

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
    uint64_t to_lock       = (uint64_t)c4m_current_heap;
    uint64_t words_to_lock = ((uint64_t)(c4m_current_heap->next_alloc)) - to_lock;
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
    stashed_heap     = c4m_current_heap;
    c4m_current_heap = c4m_new_arena(
        C4M_DEFAULT_ARENA_SIZE,
        hatrack_zarray_unsafe_copy(c4m_current_heap->roots));

    uint64_t *s = (uint64_t *)stashed_heap;
    uint64_t *e = (uint64_t *)stashed_heap->next_alloc;

    c4m_arena_register_root(c4m_current_heap, stashed_heap, e - s);

    return stashed_heap;
}

void
c4m_internal_lock_then_unstash_heap()
{
    lock_existing_heap();
    c4m_current_heap = stashed_heap;
}

void
c4m_internal_unstash_heap()
{
    c4m_arena_t *popping = c4m_current_heap;

    c4m_arena_remove_root(popping, stashed_heap);
    c4m_current_heap = stashed_heap;

    uint64_t *s = (uint64_t *)popping;
    uint64_t *e = (uint64_t *)popping->next_alloc;

    c4m_arena_register_root(c4m_current_heap, popping, e - s);
}

void
c4m_internal_set_heap(c4m_arena_t *heap)
{
    c4m_current_heap = heap;
}

static void *
raw_arena_alloc(uint64_t len, void **end)
{
    // Add two guard pages to sandwich the alloc.
    size_t total_len  = (size_t)(c4m_page_bytes * 2 + len);
    char  *full_alloc = mmap(NULL,
                            total_len,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANON,
                            0,
                            0);

    char *ret   = full_alloc + c4m_page_bytes;
    char *guard = full_alloc + total_len - c4m_page_bytes;

    mprotect(full_alloc, c4m_page_bytes, PROT_NONE);
    mprotect(guard, c4m_page_bytes, PROT_NONE);

    *end = guard;

    c4m_gc_trace(C4M_GCT_MMAP,
                 "arena:mmap:@%p-@%p (%p):%llu",
                 full_alloc,
                 full_alloc + total_len,
                 guard,
                 len);

    ASAN_POISON_MEMORY_REGION(((c4m_arena_t *)ret)->data, len);

    return ret;
}

c4m_arena_t *
c4m_new_arena(size_t num_words, hatrack_zarray_t *roots)
{
    // Convert words to bytes.
    uint64_t allocation = ((uint64_t)num_words) * 8;

    // We're okay to over-allocate here. We round up to the nearest
    // power of 2 that is a multiple of the page size.

    if (allocation & c4m_page_modulus) {
        allocation = (allocation & c4m_modulus_mask) + c4m_page_bytes;
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
_c4m_gc_raw_alloc(size_t          len,
                  c4m_mem_scan_fn scan_fn,
                  char           *debug_file,
                  int             debug_ln)

#else
#define TRACE_DEBUG_ARGS

void *
_c4m_gc_raw_alloc(size_t len, c4m_mem_scan_fn scan_fn)

#endif
{
    return c4m_alloc_from_arena(&c4m_current_heap,
                                len,
                                scan_fn,
                                false TRACE_DEBUG_ARGS);
}

#if defined(C4M_GC_STATS) || defined(C4M_DEBUG)
void *
_c4m_gc_raw_alloc_with_finalizer(size_t          len,
                                 c4m_mem_scan_fn scan_fn,
                                 char           *debug_file,
                                 int             debug_ln)
#else
void *
_c4m_gc_raw_alloc_with_finalizer(size_t len, c4m_mem_scan_fn scan_fn)
#endif
{
    return c4m_alloc_from_arena(&c4m_current_heap,
                                len,
                                scan_fn,
                                true TRACE_DEBUG_ARGS);
}

void *
c4m_gc_resize(void *ptr, size_t len)
{
    // We'd like external C code to be able to use our GC. Some things
    // (i.e., openssl) will call realloc(NULL, ...) to get memory
    // for whatever reason.
    if (ptr == NULL) {
        return c4m_gc_raw_alloc(len, C4M_GC_SCAN_ALL);
    }
    c4m_alloc_hdr *hdr = &((c4m_alloc_hdr *)ptr)[-1];

    assert(hdr->guard = c4m_gc_guard);

#if defined(C4M_GC_STATS) || defined(C4M_DEBUG)
    char *debug_file = hdr->alloc_file;
    int   debug_ln   = hdr->alloc_line;
#endif

    void *result = c4m_alloc_from_arena(&c4m_current_heap,
                                        len,
                                        hdr->scan_fn,
                                        (bool)hdr->finalize TRACE_DEBUG_ARGS);
    if (len > 0) {
        size_t bytes = ((size_t)(hdr->next_addr - hdr->data)) * 8;
        memcpy(result, ptr, c4m_min(len, bytes));
    }

    if (hdr->finalize == 1) {
        c4m_alloc_hdr *newhdr = &((c4m_alloc_hdr *)result)[-1];
        newhdr->finalize      = 1;

        c4m_finalizer_info_t *p = c4m_current_heap->to_finalize;

        while (p != NULL) {
            if (p->allocation == hdr) {
                p->allocation = newhdr;
                return result;
            }
            p = p->next;
        }
        c4m_unreachable();
    }

    return result;
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

    char *start = ((char *)arena) - c4m_page_bytes;
    char *end   = (char *)arena->heap_end + c4m_page_bytes;

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
c4m_alloc_hdr *
c4m_find_alloc(void *ptr)
{
    void **p = (void **)(((uint64_t)ptr) & ~0x0000000000000007);

    while (p > (void **)c4m_current_heap) {
        if (*p == (void *)c4m_gc_guard) {
            return (c4m_alloc_hdr *)p;
        }
        p -= 1;
    }

    return NULL;
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
    _c4m_arena_register_root(c4m_current_heap, ptr, num_words, f, l);
}
#else
void
_c4m_gc_register_root(void *ptr, uint64_t num_words)
{
    c4m_arena_register_root(c4m_current_heap, ptr, num_words);
}
#endif

void
c4m_gcm_remove_root(void *ptr)
{
    c4m_arena_remove_root(c4m_current_heap, ptr);
}

#ifdef C4M_FULL_MEMCHECK
static inline void
memcheck_process_ring()
{
    unsigned int cur = ring_tail;

    cur &= ~(C4M_MEMCHECK_RING_SZ - 1);

    while (cur != ring_head) {
        c4m_shadow_alloc_t *a = memcheck_ring[cur++];
        if (!a) {
            return;
        }

        if (a->start->guard != c4m_gc_guard) {
            c4m_alloc_display_front_guard_error(a->start,
                                                a->start->data,
                                                a->file,
                                                a->line,
                                                true);
        }

        if (*a->end != c4m_end_guard) {
            c4m_alloc_display_rear_guard_error(a->start,
                                               a->start->data,
                                               a->len,
                                               a->end,
                                               a->file,
                                               a->line,
                                               true);
        }
    }
}
#endif

#if defined(C4M_GC_STATS) || defined(C4M_DEBUG)
void *
c4m_alloc_from_arena(c4m_arena_t   **arena_ptr,
                     size_t          len,
                     c4m_mem_scan_fn scan_fn,
                     bool            finalize,
                     char           *file,
                     int             line)
#else

// Note that len is measured in WORDS not bytes.
void *
c4m_alloc_from_arena(c4m_arena_t   **arena_ptr,
                     size_t          len,
                     c4m_mem_scan_fn scan_fn,
                     bool            finalize)
#endif
{
    size_t orig_len = len;

    len += sizeof(c4m_alloc_hdr);

#ifdef C4M_DEBUG
    _c4m_watch_scan(file, line);
#endif

#ifdef C4M_FULL_MEMCHECK
    len = len + 8; // Ensure room for sentinel.
#endif
    c4m_arena_t *arena = *arena_ptr;

    // Round up to aligned length.
    len = c4m_round_up_to_given_power_of_2(C4M_FORCED_ALIGNMENT, len);

    size_t         wordlen = len / 8;
    c4m_alloc_hdr *raw     = arena->next_alloc;
    c4m_alloc_hdr *next    = (c4m_alloc_hdr *)&(raw->data[wordlen]);

    if (((uint64_t *)next) > arena->heap_end) {
        arena      = c4m_collect_arena(arena);
        *arena_ptr = arena;

        raw  = arena->next_alloc;
        next = (c4m_alloc_hdr *)&(raw->data[wordlen]);
    }

    if (len > arena->largest_alloc) {
        arena->largest_alloc = len;
    }

    ASAN_UNPOISON_MEMORY_REGION(raw, ((char *)next - (char *)raw));
    arena->alloc_count++;
    arena->next_alloc = next;
    raw->guard        = c4m_gc_guard;
    raw->arena        = arena;
    raw->next_addr    = (uint64_t *)arena->next_alloc;
    raw->alloc_len    = len;
    raw->request_len  = orig_len;
    raw->scan_fn      = scan_fn;

#ifdef C4M_FULL_MEMCHECK
    uint64_t *end_guard_addr = &raw->data[wordlen - 1];

    c4m_shadow_alloc_t *record = c4m_rc_alloc(sizeof(c4m_shadow_alloc_t));
    record->start              = raw;
    record->end                = end_guard_addr;

    record->file = file;
    record->line = line;
    record->len  = orig_len;
    record->next = NULL;
    record->prev = arena->shadow_end;
    *record->end = c4m_end_guard;

#ifdef C4M_WARN_ON_ZERO_ALLOCS
    if (orig_len == 0) {
        fprintf(stderr,
                "Memcheck zero-byte alloc from %s:%d (record @%p)\n",
                file,
                line,
                raw);
    }
#endif
    // Duplicated in the header for spot-checking; this can get corrupted;
    // the out-of-heap list is better, but we don't want to bother searching
    // through the whole heap.
    raw->end_guard_loc = record->end;

    assert(*raw->end_guard_loc == c4m_end_guard);

    if (arena->shadow_end != NULL) {
        arena->shadow_end->next = record;
        arena->shadow_end       = record;
    }
    else {
        arena->shadow_start = record;
        arena->shadow_end   = record;
    }

#ifdef C4M_USE_RING
    memcheck_process_ring();

    memcheck_ring[ring_head++] = record;
    ring_head &= ~(C4M_MEMCHECK_RING_SZ - 1);
    if (ring_tail == ring_head) {
        ring_tail++;
        ring_tail &= ~(C4M_MEMCHECK_RING_SZ - 1);
    }
#endif // C4M_USE_RING
#endif // C4M_FULL_MEMCHECK

#ifdef C4M_GC_STATS
    c4m_total_requested += orig_len;
    c4m_total_alloced += wordlen * 8;

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

        if (arena->to_finalize) {
            arena->to_finalize->prev = record;
        }

        arena->to_finalize = record;
    }

    assert(raw != NULL);
    return (void *)(raw->data);
}

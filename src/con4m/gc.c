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

static void
process_traced_pointer(uint64_t   **addr,
                       uint64_t    *ptr,
                       uint64_t    *start,
                       uint64_t    *end,
                       c4m_arena_t *new_arena);

#ifdef C4M_GC_STATS
int              c4m_gc_show_heap_stats_on = 0;
_Atomic uint32_t c4m_total_allocs          = 0;
_Atomic uint32_t c4m_total_collects        = 0;
_Atomic uint64_t c4m_total_bytes           = 0;

uint64_t
get_alloc_counter()
{
    return current_heap->alloc_counter;
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
    stashed_heap = current_heap;
    current_heap = c4m_new_arena(C4M_DEFAULT_ARENA_SIZE,
                                 hatrack_zarray_unsafe_copy(current_heap->roots));
    int32_t   l  = hatrack_zarray_len(stashed_heap->roots);
    uint64_t *s  = hatrack_zarray_cell_address(stashed_heap->roots, 0);
    uint64_t *e  = hatrack_zarray_cell_address(stashed_heap->roots, l);

    c4m_arena_register_root(current_heap, s, e - s);

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
    current_heap = stashed_heap;
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
                 "arena:mmap:@%p-@%p:%llu",
                 full_alloc,
                 full_alloc + total_len,
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

#ifdef C4M_GC_STATS
    new_arena->alloc_counter = 0;
#endif
    // new_arena->late_mutations = calloc(sizeof(queue_t), 1);

    // c4m_gc_trace("******** alloc late mutations dict: %p\n",
    //              new_arena->late_mutations);

    // queue_init(new_arena->late_mutations);

    new_arena->roots = roots;

    return new_arena;
}

#ifdef C4M_GC_STATS
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

#ifdef C4M_GC_STATS
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

#ifdef C4M_GC_STATS
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
        memcpy(result, ptr, min(len, bytes));
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

#define C4M_MADV_ZERO
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

static inline void
update_internal_allocation_pointers(c4m_alloc_hdr *hdr,
                                    uint64_t      *arena_start,
                                    uint64_t      *arena_end,
                                    c4m_arena_t   *new_arena)
{
    if (((uint64_t *)hdr) == arena_end) {
        return;
    }

    c4m_gc_trace(C4M_GCT_SCAN_PTR,
                 "ptr_update_scan:begin:@%p-@%p:%lu",
                 hdr,
                 hdr->next_addr,
                 (size_t)(((char *)hdr->next_addr) - (char *)hdr->data));

#if defined(C4M_GC_STATS) && (C4M_GCT_OBJ != 0)
    if (hdr->con4m_obj) {
        c4m_base_obj_t *obj = (c4m_base_obj_t *)hdr->data;

        c4m_gc_trace(C4M_GCT_OBJ,
                     "obj_identified:@%p:hdr@%p;type=%s (%s:%d)",
                     obj,
                     hdr,
                     obj->base_data_type->name,
                     hdr->alloc_file,
                     hdr->alloc_line);
    }
#endif

    // Loop through all aligned offsets holding a pointer to see if it
    // points into this heap.
    if (hdr->ptr_map == NULL) {
        return;
    }
    if (hdr->ptr_map == GC_SCAN_ALL) {
        uint64_t **p = (uint64_t **)hdr->data;

        while (p < (uint64_t **)hdr->next_addr) {
            if ((*p) >= arena_start && (*p) <= arena_end) {
                c4m_gc_trace(C4M_GCT_PTR_TO_MOVE,
                             "ptr_update_one:in_heap_yes:@%p:->%p",
                             p,
                             *p);

                process_traced_pointer(p,
                                       *p,
                                       arena_start,
                                       arena_end,
                                       new_arena);
            }
            p++;
        }
        return;
    }

    size_t   map_ix  = 0;
    size_t   offset  = 0;
    uint64_t map_len = hdr->ptr_map[map_ix++];
    uint64_t w;

    for (uint64_t i = 0; i < map_len; i++) {
        w = hdr->ptr_map[map_ix++];
        while (w) {
            int      clz    = __builtin_clzll(w);
            uint64_t tomask = 1LLU << (63 - clz);

            uint64_t **ploc = (uint64_t **)(&hdr->data[offset + clz]);
            w &= ~tomask;

            c4m_gc_trace(C4M_GCT_PTR_TEST,
                         "ptr_update_one:in_heap_test:@%p:->%p:%zu",
                         ploc,
                         *ploc,
                         offset);

            if ((*ploc) >= arena_start && (*ploc) <= arena_end) {
                c4m_gc_trace(C4M_GCT_PTR_TO_MOVE,
                             "ptr_update_one:in_heap_yes:@%p:->%p:%zu",
                             ploc,
                             *ploc,
                             offset);
                process_traced_pointer(ploc,
                                       *ploc,
                                       arena_start,
                                       arena_end,
                                       new_arena);
            }
        }
        offset += 64;
    }
    c4m_gc_trace(C4M_GCT_SCAN_PTR,
                 "ptr_update_scan:end:@%p-@%p:%lu",
                 hdr,
                 hdr->next_addr,
                 (size_t)(((char *)hdr->next_addr) - (char *)hdr->data));
}

static inline void
update_traced_pointer(uint64_t **addr, uint64_t **expected, uint64_t *new)
{
    c4m_gc_trace(C4M_GCT_MOVE, "replace_ptr:@%p:%p->%p", addr, *addr, new);
    *addr = new;
}

static inline c4m_alloc_hdr *
header_scan(uint64_t *ptr, uint64_t *stop_location, uint64_t *offset)
{
    uint64_t *p = (uint64_t *)(((uint64_t)ptr) & ~0x000000000000000f);

    // First header is always at least a full word behind the pointer.
    --p;

    while (p >= stop_location) {
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

static void
process_traced_pointer(uint64_t   **addr,
                       uint64_t    *ptr,
                       uint64_t    *start,
                       uint64_t    *end,
                       c4m_arena_t *new_arena)
{
    uint64_t offset = 0;

    if (ptr < start || ptr > end) {
        return;
    }

    c4m_gc_trace(C4M_GCT_PTR_TEST, "ptr_check_start:%p:@%p", ptr, addr);

    c4m_alloc_hdr *hdr         = header_scan(ptr, start, &offset);
    uint32_t       found_flags = atomic_load(&hdr->flags);

    if (found_flags & GC_FLAG_REACHED) {
        c4m_gc_trace(C4M_GCT_PTR_TEST,
                     "ptr_check_dupe:%p:@%p:record:%p",
                     ptr,
                     addr,
                     hdr);
        // We're already moving / moved, so update the root's pointer
        // to the new heap.
        //
        // If the pointer is being used by another thread,
        // we don't have to re-check, because  when other threads
        // reference an alloc in our heap, they're supposed to
        // add a note so that we can double check. Of course,
        // we're not supporting cross-heap access quite yet
        // anyway...
        uint64_t *new_ptr = hdr->fw_addr->data + (ptr - (uint64_t *)hdr->data);

        c4m_gc_trace(C4M_GCT_PTR_TO_MOVE,
                     "move:%p->%p (%s:%d)",
                     ptr,
                     new_ptr,
                     hdr->alloc_file,
                     hdr->alloc_line);
        c4m_gc_trace(C4M_GCT_PTR_TO_MOVE,
                     "distance from alloc: %p vs %p (%s:%d)",
                     (void *)(ptr - (uint64_t *)hdr->data),
                     (void *)(new_ptr - (uint64_t *)hdr->fw_addr->data),
                     hdr->alloc_file,
                     hdr->alloc_line);

        update_traced_pointer(addr, (uint64_t **)ptr, new_ptr);

        return;
    }

    c4m_gc_trace(C4M_GCT_SCAN_PTR,
                 "process_pointer:%p:@%p:record:%p (%s:%d)",
                 ptr,
                 addr,
                 hdr,
                 hdr->alloc_file,
                 hdr->alloc_line);

    // We haven't moved this allocation, so we try to write-lock the
    // cell and mark it as collecting.
    //
    // If we don't get the lock the first time, we XOR in the
    // fact that we're waiting (ignoring the lock).
    // That will prevent anyone else from winning the lock.
    //
    // Then we spin until the write thread is done.
    uint32_t flags = GC_FLAG_COLLECTING | GC_FLAG_REACHED | GC_FLAG_WRITER_LOCK;

    if (!CAS(&(hdr->flags), &found_flags, flags)) {
        c4m_gc_trace(C4M_GCT_PTR_THREAD,
                     "!!!!! busy wait; mutation in progress for alloc @%p",
                     hdr);
        atomic_fetch_xor(&(hdr->flags), GC_FLAG_OWNER_WAITING);
        do {
            found_flags = GC_FLAG_OWNER_WAITING;
        } while (!CAS(&(hdr->flags), &found_flags, flags));
    }

    uint64_t len = sizeof(uint64_t) * (uint64_t)(hdr->next_addr - hdr->data);
    c4m_gc_trace(C4M_GCT_PTR_THREAD,
                 "!!!!!! Shut off fromspace writes to alloc @%p; "
                 "about to alloc %lld bytes from new heap.",
                 hdr,
                 len);
#ifdef C4M_GC_STATS
    char *debug_file = hdr->alloc_file;
    int   debug_ln   = hdr->alloc_line;
#endif

#ifdef C4M_ALLOW_POINTER_MAPS
    c4m_alloc_hdr *forward = c4m_alloc_from_arena(&new_arena,
                                                  len / 8,
                                                  hdr->ptr_map,
                                                  (bool)hdr->finalize
                                                      TRACE_DEBUG_ARGS);
#else
    c4m_alloc_hdr *forward = c4m_alloc_from_arena(&new_arena,
                                                  len / 8,
                                                  GC_SCAN_ALL,
                                                  (bool)hdr->finalize
                                                      TRACE_DEBUG_ARGS);
#endif

    // Forward before we descend...
    // Set the hw address to the start of the header though.
    forward = &forward[-1];

    hdr->fw_addr = forward;

    uint64_t *new_ptr = forward->data + (ptr - (uint64_t *)hdr->data);

    c4m_gc_trace(C4M_GCT_PTR_TO_MOVE,
                 "needs_fw:ptr:@%p:new_ptr:@%p:len:%llx:record:"
                 "@%p:@newrecord:@%p:tospace:@%p (%s:%d)",
                 ptr,
                 new_ptr,
                 len,
                 hdr,
                 forward,
                 new_arena,
                 hdr->alloc_file,
                 hdr->alloc_line);

    c4m_gc_trace(C4M_GCT_PTR_TO_MOVE,
                 "distance from alloc: %p vs %p (%s:%d)",
                 (void *)(ptr - (uint64_t *)hdr->data),
                 (void *)(new_ptr - (uint64_t *)hdr->fw_addr->data),
                 hdr->alloc_file,
                 hdr->alloc_line);

    update_traced_pointer(addr, (uint64_t **)ptr, new_ptr);

#ifdef C4M_GC_STATS
    c4m_gc_trace(C4M_GCT_MOVED,
                 "Alloc from %s:%d moved from %p to %p\n",
                 hdr->alloc_file,
                 hdr->alloc_line,
                 ptr,
                 forward);
#endif
    update_internal_allocation_pointers(hdr, start, end, new_arena);

#ifdef C4M_GC_STATS
    new_arena->alloc_counter++;
#endif

    memcpy(forward->data, hdr->data, len);
    atomic_store(&hdr->flags,
                 GC_FLAG_COLLECTING | GC_FLAG_REACHED | GC_FLAG_MOVED);
}

static inline void
scan_arena(c4m_arena_t *old,
           c4m_arena_t *new,
           uint64_t *stack_top,
           uint64_t *stack_bottom)
{
    // TODO: should have a debug option that keeps a dict with
    // all valid allocations and ensures them.
    //
    // Currently, we are not registering cross-thread references,
    // or cycling through them.

    c4m_gc_trace(C4M_GCT_SCAN,
                 "arena_scan_start:fromspace:@%p:tospace:@%p",
                 old,
                 new);
    new->roots = old->roots;

    uint64_t *start     = old->data;
    uint64_t *end       = old->heap_end;
    uint32_t  num_roots = hatrack_zarray_len(old->roots);

    for (uint32_t i = 0; i < num_roots; i++) {
        c4m_gc_root_info_t *ri   = hatrack_zarray_cell_address(old->roots, i);
        uint64_t          **root = (uint64_t **)ri->ptr;
        uint64_t            size = ri->num_items;

        c4m_gc_trace(C4M_GCT_SCAN,
                     "root_scan_start:%lld item(s)@%p (%s:%d)",
                     size,
                     root,
                     ri->file,
                     ri->line);

        for (uint64_t j = 0; j < size; j++) {
            uint64_t **ptr = root + j;

            if (*ptr >= start && *ptr <= end) {
                process_traced_pointer(ptr, *ptr, start, end, new);
            }
        }
    }

    c4m_gc_trace(C4M_GCT_SCAN,
                 "stack_scan_start:@%p",
                 stack_top);

    uint64_t *p = stack_top;
    while (p != stack_bottom) {
        if (((uint64_t *)*p) >= start && ((uint64_t *)*p) <= end) {
            c4m_gc_trace(C4M_GCT_SCAN, "stack_scan_item:@%p (%llx)", p, *p);

            process_traced_pointer((uint64_t **)p,
                                   (uint64_t *)*p,
                                   start,
                                   end,
                                   new);
        }
        p++;
    }

    c4m_gc_trace(C4M_GCT_SCAN,
                 "stack_scan_end:@%p",
                 stack_bottom);

    uint64_t old_len = old->heap_end - old->data;
    uint64_t new_len = ((uint64_t *)new->next_alloc) - new->data;

    if (old_len < (new_len << 1)) {
        new->grow_next = true;
    }
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

// This is inlined because the ifdef C4M_GC_STATS needs to be in
// one block, or else my compiler generates bad code.

static inline void
raw_trace(c4m_arena_t **ptr_loc)
{
    c4m_arena_t      *cur = *ptr_loc;
    uint64_t          len = cur->heap_end - (uint64_t *)cur;
    hatrack_zarray_t *r   = cur->roots;
    c4m_arena_t *new;

    if (cur->grow_next) {
        len <<= 1;
    }

    new = c4m_new_arena((size_t)len, r);

    uint64_t *stack_top, *stack_bottom;

    c4m_get_stack_scan_region((uint64_t *)&stack_top,
                              (uint64_t *)&stack_bottom);
    scan_arena(cur, new, stack_top, stack_bottom);
    if (system_finalizer != NULL) {
        migrate_finalizers(cur, new);
    }
    c4m_delete_arena(*ptr_loc);
    *ptr_loc = new;
}

void
c4m_collect_arena(c4m_arena_t **ptr_loc)
{
#ifdef C4M_GC_STATS
    uint64_t used, available, total, live;
    uint64_t allocs_pre = 0, allocs_post = 0;
    c4m_gc_heap_stats(&used, &available, &total);
    allocs_pre = get_alloc_counter();
    atomic_fetch_add(&c4m_total_allocs, allocs_pre);
    atomic_fetch_add(&c4m_total_collects, 1);
    // clang-format off
    atomic_fetch_add(&c4m_total_bytes,
                     (current_heap->heap_end -
		      (uint64_t *)current_heap) * sizeof(uint64_t));
    // clang-format on
#endif

#if defined(C4M_GC_FULL_TRACE) && C4M_GCT_COLLECT != 0
    c4m_gc_trace(C4M_GCT_COLLECT,
                 "=========== COLLECT START; arena @%p",
                 current_heap);

    raw_trace(ptr_loc);

    c4m_gc_trace(C4M_GCT_COLLECT,
                 "=========== COLLECT END; arena @%p\n",
                 current_heap);
#else
    raw_trace(ptr_loc);
#endif

#ifdef C4M_GC_STATS
    if (c4m_gc_show_heap_stats_on) {
        c4m_gc_heap_stats(&live, NULL, NULL);
        allocs_post = get_alloc_counter();

        c4m_printf(
            "[b]Heap Usage:[/] [em]{:,} kb[/] of [i]{:,} "
            "kb[/] ({:,} kb free)",
            c4m_box_u64(used / 1024),
            c4m_box_u64(total / 1024),
            c4m_box_u64(available / 1024));
        c4m_printf(
            "[b][i] Copied [em]{:,}[/] allocation records. "
            "Trashed [em]{:,}[/] records.",
            c4m_box_u64(allocs_post),
            c4m_box_u64(allocs_pre - allocs_post));
        c4m_printf(
            "[b]New Usage:[/] [em]{:,} kb[/] ([i]{} kb[/] collected)",
            c4m_box_u64(live / 1024),
            c4m_box_u64((used - live) / 1024));
        c4m_printf("[b]Total allocs:[/] [em]{}[/] [b]Mb alloced:[/] [em]{}",
                   c4m_box_u64(atomic_load(&c4m_total_allocs)),
                   c4m_box_u64(atomic_load(&c4m_total_bytes) >> 20));
        c4m_printf("[b]Total collects:[/] [em]{}",
                   c4m_box_u64(atomic_load(&c4m_total_collects)));
    }

#endif
}

void
c4m_gc_thread_collect()
{
    c4m_collect_arena(&current_heap);
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

#if 0 // not used anymore.

__thread int  ro_test_pipe_fds[2] = {0, 0};
__thread bool ro_test_pipe_inited = false;

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

#ifdef C4M_GC_STATS
void *
c4m_alloc_from_arena(c4m_arena_t   **arena_ptr,
                     size_t          len,
                     const uint64_t *ptr_map,
                     bool            finalize,
                     char           *file,
                     int             line)
#else
// This currently assumes ptr_map doesn't need more than 64 entries.
void *
c4m_alloc_from_arena(c4m_arena_t   **arena_ptr,
                     size_t          len,
                     const uint64_t *ptr_map,
                     bool            finalize)
#endif
{
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
        c4m_collect_arena(arena_ptr);
        arena = *arena_ptr;

        raw  = arena->next_alloc;
        next = (c4m_alloc_hdr *)&(raw->data[wordlen]);
        if (((uint64_t *)next) > arena->heap_end) {
            arena->grow_next = true;
            c4m_collect_arena(arena_ptr);
            arena = *arena_ptr;
            goto try_again;
        }
    }

    arena->next_alloc = next;
    raw->guard        = c4m_gc_guard;
    raw->arena        = arena;
    raw->next_addr    = (uint64_t *)arena->next_alloc;
    raw->alloc_len    = wordlen;
    raw->ptr_map      = (uint64_t *)ptr_map;

#ifdef C4M_GC_STATS
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

    arena->alloc_counter++;
#endif

    if (finalize) {
        c4m_finalizer_info_t *record = c4m_rc_alloc(sizeof(c4m_finalizer_info_t));
        record->allocation           = raw;
        record->next                 = arena->to_finalize;
        arena->to_finalize           = record;
    }

    return (void *)(raw->data);
}

#ifdef C4M_GC_FULL_TRACE

int c4m_gc_trace_on = 1;

#endif

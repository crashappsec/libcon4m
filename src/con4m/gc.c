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

static c4m_dict_t               *global_roots;
uint64_t                         c4m_gc_guard     = 0;
static thread_local c4m_arena_t *current_heap     = NULL;
static c4m_set_t                *external_holds   = NULL;
static c4m_system_finalizer_fn   system_finalizer = NULL;
static uint64_t                  page_bytes;
static uint64_t                  page_modulus;
static uint64_t                  modulus_mask;

#ifdef C4M_ALLOC_STATS
uint64_t
get_alloc_counter()
{
    return current_heap->alloc_counter;
}
#endif

static void
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

void
c4m_initialize_gc()
{
    static bool once = false;

    if (!once) {
        c4m_gc_guard   = c4m_rand64();
        global_roots   = c4m_rc_alloc(sizeof(c4m_dict_t));
        external_holds = c4m_rc_alloc(sizeof(c4m_set_t));
        once           = true;
        page_bytes     = getpagesize();
        page_modulus   = page_bytes - 1; // Page size is always a power of 2.
        modulus_mask   = ~page_modulus;

        c4m_gc_trace("init:set_guard:%llx", c4m_gc_guard);
        c4m_gc_trace("init:global_root_addr:@%p", global_roots);

        hatrack_dict_init(global_roots, HATRACK_DICT_KEY_TYPE_PTR);
        hatrack_set_init(external_holds, HATRACK_DICT_KEY_TYPE_PTR);
        hatrack_dict_put(global_roots, &external_holds, (void *)1);
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
    b_to_lock              = c4m_round_up_to_given_power_of_2(getpagesize(), b_to_lock);
    mprotect((void *)to_lock, b_to_lock, PROT_READ);
}

static thread_local c4m_arena_t *stashed_heap;

c4m_arena_t *
c4m_internal_stash_heap()
{
    stashed_heap = current_heap;
    current_heap = c4m_new_arena(C4M_DEFAULT_ARENA_SIZE,
                                 c4m_rc_ref(global_roots));
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

    c4m_gc_trace("arena:mmap:@%p-@%p:%llu", ret, ret + len, len);

    return ret;
}

c4m_arena_t *
c4m_new_arena(size_t num_words, c4m_dict_t *roots)
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

    new_arena->next_alloc    = (c4m_alloc_hdr *)new_arena->data;
    new_arena->heap_end      = arena_end;
    new_arena->alloc_counter = 0;
    // new_arena->late_mutations = calloc(sizeof(queue_t), 1);

    // c4m_gc_trace("******** alloc late mutations dict: %p\n",
    //              new_arena->late_mutations);

    // queue_init(new_arena->late_mutations);

    if (roots == NULL) {
        roots = global_roots;
    }

    new_arena->roots = roots;

    return new_arena;
}

void *
c4m_gc_raw_alloc(size_t len, uint64_t *ptr_map)
{
#ifdef ALLOW_POINTER_MAPS
    return c4m_alloc_from_arena(&current_heap, len, ptr_map, false);
#else
    return c4m_alloc_from_arena(&current_heap, len, GC_SCAN_ALL, false);
#endif
}

void *
c4m_gc_raw_alloc_with_finalizer(size_t len, uint64_t *ptr_map)
{
#ifdef ALLOW_POINTER_MAPS
    return c4m_alloc_from_arena(&current_heap, len, ptr_map, true);
#else
    return c4m_alloc_from_arena(&current_heap, len, GC_SCAN_ALL, true);
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

#ifdef ALLOW_POINTER_MAPS
    void *result = c4m_alloc_from_arena(&current_heap,
                                        len,
                                        hdr->ptr_map,
                                        (bool)hdr->finalize);
#else
    void *result = c4m_alloc_from_arena(&current_heap,
                                        len,
                                        GC_SCAN_ALL,
                                        (bool)hdr->finalize);
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
    // TODO-- need to make this use mmap now.
    c4m_gc_trace("arena:skip_unmap");

    if (arena->roots != NULL) {
        c4m_rc_free_and_cleanup(arena->roots,
                                (cleanup_fn)hatrack_dict_cleanup);
    }
    // c4m_gc_trace("******** delete late mutations dict: %p\n",
    // arena->late_mutations);
    // free(arena->late_mutations);

#if defined(MADV_ZERO_WIRED_PAGES)
    char *start = ((char *)arena) - page_bytes;
    char *end   = ((char *)arena->heap_end) - page_bytes;
    madvise(start, end - start, MADV_ZERO_WIRED_PAGES);
#endif

    return;
}

void
c4m_arena_register_root(c4m_arena_t *arena, void *ptr, uint64_t len)
{
    // Len is measured in 64 bit words and must be at least 1.

    if (arena->roots == NULL) {
        arena->roots = c4m_rc_ref(global_roots);
    }

    hatrack_dict_put(arena->roots, ptr, (void *)len);
}

static void
process_traced_pointer(uint64_t   **addr,
                       uint64_t    *ptr,
                       uint64_t    *arena_start,
                       uint64_t    *arena_end,
                       c4m_arena_t *new_arena);

static inline void
update_internal_allocation_pointers(c4m_alloc_hdr *hdr,
                                    uint64_t      *arena_start,
                                    uint64_t      *arena_end,
                                    c4m_arena_t   *new_arena)
{
    if (((uint64_t *)hdr) == arena_end) {
        return;
    }

    c4m_gc_trace("ptr_update_scan:begin:@%p-@%p:%lu",
                 hdr,
                 hdr->next_addr,
                 (size_t)(((char *)hdr->next_addr) - (char *)hdr->data));

    // Loop through all aligned offsets holding a pointer to see if it
    // points into this heap.
    if (hdr->ptr_map == NULL) {
        return;
    }
    if (hdr->ptr_map == GC_SCAN_ALL) {
        uint64_t **p = (uint64_t **)hdr->data;

        while (p < (uint64_t **)hdr->next_addr) {
            process_traced_pointer(p, *p, arena_start, arena_end, new_arena);
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

            c4m_gc_trace("ptr_update_one:in_heap_test:@%p:->%p:%zu",
                         ploc,
                         *ploc,
                         offset);

            if ((*ploc) >= arena_start && (*ploc) <= arena_end) {
                c4m_gc_trace("ptr_update_one:in_heap_yes:@%p:->%p:%zu",
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
    c4m_gc_trace("ptr_update_scan:end:@%p-@%p:%lu",
                 hdr,
                 hdr->next_addr,
                 (size_t)(((char *)hdr->next_addr) - (char *)hdr->data));
}

static inline void
update_traced_pointer(uint64_t **addr, uint64_t **expected, uint64_t *new)
{
    // Hmm, the CAS isn't working; alignment issue?

    c4m_gc_trace("replace_ptr:@%p:%p->%p", addr, *addr, new);
    *addr = new;
    /* printf("Going to update the pointer at %p from %p to %p\n", */
    /* 	   addr, expected, new); */
    /* _Atomic(uint64_t *)* atomic_root = (_Atomic(uint64_t *) *) addr; */

    /* assert(CAS(atomic_root, expected, new)); */
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

static void
process_traced_pointer(uint64_t   **addr,
                       uint64_t    *ptr,
                       uint64_t    *start,
                       uint64_t    *end,
                       c4m_arena_t *new_arena)
{
    if (ptr < start || ptr > end) {
        return;
    }

    uint64_t offset = 0;

    c4m_gc_trace("ptr_check_start:%p:@%p", ptr, addr);

    c4m_alloc_hdr *hdr         = header_scan(ptr, start, &offset);
    uint32_t       found_flags = atomic_load(&hdr->flags);

    if (found_flags & GC_FLAG_REACHED) {
        c4m_gc_trace("ptr_check_dupe:%p:@%p:record:%p", ptr, addr, hdr);
        // We're already moving / moved, so update the root's pointer
        // to the new heap.
        //
        // If the pointer is being used by another thread,
        // we don't have to re-check, because  when other threads
        // reference an alloc in our heap, they're supposed to
        // add a note so that we can double check. Of course,
        // we're not supporting cross-heap access quite yet
        // anyway...
        uint64_t *new_ptr = hdr->fw_addr + (ptr - hdr->data);

        update_traced_pointer(addr, (uint64_t **)ptr, new_ptr);
        return;
    }

    c4m_gc_trace("process_pointer:%p:@%p:record:%p", ptr, addr, hdr);

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
        c4m_gc_trace("!!!!! busy wait; mution in progress for alloc @%p", hdr);
        atomic_fetch_xor(&(hdr->flags), GC_FLAG_OWNER_WAITING);
        do {
            found_flags = GC_FLAG_OWNER_WAITING;
        } while (!CAS(&(hdr->flags), &found_flags, flags));
    }

    c4m_gc_trace("!!!!!! Shut off fromspace writes to alloc @%p", hdr);

    uint64_t len = sizeof(uint64_t) * (uint64_t)(hdr->next_addr - hdr->data);

#ifdef ALLOW_POINTER_MAPS
    uint64_t *forward = c4m_alloc_from_arena(&new_arena,
                                             len / 8,
                                             hdr->ptr_map,
                                             (bool)hdr->finalize);
#else
    uint64_t *forward = c4m_alloc_from_arena(&new_arena,
                                             len / 8,
                                             GC_SCAN_ALL,
                                             (bool)hdr->finalize);
#endif

    // Forward before we descend.
    hdr->fw_addr      = forward;
    uint64_t *new_ptr = forward + (ptr - hdr->data);

    c4m_gc_trace(
        "needs_fw:ptr:@%p:new_ptr:@%p:len:%llx:record:@%p:@newrecord:@%p"
        ":tospace:@%p",
        ptr,
        new_ptr,
        len,
        hdr,
        &(((c4m_alloc_hdr *)forward)[-1]),
        new_arena);

    update_traced_pointer(addr, (uint64_t **)ptr, new_ptr);

    update_internal_allocation_pointers(hdr, start, end, new_arena);

#ifdef C4M_ALLOC_STATS
    new_arena->alloc_counter++;
#endif

    memcpy(forward, hdr->data, len);
    atomic_store(&hdr->flags,
                 GC_FLAG_COLLECTING | GC_FLAG_REACHED | GC_FLAG_MOVED);
}

static inline void
scan_arena(c4m_arena_t *old,
           c4m_arena_t *new,
           hatrack_dict_item_t *roots,
           uint64_t             num_roots,
           uint64_t            *stack_top,
           uint64_t            *stack_bottom)
{
    // TODO: should have a debug option that keeps a dict with
    // all valid allocations and ensures them.
    //
    // Currently, we are not registering cross-thread references,
    // or cycling through them.

    c4m_gc_trace("arena_scan_start:fromspace:@%p:tospace:@%p", old, new);
    uint64_t *start = old->data;
    uint64_t *end   = old->heap_end;

    for (uint64_t i = 0; i < num_roots; i++) {
        uint64_t **root = roots[i].key;
        uint64_t   size = (uint64_t)roots[i].value;

        for (uint64_t j = 0; j < size; j++) {
            uint64_t **ptr = root + j;

            c4m_gc_trace("root_scan_start:@%p", ptr);
            process_traced_pointer(ptr, *ptr, start, end, new);
            c4m_gc_trace("root_scan_done:@%p", ptr);
        }

        c4m_gc_trace("stack_scan_start:@%p", stack_top);
        uint64_t *p = stack_top;
        while (p != stack_bottom) {
            process_traced_pointer((uint64_t **)p,
                                   (uint64_t *)*p,
                                   start,
                                   end,
                                   new);
            p++;
        }
    }

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

void
c4m_collect_arena(c4m_arena_t **ptr_loc)
{
    c4m_arena_t *cur = *ptr_loc;
    uint64_t     len = cur->heap_end - (uint64_t *)cur;
    c4m_dict_t  *r   = cur->roots;
    c4m_arena_t *new;
    uint64_t             num_roots = 0;
    hatrack_dict_item_t *roots;

    if (r == NULL) {
        r = c4m_rc_ref(global_roots);
    }

    if (cur->grow_next) {
        len <<= 1;
    }

    new = c4m_new_arena((size_t)len, r);

    roots = hatrack_dict_items_nosort(r, &num_roots);

    uint64_t *stack_top, *stack_bottom;

    c4m_get_stack_scan_region((uint64_t *)&stack_top,
                              (uint64_t *)&stack_bottom);
    scan_arena(cur, new, roots, num_roots, stack_top, stack_bottom);
    if (system_finalizer != NULL) {
        migrate_finalizers(cur, new);
    }
    c4m_delete_arena(*ptr_loc);
    *ptr_loc = new;
}

void
c4m_gc_thread_collect()
{
    c4m_gc_trace("=========== COLLECT START; arena @%p", current_heap);
    c4m_collect_arena(&current_heap);
    c4m_gc_trace("=========== COLLECT END; arena @%p", current_heap);
}

void
c4m_gc_register_root(void *ptr, uint64_t num_words)
{
    c4m_gc_trace("root_register:@%p", ptr);
    c4m_arena_register_root(current_heap, ptr, num_words);
}

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

// This currently assumes ptr_map doesn't need more than 64 entries.
void *
c4m_alloc_from_arena(c4m_arena_t   **arena_ptr,
                     size_t          len,
                     const uint64_t *ptr_map,
                     bool            finalize)
{
    c4m_arena_t *arena = *arena_ptr;

    // Round up to aligned length.
    size_t wordlen = c4m_round_up_to_given_power_of_2(C4M_FORCED_ALIGNMENT,
                                                      len);

    if (arena == 0) {
        int initial_len = max(C4M_DEFAULT_ARENA_SIZE, wordlen << 4);
        arena           = c4m_new_arena(initial_len, NULL);
        *arena_ptr      = arena;
    }

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

    c4m_gc_trace("new_record:%p-%p:data:%p:len:%zu:arena:%p-%p",
                 raw,
                 raw->next_addr,
                 raw->data,
                 len,
                 arena,
                 arena->heap_end);

#ifdef C4M_ALLOC_STATS
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

#ifdef GC_TRACE

int c4m_gc_trace_on = 1;

#endif

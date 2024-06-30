#pragma once

#include "con4m.h"
/*
 * This is not necessarily the final algorithm, just initial notes
 * that aren't yet fully consistent, since this is a work in progress.
 *
 * My goal here is to support a very simple garbage collection scheme
 * where individual threads can control their own garbage collection,
 * but still hold cross-thread references.
 *
 * The basic approach is that each thread gets its own set of memory
 * arenas that it allocates from. It only allocates from a single
 * arena at a time, and can add new arenas when it runs out of room.
 *
 * It can also decide when to 'compact' its arenas, at which point it
 * marks each arena as 'collecting'. It creates a new arena to copy
 * live data into. We take a very straightforward copy-collector
 * approach with our own heap, treating the arena's list of
 * cross-references as roots; forwarding information is written INTO
 * the cross-reference, in case the other thread decides to drop the
 * reference before its next access.
 *
 * Currently, cross-thread references cannot occur at the same time as
 * a migration. Essentially, there is a single per-allocation
 * write-lock that doesn't affect in-thread writes to the thread's
 * arenas.
 *
 * As of now, we use spin locks for this. A thread holds the lock if
 * it writes its own ID into the lock successfully via CAS, and it
 * spins until it is successful. Once it's successful, it performs its
 * action, and then unlocks.
 *
 * This means that only one thread can be writing to any given
 * thread's heap at a time.
 *
 * If a thread decides it wants to compact, but cannot obtain the
 * lock, it writes a bit into the lock to indicate that no
 * cross-thread references may grab the lock, even if they win, so
 * that it takes priority over other thread writes.
 *
 * When a thread is compacting and we need to update pointers across
 * threads, we do not reach into the heaps of the other threads. We
 * instead update the other thread's registration in our heap. That
 * way, the other thread can dangle the thread and blow away the
 * arena, without us risking a bad write. These objects are reference
 * counted, and the reference count will never be above 2.
 *
 * Note that there's also the notion of a 'global' arena, where
 * allocations are *never* compacted. This is meant only to hold
 * references to our lock-free data structures that are shared across
 * threads, without having to go through the pain of locking
 * arenas. The intent is that this is for instantiation of
 * module-level (or global) values.
 *
 * For instance, let's say there's a module-level dictionary, used by
 * all threads, but thread 1 atomically changes the reference stored
 * in the module. Other threads may have copied the reference to the
 * obejct, but the object will have been allocated in the global
 * arena, so all of those copies will incref the underlying data
 * object, so that it won't be freed until all references have been
 * decref'd, and the count reaches 0.
 *
 * This is error prone if done manually, and is meant more for code
 * generated by the compiler.
 */

#ifndef DISABLE_POINTER_MAPS
#define ALLOW_POINTER_MAPS
#endif

#ifdef C4M_GC_FULL_TRACE
#define C4M_GC_STATS
#endif

#ifdef C4M_GC_ALL_ON
#define DEFAULT_ON  1
#define DEFAULT_OFF 1
#elif defined(C4M_GC_ALL_OFF)
#define DEFAULT_ON  0
#define DEFAULT_OFF 0
#else
#define DEFAULT_ON  1
#define DEFAULT_OFF 0
#endif

#ifndef C4M_GCT_INIT
#define C4M_GCT_INIT DEFAULT_ON
#endif
#ifndef C4M_GCT_MMAP
#define C4M_GCT_MMAP DEFAULT_ON
#endif
#ifndef C4M_GCT_MUNMAP
#define C4M_GCT_MUNMAP DEFAULT_ON
#endif
#ifndef C4M_GCT_SCAN
#define C4M_GCT_SCAN DEFAULT_ON
#endif
#ifndef C4M_GCT_OBJ
#define C4M_GCT_OBJ DEFAULT_OFF
#endif
#ifndef C4M_GCT_SCAN_PTR
#define C4M_GCT_SCAN_PTR DEFAULT_OFF
#endif
#ifndef C4M_GCT_PTR_TEST
#define C4M_GCT_PTR_TEST DEFAULT_OFF
#endif
#ifndef C4M_GCT_PTR_TO_MOVE
#define C4M_GCT_PTR_TO_MOVE DEFAULT_OFF
#endif
#ifndef C4M_GCT_MOVE
#define C4M_GCT_MOVE DEFAULT_OFF
#endif
#ifndef C4M_GCT_ALLOC_FOUND
#define C4M_GCT_ALLOC_FOUND DEFAULT_OFF
#endif
#ifndef C4M_GCT_PTR_THREAD
#define C4M_GCT_PTR_THREAD DEFAULT_OFF
#endif
#ifndef C4M_GCT_MOVED
#define C4M_GCT_MOVED DEFAULT_OFF
#endif
#ifndef C4M_GCT_COLLECT
#define C4M_GCT_COLLECT DEFAULT_ON
#endif
#ifndef C4M_GCT_REGISTER
#define C4M_GCT_REGISTER DEFAULT_ON
#endif
#ifndef C4M_GCT_ALLOC
#define C4M_GCT_ALLOC DEFAULT_OFF
#endif

#ifndef C4M_DEFAULT_ARENA_SIZE

#define C4M_DEFAULT_ARENA_SIZE (1 << 26)
// Was previously using 1 << 19
// But this needs to be much bigger than the stack size; 21 is probably
// the minimum value without adjusting the stack.
#endif

// In the future, we would expect that a writer seeing the
// 'collecting' field will attempt to help migration to minimize
// time spent waiting, but for the time being, any cross-thread
// writes to a thread-local heap will involve spinning.
#define GC_FLAG_COLLECTING 0x00000001

// Whether collection has reached this allocation yet.
#define GC_FLAG_REACHED 0x00000002

// Whether collection has finished migrating this allocation and all it's
// dependencies.
#define GC_FLAG_MOVED 0x00000004

// Whether another thread is currently mutating a cross-thread pointer.
#define GC_FLAG_WRITER_LOCK 0x00000008

// True when the owner is waiting to use the value.
#define GC_FLAG_OWNER_WAITING 0x00000010

// True when we are freezing everything to marshal memory in toto.
#define GC_FLAG_GLOBAL_STOP 0x00000020

// Shouldn't be accessed by developer, but allows us to inline.
extern uint64_t     c4m_gc_guard;
extern c4m_arena_t *c4m_new_arena(size_t, hatrack_zarray_t *);
extern void         c4m_delete_arena(c4m_arena_t *);
extern void         c4m_expand_arena(size_t, c4m_arena_t **);
extern void         c4m_collect_arena(c4m_arena_t **);
extern void        *c4m_gc_resize(void *ptr, size_t len);
extern void         c4m_gc_thread_collect();
extern bool         c4m_is_read_only_memory(volatile void *);
extern void         c4m_gc_set_finalize_callback(c4m_system_finalizer_fn);

#ifdef C4M_GC_STATS
extern void _c4m_arena_register_root(c4m_arena_t *,
                                     void *,
                                     uint64_t,
                                     char *,
                                     int);
extern void _c4m_gc_register_root(void *, uint64_t, char *f, int l);

#define c4m_arena_register_root(x, y, z) \
    _c4m_arena_register_root(x, y, z, __FILE__, __LINE__)
#define c4m_gc_register_root(x, y) \
    _c4m_gc_register_root(x, y, __FILE__, __LINE__)

#else
extern void _c4m_arena_register_root(c4m_arena_t *, void *, uint64_t);
extern void _c4m_gc_register_root(void *, uint64_t);

#define c4m_arena_register_root(x, y, z) _c4m_arena_register_root(x, y, z)
#define c4m_gc_register_root(x, y)       _c4m_gc_register_root(x, y)
#endif

void c4m_arena_remove_root(c4m_arena_t *arena, void *ptr);
void c4m_gc_remove_root(void *ptr);

#if defined(C4M_GC_STATS) || defined(C4M_DEBUG)
extern void *_c4m_gc_raw_alloc(size_t, uint64_t *, char *, int);
extern void *_c4m_gc_raw_alloc_with_finalizer(size_t, uint64_t *, char *, int);

#define c4m_gc_raw_alloc(x, y) \
    _c4m_gc_raw_alloc(x, y, __FILE__, __LINE__)
#define c4m_gc_raw_alloc_with_finalizer(x, y) \
    _c4m_gc_raw_alloc_with_finalizer(x, y, __FILE__, __LINE__)

extern void *c4m_alloc_from_arena(c4m_arena_t **,
                                  size_t,
                                  const uint64_t *,
                                  bool,
                                  char *,
                                  int);

extern c4m_utf8_t *c4m_gc_alloc_info(void *, int *);
#else
extern void *_c4m_gc_raw_alloc(size_t, uint64_t *);
extern void *_c4m_gc_raw_alloc_with_finalizer(size_t, uint64_t *);

#define c4m_gc_raw_alloc_with_finalizer(x, y) \
    _c4m_gc_raw_alloc_with_finalizer(x, y)
#define c4m_gc_raw_alloc(x, y) _c4m_gc_raw_alloc(x, y)
extern void *c4m_alloc_from_arena(c4m_arena_t **,
                                  size_t,
                                  const uint64_t *,
                                  bool);
#endif

#ifdef C4M_GC_STATS
extern int c4m_gc_show_heap_stats_on;

#define c4m_gc_show_heap_stats_on()  c4m_gc_show_heap_stats_on = 1
#define c4m_gc_show_heap_stats_off() c4m_gc_show_heap_stats_on = 0
#else
#define c4m_gc_show_heap_stats_on()
#define c4m_gc_show_heap_stats_off()
#endif

#ifdef C4M_GC_FULL_TRACE
extern int c4m_gc_trace_on;

#define c4m_gc_gen_trace_implementation(X)
#define c4m_gc_trace(X, ...)                            \
    {                                                   \
        if (X && c4m_gc_trace_on) {                     \
            fprintf(stderr, "gc_trace:%s: ", __func__); \
            fprintf(stderr, __VA_ARGS__);               \
            fputc('\n', stderr);                        \
        }                                               \
    }

#define c4m_trace_on()  c4m_gc_trace_on = 1
#define c4m_trace_off() c4m_gc_trace_on = 0
#else
#define c4m_trace_on()
#define c4m_trace_off()
#define c4m_gc_trace(...)
#endif

static inline uint64_t
c4m_round_up_to_given_power_of_2(uint64_t power, uint64_t n)
{
    uint64_t modulus   = (power - 1);
    uint64_t remainder = n & modulus;

    if (!remainder) {
        return n;
    }
    else {
        return (n & ~modulus) + power;
    }
}

#define GC_SCAN_ALL ((uint64_t *)0xffffffffffffffff)

// gc_malloc and gc_alloc_* should only be used for INTERNAL dynamic
// allocations. Anything that would be exposed to the language user
// should be allocated via `gc_new()`, because there's an expectation
// of an `object` header.
#define c4m_gc_malloc(l) c4m_gc_raw_alloc(l, GC_SCAN_ALL)

#define c4m_gc_flex_alloc(fixed, var, numv, map) \
    (c4m_gc_raw_alloc((size_t)(sizeof(fixed)) + (sizeof(var)) * (numv), (map)))

#define c4m_gc_alloc_mapped(typename, map) \
    (c4m_gc_raw_alloc(sizeof(typename), (uint64_t *)map))

#define c4m_gc_alloc(typename) \
    (c4m_gc_raw_alloc(sizeof(typename), GC_SCAN_ALL))

// Assumes it contains pointers. Call manually if you need otherwise.
#define c4m_gc_array_alloc(typename, n) \
    c4m_gc_raw_alloc((sizeof(typename) * n), GC_SCAN_ALL)

typedef void (*c4m_gc_hook)();

extern void           c4m_get_stack_scan_region(uint64_t *top,
                                                uint64_t *bottom);
extern void           c4m_initialize_gc();
extern void           c4m_gc_heap_stats(uint64_t *, uint64_t *, uint64_t *);
extern void           c4m_gc_add_hold(c4m_obj_t);
extern void           c4m_gc_remove_hold(c4m_obj_t);
extern c4m_arena_t   *c4m_internal_stash_heap();
extern void           c4m_internal_unstash_heap();
extern void           c4m_internal_set_heap(c4m_arena_t *);
extern void           c4m_internal_lock_then_unstash_heap();
extern void           c4m_get_heap_bounds(uint64_t *, uint64_t *, uint64_t *);
extern void           c4m_gc_register_collect_fns(c4m_gc_hook, c4m_gc_hook);
extern c4m_alloc_hdr *c4m_find_alloc(void *);

#ifdef C4M_GC_STATS
uint64_t c4m_get_alloc_counter();
#else
#define c4m_get_alloc_counter() (0)
#endif

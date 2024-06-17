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

#ifndef C4M_DEFAULT_ARENA_SIZE
// 4 Meg
#define C4M_DEFAULT_ARENA_SIZE (1 << 24)
// Was previously using 1 << 19
// But this needs to be much bigger than the stack size.
// #define C4M_DEFAULT_ARENA_SIZE 64 // Was using this for extreme tests
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
extern uint64_t c4m_gc_guard;

extern c4m_arena_t *c4m_new_arena(size_t, c4m_dict_t *);
extern void         c4m_delete_arena(c4m_arena_t *);
extern void         c4m_expand_arena(size_t, c4m_arena_t **);
extern void         c4m_collect_arena(c4m_arena_t **);
extern void        *c4m_gc_raw_alloc(size_t, uint64_t *);
extern void        *c4m_gc_raw_alloc_with_finalizer(size_t, uint64_t *);
extern void        *c4m_gc_resize(void *ptr, size_t len);
extern void         c4m_gc_thread_collect();
extern void         c4m_arena_register_root(c4m_arena_t *,
                                            void *,
                                            uint64_t);
extern void         c4m_gc_register_root(void *ptr, uint64_t num_words);
extern bool         c4m_is_read_only_memory(volatile void *);
extern void        *c4m_alloc_from_arena(c4m_arena_t **, size_t, const uint64_t *, bool);
extern void         c4m_gc_set_finalize_callback(c4m_system_finalizer_fn);

// #define GC_TRACE
#ifdef GC_TRACE
extern int c4m_gc_trace_on;

#define c4m_gc_trace(...)                               \
    {                                                   \
        if (c4m_gc_trace_on) {                          \
            fprintf(stderr, "gc_trace:%s: ", __func__); \
            fprintf(stderr, __VA_ARGS__);               \
            fputc('\n', stderr);                        \
        }                                               \
    }

#define c4m_trace_on()  c4m_gc_trace_on = 1
#define c4m_trace_off() c4m_gc_trace_on = 0

#else
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
static inline void *
c4m_gc_malloc(size_t len)
{
    void *result = c4m_gc_raw_alloc(len, GC_SCAN_ALL);
    return result;
}

#define c4m_gc_flex_alloc(fixed, var, numv, map) \
    (c4m_gc_raw_alloc((size_t)(sizeof(fixed)) + (sizeof(var)) * (numv), (map)))

#define c4m_gc_alloc_mapped(typename, map) \
    (c4m_gc_raw_alloc(sizeof(typename), (uint64_t *)map))

#define c4m_gc_alloc(typename) \
    (c4m_gc_raw_alloc(sizeof(typename), GC_SCAN_ALL))

// Assumes it contains pointers. Call manually if you need otherwise.
#define c4m_gc_array_alloc(typename, n) \
    c4m_gc_raw_alloc((sizeof(typename) * n), GC_SCAN_ALL)

extern void         c4m_get_stack_scan_region(uint64_t *top, uint64_t *bottom);
extern void         c4m_initialize_gc();
extern void         c4m_gc_heap_stats(uint64_t *, uint64_t *, uint64_t *);
extern c4m_arena_t *c4m_internal_stash_heap();
extern void         c4m_internal_unstash_heap();
extern void         c4m_internal_set_heap(c4m_arena_t *);
extern void         c4m_internal_lock_then_unstash_heap();

#ifdef C4M_ALLOC_STATS
uint64_t get_alloc_counter();
#else
#define get_alloc_counter() (0)
#endif

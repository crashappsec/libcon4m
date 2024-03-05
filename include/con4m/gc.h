#pragma once

#include <con4m/refcount.h>
#include <hatrack.h>

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

typedef struct {
    alignas(8)
    // A guard value added to every allocation so that cross-heap
    // memory accesses can scan backwards (if needed) to determine if
    // a write should be forwarded to a new location.
    //
    // It also tells us whether the cell has been allocated at all,
    // as it is not set until we allocate.
    //
    // The guard value is picked once per runtime by reading from
    // /dev/urandom, to make sure that we do not start adding
    // references in memory to it.
    uint64_t guard;

    // This is a pointer to the memory arena this allocation is from,
    // so that other threads can register their pointer with the arena
    // when doing cross-thread references.
    uint64_t arena;

    // When scanning memory allocations in the object's current arena,
    // this pointer points to the next allocation spot. We use this
    // when it's time to mark all of the allocations as 'collecting'.
    //
    uint64_t *next_addr;

    // Once the object is migrated, this field is then used to store
    // the forwarding address...
    uint64_t *fw_addr;

    // Flags associated with the allocation. This is atomic, because
    // threads attempt to lock accesses any time. This only needs to
    // be 32-bit aligned, but let's keep it in a slot that is 64-bit
    // aligned.
    _Atomic uint32_t flags;

    // This stores the allocated length of the data object measured in
    // 64-bit blocks.
    uint32_t alloc_len;

    // This is a pointer to a sized bitfield. The first word indicates the
    // number of subsequent words in the bitfield. The bits then
    // represent the words of the data structure, in order, and whether
    // they contain a pointer to track (such pointers MUST be 64-bit
    // aligned).
    //
    // The map doesn't need to be as long as the data structure; it only
    // needs to be long enough to capture all pointers to track in it.
    uint64_t *ptr_map;

    // The actual exposed data.
    uint64_t data[];

} con4m_alloc_hdr;


#ifndef CON4M_DEFAULT_ARENA_SIZE
// 4 Meg
#define CON4M_DEFAULT_ARENA_SIZE (1 << 19)
//#define CON4M_DEFAULT_ARENA_SIZE 64
#endif

// In the future, we would expect that a writer seeing the
// 'collecting' field will attempt to help migration to minimize
// time spent waiting, but for the time being, any cross-thread
// writes to a thread-local heap will involve spinning.
#define GC_FLAG_COLLECTING           0x00000001

// Whether collection has reached this allocation yet.
#define GC_FLAG_REACHED              0x00000002

// Whether collection has finished migrating this allocation and all it's
// dependencies.
#define GC_FLAG_MOVED                0x00000004

// Whether another thread is currently mutating a cross-thread pointer.
#define GC_FLAG_WRITER_LOCK          0x00000008

// True when the owner is waiting to use the value.
#define GC_FLAG_OWNER_WAITING        0x00000010

// True when we are freezing everything to marshal memory in toto.
#define GC_FLAG_GLOBAL_STOP          0x00000020

// Shouldn't be accessed by developer, but allows us to inline.
extern uint64_t gc_guard;


typedef struct con4m_arena_t {
    alignas(8)
    con4m_alloc_hdr      *next_alloc;
    hatrack_dict_t       *roots;
    queue_t              *late_mutations;
    struct con4m_arena_t *previous;
    uint64_t             *heap_end;
    uint64_t              arena_id;
    uint64_t              data[];
} con4m_arena_t;

extern con4m_arena_t *con4m_new_arena(size_t);
extern void           con4m_delete_arena(con4m_arena_t *);
extern void           con4m_expand_arena(size_t, con4m_arena_t **);
extern void           con4m_collect_arena(con4m_arena_t **);
extern void          *con4m_gc_alloc(size_t, uint64_t *);
extern void           con4m_gc_thread_collect();
extern void           con4m_arena_register_root(con4m_arena_t *, void *,
						uint64_t);
extern void           con4m_gc_register_root(void *ptr, uint64_t num_words);


#ifdef GC_TRACE
#define gc_trace(...) fprintf(stderr, "gc_trace:%s: ", __func__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr)
#else
#define gc_trace(...)
#endif

// This currently assumes ptr_map doesn't need more than 64 entries.
static inline void *
con4m_arena_alloc(con4m_arena_t **arena_ptr, size_t len, uint64_t *ptr_map)
{
    // Round up to aligned length.
    size_t         wordlen = (len + 0x7) >> 3;
    con4m_arena_t *arena   = *arena_ptr;

    if (arena == 0 ||
	((uint64_t *)arena->next_alloc->data) + wordlen > arena->heap_end) {
	con4m_expand_arena(max(CON4M_DEFAULT_ARENA_SIZE, wordlen * 2),
			   arena_ptr);
	arena = *arena_ptr;
    }

    con4m_alloc_hdr *raw = arena->next_alloc;
    arena->next_alloc    = (con4m_alloc_hdr *)&(raw->data[wordlen]);

    raw->guard     = gc_guard;
    raw->arena     = arena->arena_id;
    raw->next_addr = (uint64_t *)arena->next_alloc;
    raw->alloc_len = wordlen;
    raw->ptr_map   = ptr_map;


    gc_trace("new record of len %u @%p; data @%p", len, raw, raw->data);
    return (void *)(raw->data);
}

#define gc_flex_alloc(fixed, var, numv, map)	\
    (con4m_gc_alloc((size_t)(sizeof(fixed)) + (sizeof(var)) * (numv), (map)))

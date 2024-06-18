#pragma once
#include "con4m.h"

#ifndef C4M_DISABLE_ALLOC_STATS
#define C4M_ALLOC_STATS
#endif

#define C4M_FORCED_ALIGNMENT 16

typedef struct {
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
    uint64_t            guard;
    //
    // When scanning memory allocations in the object's current arena,
    // this pointer points to the next allocation spot. We use this
    // when it's time to mark all of the allocations as 'collecting'.
    uint64_t           *next_addr;
    //
    // Once the object is migrated, this field is then used to store
    // the forwarding address...
    uint64_t           *fw_addr;
    //
    // This is a pointer to the memory arena this allocation is from,
    // so that other threads can register their pointer with the arena
    // when doing cross-thread references.
    struct c4m_arena_t *arena;
    //
    // Flags associated with the allocation. This is atomic, because
    // threads attempt to lock accesses any time. This only needs to
    // be 32-bit aligned, but let's keep it in a slot that is 64-bit
    // aligned.
    _Atomic uint32_t    flags;
    //
    // This stores the allocated length of the data object measured in
    // 64-bit blocks.
    uint32_t            alloc_len;
    //
    // Set to 'true' if this object requires finalization. This is
    // necessary, even though the arena tracks allocations needing
    // finalization, because resizes could move the pointer.
    //
    // So when a resize is triggered and we see this bit, we go
    // update the pointer in the finalizer list.
    unsigned int        finalize : 1;
    //
    // This is a pointer to a sized bitfield. The first word indicates the
    // number of subsequent words in the bitfield. The bits then
    // represent the words of the data structure, in order, and whether
    // they contain a pointer to track (such pointers MUST be 64-bit
    // aligned).
    //
    // The map doesn't need to be as long as the data structure; it only
    // needs to be long enough to capture all pointers to track in it.
    uint64_t           *ptr_map;
    //
    // The actual exposed data. This must be 16-byte aligned!
    alignas(C4M_FORCED_ALIGNMENT) uint64_t data[];
} c4m_alloc_hdr;

typedef struct c4m_finalizer_info_t {
    c4m_alloc_hdr               *allocation;
    struct c4m_finalizer_info_t *next;
} c4m_finalizer_info_t;

typedef struct c4m_arena_t {
    c4m_alloc_hdr        *next_alloc;
    c4m_dict_t           *roots;
    c4m_set_t            *external_holds;
    //    queue_t            *late_mutations;
    uint64_t             *heap_end;
    c4m_finalizer_info_t *to_finalize;
    uint32_t              arena_id;
    bool                  grow_next;
#ifdef C4M_ALLOC_STATS
    uint64_t alloc_counter;
#endif

    // This must be 16-byte aligned!
    alignas(C4M_FORCED_ALIGNMENT) uint64_t data[];
} c4m_arena_t;

typedef void (*c4m_system_finalizer_fn)(void *);

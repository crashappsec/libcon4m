#pragma once
#include "con4m.h"

#if defined(C4M_FULL_MEMCHECK) && !defined(C4M_GC_STATS)
#define C4M_GC_STATS
#endif

#define C4M_FORCED_ALIGNMENT 16

typedef void (*c4m_mem_scan_fn)(uint64_t *, void *);

typedef struct c4m_alloc_hdr {
    // A guard value added to every allocation so that cross-heap
    // memory accesses can scan backwards (if needed) to determine if
    // a write should be forwarded to a new location.
    //
    // It also tells us whether the cell has been allocated at all,
    // as it is not set until we allocate.
    //
    // The guard value is picked once per runtime by reading from
    // /dev/urandom, to make sure that we do not start adding
    // references  in memory to it.
    uint64_t              guard;
    //
    // When scanning memory allocations in the object's current arena,
    // this pointer points to the next allocation spot. We use this
    // when it's time to mark all of the allocations as 'collecting'.
    uint64_t             *next_addr;
    //
    // Once the object is migrated, this field is then used to store
    // the forwarding address...
    struct c4m_alloc_hdr *fw_addr;
    //
    // This is a pointer to the memory arena this allocation is from,
    // so that other threads can register their pointer with the arena
    // when doing cross-thread references.
    struct c4m_arena_t   *arena;
    //
    // Flags associated with the allocation. This is atomic, because
    // threads attempt to lock accesses any time. This only needs to
    // be 32-bit aligned, but let's keep it in a slot that is 64-bit
    // aligned.
    // Currently using the individual bitfields below, until we
    // add in the multi-threading support.
    // _Atomic uint32_t      flags;
    uint32_t              alloc_len;
    uint32_t              request_len;
    //
    // This is a pointer to a sized bitfield. The first word indicates the
    // number of subsequent words in the bitfield. The bits then
    // represent the words of the data structure, in order, and whether
    // they contain a pointer to track (such pointers MUST be 64-bit
    // aligned).
    // If this function exists, it's passed the # of words in the alloc
    // and a pointer to a bitfield that contains that many bits. The
    // bits that correspond to words with pointers should be set.
    c4m_mem_scan_fn       scan_fn;

#ifdef C4M_FULL_MEMCHECK
    uint64_t *end_guard_loc;
#endif

#if defined(C4M_GC_STATS) || defined(C4M_DEBUG)
    char *alloc_file;
    int   alloc_line;
#endif

    // Set to 'true' if this object requires finalization. This is
    // necessary, even though the arena tracks allocations needing
    // finalization, because resizes could move the pointer.
    //
    // So when a resize is triggered and we see this bit, we go
    // update the pointer in the finalizer list.
    unsigned int finalize  : 1;
    // True if the memory allocation is a direct con4m object with
    // an object header.
    unsigned int con4m_obj : 1;

    __uint128_t cached_hash;

    // The actual exposed data. This must be 16-byte aligned!
    alignas(C4M_FORCED_ALIGNMENT) uint64_t data[];
} c4m_alloc_hdr;

typedef struct c4m_finalizer_info_t {
    c4m_alloc_hdr               *allocation;
    struct c4m_finalizer_info_t *next;
    struct c4m_finalizer_info_t *prev;
} c4m_finalizer_info_t;

#ifdef C4M_FULL_MEMCHECK
typedef struct c4m_shadow_alloc_t {
    struct c4m_shadow_alloc_t *next;
    struct c4m_shadow_alloc_t *prev;
    char                      *file;
    int                        line;
    int                        len;
    c4m_alloc_hdr             *start;
    uint64_t                  *end;
} c4m_shadow_alloc_t;
#endif

typedef struct {
    void    *ptr;
    uint64_t num_items;
#ifdef C4M_GC_STATS
    char *file;
    int   line;
#endif
} c4m_gc_root_info_t;

#ifndef C4M_MAX_GC_ROOTS
#define C4M_MAX_GC_ROOTS (1 << 15)
#endif

typedef struct c4m_arena_t {
#ifdef C4M_FULL_MEMCHECK
    c4m_shadow_alloc_t *shadow_start;
    c4m_shadow_alloc_t *shadow_end;
#endif
    c4m_alloc_hdr        *next_alloc;
    hatrack_zarray_t     *roots;
    c4m_set_t            *external_holds;
    //    queue_t            *late_mutations;
    uint64_t             *heap_end;
    c4m_finalizer_info_t *to_finalize;
    uint32_t              alloc_count;
    uint32_t              largest_alloc;
    bool                  grow_next;
#ifdef C4M_GC_STATS
    uint64_t legacy_count;
    uint64_t starting_counter;
    uint64_t start_size;
#endif

    // This must be 16-byte aligned!
    alignas(C4M_FORCED_ALIGNMENT) uint64_t data[];
} c4m_arena_t;

typedef void (*c4m_system_finalizer_fn)(void *);

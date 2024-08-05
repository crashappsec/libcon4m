#pragma once
#include "con4m.h"

// The goal here is to make it easy to change the amount of space
// associated with common data objects when we're treating them like
// arrays.
typedef union c4m_mem_ptr {
    void                **lvalue;
    void                 *v;
    char                 *c;
    uint8_t              *u8;
    int32_t              *codepoint;
    uint32_t             *u32;
    int64_t              *i64;
    uint64_t             *u64;
    struct c4m_alloc_hdr *alloc;
    union c4m_mem_ptr    *memptr;
    uint64_t              nonpointer;
} c4m_mem_ptr;

typedef void (*c4m_mem_scan_fn)(uint64_t *, void *);

// A ring buffer that we're using to somewhat avoid the need for
// portable assembly for GC roots. Everything in the ring buffer is
// treated like a root; it's sized to be a page in size most places.
//
// The size is several times bigger than the number of total registers
// people will tend to have; the filling being that generated code
// isn't going to keep any single thing in registers for that long
// without spilling it somewhere that we can see it portably.
//
// Still, this is not a definitive solution, more a fallback, and good
// to have in place, especially before I dig out asm for registers.

#define C4M_ALLOC_HISTORY_SIZE 0x01ff

typedef struct {
    void    *ring[C4M_ALLOC_HISTORY_SIZE];
    uint64_t cur;
} c4m_alloc_history_t;

// This needs to stay in sync w/ c4m_marshaled_hdr
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
    // When scanning memory allocations in the object's current arena,
    // this pointer points to the next allocation spot.
    // This isn't very necessary and should probably delete it soon;
    // it's only used to quickly find the alloc end for memcopy,
    // and we to have the full alloc len stored.
    char                 *next_addr;
    // Once the object is migrated, this field is then used to store
    // the forwarding address...
    struct c4m_alloc_hdr *fw_addr;
    struct c4m_type_t    *type;
    c4m_mem_scan_fn       scan_fn;
    uint32_t              alloc_len;
    uint32_t              request_len;
    // The 1st arg to the scan fn is a pointer to a sized
    // bitfield. The first word indicates the number of subsequent
    // words in the bitfield. The bits then represent the words of the
    // data structure, in order, and whether they contain a pointer to
    // track (such pointers MUST be 64-bit aligned).  If this function
    // exists, it's passed the # of words in the alloc and a pointer
    // to a bitfield that contains that many bits. The bits that
    // correspond to words with pointers should be set.
#if defined(C4M_FULL_MEMCHECK)
    uint64_t *end_guard_loc;
#endif
#if defined(C4M_ADD_ALLOC_LOC_INFO)
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
    // True if the memory allocation is a direct con4m object, in
    // which case, we expect the type field to be valid.
    unsigned int con4m_obj : 1;
    __uint128_t  cached_hash;

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
#ifdef C4M_ADD_ALLOC_LOC_INFO
    char *file;
    int   line;
#endif
} c4m_gc_root_info_t;

typedef struct c4m_arena_t {
    // First, to make sure it's page-aligned.
    c4m_alloc_history_t *history;
    uint64_t            *heap_end;
#ifdef C4M_FULL_MEMCHECK
    c4m_shadow_alloc_t *shadow_start;
    c4m_shadow_alloc_t *shadow_end;
#endif
    c4m_alloc_hdr        *next_alloc;
    hatrack_zarray_t     *roots;
    c4m_set_t            *external_holds;
    c4m_finalizer_info_t *to_finalize;
    uint32_t              alloc_count;
    bool                  grow_next;
    pthread_mutex_t       lock;
#ifdef C4M_GC_STATS
    uint64_t legacy_count;    // # of total allocs before the heap
    uint32_t start_size;      // Space after the copy
    uint32_t num_transferred; // # of records transferred in move.
#endif

    // This must be 16-byte aligned!
    alignas(C4M_FORCED_ALIGNMENT) uint64_t data[];
} c4m_arena_t;

typedef void (*c4m_system_finalizer_fn)(void *);

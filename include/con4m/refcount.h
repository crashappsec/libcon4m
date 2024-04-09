// Simple reference-counted memory object header.  Incref it BEFORE a
// thread hands away a reference, and decref it once you're 100% sure
// it's gone.
//
// So the thread that hands out a reference is responsible for the
// incref, and the thread holding the reference does the decref.
//
// Note that this is a primitive on which we're going to build out our
// garbage collector, and should not generally be used directly.
//
// Particularly, this does NOT register pointers are 'roots' for the
// GC algorithm.
//
// Instead, this is intended to be used to manage arenas,

#include "con4m.h"

typedef struct {
    _Atomic int64_t refcount;
    char            data[];
} refcount_alloc_t;

static inline void *
rc_alloc(size_t len)
{
    refcount_alloc_t *raw;

    raw = (refcount_alloc_t *)calloc(sizeof(refcount_alloc_t) + len, 1);
    atomic_store(&(raw->refcount), 1);

    return (void *)raw->data;
}

static inline void *
rc_ref(void *ptr)
{
    refcount_alloc_t *raw = ptr - sizeof(refcount_alloc_t);
    atomic_fetch_add(&(raw->refcount), 1);
    return ptr;
}

static inline void
rc_free(void *ptr)
{
    refcount_alloc_t *raw = ptr - sizeof(refcount_alloc_t);
    if (atomic_fetch_add(&(raw->refcount), -1) == 0) {
        free(raw);
    }
}

typedef void (*cleanup_fn)(void *);

static inline void
rc_free_and_cleanup(void *ptr, cleanup_fn callback)
{
    refcount_alloc_t *raw = ptr - sizeof(refcount_alloc_t);
    if (atomic_fetch_add(&(raw->refcount), -1) == 0) {
        callback(raw);
        free(raw);
    }
}

static inline void *
zalloc(size_t len)
{
    return rc_alloc(len);
}

#define zalloc_flex(fixed, variable, num_variable) \
    (void *)rc_alloc((sizeof(fixed)) + (sizeof(variable)) * (num_variable))

static inline void
zfree(void *ptr)
{
    rc_free(ptr);
}

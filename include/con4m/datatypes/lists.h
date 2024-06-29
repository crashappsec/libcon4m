#pragma once

#include "con4m.h"

typedef struct {
    alignas(8)
        // The actual length if treated properly. We should be
        // careful about it.
        int32_t append_ix;
    int32_t          length; // The allocated length.
    int64_t        **data;
    pthread_rwlock_t lock;
    // Used when we hold the write lock to prevent nested acquires.
    bool             dont_acquire;
} c4m_xlist_t;

typedef struct {
    int64_t len;
    void  **items;
} c4m_view_t;

typedef struct hatstack_t  c4m_stack_t;
typedef struct flexarray_t flexarray_t;

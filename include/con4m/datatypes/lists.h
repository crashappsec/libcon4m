#pragma once

#include "con4m.h"

typedef struct {
    int64_t        **data;
    // The actual length if treated properly. We should be
    // careful about it.
    int32_t          append_ix;
    int32_t          length; // The allocated length.
    pthread_rwlock_t lock;
    // Used when we hold the write lock to prevent nested acquires.
    bool             dont_acquire;
} c4m_list_t;

typedef struct hatstack_t c4m_stack_t;

#pragma once

#include <con4m.h>

typedef struct {
    alignas(8)
        // The actual length if treated properly. We should be
        // careful about it.
        int32_t append_ix;
    int32_t   length; // The allocated length.
    int64_t **data;
} xlist_t;

// This needs fw referencing, because this gets read before hatrack.
// I think it's the build system's fault?
typedef struct flexarray_t flexarray_t;

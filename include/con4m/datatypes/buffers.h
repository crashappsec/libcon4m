#pragma once

#include <con4m.h>
typedef struct {
    alignas(8)
    char   *data;
    int32_t flags;
    int32_t byte_len;
} buffer_t;
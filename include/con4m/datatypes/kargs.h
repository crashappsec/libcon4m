#pragma once

#include <con4m.h>

typedef struct {
    char *kw;
    void *value;
} one_karg_t;

typedef struct {
    int64_t     num_provided;
    one_karg_t *args;
} karg_info_t;

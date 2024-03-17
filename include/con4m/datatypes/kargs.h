#pragma once

#include <con4m.h>

typedef struct karg_st karg_t;

struct karg_st {
    char   *name;
    void   *outloc;
    karg_t *next;
    int     size;
};

typedef struct {
    karg_t *top;
} karg_cache_t;

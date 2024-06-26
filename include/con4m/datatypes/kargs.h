#pragma once

#include "con4m.h"

typedef struct {
    char *kw;
    void *value;
} c4m_one_karg_t;

typedef struct {
    int64_t         num_provided;
    c4m_one_karg_t *args;
} c4m_karg_info_t;

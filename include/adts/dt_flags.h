#pragma once
#include "con4m.h"

typedef struct {
    uint64_t *contents;
    int32_t   bit_modulus;
    int32_t   alloc_wordlen; // u64 words.
    uint32_t  num_flags;     // Redundant, but just makes life easier.
} c4m_flags_t;

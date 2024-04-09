#pragma once

#include "con4m.h"

typedef struct {
    // Actually, since objects already contain the full type, this really
    // only needs to hold the base type ID. Should def fix.
    type_spec_t *held_type;
    void        *held_value;
} mixed_t;

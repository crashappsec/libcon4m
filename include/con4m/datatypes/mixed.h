#pragma once

#include "con4m.h"

typedef struct {
    // Actually, since objects already contain the full type, this really
    // only needs to hold the base type ID. Should def fix.
    c4m_type_t *held_type;
    void       *held_value;
} c4m_mixed_t;

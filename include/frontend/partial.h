#pragma once
#include "con4m.h"

static inline bool
c4m_is_partial_lit(const c4m_obj_t o)
{
    return c4m_obj_type_check(o, c4m_tspec_partial_lit());
}

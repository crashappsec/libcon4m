#pragma once
#include "con4m.h"

#ifdef C4M_USE_INTERNAL_API

static inline bool
c4m_is_partial_type(c4m_type_t *t)
{
    return c4m_tspec_get_base_tid(t) == C4M_T_PARTIAL_LIT;
}

static inline bool
c4m_is_partial_lit(const c4m_obj_t o)
{
    return c4m_is_partial_type(c4m_get_my_type(o));
}
#endif

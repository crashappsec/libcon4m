#pragma once
#include "con4m.h"

#ifdef C4M_USE_INTERNAL_API

static inline bool
c4m_is_partial_type(c4m_type_t *t)
{
    switch (c4m_tspec_get_base_tid(t)) {
    case C4M_T_PARTIAL_LIT:
    case C4M_T_PARSE_NODE:
        return true;
    default:
        return false;
    }
}

static inline bool
c4m_is_partial_lit(const c4m_obj_t o)
{
    return c4m_is_partial_type(c4m_get_my_type(o));
}

extern c4m_obj_t c4m_fold_partial(c4m_compile_ctx *, c4m_obj_t);

#endif

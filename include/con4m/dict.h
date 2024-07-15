#pragma once

#include "con4m.h"

#define c4m_dict(x, y) c4m_new(c4m_type_dict(x, y))
c4m_dict_t *c4m_dict_copy(c4m_dict_t *dict);

#ifdef C4M_USE_INTERNAL_API
c4m_base_obj_t *c4m_early_alloc_dict(size_t, bool, bool);
c4m_dict_t     *c4m_new_unmanaged_dict(size_t, bool, bool);
#endif

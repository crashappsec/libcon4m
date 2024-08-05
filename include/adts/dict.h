#pragma once

#include "con4m.h"

#define c4m_dict(x, y) c4m_new(c4m_type_dict(x, y))
extern c4m_dict_t *c4m_dict_copy(c4m_dict_t *);
extern c4m_list_t *c4m_dict_keys(c4m_dict_t *);
extern c4m_list_t *c4m_dict_values(c4m_dict_t *);

#ifdef C4M_USE_INTERNAL_API
extern c4m_dict_t *c4m_new_unmanaged_dict(size_t, bool, bool);
#endif

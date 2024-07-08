#pragma once

#include "con4m.h"

extern c4m_utf32_t *c4m_wrapper_join(c4m_list_t *, const c4m_str_t *);
extern c4m_str_t   *c4m_wrapper_hostname();
extern c4m_str_t   *c4m_wrapper_os();
extern c4m_str_t   *c4m_wrapper_arch();
extern c4m_str_t   *c4m_wrapper_repr(c4m_obj_t);
extern c4m_str_t   *c4m_wrapper_to_str(c4m_obj_t);

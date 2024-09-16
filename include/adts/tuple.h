#pragma once

#include "con4m.h"

extern void    c4m_tuple_set(c4m_tuple_t *, int64_t, void *);
extern void   *c4m_tuple_get(c4m_tuple_t *, int64_t);
extern int64_t c4m_tuple_len(c4m_tuple_t *);
extern void   *c4m_clean_internal_list(c4m_list_t *l);

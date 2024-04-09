#pragma once

#include "con4m.h"

extern void    c4m_tuple_set(tuple_t *, int64_t, void *);
extern void   *c4m_tuple_get(tuple_t *, int64_t);
extern int64_t c4m_tuple_len(tuple_t *);

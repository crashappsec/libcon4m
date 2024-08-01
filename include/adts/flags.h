#pragma once
#include "con4m.h"

extern c4m_flags_t *c4m_flags_copy(const c4m_flags_t *);
extern c4m_flags_t *c4m_flags_invert(c4m_flags_t *);
extern c4m_flags_t *c4m_flags_add(c4m_flags_t *, c4m_flags_t *);
extern c4m_flags_t *c4m_flags_sub(c4m_flags_t *, c4m_flags_t *);
extern c4m_flags_t *c4m_flags_test(c4m_flags_t *, c4m_flags_t *);
extern c4m_flags_t *c4m_flags_xor(c4m_flags_t *, c4m_flags_t *);
extern bool         c4m_flags_eq(c4m_flags_t *, c4m_flags_t *);
extern uint64_t     c4m_flags_len(c4m_flags_t *);
extern bool         c4m_flags_index(c4m_flags_t *, int64_t);
extern void         c4m_flags_set_index(c4m_flags_t *, int64_t, bool);

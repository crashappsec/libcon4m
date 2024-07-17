#pragma once

#include "con4m.h"
extern void        c4m_universe_init(c4m_type_universe_t *);
extern c4m_type_t *c4m_universe_get(c4m_type_universe_t *, c4m_type_hash_t);
extern bool        c4m_universe_put(c4m_type_universe_t *, c4m_type_t *);
extern bool        c4m_universe_add(c4m_type_universe_t *, c4m_type_t *);
extern c4m_type_t *c4m_universe_attempt_to_add(c4m_type_universe_t *, c4m_type_t *);
extern void        c4m_universe_forward(c4m_type_universe_t *,
                                        c4m_type_t *,
                                        c4m_type_t *);

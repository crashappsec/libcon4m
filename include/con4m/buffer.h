#pragma once

#include "con4m.h"

extern c4m_buf_t  *c4m_buffer_add(c4m_buf_t *, c4m_buf_t *);
extern c4m_buf_t  *c4m_buffer_join(c4m_xlist_t *, c4m_buf_t *);
extern int64_t     c4m_buffer_len(c4m_buf_t *);
extern void        c4m_buffer_resize(c4m_buf_t *, uint64_t);
extern c4m_utf8_t *c4m_c4m_buf_to_utf8_string(c4m_buf_t *);

#pragma once

#include "con4m.h"
#ifndef C4M_EMPTY_BUFFER_ALLOC
#define C4M_EMPTY_BUFFER_ALLOC 128
#endif

extern c4m_buf_t  *c4m_buffer_add(c4m_buf_t *, c4m_buf_t *);
extern c4m_buf_t  *c4m_buffer_join(c4m_xlist_t *, c4m_buf_t *);
extern int64_t     c4m_buffer_len(c4m_buf_t *);
extern void        c4m_buffer_resize(c4m_buf_t *, uint64_t);
extern c4m_utf8_t *c4m_buf_to_utf8_string(c4m_buf_t *);

static inline c4m_buf_t *
c4m_buffer_empty()
{
    return c4m_new(c4m_type_buffer(), c4m_kw("length", c4m_ka(0)));
}

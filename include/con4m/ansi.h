#pragma once

#include "con4m.h"

extern void   c4m_utf8_ansi_render(const c4m_utf8_t *s,
                                   c4m_stream_t     *outstream);
extern void   c4m_utf32_ansi_render(const c4m_utf32_t *s,
                                    int32_t            start_ix,
                                    int32_t            end_ix,
                                    c4m_stream_t      *outstream);
extern void   c4m_ansi_render(const c4m_str_t *s,
                              c4m_stream_t    *out);
extern void   c4m_ansi_render_to_width(const c4m_str_t *s,
                                       int32_t          width,
                                       int32_t          hang,
                                       c4m_stream_t    *out);
extern size_t c4m_ansi_render_len(const c4m_str_t *s);

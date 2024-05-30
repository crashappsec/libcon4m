#pragma once
#include "con4m.h"

extern bool        c4m_lex(c4m_file_compile_ctx *, c4m_stream_t *);
extern c4m_utf8_t *c4m_format_one_token(c4m_token_t *, c4m_str_t *);
extern c4m_grid_t *c4m_format_tokens(c4m_file_compile_ctx *);
extern c4m_utf8_t *token_type_to_string(c4m_token_kind_t);

static inline c4m_utf8_t *
c4m_token_raw_content(c4m_token_t *tok)
{
    int64_t      diff = tok->end_ptr - tok->start_ptr - 2 * tok->adjustment;
    c4m_utf32_t *u32  = c4m_new(c4m_tspec_utf32(),
                               c4m_kw("length",
                                      c4m_ka(diff),
                                      "codepoints",
                                      tok->start_ptr + tok->adjustment));

    c4m_utf8_t *result = c4m_to_utf8(u32);

    return result;
}

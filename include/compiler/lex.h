#pragma once
#include "con4m.h"

extern bool        c4m_lex(c4m_module_t *, c4m_stream_t *);
extern c4m_utf8_t *c4m_format_one_token(c4m_token_t *, c4m_str_t *);
extern c4m_grid_t *c4m_format_tokens(c4m_module_t *);
extern c4m_utf8_t *c4m_token_type_to_string(c4m_token_kind_t);
extern c4m_utf8_t *c4m_token_raw_content(c4m_token_t *);

static inline c4m_utf8_t *
c4m_token_get_location_str(c4m_token_t *t)
{
    if (t->module) {
        return c4m_cstr_format("{}:{}:{}",
                               t->module->path,
                               c4m_box_i64(t->line_no),
                               c4m_box_i64(t->line_offset + 1));
    }

    return c4m_cstr_format("{}:{}",
                           c4m_box_i64(t->line_no),
                           c4m_box_i64(t->line_offset + 1));
}

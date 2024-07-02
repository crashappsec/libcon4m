#pragma once
#include "con4m.h"

extern bool        c4m_lex(c4m_file_compile_ctx *, c4m_stream_t *);
extern c4m_utf8_t *c4m_format_one_token(c4m_token_t *, c4m_str_t *);
extern c4m_grid_t *c4m_format_tokens(c4m_file_compile_ctx *);
extern c4m_utf8_t *c4m_token_type_to_string(c4m_token_kind_t);
extern c4m_utf8_t *c4m_token_raw_content(c4m_token_t *);

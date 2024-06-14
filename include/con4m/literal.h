#pragma once
#include "con4m.h"

extern __uint128_t         c4m_raw_int_parse(char *,
                                             c4m_compile_error_t *,
                                             bool *);
extern __uint128_t         c4m_raw_hex_parse(char *,
                                             c4m_compile_error_t *);
extern void                c4m_init_literal_handling();
extern void                c4m_register_literal(c4m_lit_syntax_t,
                                                char *,
                                                c4m_builtin_t);
extern c4m_compile_error_t c4m_parse_simple_lit(c4m_token_t *,
                                                c4m_lit_syntax_t *,
                                                c4m_utf8_t **);
extern c4m_builtin_t       c4m_base_type_from_litmod(c4m_lit_syntax_t,
                                                     c4m_utf8_t *);

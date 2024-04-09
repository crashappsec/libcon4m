#pragma once
#include "con4m.h"

extern __uint128_t c4m_raw_int_parse(char *, lit_error_t *, bool *);
extern __uint128_t c4m_raw_hex_parse(char *, lit_error_t *);
extern void        c4m_init_literal_handling();
extern void        c4m_register_literal(syntax_t, char *, c4m_builtin_t);
extern object_t    c4m_simple_lit(char *, syntax_t, char *, lit_error_t *);

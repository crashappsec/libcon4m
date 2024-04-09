#pragma once
#include "con4m.h"

extern __uint128_t raw_int_parse(char *, lit_error_t *, bool *);
extern __uint128_t raw_hex_parse(char *, lit_error_t *);
extern void        init_literal_handling();
extern void        register_literal(syntax_t, char *, c4m_builtin_t);
extern object_t    c4m_simple_lit(char *, syntax_t, char *, lit_error_t *);

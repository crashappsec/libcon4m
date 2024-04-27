#include "con4m.h"
#pragma once

extern c4m_utf8_t *c4m_str_vformat(c4m_str_t *, c4m_dict_t *);
extern c4m_utf8_t *_c4m_str_format(c4m_str_t *, ...);
extern c4m_utf8_t *_c4m_cstr_format(char *, ...);

#define c4m_cstr_format(fmt, ...) _c4m_cstr_format(fmt, KFUNC(__VA_ARGS__))
#define c4m_str_format(fmt, ...)  _c4m_str_format(fmt, KFUNC(__VA_ARGS__))

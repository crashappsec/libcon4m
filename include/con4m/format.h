#include "con4m.h"
#pragma once
extern c4m_utf8_t *c4m_base_format(const c4m_str_t *, int, va_list);
extern c4m_utf8_t *c4m_str_vformat(const c4m_str_t *, c4m_dict_t *);
extern c4m_utf8_t *_c4m_str_format(const c4m_str_t *, int, ...);
extern c4m_utf8_t *_c4m_cstr_format(char *, int, ...);
extern c4m_utf8_t *c4m_cstr_array_format(char *, int, c4m_utf8_t **);

#define c4m_cstr_format(fmt, ...) \
    _c4m_cstr_format(fmt, PP_NARG(__VA_ARGS__) __VA_OPT__(, ) __VA_ARGS__)
#define c4m_str_format(fmt, ...) \
    _c4m_str_format(fmt, PP_NARG(__VA_ARGS__) __VA_OPT__(, ) __VA_ARGS__)

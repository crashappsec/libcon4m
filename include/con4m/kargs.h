/*
 * Copyright © 2021 John Viega
 *
 * See LICENSE.txt for licensing info.
 *
 * Name:          kargs.h
 * Description:   Support for limited keyword-style arguments in C functions.
 * Author:        John Viega, john@zork.org
 *
 * The 'provided' versions allow you to get a bitfield as to which
 * arguments were provided. That's for when you can't pre-assign an
 * invalid value to check.
 *
 */

#pragma once

#include "con4m.h"

c4m_karg_info_t *c4m_get_kargs(va_list);
c4m_karg_info_t *c4m_pass_kargs(int, ...);
c4m_karg_info_t *c4m_get_kargs_and_count(va_list, int *);

static inline bool
_c4m_kw_int64(c4m_karg_info_t *provided, char *name, int64_t *ptr)
{
    if (!provided) {
        return false;
    }

    int64_t n = provided->num_provided;

    for (int64_t i = 0; i < n; i++) {
        if (!strcmp(name, provided->args[i].kw)) {
            *ptr = (int64_t)provided->args[i].value;
            return true;
        }
    }

    return false;
}

static inline bool
_c4m_kw_ptr(c4m_karg_info_t *provided, char *name, void *ptr)
{
    return _c4m_kw_int64(provided, name, (int64_t *)ptr);
}

static inline bool
_c4m_kw_int32(c4m_karg_info_t *provided, char *name, int32_t *ptr)
{
    if (!provided) {
        return false;
    }

    int64_t n = provided->num_provided;
    int64_t tmp;

    for (int64_t i = 0; i < n; i++) {
        if (!strcmp(name, provided->args[i].kw)) {
            tmp = (int64_t)provided->args[i].value;

            *ptr = (int32_t)tmp;
            return true;
        }
    }

    return false;
}

static inline bool
_c4m_kw_int16(c4m_karg_info_t *provided, char *name, int16_t *ptr)
{
    if (!provided) {
        return false;
    }

    int64_t n = provided->num_provided;
    int64_t tmp;

    for (int64_t i = 0; i < n; i++) {
        if (!strcmp(name, provided->args[i].kw)) {
            tmp = (int64_t)provided->args[i].value;

            *ptr = (int16_t)tmp;
            return true;
        }
    }

    return false;
}

static inline bool
_c4m_kw_int8(c4m_karg_info_t *provided, char *name, int8_t *ptr)
{
    if (!provided) {
        return false;
    }

    int64_t n = provided->num_provided;
    int64_t tmp;

    for (int64_t i = 0; i < n; i++) {
        if (!strcmp(name, provided->args[i].kw)) {
            tmp = (int64_t)provided->args[i].value;

            *ptr = (int8_t)tmp;
            return true;
        }
    }

    return false;
}

static inline bool
_c4m_kw_bool(c4m_karg_info_t *provided, char *name, bool *ptr)
{
    if (!provided) {
        return false;
    }

    int64_t n = provided->num_provided;
    int64_t tmp;

    for (int64_t i = 0; i < n; i++) {
        if (!strcmp(name, provided->args[i].kw)) {
            tmp = (int64_t)provided->args[i].value;

            *ptr = (bool)tmp;
            return true;
        }
    }

    return false;
}

static inline bool
_c4m_kw_float(c4m_karg_info_t *provided, char *name, double *ptr)
{
    if (!provided) {
        return false;
    }

    int64_t n = provided->num_provided;
    int64_t tmp;

    for (int64_t i = 0; i < n; i++) {
        if (!strcmp(name, provided->args[i].kw)) {
            tmp = (int64_t)provided->args[i].value;

            *ptr = (double)tmp;
            return true;
        }
    }

    return false;
}

#define c4m_kw_int64(a, b)     _c4m_kw_int64(_c4m_karg, a, &b)
#define c4m_kw_uint64(a, b)    _c4m_kw_int64(_c4m_karg, a, (int64_t *)&b)
#define c4m_kw_ptr(a, b)       _c4m_kw_ptr(_c4m_karg, a, &b)
#define c4m_kw_int32(a, b)     _c4m_kw_int32(_c4m_karg, a, &b)
#define c4m_kw_uint32(a, b)    _c4m_kw_int32(_c4m_karg, a, (int32_t *)&b)
#define c4m_kw_codepoint(a, b) _c4m_kw_int32(_c4m_karg, a, &b)
#define c4m_kw_int16(a, b)     _c4m_kw_int16(_c4m_karg, a, &b)
#define c4m_kw_uint16(a, b)    _c4m_kw_int16(_c4m_karg, a, (int16_t *)&b)
#define c4m_kw_char(a, b)      _c4m_kw_int8(_c4m_karg, a, &b)
#define c4m_kw_int8(a, b)      _c4m_kw_int8(_c4m_karg, a, &b)
#define c4m_kw_unt8(a, b)      _c4m_kw_int8(_c4m_karg, a, (int8_t *)&b)
#define c4m_kw_bool(a, b)      _c4m_kw_bool(_c4m_karg, a, &b)
#define c4m_kw_float(a, b)     _c4m_kw_float(_c4m_karg, a, &b)

// print(foo, bar, boz, kw("file", stdin, "sep", ' ', "end", '\n',
//                         "flush", false ));

#define c4m_ka(x) ((int64_t)x)

#ifdef C4M_DEBUG_KARGS
#define c4m_kw(...) c4m_pass_kargs(C4M_PP_NARG(__VA_ARGS__), \
                                   (char *)__FILE__,     \
                                   (int)__LINE__,        \
                                   __VA_ARGS__),         \
                    NULL
#else
#define c4m_kw(...) c4m_pass_kargs(C4M_PP_NARG(__VA_ARGS__), __VA_ARGS__), NULL
#endif
#define c4m_karg_only_init(last)                           \
    va_list _args;                                         \
    va_start(_args, last);                                 \
    c4m_karg_info_t *_c4m_karg = va_arg(_args, c4m_obj_t); \
    va_end(_args);

#define c4m_karg_va_init(list) \
    c4m_karg_info_t *_c4m_karg = va_arg(list, c4m_obj_t)

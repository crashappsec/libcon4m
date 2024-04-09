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

karg_info_t *get_kargs(va_list);
karg_info_t *pass_kargs(int, ...);
karg_info_t *get_kargs_and_count(va_list, int *);

static inline bool
_kw_int64(karg_info_t *provided, char *name, int64_t *ptr)
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
_kw_ptr(karg_info_t *provided, char *name, void *ptr)
{
    return _kw_int64(provided, name, (int64_t *)ptr);
}

static inline bool
_kw_int32(karg_info_t *provided, char *name, int32_t *ptr)
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
_kw_int16(karg_info_t *provided, char *name, int16_t *ptr)
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
_kw_int8(karg_info_t *provided, char *name, int8_t *ptr)
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
_kw_bool(karg_info_t *provided, char *name, bool *ptr)
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
_kw_float(karg_info_t *provided, char *name, double *ptr)
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

#define kw_int64(a, b)     _kw_int64(_karg, a, &b)
#define kw_uint64(a, b)    _kw_int64(_karg, a, (int64_t *)&b)
#define kw_ptr(a, b)       _kw_ptr(_karg, a, &b)
#define kw_int32(a, b)     _kw_int32(_karg, a, &b)
#define kw_uint32(a, b)    _kw_int32(_karg, a, (int32_t *)&b)
#define kw_codepoint(a, b) _kw_int32(_karg, a, &b)
#define kw_int16(a, b)     _kw_int16(_karg, a, &b)
#define kw_uint16(a, b)    _kw_int16(_karg, a, (int16_t *)&b)
#define kw_char(a, b)      _kw_int8(_karg, a, &b)
#define kw_int8(a, b)      _kw_int8(_karg, a, &b)
#define kw_unt8(a, b)      _kw_int8(_karg, a, (int8_t *)&b)
#define kw_bool(a, b)      _kw_bool(_karg, a, &b)
#define kw_float(a, b)     _kw_float(_karg, a, &b)

// print(foo, bar, boz, kw("file", stdin, "sep", ' ', "end", '\n',
//                         "flush", false ));

#define ka(x)   ((int64_t)x)
#define kw(...) pass_kargs(PP_NARG(__VA_ARGS__), __VA_ARGS__), NULL
#define karg_only_init(last)                      \
    va_list _args;                                \
    va_start(_args, last);                        \
    karg_info_t *_karg = va_arg(_args, object_t); \
    va_end(_args);

#define karg_va_init(list) karg_info_t *_karg = va_arg(list, object_t)

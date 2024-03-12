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

#include <con4m.h>

typedef struct karg_st karg_t;

struct karg_st {
    char   *name;
    void   *outloc;
    karg_t *next;
    int     size;
};

typedef struct {
    karg_t *top;
} karg_cache_t;

uint64_t kargs_process_w_req_param(va_list, ...);
uint64_t kargs_process_wo_req_param(char *, va_list, ...);

#define KARG(n)       #n, sizeof(n), &n
#define KRENAME(x, y) #x, sizeof(y), &y

#define kargs(lastarg, ...)                                                    \
    {                                                                          \
        va_list arglist;                                                       \
        va_start(arglist, lastarg);                                            \
        kargs_process_w_req_param(                                             \
            arglist,                                                           \
            IF(ISEMPTY(__VA_ARGS__))(EMPTY(),                                  \
               MAP_LIST(KARG, __VA_ARGS__) MAP_COMMA) 0);		       \
        va_end(arglist);                                                       \
    }

#define KARGS_ONLY char *_kwargs, ...

#define kargs0(...)                                                            \
    {                                                                          \
        va_list arglist;                                                       \
        va_start(arglist, _kwargs);                                            \
        kargs_process_wo_req_param(                                            \
            _kwargs,                                                           \
            arglist,                                                           \
            IF(ISEMPTY(__VA_ARGS__))(EMPTY(),                                  \
               MAP_LIST(KARG, __VA_ARGS__) MAP_COMMA) 0);		       \
        va_end(arglist);                                                       \
    }

#define kargs_p(lastarg, provided, ...)                                        \
    {                                                                          \
        va_list arglist;                                                       \
        va_start(arglist, lastarg);                                            \
        provided = kargs_process_w_req_param(                                  \
            arglist,                                                           \
            IF(ISEMPTY(__VA_ARGS__))(EMPTY(),                                  \
               MAP_LIST(KARG, __VA_ARGS__) MAP_COMMA) 0);                      \
        va_end(arglist);                                                       \
    }

#define kargs0_p(provided, ...)                                                \
    {                                                                          \
        va_list arglist;                                                       \
        va_start(arglist, _kwargs);                                            \
        provided = kargs_process_wo_req_param(                                 \
            _kwargs,                                                           \
            arglist,                                                           \
            IF(ISEMPTY(__VA_ARGS__))(EMPTY(), MAP_LIST(KARG, __VA_ARGS__) MAP_COMMA) 0); \
        va_end(arglist);                                                       \
    }

// Use this one when your keyword parameter collides with a keyword or function
// (e.g., "free").  We provide two macros you can manually use:
//
// -    KRENAME(x, y) binds the keyword named in the first parameter to the
//      variable named in the second parameter.
//
// -    KARG(x) says that the variable x doesn't need to be re-bound.

#define kargs_rename(lastarg, ...)                                             \
    {                                                                          \
        va_list arglist;                                                       \
        va_start(arglist, lastarg);                                            \
        kargs_process_w_req_param(arglist, __VA_ARGS__, KARG_END);             \
        va_end(arglist);                                                       \
    }

#define kargs_rename0(...)                                                     \
    {                                                                          \
        va_list arglist;                                                       \
        va_start(arglist, lastarg);                                            \
        kargs_process_wo_req_param(_kwargs, arglist, __VA_ARGS__, KARG_END);   \
        va_end(arglist);                                                       \
    }

// Inside a method, the actuals are already put into a va_list, so
// such functions cannot use the kargs() macro.  Also, methods should
// always have a self, so this does not need a version that works when
// no required parameters are present.
#define method_kargs(arglist, ...)                                             \
    kargs_process_w_req_param(                                            \
        arglist,                                                               \
        IF(ISEMPTY(__VA_ARGS__))(EMPTY(),                                      \
           MAP_LIST(KARG, __VA_ARGS__) MAP_COMMA) 0);

#define method_kargs_rename(arglist, ...)                                      \
    kargs_process_w_req_param(arglist, __VA_ARGS__, KARG_END);

// Counts from 1.
#define karg_provided(p, n) ((p >> (n - 1)) & 1)

// clang-format off
enum : int64_t
{
    KARG_1 = 1, KARG_2, KARG_3, KARG_4, KARG_5,  KARG_6,  KARG_7,  KARG_8,
    KARG_9,  KARG_10, KARG_11, KARG_12, KARG_13, KARG_14, KARG_15, KARG_16,
    KARG_17, KARG_18, KARG_19, KARG_20, KARG_21, KARG_22, KARG_23, KARG_24,
    KARG_25, KARG_26, KARG_27, KARG_28, KARG_29, KARG_30, KARG_31, KARG_32,
    KARG_33, KARG_34, KARG_35, KARG_36, KARG_37, KARG_38, KARG_39, KARG_40,
    KARG_41, KARG_42, KARG_43, KARG_44, KARG_45, KARG_46, KARG_47, KARG_48,
    KARG_49, KARG_50, KARG_51, KARG_52, KARG_53, KARG_54, KARG_55, KARG_56,
    KARG_57, KARG_58, KARG_59, KARG_60, KARG_61, KARG_62, KARG_63, KARG_64
};
// clang-format on

#define DECLARE_KARGS(x) x

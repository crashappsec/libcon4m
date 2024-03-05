/*
 * Copyright © 2021 John Viega
 *
 * See LICENSE.txt for licensing info.
 *
 * Name:          kargs.h
 * Description:   Support for limited keyword-style arguments in C functions.
 * Author:        John Viega, john@zork.org
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

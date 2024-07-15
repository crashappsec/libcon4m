/*
 * Copyright © 2021 John Viega
 *
 * See LICENSE.txt for licensing info.
 *
 * Name:          kargs.h
 * Description:   Support for limited keyword-style arguments in C functions.
 * Author:        John Viega, john@zork.org
 */

#include "con4m.h"

#ifndef C4M_MAX_KARGS_NESTING_DEPTH
// Must be a power of two.
#define C4M_MAX_KARGS_NESTING_DEPTH 32
#endif

#ifndef C4M_MAX_KEYWORD_SIZE
#define C4M_MAX_KEYWORD_SIZE 32
#endif

static thread_local c4m_karg_info_t *kcache[C4M_MAX_KARGS_NESTING_DEPTH];
static thread_local int              kargs_next_entry = -1;

const int kargs_cache_mod = C4M_MAX_KARGS_NESTING_DEPTH - 1;

static c4m_karg_info_t *
c4m_kargs_acquire()
{
    if (kargs_next_entry == -1) {
        kargs_next_entry = 0;
        int alloc_len    = sizeof(c4m_base_obj_t) + sizeof(c4m_karg_info_t);
        int arg_len      = C4M_MAX_KEYWORD_SIZE * sizeof(c4m_one_karg_t);

        for (int i = 0; i < C4M_MAX_KARGS_NESTING_DEPTH; i++) {
            c4m_base_obj_t *record = c4m_rc_alloc(alloc_len);

            record->base_data_type = (c4m_dt_info_t *)&c4m_base_type_info[C4M_T_KEYWORD];
            record->concrete_type  = c4m_type_kargs();

            c4m_karg_info_t *karg = (c4m_karg_info_t *)record->data;
            karg->args            = c4m_rc_alloc(arg_len);
            karg->num_provided    = 0;
            c4m_gc_register_root(karg->args, arg_len / 8);

            kcache[i]        = karg;
            kargs_next_entry = 0;
        }
    }

    kargs_next_entry &= kargs_cache_mod;

    c4m_karg_info_t *result = kcache[kargs_next_entry++];

    return result;
}

c4m_karg_info_t *
c4m_pass_kargs(int nargs, ...)
{
    va_list args;

    va_start(args, nargs);

#ifdef C4M_DEBUG_KARGS
    char *fn   = va_arg(args, char *);
    int   line = va_arg(args, int);

    printf("kargs: %d args called from c4m_kw() on %s:%d\n", nargs, fn, line);
#endif

    if (nargs & 1) {
        C4M_CRAISE(
            "Got an odd number of parameters to kw() keyword decl macro.");
    }

    nargs >>= 1;
    c4m_karg_info_t *kargs = c4m_kargs_acquire();

    kargs->num_provided = nargs;

    assert(nargs < C4M_MAX_KEYWORD_SIZE);
    assert(kargs->num_provided == nargs);

    for (int i = 0; i < nargs; i++) {
        kargs->args[i].kw    = va_arg(args, char *);
        kargs->args[i].value = va_arg(args, void *);
    }

    va_end(args);

    return kargs;
}

c4m_karg_info_t *
c4m_get_kargs(va_list args)
{
    c4m_obj_t cur;
    va_list   arg_copy;

    va_copy(arg_copy, args);

    cur = va_arg(arg_copy, c4m_obj_t);

    while (cur != NULL) {
        if (c4m_get_my_type(cur) == c4m_type_kargs()) {
            va_end(arg_copy);
            return cur;
        }

        cur = va_arg(arg_copy, c4m_obj_t);
    }

    va_end(arg_copy);
    return NULL;
}

// This is for varargs functions, so it def needs to copy the va_list.
c4m_karg_info_t *
c4m_get_kargs_and_count(va_list args, int *nargs)
{
    va_list   arg_copy;
    c4m_obj_t cur;
    int       count = 0;

    va_copy(arg_copy, args);

    cur = va_arg(arg_copy, c4m_obj_t);

    while (cur != NULL) {
        if (c4m_get_my_type(cur) == c4m_type_kargs()) {
            *nargs = count;
            va_end(arg_copy);
            return cur;
        }

        count++;
        cur = va_arg(arg_copy, c4m_obj_t);
    }

    *nargs = count;
    va_end(arg_copy);
    return NULL;
}

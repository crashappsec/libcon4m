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

static void
kargs_init(c4m_karg_info_t *kargs, va_list args)
{
    int nargs = va_arg(args, int);

    kargs->num_provided = nargs;
    kargs->args         = c4m_gc_raw_alloc(sizeof(c4m_one_karg_t) * nargs,
                                   NULL);
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

    c4m_karg_info_t *kargs = c4m_new(c4m_type_kargs(), nargs);

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

const c4m_vtable_t c4m_kargs_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)kargs_init,
        // Explicit because some compilers don't seem to always properly
        // zero it (Was sometimes crashing on a `c4m_stream_t` on my mac).
        [C4M_BI_FINALIZER]   = NULL,
        NULL, // Aboslutelty nothing else.
    },
};

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

static thread_local c4m_karg_info_t *kcache[C4M_MAX_KARGS_NESTING_DEPTH];
static thread_local int              kargs_next_entry = 0;

const int kargs_cache_mod = C4M_MAX_KARGS_NESTING_DEPTH - 1;

static thread_local bool init_kargs = false;

static c4m_karg_info_t *
c4m_kargs_acquire()
{
    if (!init_kargs) {
        c4m_gc_register_root(kcache, C4M_MAX_KARGS_NESTING_DEPTH);

        for (int i = 0; i < C4M_MAX_KARGS_NESTING_DEPTH; i++) {
            c4m_karg_info_t *karg = c4m_gc_alloc_mapped(c4m_karg_info_t,
                                                        C4M_GC_SCAN_ALL);
            c4m_alloc_hdr   *h    = c4m_object_header(karg);
            h->type               = c4m_type_kargs();
            h->con4m_obj          = true;

            karg->args = c4m_gc_array_alloc(c4m_one_karg_t,
                                            C4M_MAX_KEYWORD_SIZE);
            kcache[i]  = karg;
        }
        init_kargs = true;
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

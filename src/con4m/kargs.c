/*
 * Copyright © 2021 John Viega
 *
 * See LICENSE.txt for licensing info.
 *
 * Name:          kargs.h
 * Description:   Support for limited keyword-style arguments in C functions.
 * Author:        John Viega, john@zork.org
 */

#include <con4m.h>

const uint64_t pmap_karg_t[2] = {
    0x0000000000000001,
    0xe000000000000000
};
const uint64_t pmap_kcache[2] = {
    0x0000000000000001,
    0x8000000000000000
};


#define BAD_ARG_FMT  "Unrecognized keyword argument: '%s'. Options here are: "

static uint64_t      kargs_process_base(char *, va_list, va_list);
static void          kargs_init(void);
static karg_cache_t *kargs_get_cache(void);

__thread bool          kargs_inited = false;
__thread karg_cache_t *karg_cache   = NULL;

uint64_t
kargs_process_w_req_param(va_list actuals, ...)
{
    char    *first_key;
    va_list  formals;
    uint64_t provided;

    va_start(formals, actuals);
    first_key = (char *)va_arg(actuals, char *);
    if (first_key == KARG_END) {
        va_end(formals);
        return 0;
    }
    provided = kargs_process_base(first_key, actuals, formals);
    va_end(formals);

    return provided;
}

uint64_t
kargs_process_wo_req_param(char *first_key, va_list actuals, ...)
{
    va_list  formals;
    uint64_t provided;

    if (first_key == KARG_END) {
        return 0;
    }
    va_start(formals, actuals);
    provided = kargs_process_base(first_key, actuals, formals);
    va_end(formals);

    return provided;
}

static uint64_t
kargs_process_base(char *first_key, va_list actuals, va_list formals)
{
    karg_cache_t *cache;
    karg_t       *kwinfo = NULL;
    karg_t       *last   = NULL;
    karg_t       *cur    = NULL;
    char         *argname;
    uint64_t      provided = 0;
    int           argcount = 0;

    // Process the list of keyword args.

    if (!kargs_inited) {
	kargs_init();
	kargs_inited = true;
    }

    argname = (char *)va_arg(formals, char *);
    cache   = kargs_get_cache();

    while (argname != KARG_END) {
        if (cache->top) {
            cur        = cache->top;
            cache->top = cur->next;
        }
        else {
            cur = (karg_t *)gc_alloc_mapped(karg_t, &pmap_karg_t[0]);
        }
        if (!kwinfo) {
            kwinfo = cur;
            last   = cur;
        }
        else {
            last->next = cur;
            last       = cur;
        }
        cur->next   = NULL;
        cur->name   = argname;
        cur->size   = va_arg(formals, int);
        cur->outloc = va_arg(formals, intptr_t *);
        argname     = (char *)va_arg(formals, char *);
    }
    argname = first_key;

    while (argname != KARG_END) {
        cur      = kwinfo;
	argcount = 0;
        while (true) {
            if (!cur) {
		// For now, we'll just print error info and abort,
		// until we add in some exception handling (which I plan
		// to take from past code of mine.
		fprintf(stderr, BAD_ARG_FMT, argname);
                cur = kwinfo;
                goto start_here;
                while (cur) {
		    fprintf(stderr, ", ");
		start_here:
		    fprintf(stderr, "%s", cur->name);
		    cur = cur->next;
                }
                abort();
            }
// is_read_only_memory() is kinda slow. Make it easy to turn off.
#ifdef DO_KARG_STATIC_MEM_CHECK
            if (!is_read_only_memory(argname) ||
                // This is a dumb heuristic to try to avoid most
                // silent crashes that are likely to happen here.  At
                // this point, we've determined the memory is
                // read-only, but that's not just char *'s (which is
                // what we want to see), but could be int constants.
                // Ideally, we'd know the memory address range for
                // static data, but there's no good, portable way to
                // figure that out.  So we cheat and just special-case
                // constants below 4096, since that should cover the
                // vast majority of values seen in the real world.
                // Note that 0 is our sentinel though (to indicate end
                // of arguments), and so that's going to be a common
                // reason to crash here. I might end up changing the
                // sentinel to (uintptr_t)-1 to make this problem a
                // bit better.  Or, (int)-1 stored in a 64-bit word,
                // since it will end up not colliding with small
                // negatives when we cast back to 64 bits.  The
                // problem is, it requires subtle changes to the macro
                // code that is pretty fragile, so I'm not going to do
                // this soon.
                (argname && (intptr_t)argname < 0x1000)) {
                fprintf(stderr, "Got a value when expecting a keyword.");
		abort();
            }
#endif
            if (!strcmp(cur->name, argname)) {
                break;
            }
            cur = cur->next;
            argcount++;
        }
        if (argcount < 64) {
            if (provided & (1 << argcount)) {
                fprintf(stderr, "Duplicate keyword argument provided.");
		abort();
            }
            provided |= (1 << argcount);
        }
        switch (cur->size) {
        case sizeof(int32_t):
            *((int32_t *)cur->outloc) = (intptr_t)va_arg(actuals, int32_t);
            break;
        case sizeof(int64_t):
            *((int64_t *)cur->outloc) = (intptr_t)va_arg(actuals, int64_t);
            break;
        default:
            fprintf(stderr, "keyword args must be 32 bit or 64 bit values.");
	    abort();
        }
        argname = (char *)va_arg(actuals, char *);
    }
    // Cache all karg_t's for future use.
    if (!cache->top) {
        cache->top = kwinfo;
    }
    else {
        cur = cache->top;
        while (cur->next) {
            cur = cur->next;
        }
        cur->next = kwinfo;
    }

    return provided;
}

static void
kargs_init(void)
{
    karg_cache = (karg_cache_t *)gc_alloc_mapped(karg_cache_t, &pmap_kcache[0]);
    con4m_gc_register_root(&karg_cache, 1);
}

static karg_cache_t *
kargs_get_cache(void)
{
    karg_cache_t *ret;

    ret = karg_cache;
    if (!ret) {
        karg_cache = (karg_cache_t *)gc_alloc_mapped(karg_cache_t,
						     &pmap_kcache[0]);
        ret        = karg_cache;
    }

    return ret;
}

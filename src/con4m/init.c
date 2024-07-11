// When libcon4m is actually used as a library, call this, because the
// constructors are likely to not get called properly.

#include "con4m.h"

char **c4m_stashed_argv;
char **c4m_stashed_envp;

// A few builtins here; will break this out soon.
uint64_t
c4m_clz(uint64_t n)
{
    return __builtin_clzll(n);
}

uint64_t
c4m_rand()
{
    return c4m_rand64();
}

static void
c4m_register_builtins()
{
    c4m_add_static_function(c4m_new_utf8("c4m_clz"), c4m_clz);
    c4m_add_static_function(c4m_new_utf8("c4m_gc_remove_hold"),
                            c4m_gc_remove_hold);
    c4m_add_static_function(c4m_new_utf8("c4m_rand"), c4m_rand);
}

__attribute__((constructor)) void
c4m_init(int argc, char **argv, char **envp)
{
    c4m_stashed_argv = argv;
    c4m_stashed_envp = envp;

    c4m_backtrace_init(argv[0]);
    c4m_gc_openssl();
    c4m_initialize_gc();
    c4m_gc_set_finalize_callback((void *)c4m_finalize_allocation);
    c4m_initialize_global_types();
    c4m_init_std_streams();
    c4m_register_builtins();
}

c4m_list_t *
c4m_get_program_arguments()
{
    c4m_list_t *result = c4m_list(c4m_type_utf8());
    char      **cur    = c4m_stashed_argv + 1; // Skip argv0.

    while (*cur != NULL) {
        c4m_list_append(result, c4m_new_utf8(*cur));
        cur++;
    }

    return result;
}

c4m_utf8_t *
c4m_get_argv0()
{
    return c4m_new_utf8(*c4m_stashed_argv);
}

static c4m_dict_t *cached_environment_vars = NULL;

static inline int
find_env_value(char *c, char **next)
{
    char n;
    int  i = 0;

    while ((n = *c++) != 0) {
        if (n == '=') {
            *next = c;
            return i;
        }
        i++;
    }
    *next = 0;
    return 0;
}

static void
load_env(c4m_dict_t *environment_vars)
{
    char **ptr = c4m_stashed_envp;
    char  *item;
    char  *val;
    int    len1;

    while ((item = *ptr++) != NULL) {
        len1 = find_env_value(item, &val);
        if (!len1) {
            continue;
        }
        c4m_utf8_t *key   = c4m_new(c4m_type_utf8(),
                                  c4m_kw("length",
                                         c4m_ka(len1),
                                         "cstring",
                                         c4m_ka(item)));
        c4m_utf8_t *value = c4m_new_utf8(val);

        hatrack_dict_put(environment_vars, key, value);
        assert(hatrack_dict_get(environment_vars, key, NULL) == value);
    }

    c4m_gc_register_root(&environment_vars, 1);
}

c4m_utf8_t *
c4m_get_env(c4m_utf8_t *name)
{
    if (cached_environment_vars == NULL) {
        c4m_gc_register_root(&cached_environment_vars, 1);
        cached_environment_vars = c4m_new(c4m_type_dict(c4m_type_utf8(),
                                                        c4m_type_utf8()));
        load_env(cached_environment_vars);
    }

    return hatrack_dict_get(cached_environment_vars, name, NULL);
}

c4m_dict_t *
c4m_environment()
{
    c4m_dict_t *result = c4m_new(c4m_type_dict(c4m_type_utf8(),
                                               c4m_type_utf8()));

    load_env(result);

    return result;
}

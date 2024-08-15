// When libcon4m is actually used as a library, call this, because the
// constructors are likely to not get called properly.

#include "con4m.h"

char **c4m_stashed_argv;
char **c4m_stashed_envp;

static void
c4m_register_builtins(void)
{
    c4m_add_static_function(c4m_new_utf8("c4m_clz"), c4m_clz);
    c4m_add_static_function(c4m_new_utf8("c4m_gc_remove_hold"),
                            c4m_gc_remove_hold);
    c4m_add_static_function(c4m_new_utf8("c4m_rand64"), c4m_rand64);
}

c4m_list_t *
c4m_get_program_arguments(void)
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
c4m_get_argv0(void)
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

        c4m_gc_register_root(&environment_vars, 1);
        hatrack_dict_put(environment_vars, key, value);
        assert(hatrack_dict_get(environment_vars, key, NULL) == value);
    }
}

c4m_utf8_t *
c4m_get_env(c4m_utf8_t *name)
{
    if (cached_environment_vars == NULL) {
        cached_environment_vars = c4m_new(c4m_type_dict(c4m_type_utf8(),
                                                        c4m_type_utf8()));
        load_env(cached_environment_vars);
    }

    return hatrack_dict_get(cached_environment_vars, name, NULL);
}

c4m_dict_t *
c4m_environment(void)
{
    c4m_dict_t *result = c4m_new(c4m_type_dict(c4m_type_utf8(),
                                               c4m_type_utf8()));

    load_env(result);

    return result;
}

static c4m_utf8_t *con4m_root       = NULL;
c4m_list_t        *con4m_path       = NULL;
c4m_set_t         *con4m_extensions = NULL;

c4m_utf8_t *
c4m_con4m_root(void)
{
    if (con4m_root == NULL) {
        con4m_root = c4m_get_env(c4m_new_utf8("CON4M_ROOT"));

        if (con4m_root == NULL) {
            c4m_utf8_t *tmp = c4m_resolve_path(c4m_app_path());
            C4M_TRY
            {
                c4m_utf8_t *tmp2 = c4m_cstr_format("{}/../..", tmp);
                con4m_root       = c4m_resolve_path(tmp2);
            }
            C4M_EXCEPT
            {
                con4m_root = c4m_get_current_directory();
            }
            C4M_TRY_END;
        }
        else {
            con4m_root = c4m_resolve_path(con4m_root);
        }
    }

    return con4m_root;
}

c4m_utf8_t *
c4m_system_module_path(void)
{
    c4m_list_t *l = c4m_list(c4m_type_utf8());

    c4m_list_append(l, c4m_con4m_root());
    c4m_list_append(l, c4m_new_utf8("sys"));

    return c4m_path_join(l);
}

static void
c4m_init_path(void)
{
    c4m_list_t *parts;

    con4m_extensions = c4m_set(c4m_type_utf8());

    c4m_set_add(con4m_extensions, c4m_new_utf8("c4m"));

    c4m_utf8_t *extra = c4m_get_env(c4m_new_utf8("CON4M_EXTENSIONS"));

    if (extra != NULL) {
        parts = c4m_str_split(extra, c4m_new_utf8(":"));
        for (int i = 0; i < c4m_list_len(parts); i++) {
            c4m_set_add(con4m_extensions,
                        c4m_to_utf8(c4m_list_get(parts, i, NULL)));
        }
    }

    c4m_set_package_search_path(
        c4m_system_module_path(),
        c4m_resolve_path(c4m_new_utf8(".")));

    extra = c4m_get_env(c4m_new_utf8("CON4M_PATH"));

    if (extra == NULL) {
        c4m_list_append(con4m_path, c4m_con4m_root());
        return;
    }

    parts = c4m_str_split(extra, c4m_new_utf8(":"));

    // Always keep sys and cwd in the path; sys is first, . last.
    c4m_list_t *new_path = c4m_list(c4m_type_utf8());

    c4m_list_append(new_path, c4m_system_module_path());

    for (int i = 0; i < c4m_list_len(parts); i++) {
        c4m_utf8_t *s = c4m_to_utf8(c4m_list_get(parts, i, NULL));

        c4m_list_append(con4m_path, c4m_resolve_path(s));
    }

    c4m_list_append(new_path, c4m_resolve_path(c4m_new_utf8(".")));

    con4m_path = new_path;
}

c4m_utf8_t *
c4m_path_search(c4m_utf8_t *package, c4m_utf8_t *module)
{
    uint64_t     n_items;
    c4m_utf8_t  *munged     = NULL;
    c4m_utf8_t **extensions = c4m_set_items_sort(con4m_extensions, &n_items);

    if (package != NULL && c4m_str_codepoint_len(package) != 0) {
        c4m_list_t *parts = c4m_str_split(package, c4m_new_utf8("."));

        c4m_list_append(parts, module);
        munged = c4m_to_utf8(c4m_str_join(parts, c4m_new_utf8("/")));
    }

    int l = (int)c4m_list_len(con4m_path);

    if (munged != NULL) {
        for (int i = 0; i < l; i++) {
            c4m_str_t *dir = c4m_list_get(con4m_path, i, NULL);

            for (int j = 0; j < (int)n_items; j++) {
                c4m_utf8_t *ext = extensions[j];
                c4m_str_t  *s   = c4m_cstr_format("{}/{}.{}", dir, munged, ext);
                s               = c4m_to_utf8(s);

                struct stat info;

                if (!stat(s->data, &info)) {
                    return s;
                }
            }
        }
    }
    else {
        for (int i = 0; i < l; i++) {
            c4m_str_t *dir = c4m_list_get(con4m_path, i, NULL);
            for (int j = 0; j < (int)n_items; j++) {
                c4m_utf8_t *ext = extensions[j];
                c4m_str_t  *s   = c4m_cstr_format("{}/{}.{}", dir, module, ext);
                s               = c4m_to_utf8(s);

                struct stat info;

                if (!stat(s->data, &info)) {
                    return s;
                }
            }
        }
    }

    return NULL;
}

void
_c4m_set_package_search_path(c4m_utf8_t *dir, ...)
{
    con4m_path = c4m_list(c4m_type_utf8());

    va_list args;

    va_start(args, dir);

    while (dir != NULL) {
        c4m_list_append(con4m_path, dir);
        dir = va_arg(args, c4m_utf8_t *);
    }

    va_end(args);
}
#ifdef C4M_STATIC_FFI_BINDING
#define FSTAT(x) c4m_add_static_function(c4m_new_utf8(#x), x)
#else
#define FSTAT(x)
#endif

void
c4m_add_static_symbols(void)
{
    FSTAT(c4m_list_append);
    FSTAT(c4m_wrapper_join);
    FSTAT(c4m_str_upper);
    FSTAT(c4m_str_lower);
    FSTAT(c4m_str_split);
    FSTAT(c4m_str_pad);
    FSTAT(c4m_str_starts_with);
    FSTAT(c4m_str_ends_with);
    FSTAT(c4m_wrapper_hostname);
    FSTAT(c4m_wrapper_os);
    FSTAT(c4m_wrapper_arch);
    FSTAT(c4m_wrapper_repr);
    FSTAT(c4m_wrapper_to_str);
    FSTAT(c4m_high);
    FSTAT(c4m_len);
    FSTAT(c4m_snap_column);
    FSTAT(c4m_now);
    FSTAT(c4m_timestamp);
    FSTAT(c4m_process_cpu);
    FSTAT(c4m_thread_cpu);
    FSTAT(c4m_uptime);
    FSTAT(c4m_program_clock);
    FSTAT(c4m_copy_object);
    FSTAT(c4m_get_c_backtrace);
    FSTAT(c4m_lookup_color);
    FSTAT(c4m_to_vga);
    FSTAT(c4m_read_utf8_file);
    FSTAT(c4m_read_binary_file);
    FSTAT(c4m_list_resize);
    FSTAT(c4m_list_append);
    FSTAT(c4m_list_sort);
    FSTAT(c4m_list_pop);
    FSTAT(c4m_list_contains);
}

static void
c4m_initialize_library(void)
{
    c4m_init_program_timestamp();
    c4m_init_std_streams();
}

__attribute__((constructor)) void
c4m_init(int argc, char **argv, char **envp)
{
    c4m_stashed_argv = argv;
    c4m_stashed_envp = envp;

    c4m_backtrace_init(argv[0]);
    c4m_gc_openssl();
    c4m_initialize_gc();
    c4m_gc_register_root(&cached_environment_vars, 1);
    c4m_gc_register_root(&con4m_root, 1);
    c4m_gc_register_root(&con4m_path, 1);
    c4m_gc_register_root(&con4m_extensions, 1);
    c4m_gc_set_finalize_callback((void *)c4m_finalize_allocation);
    c4m_initialize_global_types();
    c4m_initialize_library();
    c4m_register_builtins();
    c4m_init_path();
}

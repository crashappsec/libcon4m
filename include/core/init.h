#pragma once
#include "con4m.h"

__attribute__((constructor)) void c4m_init(int, char **, char **);

extern char **c4m_stashed_argv;
extern char **c4m_stashed_envp;

static inline char **
c4m_raw_argv(void)
{
    return c4m_stashed_argv;
}

static inline char **
c4m_raw_envp(void)
{
    return c4m_stashed_envp;
}

extern c4m_list_t *c4m_get_program_arguments(void);
extern c4m_utf8_t *c4m_get_argv0(void);
extern c4m_utf8_t *c4m_get_env(c4m_utf8_t *);
extern c4m_dict_t *c4m_environment(void);
extern c4m_utf8_t *c4m_path_search(c4m_utf8_t *, c4m_utf8_t *);
extern c4m_utf8_t *c4m_con4m_root(void);
extern c4m_utf8_t *c4m_system_module_path(void);
extern void        c4m_add_static_symbols(void);
extern c4m_list_t *con4m_path;
extern c4m_dict_t *con4m_extensions;
extern c4m_list_t *c4m_get_allowed_file_extensions(void);

static inline c4m_list_t *
c4m_get_module_search_path(void)
{
    return con4m_path;
}

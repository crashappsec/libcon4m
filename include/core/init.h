#pragma once
#include "con4m.h"

__attribute__((constructor)) void c4m_init(int, char **, char **);

extern char **c4m_stashed_argv;
extern char **c4m_stashed_envp;

static inline char **
c4m_raw_argv()
{
    return c4m_stashed_argv;
}

static inline char **
c4m_raw_envp()
{
    return c4m_stashed_envp;
}

extern c4m_list_t *c4m_get_program_arguments();
extern c4m_utf8_t *c4m_get_argv0();
extern c4m_utf8_t *c4m_get_env(c4m_utf8_t *);
extern c4m_dict_t *c4m_environment();
extern c4m_utf8_t *c4m_path_search(c4m_utf8_t *, c4m_utf8_t *);
extern c4m_utf8_t *c4m_con4m_root();
c4m_utf8_t        *c4m_system_module_path();

extern c4m_list_t *con4m_path;
extern c4m_set_t  *con4m_extensions;

static inline c4m_list_t *
c4m_get_module_search_path()
{
    return con4m_path;
}

static inline c4m_set_t *
c4m_get_allowed_file_extensions()
{
    return con4m_extensions;
}

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

c4m_xlist_t *c4m_get_program_arguments();
c4m_utf8_t  *c4m_get_argv0();

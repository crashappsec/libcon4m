// When libcon4m is actually used as a library, call this, because the
// constructors are likely to not get called properly.

#include "con4m.h"

char **c4m_stashed_argv;
char **c4m_stashed_envp;

__attribute__((constructor)) void
c4m_init(int argc, char **argv, char **envp)
{
    c4m_stashed_argv = argv;
    c4m_stashed_envp = envp;

    c4m_gc_openssl();
    c4m_initialize_gc();
    c4m_initialize_global_types();
}

c4m_xlist_t *
c4m_get_program_arguments()
{
    c4m_xlist_t *result = c4m_new(c4m_tspec_xlist(c4m_tspec_utf8()));
    char       **cur    = c4m_stashed_argv + 1; // Skip argv0.

    while (*cur != NULL) {
        c4m_xlist_append(result, c4m_new_utf8(*cur));
        cur++;
    }

    return result;
}

c4m_utf8_t *
c4m_get_argv0()
{
    return c4m_new_utf8(*c4m_stashed_argv);
}

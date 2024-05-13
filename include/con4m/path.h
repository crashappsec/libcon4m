#pragma once
#include "con4m.h"

c4m_utf8_t *c4m_resolve_path(c4m_utf8_t *);
c4m_utf8_t *c4m_path_tilde_expand(c4m_utf8_t *);
c4m_utf8_t *c4m_get_user_dir(c4m_utf8_t *);
c4m_utf8_t *c4m_get_current_directory(c4m_utf8_t *);
c4m_utf8_t *c4m_path_join(c4m_xlist_t *);

static inline c4m_utf8_t *
c4m_get_home_directory()
{
    return c4m_path_tilde_expand(NULL);
}

// This maybe should move into a user / uid module, but I did it in
// the context of path testing (getting the proper user name for the
// tests).
static inline c4m_utf8_t *
c4m_get_user_name()
{
    struct passwd *pw = getpwuid(getuid());

    return c4m_new_utf8(pw->pw_name);
}

#pragma once
#include "con4m.h"

typedef enum {
    C4M_FK_NOT_FOUND       = 0,
    C4M_FK_IS_REG_FILE     = S_IFREG,
    C4M_FK_IS_DIR          = S_IFDIR,
    C4M_FK_IS_FLINK        = S_IFLNK,
    C4M_FK_IS_DLINK        = S_IFLNK | S_IFDIR,
    C4M_FK_IS_SOCK         = S_IFSOCK,
    C4M_FK_IS_CHR_DEVICE   = S_IFCHR,
    C4M_FK_IS_BLOCK_DEVICE = S_IFBLK,
    C4M_FK_IS_FIFO         = S_IFIFO,
    C4M_FK_OTHER           = ~0,
} c4m_file_kind;

c4m_utf8_t   *c4m_resolve_path(c4m_utf8_t *);
c4m_utf8_t   *c4m_path_tilde_expand(c4m_utf8_t *);
c4m_utf8_t   *c4m_get_user_dir(c4m_utf8_t *);
c4m_utf8_t   *c4m_get_current_directory(c4m_utf8_t *);
c4m_utf8_t   *c4m_path_join(c4m_xlist_t *);
c4m_file_kind c4m_get_file_kind(c4m_utf8_t *);
c4m_xlist_t  *_c4m_path_walk(c4m_utf8_t *, ...);

#define c4m_path_walk(x, ...) _c4m_path_walk(x, KFUNC(__VA_ARGS__))

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

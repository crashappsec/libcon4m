#define C4M_USE_INTERNAL_API

#include "con4m.h"

c4m_utf8_t *
c4m_get_current_directory(c4m_utf8_t *s)
{
    char buf[MAXPATHLEN + 1];

    getcwd(buf, MAXPATHLEN);

    return c4m_new_utf8(buf);
}

// This is private; it mutates the string, which we don't normally
// want to support, and only do so because we know it's all private.
static c4m_utf8_t *
remove_extra_slashes(c4m_utf8_t *result)
{
    int i = result->codepoints;

    while (result->data[--i] == '/') {
        result->data[i] = 0;
        result->codepoints--;
    }

    return result;
}

c4m_utf8_t *
c4m_get_user_dir(c4m_utf8_t *user)
{
    c4m_utf8_t    *result;
    struct passwd *pw;

    if (user == NULL) {
        result = c4m_get_env(c4m_new_utf8("HOME"));
        if (!result) {
            pw = getpwent();
            if (pw == NULL) {
                result = c4m_new_utf8("/");
            }
            else {
                result = c4m_new_utf8(pw->pw_dir);
            }
        }
    }
    else {
        pw = getpwnam(user->data);
        if (pw == NULL) {
            result = user;
        }
        result = c4m_new_utf8(pw->pw_dir);
    }

    return remove_extra_slashes(result);
}

static c4m_utf8_t *
internal_normalize_and_join(c4m_xlist_t *pieces)
{
    int partlen = c4m_xlist_len(pieces);
    int nextout = 0;

    for (int i = 0; i < partlen; i++) {
        c4m_utf8_t *s = c4m_to_utf8(c4m_xlist_get(pieces, i, NULL));

        if (s->codepoints == 0) {
            continue;
        }

        if (s->data[0] == '.') {
            switch (s->codepoints) {
            case 1:
                continue;
            case 2:
                if (s->data[1] == '.') {
                    --nextout;
                    continue;
                }
                break;
            default:
                break;
            }
        }

        if (nextout < 0) {
            C4M_CRAISE("Invalid path; backs up past the root directory.");
        }

        c4m_xlist_set(pieces, nextout++, s);
    }

    if (nextout == 0) {
        return c4m_get_slash_const();
    }

    c4m_utf8_t *result = NULL;

    for (int i = 0; i < nextout; i++) {
        c4m_utf8_t *s = c4m_xlist_get(pieces, i, NULL);

        if (!s->codepoints) {
            continue;
        }

        if (!result) {
            result = c4m_cstr_format("/{}", s);
        }
        else {
            result = c4m_cstr_format("{}/{}", result, s);
        }
    }

    return c4m_to_utf8(result);
}

static c4m_xlist_t *
raw_path_tilde_expand(c4m_utf8_t *in)
{
    if (!in || !in->codepoints) {
        in = c4m_get_slash_const();
    }

    if (in->data[0] != '~') {
        return c4m_str_xsplit(in, c4m_get_slash_const());
    }

    c4m_xlist_t *parts = c4m_str_xsplit(in, c4m_get_slash_const());
    c4m_utf8_t  *home  = c4m_to_utf8(c4m_xlist_get(parts, 0, NULL));

    if (c4m_str_codepoint_len(home) == 1) {
        c4m_xlist_set(parts, 0, c4m_empty_string());
        parts = c4m_xlist_plus(c4m_str_xsplit(c4m_get_user_dir(NULL),
                                              c4m_get_slash_const()),
                               parts);
    }
    else {
        home->data++;
        c4m_xlist_set(parts, 0, c4m_empty_string());
        parts = c4m_xlist_plus(c4m_str_xsplit(c4m_get_user_dir(home),
                                              c4m_get_slash_const()),
                               parts);
        home->data--;
    }

    return parts;
}

c4m_utf8_t *
c4m_path_tilde_expand(c4m_utf8_t *in)
{
    return internal_normalize_and_join(raw_path_tilde_expand(in));
}

c4m_utf8_t *
c4m_resolve_path(c4m_utf8_t *s)
{
    c4m_xlist_t *parts;

    if (s == NULL || s->codepoints == 0) {
        return c4m_get_home_directory();
    }

    switch (s->data[0]) {
    case '~':
        return c4m_path_tilde_expand(s);
    case '/':
        return internal_normalize_and_join(
            c4m_str_xsplit(s, c4m_get_slash_const()));
    default:
        parts = c4m_str_xsplit(c4m_get_current_directory(NULL),
                               c4m_get_slash_const());
        c4m_xlist_plus_eq(parts, c4m_str_xsplit(s, c4m_get_slash_const()));
        return internal_normalize_and_join(parts);
    }
}

c4m_utf8_t *
c4m_path_join(c4m_xlist_t *items)
{
    c4m_utf8_t *result;
    c4m_utf8_t *tmp;
    uint8_t    *p;
    int         len   = 0; // Total length of output.
    int         first = 0; // First array index we'll use.
    int         last  = c4m_xlist_len(items);
    int         tmplen;    // Length of individual strings.

    for (int i = 0; i < last; i++) {
        tmp = c4m_xlist_get(items, i, NULL);
        if (!c4m_str_is_u8(tmp)) {
            C4M_CRAISE("Strings passed to c4m_path_join must be utf8 encoded.");
        }

        tmplen = c4m_str_byte_len(tmp);

        if (tmplen == 0) {
            continue;
        }
        if (tmp->data[0] == '/') {
            len   = tmplen;
            first = i;
        }
        else {
            len += tmplen;
        }
        if (tmp->data[tmplen - 1] != '/') {
            len++;
        }
    }

    result = c4m_new(c4m_tspec_utf8(), c4m_kw("length", c4m_ka(len)));
    p      = (uint8_t *)result->data;

    for (int i = first; i < last; i++) {
        tmp    = c4m_xlist_get(items, i, NULL);
        tmplen = c4m_str_byte_len(tmp);

        if (tmplen == 0) {
            continue;
        }

        memcpy(p, tmp->data, tmplen);
        p += (tmplen - 1);

        if (i + 1 != last && *p++ != '/') {
            *p++ = '/';
        }
    }

    result->byte_len = len;
    c4m_internal_utf8_set_codepoint_count(result);

    return result;
}

c4m_file_kind
c4m_get_file_kind(c4m_utf8_t *p)
{
    struct stat file_info;
    p = c4m_resolve_path(p);
    if (lstat(p->data, &file_info) != 0) {
        return C4M_FK_NOT_FOUND;
    }

    switch (file_info.st_mode & S_IFMT) {
    case S_IFREG:
        return C4M_FK_IS_REG_FILE;
    case S_IFDIR:
        return C4M_FK_IS_DIR;
    case S_IFSOCK:
        return C4M_FK_IS_SOCK;
    case S_IFCHR:
        return C4M_FK_IS_CHR_DEVICE;
    case S_IFBLK:
        return C4M_FK_IS_BLOCK_DEVICE;
    case S_IFIFO:
        return C4M_FK_IS_FIFO;
    case S_IFLNK:
        if (stat(p->data, &file_info) != 0) {
            return C4M_FK_NOT_FOUND;
        }
        switch (file_info.st_mode & S_IFMT) {
        case S_IFREG:
            return C4M_FK_IS_FLINK;
        case S_IFDIR:
            return C4M_FK_IS_DLINK;
        default:
            return C4M_FK_OTHER;
        }
    default:
        return C4M_FK_OTHER;
    }
}

typedef struct {
    bool         recurse;
    bool         yield_links;
    bool         yield_dirs;
    bool         follow_links;
    bool         ignore_special;
    bool         done_with_safety_checks;
    c4m_utf8_t  *sc_proc;
    c4m_utf8_t  *sc_dev;
    c4m_utf8_t  *sc_cwd;
    c4m_utf8_t  *sc_up;
    c4m_xlist_t *result;
    c4m_utf8_t  *resolved;
} c4m_walk_ctx;

static void
internal_path_walk(c4m_walk_ctx *ctx)
{
    struct stat    file_info;
    DIR           *dirobj;
    struct dirent *entry;
    c4m_utf8_t    *saved;
    bool           add_slash;

    if (!ctx->done_with_safety_checks) {
        if (c4m_str_starts_with(ctx->resolved, ctx->sc_proc)) {
            return;
        }
        if (c4m_str_starts_with(ctx->resolved, ctx->sc_dev)) {
            return;
        }
        if (c4m_str_codepoint_len(ctx->resolved) != 1) {
            ctx->done_with_safety_checks = true;
        }
    }

    if (lstat(ctx->resolved->data, &file_info) != 0) {
        return;
    }

    switch (file_info.st_mode & S_IFMT) {
    case S_IFREG:
        c4m_xlist_append(ctx->result, ctx->resolved);
        return;

    case S_IFDIR:
        if (ctx->yield_dirs) {
            c4m_xlist_append(ctx->result, ctx->resolved);
            return;
        }

actual_directory:
        if (!ctx->recurse) {
            return;
        }
        dirobj = opendir(ctx->resolved->data);
        if (dirobj == NULL) {
            return;
        }

        saved = ctx->resolved;
        if (c4m_index(saved, c4m_str_codepoint_len(saved) - 1) == '/') {
            add_slash = false;
        }
        else {
            add_slash = true;
        }

        while (true) {
            entry = readdir(dirobj);

            if (entry == NULL) {
                ctx->resolved = saved;
                return;
            }

            if (!strcmp(entry->d_name, "..")) {
                continue;
            }

            if (!strcmp(entry->d_name, ".")) {
                continue;
            }

            if (add_slash) {
                ctx->resolved = c4m_cstr_format("{}/{}",
                                                saved,
                                                c4m_new_utf8(entry->d_name));
            }
            else {
                ctx->resolved = c4m_cstr_format("{}{}",
                                                saved,
                                                c4m_new_utf8(entry->d_name));
            }
            internal_path_walk(ctx);
        }
        ctx->resolved = saved;
        return;

    case S_IFLNK:
        if (stat(ctx->resolved->data, &file_info) != 0) {
            return;
        }

        switch (file_info.st_mode & S_IFMT) {
        case S_IFREG:
            if (ctx->follow_links && ctx->yield_links) {
                char buf[PATH_MAX + 1] = {
                    0,
                };
                readlink(ctx->resolved->data, buf, PATH_MAX);
                c4m_xlist_append(ctx->result,
                                 c4m_resolve_path(c4m_new_utf8(buf)));
            }
            else {
                if (ctx->yield_links) {
                    c4m_xlist_append(ctx->result, ctx->resolved);
                }
            }
            return;
        case S_IFDIR:

            if (ctx->yield_dirs && ctx->yield_links) {
                c4m_xlist_append(ctx->result, ctx->resolved);
            }

            if (!ctx->follow_links || !ctx->recurse) {
                return;
            }

            saved                  = ctx->resolved;
            char buf[PATH_MAX + 1] = {
                0,
            };
            readlink(ctx->resolved->data, buf, PATH_MAX);
            ctx->resolved = c4m_resolve_path(c4m_new_utf8(buf));
            if (ctx->yield_dirs && !ctx->yield_links) {
                c4m_xlist_append(ctx->result, ctx->resolved);
            }

            goto actual_directory;

        default:
            if (!ctx->ignore_special) {
                c4m_xlist_append(ctx->result, ctx->resolved);
            }
            return;
        }
    case S_IFSOCK:
    case S_IFCHR:
    case S_IFBLK:
    case S_IFIFO:
    default:
        if (!ctx->ignore_special) {
            c4m_xlist_append(ctx->result, ctx->resolved);
        }
        return;
    }
}

c4m_xlist_t *
_c4m_path_walk(c4m_utf8_t *dir, ...)
{
    bool recurse        = true;
    bool yield_links    = false;
    bool yield_dirs     = false;
    bool ignore_special = true;
    bool follow_links   = false;

    c4m_karg_only_init(dir);
    c4m_kw_bool("recurse", recurse);
    c4m_kw_bool("yield_links", yield_links);
    c4m_kw_bool("yield_dirs", yield_dirs);
    c4m_kw_bool("follow_links", follow_links);
    c4m_kw_bool("ignore_special", ignore_special);

    c4m_walk_ctx ctx = {
        .sc_proc                 = c4m_new_utf8("/proc/"),
        .sc_dev                  = c4m_new_utf8("/dev/"),
        .sc_cwd                  = c4m_new_utf8("."),
        .sc_up                   = c4m_new_utf8(".."),
        .recurse                 = recurse,
        .yield_links             = yield_links,
        .yield_dirs              = yield_dirs,
        .follow_links            = follow_links,
        .ignore_special          = ignore_special,
        .done_with_safety_checks = false,
        .result                  = c4m_xlist(c4m_tspec_utf8()),
        .resolved                = c4m_resolve_path(dir),
    };

    internal_path_walk(&ctx);

    return ctx.result;
}

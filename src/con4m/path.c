#define C4M_INTERNAL_API

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
            pw     = getpwent();
            result = c4m_new_utf8(pw->pw_dir);
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

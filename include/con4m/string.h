#pragma once

#include "con4m.h"

extern c4m_str_t          *c4m_str_copy(const c4m_str_t *s);
extern c4m_utf32_t        *c4m_str_concat(const c4m_str_t *, const c4m_str_t *);
extern c4m_utf8_t         *c4m_to_utf8(const c4m_utf32_t *);
extern c4m_utf32_t        *c4m_to_utf32(const c4m_utf8_t *);
extern c4m_utf8_t         *c4m_from_file(const c4m_str_t *, int *);
extern int64_t             c4m_utf8_validate(const c4m_utf8_t *);
extern c4m_utf32_t        *c4m_str_slice(const c4m_str_t *, int64_t, int64_t);
extern c4m_utf8_t         *c4m_utf8_repeat(c4m_codepoint_t, int64_t);
extern c4m_utf32_t        *c4m_utf32_repeat(c4m_codepoint_t, int64_t);
extern c4m_utf32_t        *_c4m_str_strip(const c4m_str_t *s, ...);
extern c4m_str_t          *_c4m_str_truncate(const c4m_str_t *s, int64_t, ...);
extern c4m_utf32_t        *_c4m_str_join(const c4m_xlist_t *,
                                         const c4m_str_t *,
                                         ...);
extern c4m_utf8_t         *c4m_str_from_int(int64_t n);
extern int64_t             _c4m_str_find(c4m_str_t *, c4m_str_t *, ...);
extern c4m_utf8_t         *c4m_cstring(char *s, int64_t len);
extern c4m_utf8_t         *c4m_rich(c4m_utf8_t *, c4m_utf8_t *style);
extern c4m_codepoint_t     c4m_index(const c4m_str_t *, int64_t);
extern bool                c4m_str_can_coerce_to(c4m_type_t *, c4m_type_t *);
extern c4m_obj_t           c4m_str_coerce_to(const c4m_str_t *, c4m_type_t *);
extern c4m_xlist_t        *c4m_str_xsplit(c4m_str_t *, c4m_str_t *);
extern struct flexarray_t *c4m_str_split(c4m_str_t *, c4m_str_t *);
// This is in richlit.c
extern c4m_utf8_t         *c4m_rich_lit(char *);

#define c4m_str_strip(s, ...)       _c4m_str_strip(s, KFUNC(__VA_ARGS__))
#define c4m_str_truncate(s, n, ...) _c4m_str_truncate(s, n, KFUNC(__VA_ARGS__))
#define c4m_str_join(l, s, ...)     _c4m_str_join(l, s, KFUNC(__VA_ARGS__))
#define c4m_str_find(str, sub, ...) _c4m_str_find(str, sub, KFUNC(__VA_ARGS__))

extern const c4m_utf8_t *c4m_empty_string_const;
extern const c4m_utf8_t *c4m_newline_const;
extern const c4m_utf8_t *c4m_crlf_const;

static inline bool
c4m_str_is_u32(const c4m_str_t *s)
{
    return (bool)(s->codepoints < 0);
}

static inline bool
c4m_str_is_u8(const c4m_str_t *s)
{
    return (bool)(s->codepoints >= 0);
}

static inline int64_t
c4m_str_codepoint_len(const c4m_str_t *s)
{
    if (!s) {
        return 0;
    }
    if (s->codepoints < 0) {
        return ~(s->codepoints);
    }
    else {
        return s->codepoints;
    }
}

static inline int64_t
c4m_str_byte_len(const c4m_str_t *s)
{
    return s->byte_len;
}

extern int64_t c4m_str_render_len(const c4m_str_t *);

static inline c4m_utf8_t *
c4m_empty_string()
{
    c4m_utf8_t *s = c4m_str_copy(c4m_empty_string_const);

    s->codepoints = 0;

    return s;
}

static inline c4m_utf8_t *
c4m_str_newline()
{
    return c4m_str_copy(c4m_newline_const);
}

static inline c4m_utf8_t *
c4m_str_crlf()
{
    return c4m_str_copy(c4m_crlf_const);
}

static inline c4m_utf8_t *
c4m_new_utf8(const char *to_copy)
{
    return c4m_new(c4m_tspec_utf8(), c4m_kw("cstring", c4m_ka(to_copy)));
}

static inline char *
c4m_to_cstring(c4m_str_t *s)
{
    return s->data;
}

extern const uint64_t c4m_pmap_str[2];

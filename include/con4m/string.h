#pragma once

#include <con4m.h>

extern const int str_header_size;
extern const uint64_t pmap_str[2];
#define PMAP_STR ((uint64_t *)&pmap_str[0])


extern any_str_t  *string_copy(const any_str_t *s);
extern utf32_t    *string_concat(const any_str_t *, const any_str_t *);
extern utf8_t     *utf32_to_utf8(const utf32_t *);
extern utf32_t    *utf8_to_utf32(const utf8_t *);
extern utf8_t     *utf8_from_file(const any_str_t *, int *);
extern int64_t     utf8_validate(const utf8_t *);
extern utf32_t    *string_slice(const any_str_t *, int64_t, int64_t);
extern utf8_t     *utf8_repeat(codepoint_t, int64_t);
extern utf32_t    *utf32_repeat(codepoint_t, int64_t);
extern utf32_t    *_string_strip(const any_str_t *s, ...);
extern any_str_t  *_string_truncate(const any_str_t *s, int64_t, ...);
extern utf32_t    *_string_join(const xlist_t *l, const any_str_t *joiner, ...);
extern utf8_t     *string_from_int(int64_t n);
extern int64_t     _string_find(any_str_t *, any_str_t *, ...);
extern utf8_t     *con4m_cstring(char *s, int64_t len);
extern utf8_t     *con4m_rich(utf8_t *, utf8_t *style);
extern codepoint_t utf8_index(const utf8_t *, int64_t);
extern codepoint_t utf32_index(const utf32_t *, int64_t);
extern bool        string_can_coerce_to(type_spec_t *, type_spec_t *);
extern object_t    string_coerce_to(const any_str_t *, type_spec_t *);
extern xlist_t    *string_xsplit(any_str_t *, any_str_t *);


extern struct flexarray_t *string_split(any_str_t *, any_str_t *);

#define force_utf8(x) utf32_to_utf8(x)
#define force_utf32(x) utf8_to_utf32(x)
#define string_strip(s, ...) _string_strip(s, KFUNC(__VA_ARGS__))
#define string_truncate(s, n, ...) _string_truncate(s, n,  KFUNC(__VA_ARGS__))
#define string_join(l, s, ...) _string_join(l, s, KFUNC(__VA_ARGS__))
#define string_find(str, sub, ...) _string_find(str, sub, KFUNC(__VA_ARGS__))

extern const utf8_t *empty_string_const;
extern const utf8_t *newline_const;
extern const utf8_t *crlf_const;

static inline bool
string_is_u32(const any_str_t *s)
{
    return (bool)(s->codepoints < 0);
}

static inline bool
string_is_u8(const any_str_t *s)
{
    return (bool)(s->codepoints >= 0);
}

static inline int64_t
string_codepoint_len(const any_str_t *s)
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
string_byte_len(const any_str_t *s)
{
    return s->byte_len;
}

extern int64_t string_render_len(const any_str_t *);

static inline utf8_t *
empty_string()
{
    utf8_t *s     = string_copy(empty_string_const);
    s->codepoints = 0;

    return s;
}

static inline utf8_t *
string_newline()
{
    return string_copy(newline_const);
}

static inline utf8_t *
string_crlf()
{
    return string_copy(crlf_const);
}

static inline utf8_t *
new_utf8(const char *to_copy)
{
    return con4m_new(tspec_utf8(), "cstring", to_copy);
}

static inline char *
to_cstring(any_str_t *s)
{
    return s->data;
}

// This is in richlit.c
extern utf8_t * rich_lit(char *);

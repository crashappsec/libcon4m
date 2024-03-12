#pragma once

#include <con4m.h>

static inline bool
internal_is_space(codepoint_t cp)
{
    // Fast path.
    if (cp == ' ' || cp == '\n' || cp == '\r') {
	return true;
    }

    switch (utf8proc_category(cp)) {
    case CP_CATEGORY_ZS:
	return true;
    case CP_CATEGORY_ZL:
    case CP_CATEGORY_ZP:
	return true;
    default:
	return false;
    }
}

extern str_t   *c4str_new_u32(int64_t);
extern void     c4str_apply_style(str_t *, style_t);
extern int64_t  c4str_byte_len(const str_t *);
extern int64_t  c4str_len(const str_t *);
extern int64_t  c4str_render_len(const str_t *);
extern str_t   *c4str_copy(const str_t *s);
extern str_t   *c4str_concat(const str_t *, const str_t *);
extern str_t   *c4str_u32_to_u8(const str_t *);
extern str_t   *c4str_u8_to_u32(const str_t *);
extern str_t   *c4str_internal_new_u8(va_list);
extern str_t   *c4str_from_file(const char *, int *);
extern str_t   *c4str_slice(const str_t *, int64_t, int64_t);
extern str_t   *c4str_repeat_u8(codepoint_t, int64_t);
extern str_t   *c4str_repeat(codepoint_t, int64_t);
extern str_t   *_c4str_strip(const str_t *s, ...);
extern str_t   *_c4str_truncate(const str_t *s, int64_t, ...);
extern str_t   *_c4str_join(const xlist_t *l, const str_t *joiner, ...);

extern const uint64_t str_ptr_info[];
extern const con4m_vtable u8str_vtable;
extern const con4m_vtable u32str_vtable;

#define force_utf8(x) c4str_u32_to_u8(x)
#define force_utf32(x) c4str_u8_to_u32(x)
#define c4str_strip(s, ...) _c4str_strip(s, KFUNC(__VA_ARGS__))
#define c4str_truncate(s, n, ...) _c4str_truncate(s, n,  KFUNC(__VA_ARGS__))
#define c4str_join(l, s, ...) _c4str_join(l, s, KFUNC(__VA_ARGS__))

extern const str_t *empty_string_const;
extern const str_t *newline_const;
extern const str_t *crlf_const;

static inline str_t *
empty_string()
{
    str_t *s = c4str_copy(empty_string_const);
    to_internal(s)->codepoints = 0;

    return s;
}

static inline str_t *
c4str_newline()
{
    return c4str_copy(newline_const);
}

static inline str_t *
c4str_crlf()
{
    return c4str_copy(crlf_const);
}

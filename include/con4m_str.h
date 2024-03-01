#pragma once

#include <con4m_style.h>
#include <unibreak.h>
#include <utf8proc.h>

/**
 ** For UTF-32, we actually store in the codepoints field the bitwise
 ** NOT to distinguish whether strings are UTF-8 (the high bit will
 ** always be 0 with UTF-8).
 **/
typedef struct {
    int32_t       codepoints;
    int32_t       byte_len;
    style_info_t *styling;
    char          data[];
} real_str_t;

extern const int str_header_size;

typedef char str_t;

static inline real_str_t *
to_internal(str_t *s)
{
    return (real_str_t *)(s - str_header_size);
}

static inline bool
internal_is_u32(real_str_t *s)
{
    return (bool)(s->codepoints < 0);
}

static inline uint64_t
style_num_entries(real_str_t *s)
{
    if (s->styling == NULL) {
	return 0;
    }
    return s->styling->num_entries;
}

static inline void
alloc_styles(real_str_t *s, int n)
{
    if (s->styling != NULL) {
	free(s->styling);
    }

    s->styling = zalloc(sizeof(style_info_t) +
			n * sizeof(style_entry_t));

    s->styling->num_entries = n;
}

static inline void
copy_style_info(real_str_t *from_str, real_str_t *to_str)
{
    if (from_str->styling == NULL) {
	return;
    }

    size_t sz = style_size(from_str->styling->num_entries);

    alloc_styles(to_str, (int)(from_str->styling->num_entries));
    memcpy(to_str->styling, from_str->styling, sz);
}

static inline int32_t
internal_num_cp(real_str_t *s)
{
    if (s->codepoints < 0) {
	return ~s->codepoints;
    }
    else {
	return s->codepoints;
    }
}

static inline bool
internal_is_space(int32_t cp)
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

extern str_t   *c4str_new(int64_t len);
extern str_t   *c4str_new_u32(int64_t len);
extern str_t   *c4str_new_with_style(int64_t len, style_t style);
extern void     c4str_apply_style(str_t *s, style_t style);
extern str_t   *c4str_from_cstr(char *s);
extern str_t   *c4str_from_cstr_styled(char *str, style_t style);
extern str_t   *c4str_from_file(char *name, int *err);
extern int64_t  c4str_byte_len(str_t *s);
extern int64_t  c4str_len(str_t *s);
extern void     c4str_free(str_t *s);
extern str_t *  c4str_concat(str_t *p1, str_t *p2, ownership_t ownership);
extern str_t *  c4str_u32_to_u8(str_t *instr, ownership_t ownership);
extern str_t *  c4str_u8_to_u32(str_t *instr, ownership_t ownership);

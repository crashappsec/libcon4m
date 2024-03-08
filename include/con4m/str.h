#pragma once

#include <con4m.h>

/**
 ** For UTF-32, we actually store in the codepoints field the bitwise
 ** NOT to distinguish whether strings are UTF-8 (the high bit will
 ** always be 0 with UTF-8).
 **/
typedef struct {
    alignas(8)
    int32_t       codepoints;
    int32_t       byte_len;
    style_info_t *styling;
    char          data[];
} real_str_t;

extern const uint64_t pmap_str[2];
#define PMAP_STR ((uint64_t *)&pmap_str[0])

extern const int str_header_size;

static inline size_t
get_real_alloc_len(int rawbytes)
{
    return sizeof(real_str_t) + 4 + rawbytes;
}

static inline size_t
real_alloc_len(real_str_t *r)
{
    return get_real_alloc_len(r->byte_len);
}

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

static inline size_t
alloc_style_len(real_str_t *s)
{
    return sizeof(style_info_t) +
        s->styling->num_entries * sizeof(style_entry_t);
}

static inline void
alloc_styles(real_str_t *s, int n)
{
    s->styling = gc_flex_alloc(style_info_t, style_entry_t, n, PMAP_STR);

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

extern str_t   *c4str_new_u32(int64_t len);
extern void     c4str_apply_style(str_t *s, style_t style);
extern int64_t  c4str_byte_len(str_t *s);
extern int64_t  c4str_len(str_t *s);
extern str_t *  c4str_concat(str_t *p1, str_t *p2);
extern str_t *  c4str_u32_to_u8(str_t *instr);
extern str_t *  c4str_u8_to_u32(str_t *instr);
extern str_t *  c4str_internal_new_u8(va_list args);
extern str_t   *c4str_from_file(char *name, int *err);


extern const uint64_t str_ptr_info[];
extern const con4m_vtable u8str_vtable;
extern const con4m_vtable u32str_vtable;

#define force_utf8(x) c4str_u32_to_u8(x)

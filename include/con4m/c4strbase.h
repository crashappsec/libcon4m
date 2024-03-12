#pragma once

extern const int str_header_size;
extern const uint64_t pmap_str[2];
#define PMAP_STR ((uint64_t *)&pmap_str[0])

#define STATIC_STR_STRUCT(id, length) struct static_str_ ## id ## _st {        \
    con4m_dt_info *base_data_type;                                             \
    uint64_t       concrete_type;                                              \
    struct {                                                                   \
	alignas(8)                                                             \
	int32_t       codepoints;                                              \
	int32_t       byte_len;                                                \
	style_info_t *styling;                                                 \
	char          data[length];                                            \
    } r;                                                                       \
}

// This only works with ASCII strings, not arbitrary utf8.
#define STATIC_ASCII_STR(id, val) STATIC_STR_STRUCT(id, sizeof(val));          \
const struct static_str_ ## id ## _st _static_ ## id = {		       \
	.base_data_type = (con4m_dt_info *)&builtin_type_info[T_STR],          \
	.concrete_type  = T_STR,                                               \
        .r.byte_len     = sizeof(val),  				       \
	.r.codepoints   = sizeof(val),					       \
	.r.styling      = NULL,                                                \
        .r.data         = val };                                               \
    const str_t *id = (str_t *)& (_static_ ## id).r.data

typedef uint64_t style_t;

typedef struct {
    uint32_t start;
    uint32_t end;
    style_t  info;  // 16 bits of flags, 24 bits bg color, 24 bits fg color
} style_entry_t;

typedef struct {
    int64_t       num_entries;
    style_entry_t styles[];
} style_info_t;


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

typedef char str_t;

static inline real_str_t *
to_internal(const str_t *s)
{
    return (real_str_t *)(((str_t *)s) - str_header_size);
}

static inline bool
internal_is_u32(real_str_t *s)
{
    return (bool)(s->codepoints < 0);
}

static inline size_t
get_real_alloc_len(int rawbytes)
{
    return sizeof(object_t) + sizeof(real_str_t) + 4 + rawbytes;
}

static inline size_t
real_alloc_len(real_str_t *r)
{
    return get_real_alloc_len(r->byte_len);
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

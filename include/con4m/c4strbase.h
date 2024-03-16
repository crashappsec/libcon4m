#pragma once

extern const int str_header_size;
extern const uint64_t pmap_str[2];
#define PMAP_STR ((uint64_t *)&pmap_str[0])

typedef uint64_t style_t;
typedef int32_t  codepoint_t;

typedef struct {
    int32_t  start;
    int32_t  end;
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
    char         *data;
} base_str_t;

typedef base_str_t utf8_t;
typedef base_str_t utf32_t;
typedef base_str_t any_str_t;

struct internal_string_st {
    con4m_dt_info *base_data_type;
    uint64_t       concrete_type;
    base_str_t     s;
};

// This only works with ASCII strings, not arbitrary utf8.
#define STATIC_ASCII_STR(id, val)                                              \
const struct internal_string_st _static_ ## id = {                             \
	.base_data_type = (con4m_dt_info *)&builtin_type_info[T_UTF8],         \
	.concrete_type  = T_UTF8,                                              \
        .s.byte_len     = sizeof(val),  				       \
	.s.codepoints   = sizeof(val),					       \
	.s.styling      = NULL,                                                \
        .s.data         = val						       \
    };								               \
    const base_str_t *id = (base_str_t *)&(_static_ ## id.s)

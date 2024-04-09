#pragma once
#include "con4m.h"

/**
 ** For UTF-32, we actually store in the codepoints field the bitwise
 ** NOT to distinguish whether strings are UTF-8 (the high bit will
 ** always be 0 with UTF-8).
 **/
typedef struct {
    alignas(8)
        int32_t codepoints;
    int32_t       byte_len;
    style_info_t *styling;
    char         *data;
} base_str_t;

typedef struct break_info_st {
    int32_t num_slots;
    int32_t num_breaks;
    int32_t breaks[];
} break_info_t;

typedef base_str_t utf8_t;
typedef base_str_t utf32_t;
typedef base_str_t any_str_t;

// Only used for the static macro.
struct internal_string_st {
    dt_info   *base_data_type;
    uint64_t   concrete_type;
    uint64_t   hash_cache_1;
    uint64_t   hash_cache_2;
    base_str_t s;
};

// This struct is used to manage state when rending ansi.
typedef enum {
    U8_STATE_START_DEFAULT,
    U8_STATE_START_STYLE,
    U8_STATE_DEFAULT_STYLE, // Stop at a new start ix or at the end.
    U8_STATE_IN_STYLE       // Stop at a new end ix or at the end.
} u8_state_t;

// This only works with ASCII strings, not arbitrary utf8.
#define STATIC_ASCII_STR(id, val)                                \
    const struct internal_string_st _static_##id = {             \
        .base_data_type = (dt_info *)&builtin_type_info[T_UTF8], \
        .concrete_type  = T_UTF8,                                \
        .s.byte_len     = sizeof(val),                           \
        .s.codepoints   = sizeof(val),                           \
        .s.styling      = NULL,                                  \
        .s.data         = val};                                          \
    const base_str_t *id = (base_str_t *)&(_static_##id.s)

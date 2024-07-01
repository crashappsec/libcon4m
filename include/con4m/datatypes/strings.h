#pragma once
#include "con4m.h"

/**
 ** For UTF-32, we actually store in the codepoints field the bitwise
 ** NOT to distinguish whether strings are UTF-8 (the high bit will
 ** always be 0 with UTF-8).
 **/

#define C4M_STR_HASH_KEY_POINTER_OFFSET 0
#define C4M_HASH_CACHE_OFFSET           (-2 * (int32_t)sizeof(uint64_t))
typedef struct c4m_str_t {
    char             *data;
    c4m_style_info_t *styling;
    int32_t           byte_len;
    int32_t           codepoints : 31;
    unsigned int      utf32      : 1;
} c4m_str_t;

typedef c4m_str_t c4m_utf8_t;
typedef c4m_str_t c4m_utf32_t;

typedef struct break_info_st {
    int32_t num_slots;
    int32_t num_breaks;
    int32_t breaks[];
} c4m_break_info_t;

// Only used for the static macro.
struct c4m_internal_string_st {
    c4m_dt_info_t     *base_data_type;
    struct c4m_type_t *concrete_type;
    uint64_t           hash_cache_1;
    uint64_t           hash_cache_2;
    c4m_str_t          s;
};

// This struct is used to manage state when rending ansi.
typedef enum {
    C4M_U8_STATE_START_DEFAULT,
    C4M_U8_STATE_START_STYLE,
    C4M_U8_STATE_DEFAULT_STYLE, // Stop at a new start ix or at the end.
    C4M_U8_STATE_IN_STYLE       // Stop at a new end ix or at the end.
} c4m_u8_state_t;

// This only works with ASCII strings, not arbitrary utf8.
#define C4M_STATIC_ASCII_STR(id, val)                                       \
    const struct c4m_internal_string_st _static_##id = {                    \
        .base_data_type = (c4m_dt_info_t *)&c4m_base_type_info[C4M_T_UTF8], \
        .concrete_type  = (struct c4m_type_t *)&c4m_bi_types[C4M_T_UTF8],   \
        .s.byte_len     = sizeof(val),                                      \
        .s.codepoints   = sizeof(val),                                      \
        .s.styling      = NULL,                                             \
        .s.data         = val,                                              \
    };                                                                      \
    const c4m_str_t *id = (c4m_str_t *)&(_static_##id.s)

#pragma once

#include "con4m.h"

#define c4m_codepoint_category(c)     utf8proc_category(c)
#define c4m_codepoint_valid(c)        utf8proc_codepoint_valid(c)
#define c4m_codepoint_lower(c)        utf8proc_tolower(c)
#define c4m_codepoint_upper(c)        utf8proc_toupper(c)
#define c4m_codepoint_width(c)        utf8proc_charwidth(c)
#define c4m_codepoint_is_printable(c) (c4m_codepoint_width(c) != 0)

static inline bool
c4m_codepoint_is_space(c4m_codepoint_t cp)
{
    // Fast path.
    if (cp == ' ' || cp == '\n' || cp == '\r') {
        return true;
    }

    switch (c4m_codepoint_category(cp)) {
    case UTF8PROC_CATEGORY_ZS:
        return true;
    case UTF8PROC_CATEGORY_ZL:
    case UTF8PROC_CATEGORY_ZP:
        return true;
    default:
        return false;
    }
}

static inline bool
c4m_codepoint_is_c4m_id_start(c4m_codepoint_t cp)
{
    switch (utf8proc_category(cp)) {
    case UTF8PROC_CATEGORY_LU:
    case UTF8PROC_CATEGORY_LL:
    case UTF8PROC_CATEGORY_LT:
    case UTF8PROC_CATEGORY_LM:
    case UTF8PROC_CATEGORY_LO:
    case UTF8PROC_CATEGORY_NL:
        return true;
    default:
        switch (cp) {
        case '_':
        case '?':
        case '$':
            return true;
        default:
            return false;
        }
    }
}

static inline bool
c4m_codepoint_is_id_start(c4m_codepoint_t cp)
{
    switch (utf8proc_category(cp)) {
    case UTF8PROC_CATEGORY_LU:
    case UTF8PROC_CATEGORY_LL:
    case UTF8PROC_CATEGORY_LT:
    case UTF8PROC_CATEGORY_LM:
    case UTF8PROC_CATEGORY_LO:
    case UTF8PROC_CATEGORY_NL:
        return true;
    default:
        return false;
    }
}

static inline bool
c4m_codepoint_is_id_continue(c4m_codepoint_t cp)
{
    switch (utf8proc_category(cp)) {
    case UTF8PROC_CATEGORY_LU:
    case UTF8PROC_CATEGORY_LL:
    case UTF8PROC_CATEGORY_LT:
    case UTF8PROC_CATEGORY_LM:
    case UTF8PROC_CATEGORY_LO:
    case UTF8PROC_CATEGORY_NL:
    case UTF8PROC_CATEGORY_ND:
    case UTF8PROC_CATEGORY_MN:
    case UTF8PROC_CATEGORY_MC:
    case UTF8PROC_CATEGORY_PC:
        return true;
    default:
        return false;
    }
}

static inline bool
c4m_codepoint_is_c4m_id_continue(c4m_codepoint_t cp)
{
    switch (utf8proc_category(cp)) {
    case UTF8PROC_CATEGORY_LU:
    case UTF8PROC_CATEGORY_LL:
    case UTF8PROC_CATEGORY_LT:
    case UTF8PROC_CATEGORY_LM:
    case UTF8PROC_CATEGORY_LO:
    case UTF8PROC_CATEGORY_NL:
    case UTF8PROC_CATEGORY_ND:
    case UTF8PROC_CATEGORY_MN:
    case UTF8PROC_CATEGORY_MC:
    case UTF8PROC_CATEGORY_PC:
        return true;
    default:
        switch (cp) {
        case '_':
        case '?':
        case '$':
            return true;
        default:
            return false;
        }
    }
}

static inline bool
c4m_codepoint_is_ascii_digit(c4m_codepoint_t cp)
{
    return cp >= '0' && cp <= '9';
}

static inline bool
c4m_codepoint_is_ascii_upper(c4m_codepoint_t cp)
{
    return cp >= 'A' && cp <= 'Z';
}

static inline bool
c4m_codepoint_is_ascii_lower(c4m_codepoint_t cp)
{
    return cp >= 'a' && cp <= 'z';
}

static inline bool
c4m_codepoint_is_unicode_lower(c4m_codepoint_t cp)
{
    return utf8proc_category(cp) == UTF8PROC_CATEGORY_LL;
}

static inline bool
c4m_codepoint_is_unicode_upper(c4m_codepoint_t cp)
{
    return utf8proc_category(cp) == UTF8PROC_CATEGORY_LU;
}

static inline bool
c4m_codepoint_is_unicode_digit(c4m_codepoint_t cp)
{
    switch (utf8proc_category(cp)) {
    case UTF8PROC_CATEGORY_NL:
    case UTF8PROC_CATEGORY_ND:
        return true;
    default:
        return false;
    }
}

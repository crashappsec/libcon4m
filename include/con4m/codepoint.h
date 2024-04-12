#pragma once

#include "con4m.h"

#define c4m_codepoint_category(c) utf8proc_category(c)
#define c4m_codepoint_valid(c)    utf8proc_codepoint_valid(c)
#define c4m_codepoint_lower(c)    utf8proc_tolower(c)
#define c4m_codepoint_upper(c)    utf8proc_toupper(c)
#define c4m_codepoint_width(c)    utf8proc_charwidth(c)

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

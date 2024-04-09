#pragma once

#include "con4m.h"

#define c4m_codepoint_category(c) utf8proc_category(c)
#define c4m_codepoint_valid(c)    utf8proc_codepoint_valid(c)
#define c4m_codepoint_lower(c)    utf8proc_tolower(c)
#define c4m_codepoint_upper(c)    utf8proc_toupper(c)
#define c4m_codepoint_width(c)    utf8proc_charwidth(c)

static inline bool
c4m_codepoint_is_space(codepoint_t cp)
{
    // Fast path.
    if (cp == ' ' || cp == '\n' || cp == '\r') {
        return true;
    }

    switch (c4m_codepoint_category(cp)) {
    case CP_CATEGORY_ZS:
        return true;
    case CP_CATEGORY_ZL:
    case CP_CATEGORY_ZP:
        return true;
    default:
        return false;
    }
}

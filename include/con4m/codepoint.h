#pragma once

#include "con4m.h"

#define codepoint_category(c) utf8proc_category(c)
#define codepoint_valid(c)    utf8proc_codepoint_valid(c)
#define codepoint_lower(c)    utf8proc_tolower(c)
#define codepoint_upper(c)    utf8proc_toupper(c)
#define codepoint_width(c)    utf8proc_charwidth(c)

static inline bool
codepoint_is_space(codepoint_t cp)
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

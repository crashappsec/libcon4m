#pragma once

#include <con4m.h>

typedef int32_t codepoint_t;

#define codepoint_category(c) utf8proc_category(c)
#define codepoint_valid(c) utf8proc_codepoint_valid(c)
#define codepoint_lower(c) utf8proc_tolower(c)
#define codepoint_upper(c) utf8proc_toupper(c)
#define codepoint_width(c) utf8proc_charwidth(c)

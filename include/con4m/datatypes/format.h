#include "con4m.h"
#pragma once

typedef struct c4m_fmt_spec_t {
    c4m_codepoint_t fill;
    int64_t         width;
    int64_t         precision;
    unsigned int    kind  : 2; // C4M_FMT_FMT_ONLY / NUMBERED / NAMED
    unsigned int    align : 2;
    unsigned int    sign  : 2;
    unsigned int    sep   : 2;
    unsigned int    empty : 1;
    c4m_codepoint_t type;
} c4m_fmt_spec_t;

typedef struct c4m_fmt_info_t {
    union {
        char   *name;
        int64_t position;
    } reference;
    struct c4m_fmt_info_t *next;
    c4m_fmt_spec_t         spec;
    int                    start;
    int                    end;
} c4m_fmt_info_t;

#define C4M_FMT_FMT_ONLY 0
#define C4M_FMT_NUMBERED 1
#define C4M_FMT_NAMED    2

// <, >, =
#define C4M_FMT_ALIGN_LEFT   0
#define C4M_FMT_ALIGN_RIGHT  1
#define C4M_FMT_ALIGN_CENTER 2

// +, -, ' '
#define C4M_FMT_SIGN_DEFAULT   0
#define C4M_FMT_SIGN_ALWAYS    1
#define C4M_FMT_SIGN_POS_SPACE 2

// "_", ","

#define C4M_FMT_SEP_DEFAULT 0
#define C4M_FMT_SEP_COMMA   1
#define C4M_FMT_SEP_USCORE  2

/*
  [[fill]align][sign][width][grouping_option]["." precision][type]
fill            ::=  *
align           ::=  "<" | ">" | "^"
sign            ::=  "+" | "-" | " "
width           ::=  digit+
grouping_option ::=  "_" | ","
precision       ::=  digit+
type            ::=  [^0-9<>^_, ]
*/

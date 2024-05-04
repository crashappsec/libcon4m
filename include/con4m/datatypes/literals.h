#pragma once
#include "con4m.h"

typedef enum {
    ST_Base10 = 0,
    ST_Hex    = 1,
    ST_Float  = 2,
    ST_Bool   = 3,
    ST_2Quote = 4,
    ST_1Quote = 5,
    // 6 was 'other' which has gone away.
    ST_List   = 7,
    ST_Dict   = 8,
    ST_Tuple  = 9,
    ST_MAX    = 10
} c4m_lit_syntax_t;

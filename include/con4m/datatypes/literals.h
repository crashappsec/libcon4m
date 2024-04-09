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
} syntax_t;

typedef enum {
    LE_NoError        = 0,
    LE_InvalidChar    = 1,
    LE_Overflow       = 2,
    LE_Underflow      = 3,
    LE_OddHex         = 4,
    LE_InvalidNeg     = 5,
    LE_WrongNumDigits = 6,
    LE_NoLitmodMatch  = 7,
} lit_error_code_t;

typedef struct {
    uint64_t         loc;
    lit_error_code_t code;
} lit_error_t;

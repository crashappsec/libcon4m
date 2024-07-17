#pragma once
#include "con4m.h"

typedef enum {
    ST_Base10 = 0,
    ST_Hex    = 1,
    ST_Float  = 2,
    ST_Bool   = 3,
    ST_2Quote = 4,
    ST_1Quote = 5,
    ST_List   = 6,
    ST_Dict   = 7,
    ST_Tuple  = 8,
    ST_MAX    = 9
} c4m_lit_syntax_t;

typedef struct {
    struct c4m_str_t  *litmod;
    struct c4m_type_t *cast_to;
    struct c4m_type_t *type;
    c4m_builtin_t      base_type; // only set for containers from here down.
    c4m_lit_syntax_t   st;
    int                num_items;
} c4m_lit_info_t;

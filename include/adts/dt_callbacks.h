#pragma once
#include "con4m.h"

typedef struct {
    c4m_utf8_t      *target_symbol_name;
    c4m_type_t      *target_type;
    c4m_tree_node_t *decl_loc;
    c4m_funcinfo_t   binding;
} c4m_callback_t;

#define C4M_CB_FLAG_FFI    1
#define C4M_CB_FLAG_STATIC 2

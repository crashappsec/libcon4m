#pragma once
#include "con4m.h"

typedef struct c4m_tpat_node_t {
    c4m_obj_t                contents;
    struct c4m_tpat_node_t **children;
    uint16_t                 min;
    uint16_t                 max;
    uint16_t                 num_kids;
    unsigned int             walk        : 1;
    unsigned int             capture     : 1;
    unsigned int             ignore_kids : 1;
} c4m_tpat_node_t;

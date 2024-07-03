#pragma once
#include "con4m.h"

typedef struct c4m_tpat_node_t {
    struct c4m_tpat_node_t **children;
    c4m_obj_t                contents;
    int64_t                  min;
    int64_t                  max;
    uint64_t                 num_kids;
    unsigned int             walk        : 1;
    unsigned int             capture     : 1;
    unsigned int             ignore_kids : 1;
} c4m_tpat_node_t;

typedef c4m_utf8_t *(*c4m_pattern_fmt_fn)(void *);
extern c4m_tree_node_t *c4m_pat_repr(c4m_tpat_node_t *, c4m_pattern_fmt_fn);

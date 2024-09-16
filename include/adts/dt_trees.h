#pragma once

#include "con4m.h"

typedef struct c4m_tree_node_t {
    struct c4m_tree_node_t **children;
    struct c4m_tree_node_t  *parent;
    c4m_obj_t                contents;
    int32_t                  alloced_kids;
    int32_t                  num_kids;
} c4m_tree_node_t;

typedef bool (*c4m_walker_fn)(c4m_tree_node_t *, int64_t, void *);

#pragma once

#include "con4m.h"

typedef struct tree_node_t {
    int32_t              alloced_kids;
    int32_t              num_kids;
    object_t             contents;
    struct tree_node_t **children;
    struct tree_node_t  *parent;
} tree_node_t;

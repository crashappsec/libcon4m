#pragma once

#include "con4m.h"

extern xlist_t     *tree_children(tree_node_t *);
extern tree_node_t *tree_get_child(tree_node_t *, int64_t);
extern tree_node_t *tree_add_node(tree_node_t *, void *);

static inline object_t
tree_get_contents(tree_node_t *t)
{
    return t->contents;
}

static inline int64_t
tree_get_number_children(tree_node_t *t)
{
    return t->num_kids;
}

static inline tree_node_t *
tree_get_parent(tree_node_t *t)
{
    return t->parent;
}

// For use from Nim where we only instantiate w/ strings.
static inline tree_node_t *
con4m_tree(any_str_t *s)
{
    return con4m_new(tspec_tree(tspec_utf32()), kw("contents", ka(s)));
}

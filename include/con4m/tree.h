#pragma once

#include "con4m.h"

extern xlist_t     *c4m_tree_children(tree_node_t *);
extern tree_node_t *c4m_tree_get_child(tree_node_t *, int64_t);
extern tree_node_t *c4m_tree_add_node(tree_node_t *, void *);

static inline object_t
c4m_tree_get_contents(tree_node_t *t)
{
    return t->contents;
}

static inline int64_t
c4m_tree_get_number_children(tree_node_t *t)
{
    return t->num_kids;
}

static inline tree_node_t *
c4m_tree_get_parent(tree_node_t *t)
{
    return t->parent;
}

// For use from Nim where we only instantiate w/ strings.
static inline tree_node_t *
c4m_tree(any_str_t *s)
{
    return c4m_new(c4m_tspec_tree(c4m_tspec_utf32()),
                   c4m_kw("contents", c4m_ka(s)));
}

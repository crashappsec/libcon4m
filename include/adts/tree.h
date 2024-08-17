#pragma once

#include "con4m.h"

extern c4m_list_t      *c4m_tree_children(c4m_tree_node_t *);
extern c4m_tree_node_t *c4m_tree_get_child(c4m_tree_node_t *, int64_t);
extern c4m_tree_node_t *c4m_tree_add_node(c4m_tree_node_t *, void *);
extern c4m_tree_node_t *c4m_tree_prepend_node(c4m_tree_node_t *, void *);
extern void             c4m_tree_adopt_node(c4m_tree_node_t *,
                                            c4m_tree_node_t *);
extern void             c4m_tree_adopt_and_prepend(c4m_tree_node_t *,
                                                   c4m_tree_node_t *);
extern c4m_tree_node_t *
c4m_tree_str_transform(c4m_tree_node_t *, c4m_str_t *(*fn)(void *));

void c4m_tree_walk(c4m_tree_node_t *, c4m_walker_fn);

static inline c4m_obj_t
c4m_tree_get_contents(c4m_tree_node_t *t)
{
    return t->contents;
}

static inline int64_t
c4m_tree_get_number_children(c4m_tree_node_t *t)
{
    return t->num_kids;
}

static inline c4m_tree_node_t *
c4m_tree_get_parent(c4m_tree_node_t *t)
{
    return t->parent;
}

// For use from Nim where we only instantiate w/ strings.
static inline c4m_tree_node_t *
c4m_tree(c4m_str_t *s)
{
    return c4m_new(c4m_type_tree(c4m_type_utf32()),
                   c4m_kw("contents", c4m_ka(s)));
}

static inline c4m_tree_node_t *
c4m_new_tree_node(c4m_type_t *t, void *node)
{
    return c4m_new(c4m_type_tree(t), c4m_kw("contents", c4m_ka(node)));
}

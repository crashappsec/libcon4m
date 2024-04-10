#include "con4m.h"

static void
tree_node_init(c4m_tree_node_t *t, va_list args)
{
    c4m_obj_t contents = NULL;

    c4m_karg_va_init(args);
    c4m_kw_ptr("contents", contents);

    t->children     = c4m_gc_array_alloc(c4m_tree_node_t **, 4);
    t->alloced_kids = 4;
    t->num_kids     = 0;
    t->contents     = contents;
}

c4m_tree_node_t *
c4m_tree_add_node(c4m_tree_node_t *t, void *item)
{
    c4m_type_t      *tree_type   = c4m_get_my_type(t);
    c4m_xlist_t     *type_params = c4m_tspec_get_parameters(tree_type);
    c4m_type_t      *item_type   = c4m_xlist_get(type_params, 0, NULL);
    c4m_tree_node_t *kid         = c4m_new(c4m_tspec_tree(item_type),
                                   c4m_kw("contents", c4m_ka(item)));

    kid->parent = t;

    if (t->num_kids == t->alloced_kids) {
        t->alloced_kids *= 2;
        c4m_tree_node_t **new_kids = c4m_gc_array_alloc(c4m_tree_node_t **,
                                                        t->alloced_kids);
        for (int i = 0; i < t->num_kids; i++) {
            new_kids[i] = t->children[i];
        }
        t->children = new_kids;
    }
    t->children[t->num_kids++] = kid;

    return kid;
}

c4m_tree_node_t *
c4m_tree_get_child(c4m_tree_node_t *t, int64_t i)
{
    if (i < 0 || i >= t->num_kids) {
        C4M_CRAISE("Index out of bounds.");
    }

    return t->children[i];
}

c4m_xlist_t *
c4m_tree_children(c4m_tree_node_t *t)
{
    c4m_type_t  *tree_type   = c4m_get_my_type(t);
    c4m_xlist_t *type_params = c4m_tspec_get_parameters(tree_type);
    c4m_type_t  *item_type   = c4m_xlist_get(type_params, 0, NULL);
    c4m_xlist_t *result;

    result = c4m_new(c4m_tspec_list(item_type),
                     c4m_kw("length", c4m_ka(t->num_kids)));

    for (int i = 0; i < t->num_kids; i++) {
        c4m_xlist_append(result, t->children[i]);
    }

    return result;
}

static void
tree_node_marshal(c4m_tree_node_t *t, c4m_stream_t *s, dict_t *memos, int64_t *mid)
{
    c4m_type_t    *list_type   = c4m_get_my_type(t);
    c4m_xlist_t   *type_params = c4m_tspec_get_parameters(list_type);
    c4m_type_t    *item_type   = c4m_xlist_get(type_params, 0, NULL);
    c4m_dt_info_t *item_info   = c4m_tspec_get_data_type_info(item_type);
    bool           by_val      = item_info->by_value;

    c4m_marshal_i32(t->alloced_kids, s);
    c4m_marshal_i32(t->num_kids, s);
    c4m_sub_marshal(t->parent, s, memos, mid);

    if (by_val) {
        c4m_marshal_u64((uint64_t)t->contents, s);
    }
    else {
        c4m_sub_marshal(t->contents, s, memos, mid);
    }

    for (int i = 0; i < t->num_kids; i++) {
        c4m_sub_marshal(t->children[i], s, memos, mid);
    }
}

static void
tree_node_unmarshal(c4m_tree_node_t *t, c4m_stream_t *s, dict_t *memos)
{
    c4m_type_t    *list_type   = c4m_get_my_type(t);
    c4m_xlist_t   *type_params = c4m_tspec_get_parameters(list_type);
    c4m_type_t    *item_type   = c4m_xlist_get(type_params, 0, NULL);
    c4m_dt_info_t *item_info   = c4m_tspec_get_data_type_info(item_type);
    bool           by_val      = item_info->by_value;

    t->alloced_kids = c4m_unmarshal_i32(s);
    t->num_kids     = c4m_unmarshal_i32(s);
    t->parent       = c4m_sub_unmarshal(s, memos);
    t->children     = c4m_gc_array_alloc(c4m_tree_node_t **, t->alloced_kids);

    if (by_val) {
        t->contents = (c4m_obj_t)c4m_unmarshal_u64(s);
    }
    else {
        t->contents = c4m_sub_unmarshal(s, memos);
    }

    for (int i = 0; i < t->num_kids; i++) {
        t->children[i] = c4m_sub_unmarshal(s, memos);
    }
}

const c4m_vtable_t c4m_tree_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        (c4m_vtable_entry)tree_node_init,
        NULL,
        NULL,
        (c4m_vtable_entry)tree_node_marshal,
        (c4m_vtable_entry)tree_node_unmarshal,
    },
};

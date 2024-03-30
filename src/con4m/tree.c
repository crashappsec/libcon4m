#include <con4m.h>

static void
tree_node_init(tree_node_t *t, va_list args)
{
    object_t contents;

    karg_va_init(args);
    kw_ptr("root", contents);

    t->children     = gc_array_alloc(tree_node_t **, 4);
    t->alloced_kids = 4;
    t->num_kids     = 0;
    t->contents     = contents;
}

tree_node_t *
tree_add_node(tree_node_t *t, void *item)
{
    type_spec_t *tree_type   = get_my_type(t);
    xlist_t     *type_params = tspec_get_parameters(tree_type);
    type_spec_t *item_type   = xlist_get(type_params, 0, NULL);
    tree_node_t *kid         = con4m_new(tspec_tree(item_type),
					 "contents", item);

    kid->parent = t;

    if (t->num_kids == t->alloced_kids) {
	t->alloced_kids *= 2;
	tree_node_t **new_kids = gc_array_alloc(tree_node_t **,
						t->alloced_kids);
	for (int i = 0; i < t->num_kids; i++) {
	    new_kids[i] = t->children[i];
	}
	t->children = new_kids;
    }
    t->children[t->num_kids++] = kid;

    return kid;
}

tree_node_t *
tree_get_child(tree_node_t *t, int64_t i)
{
    if (i < 0 || i >= t->num_kids) {
	CRAISE("Index out of bounds.");
    }

    return t->children[i];
}

xlist_t *
tree_children(tree_node_t *t)
{
    type_spec_t *tree_type   = get_my_type(t);
    xlist_t     *type_params = tspec_get_parameters(tree_type);
    type_spec_t *item_type   = xlist_get(type_params, 0, NULL);
    xlist_t     *result;

    result = con4m_new(tspec_list(item_type), kw("length", ka(t->num_kids)));

    for (int i = 0; i < t->num_kids; i++) {
	xlist_append(result, t->children[i]);
    }

    return result;
}

static void
tree_node_marshal(tree_node_t *t, stream_t *s, dict_t *memos, int64_t *mid)
{
    type_spec_t *list_type   = get_my_type(t);
    xlist_t     *type_params = tspec_get_parameters(list_type);
    type_spec_t *item_type   = xlist_get(type_params, 0, NULL);
    dt_info     *item_info   = tspec_get_data_type_info(item_type);
    bool         by_val      = item_info->by_value;

    marshal_i32(t->alloced_kids, s);
    marshal_i32(t->num_kids, s);
    con4m_sub_marshal(t->parent, s, memos, mid);

    if (by_val) {
	marshal_u64((uint64_t)t->contents, s);
    }
    else {
	con4m_sub_marshal(t->contents, s, memos, mid);
    }

    for (int i = 0; i < t->num_kids; i++) {
	con4m_sub_marshal(t->children[i], s, memos, mid);
    }
}

static void
tree_node_unmarshal(tree_node_t *t, stream_t *s, dict_t *memos)
{
    type_spec_t *list_type   = get_my_type(t);
    xlist_t     *type_params = tspec_get_parameters(list_type);
    type_spec_t *item_type   = xlist_get(type_params, 0, NULL);
    dt_info     *item_info   = tspec_get_data_type_info(item_type);
    bool         by_val      = item_info->by_value;

    t->alloced_kids = unmarshal_i32(s);
    t->num_kids     = unmarshal_i32(s);
    t->parent       = con4m_sub_unmarshal(s, memos);
    t->children     = gc_array_alloc(tree_node_t **, t->alloced_kids);

    if (by_val) {
	t->contents = (object_t)unmarshal_u64(s);
    }
    else {
	t->contents = con4m_sub_unmarshal(s, memos);
    }

    for (int i = 0; i < t->num_kids; i++) {
	t->children[i] = con4m_sub_unmarshal(s, memos);
    }
}

const con4m_vtable tree_vtable = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	(con4m_vtable_entry)tree_node_init,
	NULL,
	NULL,
	(con4m_vtable_entry)tree_node_marshal,
	(con4m_vtable_entry)tree_node_unmarshal,
    }
};

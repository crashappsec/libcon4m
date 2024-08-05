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

void
c4m_tree_adopt_node(c4m_tree_node_t *t, c4m_tree_node_t *kid)
{
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
}

c4m_tree_node_t *
c4m_tree_add_node(c4m_tree_node_t *t, void *item)
{
    c4m_type_t      *tree_type   = c4m_get_my_type(t);
    c4m_list_t      *type_params = c4m_type_get_params(tree_type);
    c4m_type_t      *item_type   = c4m_list_get(type_params, 0, NULL);
    c4m_tree_node_t *kid         = c4m_new(c4m_type_tree(item_type),
                                   c4m_kw("contents", c4m_ka(item)));

    c4m_tree_adopt_node(t, kid);

    return kid;
}

void
c4m_tree_adopt_and_prepend(c4m_tree_node_t *t, c4m_tree_node_t *kid)
{
    kid->parent = t;

    if (t->num_kids == t->alloced_kids) {
        t->alloced_kids *= 2;
        c4m_tree_node_t **new_kids = c4m_gc_array_alloc(c4m_tree_node_t **,
                                                        t->alloced_kids);
        for (int i = 0; i < t->num_kids; i++) {
            new_kids[i + 1] = t->children[i];
        }
        t->children = new_kids;
    }
    else {
        int i = t->num_kids;

        while (i != 0) {
            t->children[i] = t->children[i - 1];
            i--;
        }
    }

    t->children[0] = kid;
    t->num_kids++;
}

c4m_tree_node_t *
c4m_tree_prepend_node(c4m_tree_node_t *t, void *item)
{
    c4m_type_t      *tree_type   = c4m_get_my_type(t);
    c4m_list_t      *type_params = c4m_type_get_params(tree_type);
    c4m_type_t      *item_type   = c4m_list_get(type_params, 0, NULL);
    c4m_tree_node_t *kid         = c4m_new(c4m_type_tree(item_type),
                                   c4m_kw("contents", c4m_ka(item)));

    c4m_tree_adopt_and_prepend(t, kid);

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

c4m_list_t *
c4m_tree_children(c4m_tree_node_t *t)
{
    c4m_type_t *tree_type   = c4m_get_my_type(t);
    c4m_list_t *type_params = c4m_type_get_params(tree_type);
    c4m_type_t *item_type   = c4m_list_get(type_params, 0, NULL);
    c4m_list_t *result;

    result = c4m_new(c4m_type_list(item_type),
                     c4m_kw("length", c4m_ka(t->num_kids)));

    for (int i = 0; i < t->num_kids; i++) {
        c4m_list_append(result, t->children[i]);
    }

    return result;
}

bool print_xform_info = false;

c4m_tree_node_t *
c4m_tree_str_transform(c4m_tree_node_t *t, c4m_str_t *(*fn)(void *))
{
    if (t == NULL) {
        return NULL;
    }

    c4m_str_t       *str    = c4m_to_utf8(fn(c4m_tree_get_contents(t)));
    c4m_tree_node_t *result = c4m_new(c4m_type_tree(c4m_type_utf8()),
                                      c4m_kw("contents", c4m_ka(str)));

    for (int64_t i = 0; i < t->num_kids; i++) {
        c4m_tree_adopt_node(result, c4m_tree_str_transform(t->children[i], fn));
    }

    return result;
}

void
c4m_tree_walk(c4m_tree_node_t *t, c4m_walker_fn callback)
{
    int64_t num_kids = c4m_tree_get_number_children(t);

    (*callback)(t);

    for (int64_t i = 0; i < num_kids; i++) {
        c4m_tree_walk(c4m_tree_get_child(t, i), callback);
    }
}

void
c4m_tree_node_set_gc_bits(uint64_t *bitfield, void *alloc)
{
    c4m_tree_node_t *n = (c4m_tree_node_t *)alloc;
    c4m_mark_raw_to_addr(bitfield, alloc, &n->contents);
}

const c4m_vtable_t c4m_tree_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)tree_node_init,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)c4m_tree_node_set_gc_bits,
        // Explicit because some compilers don't seem to always properly
        // zero it (Was sometimes crashing on a `c4m_stream_t` on my mac).
        [C4M_BI_FINALIZER]   = NULL,
    },
};

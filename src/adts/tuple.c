#include "con4m.h"

static void
tuple_init(c4m_tuple_t *tup, va_list args)
{
    c4m_list_t *contents = NULL;

    c4m_karg_va_init(args);
    c4m_kw_ptr("contents", contents);

    uint64_t n = c4m_list_len(c4m_type_get_params(c4m_get_my_type(tup)));

    // Caution: the GC might scan some non-pointers here.  Could add a
    // hook for dealing w/ it, but would be more effort than it is
    // worth.
    tup->items     = c4m_gc_array_alloc(uint64_t, n);
    tup->num_items = n;

    if (contents != NULL) {
        for (unsigned int i = 0; i < n; i++) {
            tup->items[i] = c4m_list_get(contents, i, NULL);
        }
    }
}

// This should always be statically checked.
void
c4m_tuple_set(c4m_tuple_t *tup, int64_t ix, void *item)
{
    tup->items[ix] = item;
}

void *
c4m_tuple_get(c4m_tuple_t *tup, int64_t ix)
{
    return tup->items[ix];
}
int64_t
c4m_tuple_len(c4m_tuple_t *tup)
{
    return tup->num_items;
}

static c4m_str_t *
tuple_repr(c4m_tuple_t *tup)
{
    c4m_list_t *tparams = c4m_type_get_params(c4m_get_my_type(tup));
    int         len     = tup->num_items;
    c4m_list_t *items   = c4m_new(c4m_type_list(c4m_type_utf32()));

    for (int i = 0; i < len; i++) {
        c4m_type_t *one_type = c4m_list_get(tparams, i, NULL);
        c4m_list_append(items, c4m_repr(tup->items[i], one_type));
    }

    c4m_str_t *sep    = c4m_get_comma_const();
    c4m_str_t *result = c4m_str_join(items, sep);

    result = c4m_str_concat(c4m_get_lparen_const(),
                            c4m_str_concat(result, c4m_get_rparen_const()));

    return result;
}

static bool
tuple_can_coerce(c4m_type_t *src, c4m_type_t *dst)
{
    return c4m_types_are_compat(src, dst, NULL);
}

static c4m_tuple_t *
tuple_coerce(c4m_tuple_t *tup, c4m_type_t *dst)
{
    c4m_list_t  *srcparams = c4m_type_get_params(c4m_get_my_type(tup));
    c4m_list_t  *dstparams = c4m_type_get_params(dst);
    int          len       = tup->num_items;
    c4m_tuple_t *res       = c4m_new(dst);

    for (int i = 0; i < len; i++) {
        c4m_type_t *src_type = c4m_list_get(srcparams, i, NULL);
        c4m_type_t *dst_type = c4m_list_get(dstparams, i, NULL);

        res->items[i] = c4m_coerce(tup->items[i], src_type, dst_type);
    }

    return res;
}

static c4m_tuple_t *
tuple_copy(c4m_tuple_t *tup)
{
    return tuple_coerce(tup, c4m_get_my_type(tup));
}

static c4m_obj_t
tuple_from_lit(c4m_type_t *objtype, c4m_list_t *items, c4m_utf8_t *litmod)
{
    int l = c4m_list_len(items);

    if (l == 1) {
        return c4m_list_get(items, 0, NULL);
    }
    return c4m_new(objtype, c4m_kw("contents", c4m_ka(items)));
}

// TODO:
// We need to scan the entire tuple, because we are currently
// improperly boxing some ints when we shouldn't.

/*
void
tuple_set_gc_bits(uint64_t       *bitfield,
                  c4m_base_obj_t *alloc)
{
    c4m_tuple_t *tup  = (c4m_tuple_t *)alloc->data;
    c4m_type_t  *t    = alloc->concrete_type;
    int          len  = c4m_type_get_num_params(t);
    int          base = c4m_ptr_diff(alloc, &tup->items[0]);

    for (int i = 0; i < len; i++) {
        if (c4m_type_requires_gc_scan(c4m_type_get_param(t, i))) {
            c4m_set_bit(bitfield, base + i);
        }
    }
}
*/

void *
c4m_clean_internal_list(c4m_list_t *l)
{
    // The goal here is to take an internal list and convert it into one
    // of the following:
    //
    // 1. A single object if there was one item.
    // 2. A list of the item type if all the items are the same.
    // 3. An appropriate tuple type otherwise.

    int len = c4m_list_len(l);

    switch (len) {
    case 0:
        return NULL;
    case 1:
        return c4m_autobox(c4m_list_get(l, 0, NULL));
    default:
        break;
    }

    c4m_list_t *items = c4m_list(c4m_type_internal());

    for (int i = 0; i < len; i++) {
        void *item = c4m_autobox(c4m_list_get(l, i, NULL));
        if (c4m_type_is_list(c4m_get_my_type(item))) {
            item = c4m_clean_internal_list(item);
        }
        c4m_list_append(items, item);
    }

    c4m_list_t *tup_types      = c4m_list(c4m_type_typespec());
    c4m_type_t *li_type        = c4m_new_typevar();
    bool        requires_tuple = false;

    for (int i = 0; i < len; i++) {
        c4m_type_t *t = c4m_get_my_type(c4m_list_get(items, i, NULL));
        if (!requires_tuple) {
            if (c4m_type_is_error(c4m_unify(li_type, t))) {
                requires_tuple = true;
            }
        }
        c4m_list_append(tup_types, t);
    }

    if (!requires_tuple) {
        c4m_type_t *res_type = c4m_type_resolve(c4m_get_my_type(items));
        res_type->items      = c4m_type_resolve(li_type)->items;

        return items;
    }

    return c4m_new(c4m_type_tuple_from_xlist(tup_types),
                   c4m_kw("contents", c4m_ka(items)));
}

const c4m_vtable_t c4m_tuple_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR]   = (c4m_vtable_entry)tuple_init,
        [C4M_BI_TO_STR]        = (c4m_vtable_entry)tuple_repr,
        [C4M_BI_COERCIBLE]     = (c4m_vtable_entry)tuple_can_coerce,
        [C4M_BI_COERCE]        = (c4m_vtable_entry)tuple_coerce,
        [C4M_BI_COPY]          = (c4m_vtable_entry)tuple_copy,
        [C4M_BI_LEN]           = (c4m_vtable_entry)c4m_tuple_len,
        [C4M_BI_INDEX_GET]     = (c4m_vtable_entry)c4m_tuple_get,
        [C4M_BI_INDEX_SET]     = (c4m_vtable_entry)c4m_tuple_set,
        [C4M_BI_CONTAINER_LIT] = (c4m_vtable_entry)tuple_from_lit,
        [C4M_BI_GC_MAP]        = C4M_GC_SCAN_ALL,
        [C4M_BI_FINALIZER]     = NULL,
    },
};

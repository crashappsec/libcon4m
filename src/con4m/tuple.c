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

static void
tuple_marshal(c4m_tuple_t  *tup,
              c4m_stream_t *s,
              c4m_dict_t   *memos,
              int64_t      *mid)
{
    c4m_list_t *tparams = c4m_type_get_params(c4m_get_my_type(tup));

    for (int i = 0; i < tup->num_items; i++) {
        c4m_type_t    *param   = c4m_list_get(tparams, i, NULL);
        c4m_dt_info_t *dt_info = c4m_type_get_data_type_info(param);

        if (dt_info->by_value) {
            c4m_marshal_u64((uint64_t)tup->items[i], s);
        }
        else {
            c4m_sub_marshal(tup->items[i], s, memos, mid);
        }
    }
}

static void
tuple_unmarshal(c4m_tuple_t *tup, c4m_stream_t *s, c4m_dict_t *memos)
{
    c4m_list_t *tparams = c4m_type_get_params(c4m_get_my_type(tup));

    tup->num_items = c4m_list_len(tparams);
    tup->items     = c4m_gc_array_alloc(uint64_t, tup->num_items);

    for (int i = 0; i < tup->num_items; i++) {
        c4m_type_t    *param   = c4m_list_get(tparams, i, NULL);
        c4m_dt_info_t *dt_info = c4m_type_get_data_type_info(param);

        if (dt_info->by_value) {
            tup->items[i] = (void *)c4m_unmarshal_u64(s);
        }
        else {
            tup->items[i] = c4m_sub_unmarshal(s, memos);
        }
    }
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
static void
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

const c4m_vtable_t c4m_tuple_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR]   = (c4m_vtable_entry)tuple_init,
        [C4M_BI_TO_STR]        = (c4m_vtable_entry)tuple_repr,
        [C4M_BI_UNMARSHAL]     = (c4m_vtable_entry)tuple_unmarshal,
        [C4M_BI_COERCIBLE]     = (c4m_vtable_entry)tuple_can_coerce,
        [C4M_BI_COERCE]        = (c4m_vtable_entry)tuple_coerce,
        [C4M_BI_COPY]          = (c4m_vtable_entry)tuple_copy,
        [C4M_BI_LEN]           = (c4m_vtable_entry)c4m_tuple_len,
        [C4M_BI_INDEX_GET]     = (c4m_vtable_entry)c4m_tuple_get,
        [C4M_BI_INDEX_SET]     = (c4m_vtable_entry)c4m_tuple_set,
        [C4M_BI_CONTAINER_LIT] = (c4m_vtable_entry)tuple_from_lit,
        [C4M_BI_GC_MAP]        = C4M_GC_SCAN_ALL,
        [C4M_BI_MARSHAL]       = (c4m_vtable_entry)tuple_marshal,
        [C4M_BI_FINALIZER]     = NULL,
    },
};

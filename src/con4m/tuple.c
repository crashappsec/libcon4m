#include "con4m.h"

static void
tuple_init(c4m_tuple_t *tup, va_list args)
{
    void **ptr = NULL;

    c4m_karg_va_init(args);
    c4m_kw_ptr("contents", ptr);

    tup->num_items = c4m_xlist_len(c4m_tspec_get_parameters(c4m_get_my_type(tup)));

    if (ptr != NULL) {
        for (int i = 0; i < tup->num_items; i++) {
            tup->items[i] = ptr[i];
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
tuple_marshal(c4m_tuple_t *tup, c4m_stream_t *s, c4m_dict_t *memos, int64_t *mid)
{
    c4m_xlist_t *tparams = c4m_tspec_get_parameters(c4m_get_my_type(tup));

    for (int i = 0; i < tup->num_items; i++) {
        c4m_type_t    *param   = c4m_xlist_get(tparams, i, NULL);
        c4m_dt_info_t *dt_info = c4m_tspec_get_data_type_info(param);

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
    c4m_xlist_t *tparams = c4m_tspec_get_parameters(c4m_get_my_type(tup));

    tup->num_items = c4m_xlist_len(tparams);

    for (int i = 0; i < tup->num_items; i++) {
        c4m_type_t    *param   = c4m_xlist_get(tparams, i, NULL);
        c4m_dt_info_t *dt_info = c4m_tspec_get_data_type_info(param);

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
tuple_repr(c4m_tuple_t *tup, to_str_use_t how)
{
    c4m_xlist_t *tparams = c4m_tspec_get_parameters(c4m_get_my_type(tup));
    int          len     = tup->num_items;
    c4m_xlist_t *items   = c4m_new(c4m_tspec_xlist(c4m_tspec_utf32()));

    for (int i = 0; i < len; i++) {
        c4m_type_t *one_type = c4m_xlist_get(tparams, i, NULL);

        c4m_xlist_append(items, c4m_repr(tup->items[i], one_type, how));
    }

    c4m_str_t *sep    = c4m_get_comma_const();
    c4m_str_t *result = c4m_str_join(items, sep);

    if (how == C4M_REPR_QUOTED) {
        result = c4m_str_concat(c4m_get_lparen_const(),
                                c4m_str_concat(result, c4m_get_rparen_const()));
    }

    return result;
}

static bool
tuple_can_coerce(c4m_type_t *src, c4m_type_t *dst)
{
    return c4m_tspecs_are_compat(src, dst);
}

static c4m_tuple_t *
tuple_coerce(c4m_tuple_t *tup, c4m_type_t *dst)
{
    c4m_xlist_t *srcparams = c4m_tspec_get_parameters(c4m_get_my_type(tup));
    c4m_xlist_t *dstparams = c4m_tspec_get_parameters(dst);
    int          len       = tup->num_items;
    c4m_tuple_t *res       = c4m_new(dst);

    for (int i = 0; i < len; i++) {
        c4m_type_t *src_type = c4m_xlist_get(srcparams, i, NULL);
        c4m_type_t *dst_type = c4m_xlist_get(dstparams, i, NULL);

        res->items[i] = c4m_coerce(tup->items[i], src_type, dst_type);
    }

    return res;
}

static c4m_tuple_t *
tuple_copy(c4m_tuple_t *tup)
{
    return tuple_coerce(tup, c4m_get_my_type(tup));
}

const c4m_vtable_t c4m_tuple_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        (c4m_vtable_entry)tuple_init,
        (c4m_vtable_entry)tuple_repr,
        NULL,
        (c4m_vtable_entry)tuple_marshal,
        (c4m_vtable_entry)tuple_unmarshal,
        (c4m_vtable_entry)tuple_can_coerce,
        (c4m_vtable_entry)tuple_coerce,
        NULL,
        (c4m_vtable_entry)tuple_copy,
        NULL, // Plus
        NULL, // Subtract
        NULL, // Mul
        NULL, // Div
        NULL, // MOD
        NULL, // EQ
        NULL, // LT
        NULL, // GT
        (c4m_vtable_entry)c4m_tuple_len,
        (c4m_vtable_entry)c4m_tuple_get,
        (c4m_vtable_entry)c4m_tuple_set,
        NULL, // Slice get
        NULL, // Slice set
    },
};

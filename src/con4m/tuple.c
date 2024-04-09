#include "con4m.h"

static void
tuple_init(tuple_t *tup, va_list args)
{
    void **ptr = NULL;

    karg_va_init(args);
    kw_ptr("contents", ptr);

    tup->num_items = xlist_len(tspec_get_parameters(get_my_type(tup)));

    if (ptr != NULL) {
        for (int i = 0; i < tup->num_items; i++) {
            tup->items[i] = ptr[i];
        }
    }
}

// This should always be statically checked.
void
tuple_set(tuple_t *tup, int64_t ix, void *item)
{
    tup->items[ix] = item;
}

void *
tuple_get(tuple_t *tup, int64_t ix)
{
    return tup->items[ix];
}

static void
tuple_marshal(tuple_t *tup, stream_t *s, dict_t *memos, int64_t *mid)
{
    xlist_t *tparams = tspec_get_parameters(get_my_type(tup));

    for (int i = 0; i < tup->num_items; i++) {
        type_spec_t *param   = xlist_get(tparams, i, NULL);
        dt_info     *dt_info = tspec_get_data_type_info(param);

        if (dt_info->by_value) {
            marshal_u64((uint64_t)tup->items[i], s);
        }
        else {
            c4m_sub_marshal(tup->items[i], s, memos, mid);
        }
    }
}

static void
tuple_unmarshal(tuple_t *tup, stream_t *s, dict_t *memos)
{
    xlist_t *tparams = tspec_get_parameters(get_my_type(tup));

    tup->num_items = xlist_len(tparams);

    for (int i = 0; i < tup->num_items; i++) {
        type_spec_t *param   = xlist_get(tparams, i, NULL);
        dt_info     *dt_info = tspec_get_data_type_info(param);

        if (dt_info->by_value) {
            tup->items[i] = (void *)unmarshal_u64(s);
        }
        else {
            tup->items[i] = c4m_sub_unmarshal(s, memos);
        }
    }
}

int64_t
tuple_len(tuple_t *tup)
{
    return tup->num_items;
}

static any_str_t *
tuple_repr(tuple_t *tup, to_str_use_t how)
{
    xlist_t *tparams = tspec_get_parameters(get_my_type(tup));
    int      len     = tup->num_items;
    xlist_t *items   = c4m_new(tspec_xlist(tspec_utf32()));

    for (int i = 0; i < len; i++) {
        type_spec_t *one_type = xlist_get(tparams, i, NULL);

        xlist_append(items, c4m_repr(tup->items[i], one_type, how));
    }

    any_str_t *sep    = c4m_get_comma_const();
    any_str_t *result = string_join(items, sep);

    if (how == TO_STR_USE_QUOTED) {
        result = string_concat(c4m_get_lparen_const(),
                               string_concat(result, c4m_get_rparen_const()));
    }

    return result;
}

static bool
tuple_can_coerce(type_spec_t *src, type_spec_t *dst)
{
    return tspecs_are_compat(src, dst);
}

static tuple_t *
tuple_coerce(tuple_t *tup, type_spec_t *dst)
{
    xlist_t *srcparams = tspec_get_parameters(get_my_type(tup));
    xlist_t *dstparams = tspec_get_parameters(dst);
    int      len       = tup->num_items;
    tuple_t *res       = c4m_new(dst);

    for (int i = 0; i < len; i++) {
        type_spec_t *src_type = xlist_get(srcparams, i, NULL);
        type_spec_t *dst_type = xlist_get(dstparams, i, NULL);

        res->items[i] = c4m_coerce(tup->items[i], src_type, dst_type);
    }

    return res;
}

static tuple_t *
tuple_copy(tuple_t *tup)
{
    return tuple_coerce(tup, get_my_type(tup));
}

const c4m_vtable tuple_vtable = {
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
        (c4m_vtable_entry)tuple_len,
        (c4m_vtable_entry)tuple_get,
        (c4m_vtable_entry)tuple_set,
        NULL, // Slice get
        NULL, // Slice set
    },
};

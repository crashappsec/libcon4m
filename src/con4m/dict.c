#include "con4m.h"

static void
c4m_dict_init(c4m_dict_t *dict, va_list args)
{
    size_t         hash_fn;
    c4m_xlist_t   *type_params;
    c4m_type_t    *key_type;
    c4m_dt_info_t *info;
    c4m_type_t    *c4m_dict_type = c4m_get_my_type(dict);

    if (c4m_dict_type != NULL) {
        type_params = c4m_type_get_params(c4m_dict_type);
        key_type    = c4m_xlist_get(type_params, 0, NULL);
        info        = c4m_type_get_data_type_info(key_type);

        hash_fn = info->hash_fn;
    }

    else {
        hash_fn = va_arg(args, size_t);
    }

    hatrack_dict_init(dict, hash_fn);

    switch (hash_fn) {
    case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
        hatrack_dict_set_hash_offset(dict, 2 * (int32_t)sizeof(uint64_t));
        /* fallthrough */
    case HATRACK_DICT_KEY_TYPE_OBJ_PTR:
    case HATRACK_DICT_KEY_TYPE_OBJ_INT:
    case HATRACK_DICT_KEY_TYPE_OBJ_REAL:
        hatrack_dict_set_cache_offset(dict, -2 * (int32_t)sizeof(uint64_t));
        break;
    default:
        // nada.
    }

    dict->slow_views = false;
}

static void
c4m_dict_marshal(c4m_dict_t   *d,
                 c4m_stream_t *s,
                 c4m_dict_t   *memos,
                 int64_t      *mid)
{
    uint64_t    length;
    c4m_type_t *c4m_dict_type = c4m_get_my_type(d);

    if (c4m_dict_type == NULL) {
        C4M_CRAISE("Cannot marshal untyped dictionaries.");
    }

    c4m_xlist_t         *type_params = c4m_type_get_params(c4m_dict_type);
    c4m_type_t          *key_type    = c4m_xlist_get(type_params, 0, NULL);
    c4m_type_t          *val_type    = c4m_xlist_get(type_params, 1, NULL);
    hatrack_dict_item_t *view        = hatrack_dict_items_sort(d, &length);
    c4m_dt_info_t       *kinfo       = c4m_type_get_data_type_info(key_type);
    c4m_dt_info_t       *vinfo       = c4m_type_get_data_type_info(val_type);
    bool                 key_by_val  = kinfo->by_value;
    bool                 val_by_val  = vinfo->by_value;

    c4m_marshal_u32((uint32_t)length, s);

    // keyhash field is the easiest way to tell whether we're passing by
    // value of

    for (uint64_t i = 0; i < length; i++) {
        if (key_by_val) {
            c4m_marshal_u64((uint64_t)view[i].key, s);
        }
        else {
            c4m_sub_marshal(view[i].key, s, memos, mid);
        }

        if (val_by_val) {
            c4m_marshal_u64((uint64_t)view[i].value, s);
        }
        else {
            c4m_sub_marshal(view[i].value, s, memos, mid);
        }
    }
}

static void
c4m_dict_unmarshal(c4m_dict_t *d, c4m_stream_t *s, c4m_dict_t *memos)
{
    uint32_t       length        = c4m_unmarshal_u32(s);
    c4m_type_t    *c4m_dict_type = c4m_get_my_type(d);
    c4m_xlist_t   *type_params   = c4m_type_get_params(c4m_dict_type);
    c4m_type_t    *key_type      = c4m_xlist_get(type_params, 0, NULL);
    c4m_type_t    *val_type      = c4m_xlist_get(type_params, 1, NULL);
    c4m_dt_info_t *kinfo         = c4m_type_get_data_type_info(key_type);
    c4m_dt_info_t *vinfo         = c4m_type_get_data_type_info(val_type);
    bool           key_by_val    = kinfo->by_value;
    bool           val_by_val    = vinfo->by_value;

    hatrack_dict_init(d, kinfo->hash_fn);

    switch (kinfo->hash_fn) {
    case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
        hatrack_dict_set_hash_offset(d, 2 * (int32_t)sizeof(uint64_t));
        /* fallthrough */
    case HATRACK_DICT_KEY_TYPE_OBJ_PTR:
    case HATRACK_DICT_KEY_TYPE_OBJ_INT:
    case HATRACK_DICT_KEY_TYPE_OBJ_REAL:
        hatrack_dict_set_cache_offset(d, -2 * (int32_t)sizeof(uint64_t));
        break;
    default:
        // nada.
    }

    for (uint32_t i = 0; i < length; i++) {
        void *key;
        void *val;

        if (key_by_val) {
            key = (void *)c4m_unmarshal_u64(s);
        }
        else {
            key = c4m_sub_unmarshal(s, memos);
        }

        if (val_by_val) {
            val = (void *)c4m_unmarshal_u64(s);
        }
        else {
            val = c4m_sub_unmarshal(s, memos);
        }

        hatrack_dict_put(d, key, val);
    }
}

static c4m_str_t *
dict_repr(c4m_dict_t *dict)
{
    uint64_t             view_len;
    c4m_type_t          *dict_type   = c4m_get_my_type(dict);
    c4m_xlist_t         *type_params = c4m_type_get_params(dict_type);
    c4m_type_t          *key_type    = c4m_xlist_get(type_params, 0, NULL);
    c4m_type_t          *val_type    = c4m_xlist_get(type_params, 1, NULL);
    hatrack_dict_item_t *view        = hatrack_dict_items_sort(dict, &view_len);

    c4m_xlist_t *items    = c4m_new(c4m_type_xlist(c4m_type_utf32()),
                                 c4m_kw("length", c4m_ka(view_len)));
    c4m_xlist_t *one_item = c4m_new(c4m_type_xlist(c4m_type_utf32()));
    c4m_utf8_t  *colon    = c4m_get_colon_const();

    for (uint64_t i = 0; i < view_len; i++) {
        c4m_xlist_set(one_item, 0, c4m_repr(view[i].key, key_type));
        c4m_xlist_set(one_item, 1, c4m_repr(view[i].value, val_type));
        c4m_xlist_append(items, c4m_str_join(one_item, colon));
    }

    c4m_xlist_set(one_item, 0, c4m_get_lbrace_const());
    c4m_xlist_set(one_item, 1, c4m_str_join(items, c4m_get_comma_const()));
    c4m_xlist_append(one_item, c4m_get_rbrace_const());

    return c4m_str_join(one_item, c4m_get_comma_const());
}

static bool
dict_can_coerce_to(c4m_type_t *my_type, c4m_type_t *dst_type)
{
    return c4m_types_are_compat(my_type, dst_type, NULL);
}

static c4m_dict_t *
dict_coerce_to(c4m_dict_t *dict, c4m_type_t *dst_type)
{
    uint64_t             len;
    hatrack_dict_item_t *view     = hatrack_dict_items_sort(dict, &len);
    c4m_dict_t          *res      = c4m_new(dst_type);
    c4m_type_t          *src_type = c4m_get_my_type(dict);
    c4m_type_t          *kt_src   = c4m_type_get_param(src_type, 0);
    c4m_type_t          *kt_dst   = c4m_type_get_param(dst_type, 0);
    c4m_type_t          *vt_src   = c4m_type_get_param(src_type, 1);
    c4m_type_t          *vt_dst   = c4m_type_get_param(dst_type, 1);

    for (uint64_t i = 0; i < len; i++) {
        void *key_copy = c4m_coerce(view[i].key, kt_src, kt_dst);
        void *val_copy = c4m_coerce(view[i].value, vt_src, vt_dst);

        hatrack_dict_put(res, key_copy, val_copy);
    }

    return res;
}

c4m_dict_t *
dict_copy(c4m_dict_t *dict)
{
    return dict_coerce_to(dict, c4m_get_my_type(dict));
}

int64_t
dict_len(c4m_dict_t *dict)
{
    uint64_t len;
    uint64_t view = (uint64_t)hatrack_dict_items_nosort(dict, &len);

    return (int64_t)len | (int64_t)(view ^ view);
}

c4m_dict_t *
dict_plus(c4m_dict_t *d1, c4m_dict_t *d2)
{
    uint64_t             l1;
    uint64_t             l2;
    hatrack_dict_item_t *v1 = hatrack_dict_items_sort(d1, &l1);
    hatrack_dict_item_t *v2 = hatrack_dict_items_sort(d2, &l2);

    c4m_dict_t *result = c4m_new(c4m_get_my_type(d1));

    for (uint64_t i = 0; i < l1; i++) {
        hatrack_dict_put(result, v1[i].key, v1[i].value);
    }

    for (uint64_t i = 0; i < l2; i++) {
        hatrack_dict_put(result, v2[i].key, v2[i].value);
    }

    return result;
}

void *
dict_get(c4m_dict_t *d, void *k)
{
    bool found = false;

    void *result = hatrack_dict_get(d, k, &found);

    if (found == false) {
        C4M_CRAISE("Dictionary key not found.");
    }

    return result;
}

static c4m_dict_t *
to_dict_lit(c4m_type_t *objtype, c4m_xlist_t *items, c4m_utf8_t *lm)
{
    uint64_t    n      = c4m_xlist_len(items);
    c4m_dict_t *result = c4m_new(objtype);

    for (unsigned int i = 0; i < n; i++) {
        c4m_tuple_t *tup = c4m_xlist_get(items, i, NULL);
        hatrack_dict_put(result, c4m_tuple_get(tup, 0), c4m_tuple_get(tup, 1));
    }

    return result;
}

const c4m_vtable_t c4m_dict_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR]   = (c4m_vtable_entry)c4m_dict_init,
        [C4M_BI_FINALIZER]     = (c4m_vtable_entry)hatrack_dict_cleanup,
        [C4M_BI_TO_STR]        = (c4m_vtable_entry)dict_repr,
        [C4M_BI_MARSHAL]       = (c4m_vtable_entry)c4m_dict_marshal,
        [C4M_BI_UNMARSHAL]     = (c4m_vtable_entry)c4m_dict_unmarshal,
        [C4M_BI_COERCIBLE]     = (c4m_vtable_entry)dict_can_coerce_to,
        [C4M_BI_COERCE]        = (c4m_vtable_entry)dict_coerce_to,
        [C4M_BI_COPY]          = (c4m_vtable_entry)dict_copy,
        [C4M_BI_ADD]           = (c4m_vtable_entry)dict_plus,
        [C4M_BI_LEN]           = (c4m_vtable_entry)dict_len,
        [C4M_BI_INDEX_GET]     = (c4m_vtable_entry)dict_get,
        [C4M_BI_INDEX_SET]     = (c4m_vtable_entry)hatrack_dict_put,
        [C4M_BI_VIEW]          = (c4m_vtable_entry)hatrack_dict_items_sort,
        [C4M_BI_CONTAINER_LIT] = (c4m_vtable_entry)to_dict_lit,
        NULL,
    },
};

#include "con4m.h"

static void
c4m_set_init(c4m_set_t *set, va_list args)
{
    size_t         hash_fn;
    c4m_type_t    *stype = c4m_get_my_type(set);
    c4m_dt_info_t *info;

    if (stype != NULL) {
        stype   = c4m_xlist_get(c4m_tspec_get_parameters(stype), 0, NULL);
        info    = c4m_tspec_get_data_type_info(stype);
        hash_fn = info->hash_fn;
    }
    else {
        hash_fn = (uint32_t)va_arg(args, size_t);
    }

    hatrack_set_init(set, hash_fn);

    switch (hash_fn) {
    case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
        hatrack_set_set_hash_offset(set, 2 * (int32_t)sizeof(uint64_t));
        /* fallthrough */
    case HATRACK_DICT_KEY_TYPE_OBJ_PTR:
    case HATRACK_DICT_KEY_TYPE_OBJ_INT:
    case HATRACK_DICT_KEY_TYPE_OBJ_REAL:
        hatrack_set_set_cache_offset(set, -2 * (int32_t)sizeof(uint64_t));
        break;
    default:
        // nada.
    }
}

// Same container challenge as with other types, for values anyway.
// For keys, we leverage the key_type field being CSTR or PTR.
// We don't use the OBJ_ options currently. We will use that
// for strings at some point soon though.

static void
c4m_set_marshal(c4m_set_t *d, c4m_stream_t *s, c4m_dict_t *memos, int64_t *mid)
{
    uint64_t length;
    uint8_t  kt   = (uint8_t)d->item_type;
    void   **view = hatrack_set_items_sort(d, &length);

    c4m_marshal_u32((uint32_t)length, s);
    c4m_marshal_u8(kt, s);

    for (uint64_t i = 0; i < length; i++) {
        switch (kt) {
        case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
        case HATRACK_DICT_KEY_TYPE_OBJ_PTR:
            c4m_sub_marshal(view[i], s, memos, mid);
            break;
        case HATRACK_DICT_KEY_TYPE_CSTR:
            c4m_marshal_cstring(view[i], s);
            break;
        default:
            c4m_marshal_u64((uint64_t)view[i], s);
            break;
        }
    }
}

static void
c4m_set_unmarshal(c4m_set_t *d, c4m_stream_t *s, c4m_dict_t *memos)
{
    uint32_t length;
    uint8_t  kt;

    length = c4m_unmarshal_u32(s);
    kt     = c4m_unmarshal_u8(s);

    hatrack_set_init(d, (uint32_t)kt);

    switch (kt) {
    case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
        hatrack_set_set_hash_offset(d, sizeof(uint64_t) * 2);
        /* fallthrough */
    case HATRACK_DICT_KEY_TYPE_PTR:
        hatrack_set_set_cache_offset(d, (int32_t)(-sizeof(uint64_t) * 2));
        break;
    default:
        // nada.
    }

    for (uint32_t i = 0; i < length; i++) {
        void *key;

        switch (kt) {
        case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
        case HATRACK_DICT_KEY_TYPE_OBJ_PTR:
            key = c4m_sub_unmarshal(s, memos);
            break;
        case HATRACK_DICT_KEY_TYPE_CSTR:
            key = c4m_unmarshal_cstring(s);
            break;
        default:
            key = (void *)c4m_unmarshal_u64(s);
            break;
        }

        hatrack_set_add(d, key);
    }
}

c4m_set_t *
c4m_set_shallow_copy(c4m_set_t *s)
{
    if (s == NULL) {
        return NULL;
    }

    c4m_set_t *result = c4m_new(c4m_get_my_type(s));
    uint64_t   count  = 0;
    void     **items  = (void **)hatrack_set_items_sort(s, &count);

    for (uint64_t i = 0; i < count; i++) {
        assert(items[i] != NULL);
        hatrack_set_add(result, items[i]);
    }

    return result;
}

c4m_xlist_t *
c4m_set_to_xlist(c4m_set_t *s)
{
    if (s == NULL) {
        return NULL;
    }

    c4m_type_t  *item_type = c4m_tspec_get_param(c4m_get_my_type(s), 0);
    c4m_xlist_t *result    = c4m_new(c4m_tspec_xlist(item_type));
    uint64_t     count     = 0;
    void       **items     = (void **)hatrack_set_items_sort(s, &count);

    for (uint64_t i = 0; i < count; i++) {
        assert(items[i] != NULL);
        c4m_xlist_append(result, items[i]);
    }

    return result;
}

static c4m_set_t *
to_set_lit(c4m_type_t *objtype, c4m_xlist_t *items, c4m_utf8_t *litmod)
{
    c4m_set_t *result = c4m_new(objtype);
    int        n      = c4m_xlist_len(items);

    for (int i = 0; i < n; i++) {
        void *item = c4m_xlist_get(items, i, NULL);

        assert(item != NULL);
        hatrack_set_add(result, item);
    }

    return result;
}

const c4m_vtable_t c4m_set_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR]   = (c4m_vtable_entry)c4m_set_init,
        [C4M_BI_FINALIZER]     = (c4m_vtable_entry)hatrack_set_cleanup,
        [C4M_BI_MARSHAL]       = (c4m_vtable_entry)c4m_set_marshal,
        [C4M_BI_UNMARSHAL]     = (c4m_vtable_entry)c4m_set_unmarshal,
        [C4M_BI_VIEW]          = (c4m_vtable_entry)hatrack_set_items_sort,
        [C4M_BI_CONTAINER_LIT] = (c4m_vtable_entry)to_set_lit,
        NULL,
    },
};

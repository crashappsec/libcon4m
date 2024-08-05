#include "con4m.h"

#ifndef NO___INT128_T

static inline bool
hatrack_bucket_unreserved(hatrack_hash_t hv)
{
    return !hv;
}

#else
static inline bool
hatrack_bucket_unreserved(hatrack_hash_t hv)
{
    return !hv.w1 && !hv.w2;
}

#endif

hatrack_hash_t
c4m_custom_string_hash(c4m_str_t *s)
{
    union {
        hatrack_hash_t local_hv;
        XXH128_hash_t  xxh_hv;
    } hv;

    static c4m_utf8_t *c4m_null_string = NULL;

    if (c4m_null_string == NULL) {
        c4m_null_string = c4m_new_utf8("");
        c4m_gc_register_root(&c4m_null_string, 1);
    }

    hatrack_hash_t *cache = (void *)(((char *)s) + C4M_HASH_CACHE_OBJ_OFFSET);

    hv.local_hv = *cache;

    if (!c4m_str_codepoint_len(s)) {
        s = c4m_null_string;
    }

    if (hatrack_bucket_unreserved(hv.local_hv)) {
        if (s->utf32) {
            s = c4m_to_utf8(s);
        }

        hv.xxh_hv = XXH3_128bits(s->data, s->byte_len);

        *cache = hv.local_hv;
    }

    return *cache;
}

void
c4m_store_bits(uint64_t     *bitfield,
               mmm_header_t *alloc)
{
    crown_store_t *store = (crown_store_t *)alloc->data;
    void          *end   = (void *)&store->buckets[store->last_slot + 1];

    c4m_mark_raw_to_addr(bitfield, alloc, end);
}

void
c4m_dict_gc_bits_raw(uint64_t *bitfield, void *alloc)
{
    c4m_dict_t *dict = (c4m_dict_t *)alloc;

    c4m_mark_address(bitfield, alloc, &dict->crown_instance.store_current);
    c4m_mark_address(bitfield, alloc, &dict->crown_instance.aux_info_for_store);
    c4m_mark_address(bitfield, alloc, &dict->bucket_aux);
}

void
c4m_dict_gc_bits_bucket_full(uint64_t     *bitfield,
                             mmm_header_t *alloc)
{
    c4m_mark_address(bitfield, alloc, &alloc->next);
    c4m_mark_address(bitfield, alloc, &alloc->cleanup_aux);
    hatrack_dict_item_t *item = (hatrack_dict_item_t *)alloc->data;
    c4m_mark_address(bitfield, alloc, &item->key);
    c4m_mark_address(bitfield, alloc, &item->value);
}

void
c4m_dict_gc_bits_bucket_key(uint64_t     *bitfield,
                            mmm_header_t *alloc)
{
    c4m_mark_address(bitfield, alloc, &alloc->next);
    c4m_mark_address(bitfield, alloc, &alloc->cleanup_aux);
    hatrack_dict_item_t *item = (hatrack_dict_item_t *)alloc->data;
    c4m_mark_address(bitfield, alloc, &item->key);
}

void
c4m_dict_gc_bits_bucket_value(uint64_t     *bitfield,
                              mmm_header_t *alloc)
{
    c4m_mark_address(bitfield, alloc, &alloc->next);
    c4m_mark_address(bitfield, alloc, &alloc->cleanup_aux);
    hatrack_dict_item_t *item = (hatrack_dict_item_t *)alloc->data;
    c4m_mark_address(bitfield, alloc, &item->value);
}

void
c4m_dict_gc_bits_bucket_hdr_only(uint64_t     *bitfield,
                                 mmm_header_t *alloc)
{
    c4m_mark_address(bitfield, alloc, &alloc->next);
    c4m_mark_address(bitfield, alloc, &alloc->cleanup_aux);
}

void
c4m_setup_unmanaged_dict(c4m_dict_t *dict,
                         size_t      hash_type,
                         bool        trace_keys,
                         bool        trace_vals)
{
    hatrack_dict_init(dict, hash_type, C4M_GC_SCAN_ALL);

    if (trace_keys && trace_vals) {
        hatrack_dict_set_aux(dict, c4m_dict_gc_bits_bucket_full);
        return;
    }
    if (trace_keys) {
        hatrack_dict_set_aux(dict, c4m_dict_gc_bits_bucket_key);
        return;
    }
    if (trace_vals) {
        hatrack_dict_set_aux(dict, c4m_dict_gc_bits_bucket_value);
        return;
    }

    hatrack_dict_set_aux(dict, c4m_dict_gc_bits_bucket_hdr_only);
    return;
}

// Used for dictionaries that are temporary and cannot ever be used in
// an object context. This is mainly for short-term state like marhal memos
// and for type hashing.
c4m_dict_t *
c4m_new_unmanaged_dict(size_t hash, bool trace_keys, bool trace_vals)
{
    c4m_dict_t *dict = c4m_gc_raw_alloc(sizeof(c4m_dict_t),
                                        (c4m_mem_scan_fn)c4m_dict_gc_bits_raw);

    c4m_setup_unmanaged_dict(dict, hash, trace_keys, trace_vals);
    return dict;
}

static void
c4m_dict_init(c4m_dict_t *dict, va_list args)
{
    size_t         hash_fn;
    c4m_list_t    *type_params;
    c4m_type_t    *key_type;
    c4m_type_t    *value_type;
    c4m_dt_info_t *info;
    c4m_type_t    *c4m_dict_type = c4m_get_my_type(dict);

    if (c4m_dict_type != NULL) {
        type_params = c4m_type_get_params(c4m_dict_type);
        key_type    = c4m_list_get(type_params, 0, NULL);
        value_type  = c4m_list_get(type_params, 1, NULL);
        info        = c4m_type_get_data_type_info(key_type);
        hash_fn     = info->hash_fn;
    }
    else {
        hash_fn = va_arg(args, size_t);
    }

    hatrack_dict_init(dict, hash_fn, C4M_GC_SCAN_ALL);

    if (c4m_dict_type) {
        void *aux_fun = NULL;

        if (c4m_type_requires_gc_scan(key_type)) {
            if (c4m_type_requires_gc_scan(value_type)) {
                aux_fun = c4m_dict_gc_bits_bucket_full;
            }
            else {
                aux_fun = c4m_dict_gc_bits_bucket_key;
            }
        }
        else {
            if (c4m_type_requires_gc_scan(value_type)) {
                aux_fun = c4m_dict_gc_bits_bucket_value;
            }
            else {
                aux_fun = c4m_dict_gc_bits_bucket_hdr_only;
            }
        }
        hatrack_dict_set_aux(dict, aux_fun);
    }

    switch (hash_fn) {
    case HATRACK_DICT_KEY_TYPE_OBJ_CUSTOM:
        // clang-format off
        hatrack_dict_set_custom_hash(dict,
                                (hatrack_hash_func_t)c4m_custom_string_hash);
        // clang-format on
        break;
    case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
        hatrack_dict_set_hash_offset(dict, C4M_STR_HASH_KEY_POINTER_OFFSET);
        /* fallthrough */
    case HATRACK_DICT_KEY_TYPE_OBJ_PTR:
    case HATRACK_DICT_KEY_TYPE_OBJ_INT:
    case HATRACK_DICT_KEY_TYPE_OBJ_REAL:
        hatrack_dict_set_cache_offset(dict, C4M_HASH_CACHE_OBJ_OFFSET);
        break;
    default:
        break;
    }

    dict->slow_views = false;
}

static c4m_str_t *
dict_repr(c4m_dict_t *dict)
{
    uint64_t             view_len;
    c4m_type_t          *dict_type   = c4m_get_my_type(dict);
    c4m_list_t          *type_params = c4m_type_get_params(dict_type);
    c4m_type_t          *key_type    = c4m_list_get(type_params, 0, NULL);
    c4m_type_t          *val_type    = c4m_list_get(type_params, 1, NULL);
    hatrack_dict_item_t *view        = hatrack_dict_items_sort(dict, &view_len);

    c4m_list_t *items    = c4m_new(c4m_type_list(c4m_type_utf32()),
                                c4m_kw("length", c4m_ka(view_len)));
    c4m_list_t *one_item = c4m_new(c4m_type_list(c4m_type_utf32()));
    c4m_utf8_t *colon    = c4m_get_colon_const();

    for (uint64_t i = 0; i < view_len; i++) {
        c4m_list_set(one_item, 0, c4m_repr(view[i].key, key_type));
        c4m_list_set(one_item, 1, c4m_repr(view[i].value, val_type));
        c4m_list_append(items, c4m_str_join(one_item, colon));
    }

    return c4m_cstr_format("\\{{}\\}",
                           c4m_str_join(items, c4m_get_comma_const()));
    /*
    c4m_list_set(one_item, 0, c4m_get_lbrace_const());
    c4m_list_set(one_item, 1, c4m_str_join(items, c4m_get_comma_const()));
    c4m_list_append(one_item, c4m_get_rbrace_const());

    return c4m_str_join(one_item, c4m_get_space_const());*/
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
    hatrack_dict_item_t *view = hatrack_dict_items_sort(dict, &len);
    c4m_dict_t          *res  = c4m_new(dst_type);

    for (uint64_t i = 0; i < len; i++) {
        void *key_copy = c4m_copy(view[i].key);
        void *val_copy = c4m_copy(view[i].value);

        hatrack_dict_put(res, key_copy, val_copy);
    }

    return res;
}

c4m_dict_t *
c4m_dict_copy(c4m_dict_t *dict)
{
    return dict_coerce_to(dict, c4m_get_my_type(dict));
}

static c4m_dict_t *
c4m_dict_shallow_copy(c4m_dict_t *dict)
{
    uint64_t             len;
    hatrack_dict_item_t *view = hatrack_dict_items_sort(dict, &len);
    c4m_dict_t          *res  = c4m_new(c4m_get_my_type(dict));

    for (uint64_t i = 0; i < len; i++) {
        void *key_copy = view[i].key;
        void *val_copy = view[i].value;

        hatrack_dict_put(res, key_copy, val_copy);
    }

    return res;
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

c4m_list_t *
c4m_dict_keys(c4m_dict_t *dict)
{
    uint64_t    view_len;
    c4m_type_t *dict_type = c4m_get_my_type(dict);
    c4m_type_t *key_type  = c4m_list_get(dict_type->items, 0, NULL);
    void      **keys      = hatrack_dict_keys_sort(dict, &view_len);
    c4m_list_t *result    = c4m_list(key_type);

    for (unsigned int i = 0; i < view_len; i++) {
        c4m_list_append(result, keys[i]);
    }

    return result;
}

static c4m_dict_t *
to_dict_lit(c4m_type_t *objtype, c4m_list_t *items, c4m_utf8_t *lm)
{
    uint64_t    n      = c4m_list_len(items);
    c4m_dict_t *result = c4m_new(objtype);

    for (unsigned int i = 0; i < n; i++) {
        c4m_tuple_t *tup = c4m_list_get(items, i, NULL);
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
        [C4M_BI_COERCIBLE]     = (c4m_vtable_entry)dict_can_coerce_to,
        [C4M_BI_COERCE]        = (c4m_vtable_entry)dict_coerce_to,
        [C4M_BI_SHALLOW_COPY]  = (c4m_vtable_entry)c4m_dict_shallow_copy,
        [C4M_BI_COPY]          = (c4m_vtable_entry)c4m_dict_copy,
        [C4M_BI_ADD]           = (c4m_vtable_entry)dict_plus,
        [C4M_BI_LEN]           = (c4m_vtable_entry)dict_len,
        [C4M_BI_INDEX_GET]     = (c4m_vtable_entry)dict_get,
        [C4M_BI_INDEX_SET]     = (c4m_vtable_entry)hatrack_dict_put,
        [C4M_BI_VIEW]          = (c4m_vtable_entry)hatrack_dict_items_sort,
        [C4M_BI_CONTAINER_LIT] = (c4m_vtable_entry)to_dict_lit,
        [C4M_BI_GC_MAP]        = (c4m_vtable_entry)c4m_dict_gc_bits_raw,
        NULL,
    },
};

#include "con4m.h"

extern hatrack_hash_t c4m_custom_string_hash(c4m_str_t *s);

static void
c4m_set_init(c4m_set_t *set, va_list args)
{
    size_t              hash_fn;
    c4m_type_t         *stype       = c4m_get_my_type(set);
    hatrack_hash_func_t custom_hash = NULL;
    c4m_dt_info_t      *info;

    stype = c4m_list_get(c4m_type_get_params(stype), 0, NULL);
    info  = c4m_type_get_data_type_info(stype);

    switch (info->typeid) {
    case C4M_T_REF:
        hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_PTR;
        break;
    case C4M_T_UTF8:
    case C4M_T_UTF32:
        custom_hash = (hatrack_hash_func_t)c4m_custom_string_hash;
        break;
    default:
        hash_fn = info->hash_fn;
        break;
    }

    c4m_karg_va_init(args);
    c4m_kw_ptr("hash", custom_hash);

    if (custom_hash != NULL) {
        hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_CUSTOM;
        hatrack_set_init(set, hash_fn);
        hatrack_set_set_custom_hash(set, custom_hash);
    }
    else {
        hatrack_set_init(set, hash_fn);
        hatrack_set_set_hash_offset(set, C4M_STR_HASH_KEY_POINTER_OFFSET);
        hatrack_set_set_cache_offset(set, C4M_HASH_CACHE_OBJ_OFFSET);
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

c4m_list_t *
c4m_set_to_xlist(c4m_set_t *s)
{
    if (s == NULL) {
        return NULL;
    }

    c4m_type_t *item_type = c4m_type_get_param(c4m_get_my_type(s), 0);
    c4m_list_t *result    = c4m_new(c4m_type_list(item_type));
    uint64_t    count     = 0;
    void      **items     = (void **)hatrack_set_items_sort(s, &count);

    for (uint64_t i = 0; i < count; i++) {
        c4m_list_append(result, items[i]);
    }

    return result;
}

static c4m_set_t *
to_set_lit(c4m_type_t *objtype, c4m_list_t *items, c4m_utf8_t *litmod)
{
    c4m_set_t *result = c4m_new(objtype);
    int        n      = c4m_list_len(items);

    for (int i = 0; i < n; i++) {
        void *item = c4m_list_get(items, i, NULL);

        assert(item != NULL);
        hatrack_set_add(result, item);
    }

    return result;
}

void
c4m_set_set_gc_bits(uint64_t *bitfield, void *alloc)
{
    // TODO: do this up like dicts.
}

const c4m_vtable_t c4m_set_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR]   = (c4m_vtable_entry)c4m_set_init,
        [C4M_BI_FINALIZER]     = (c4m_vtable_entry)hatrack_set_cleanup,
        [C4M_BI_SHALLOW_COPY]  = (c4m_vtable_entry)c4m_set_shallow_copy,
        [C4M_BI_VIEW]          = (c4m_vtable_entry)hatrack_set_items_sort,
        [C4M_BI_CONTAINER_LIT] = (c4m_vtable_entry)to_set_lit,
        [C4M_BI_GC_MAP]        = (c4m_vtable_entry)C4M_GC_SCAN_ALL,
        NULL,
    },
};

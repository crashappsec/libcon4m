#include <con4m.h>

static void
con4m_dict_init(hatrack_dict_t *dict, va_list args)
{
    size_t       hash_fn;
    xlist_t     *type_params;
    type_spec_t *key_type;
    dt_info     *info;
    type_spec_t *dict_type = get_my_type(dict);

    if (dict_type != NULL) {
	type_params = tspec_get_parameters(dict_type);
	key_type    = xlist_get(type_params, 0, NULL);
	info        = tspec_get_data_type_info(key_type);

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
}

static void
con4m_dict_marshal(dict_t *d, FILE *f, dict_t *memos, int64_t *mid)
{
    uint64_t     length;
    type_spec_t *dict_type = get_my_type(d);

    if (dict_type == NULL) {
	CRAISE("Cannot marshal untyped dictionaries.");
    }

    xlist_t             *type_params = tspec_get_parameters(dict_type);
    type_spec_t         *key_type    = xlist_get(type_params, 0, NULL);
    type_spec_t         *val_type    = xlist_get(type_params, 1, NULL);
    hatrack_dict_item_t *view        = hatrack_dict_items_sort(d, &length);
    dt_info             *kinfo       = tspec_get_data_type_info(key_type);
    dt_info             *vinfo       = tspec_get_data_type_info(val_type);
    bool                 key_by_val  = kinfo->by_value;
    bool                 val_by_val  = vinfo->by_value;

    marshal_u32((uint32_t)length, f);

    // keyhash field is the easiest way to tell whether we're passing by
    // value of

    for (uint64_t i = 0; i < length; i++)
    {
	if (key_by_val) {
	    marshal_u64((uint64_t)view[i].key, f);
	}
	else {
	    con4m_sub_marshal(view[i].key, f, memos, mid);
	}


	if (val_by_val) {
	    marshal_u64((uint64_t)view[i].value, f);
	}
	else {
	    con4m_sub_marshal(view[i].value, f, memos, mid);
	}
    }
}

static void
con4m_dict_unmarshal(dict_t *d, FILE *f, dict_t *memos)
{
    uint32_t     length      = unmarshal_u32(f);
    type_spec_t *dict_type   = get_my_type(d);
    xlist_t     *type_params = tspec_get_parameters(dict_type);
    type_spec_t *key_type    = xlist_get(type_params, 0, NULL);
    type_spec_t *val_type    = xlist_get(type_params, 1, NULL);
    dt_info     *kinfo       = tspec_get_data_type_info(key_type);
    dt_info     *vinfo       = tspec_get_data_type_info(val_type);
    bool         key_by_val  = kinfo->by_value;
    bool         val_by_val  = vinfo->by_value;


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
	    key = (void *)unmarshal_u64(f);
	}
	else {
	    key = con4m_sub_unmarshal(f, memos);
	}

	if (val_by_val) {
	    val = (void *)unmarshal_u64(f);
	}
	else {
	    val = con4m_sub_unmarshal(f, memos);
	}

	hatrack_dict_put(d, key, val);
    }
}

const con4m_vtable dict_vtable = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	(con4m_vtable_entry)con4m_dict_init,
	NULL,
	NULL,
	(con4m_vtable_entry)con4m_dict_marshal,
	(con4m_vtable_entry)con4m_dict_unmarshal
    }
};

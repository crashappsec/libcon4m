#include <con4m.h>

static void
con4m_dict_init(hatrack_dict_t *dict, va_list args)
{
  //    type_spec_t *key_type = va_arg(args, type_spec_t *);
  //  type_spec_t *val_type = va_arg(args, type_spec_t *);


    // Until we push the type system to C, let's just have the
    // constructor require custom invocation I suppose.
    // I.e., for now, this is a C-only API.
    //
    // For now, the constructor should only be called positionally and
    // has but one parameter, which should be one of:
    //
    //  - HATRACK_DICT_KEY_TYPE_INT
    //  - HATRACK_DICT_KEY_TYPE_REAL
    //  - HATRACK_DICT_KEY_TYPE_OBJ_PTR
    //  - HATRACK_DICT_KEY_TYPE_OBJ_CSTR (for a string object)
    //
    // Use that for actual utf8_t / utf32 in which case we assume the
    // offset into the object to use right now.
    //
    // Note that this argument will end up getting removed once the type
    // system is pushed down to C because we can put it in the DT info
    // and just look it up.
    //
    // While HATRACK_DICT_KEY_TYPE_CSTR also exists, this is only
    // meant for pure C strings as keys.


    size_t key_type = (uint32_t)va_arg(args, size_t);
    assert(!(uint64_t)va_arg(args, uint64_t));

    hatrack_dict_init(dict, key_type);

    switch (key_type) {
    case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
	hatrack_dict_set_hash_offset(dict, sizeof(uint64_t) * 2);
	/* fallthrough */
    case HATRACK_DICT_KEY_TYPE_OBJ_PTR:
    case HATRACK_DICT_KEY_TYPE_OBJ_INT:
    case HATRACK_DICT_KEY_TYPE_OBJ_REAL:
	hatrack_dict_set_cache_offset(dict, (int32_t)(-sizeof(uint64_t) * 2));
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
con4m_dict_marshal(dict_t *d, FILE *f, dict_t *memos, int64_t *mid)
{
    uint64_t length;
    uint8_t  kt = (uint8_t)d->key_type;

    hatrack_dict_item_t *view = hatrack_dict_items_sort(d, &length);

    marshal_u32((uint32_t)length, f);
    marshal_u8(kt, f);

    for (uint64_t i = 0; i < length; i++)
    {
	switch (kt) {
	case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
	case HATRACK_DICT_KEY_TYPE_OBJ_PTR:
	    con4m_sub_marshal(view[i].key, f, memos, mid);
	    break;
	case HATRACK_DICT_KEY_TYPE_CSTR:
	    marshal_cstring(view[i].key, f);
	    break;
	default:
	    marshal_u64((uint64_t)view[i].key, f);
	    break;
	}

	// For now, assume all values are objects even though it is
	// clearly WRONG.
	con4m_sub_marshal(view[i].value, f, memos, mid);
    }
}

static void
con4m_dict_unmarshal(dict_t *d, FILE *f, dict_t *memos)
{
    uint32_t length;
    uint8_t  kt;

    length = unmarshal_u32(f);
    kt     = unmarshal_u8(f);

    hatrack_dict_init(d, (uint32_t)kt);

    switch (kt) {
    case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
	hatrack_dict_set_hash_offset(d, sizeof(uint64_t) * 2);
	/* fallthrough */
    case HATRACK_DICT_KEY_TYPE_PTR:
	hatrack_dict_set_cache_offset(d, (int32_t)(-sizeof(uint64_t) * 2));
	break;
    default:
	// nada.
    }

    for (uint32_t i = 0; i < length; i++) {
	void *key;
	void *val;

	switch (kt) {
	case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
	case HATRACK_DICT_KEY_TYPE_OBJ_PTR:
	    key = con4m_sub_unmarshal(f, memos);
	    break;
	case HATRACK_DICT_KEY_TYPE_CSTR:
	    key = unmarshal_cstring(f);
	    break;
	default:
	    key = (void *)unmarshal_u64(f);
	    break;
	}

	// Again, this is not right.
	val = con4m_sub_unmarshal(f, memos);

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

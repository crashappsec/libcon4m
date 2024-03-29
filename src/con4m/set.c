#include <con4m.h>

static void
con4m_set_init(set_t *set, va_list args)
{
    size_t key_type = (uint32_t)va_arg(args, size_t);
    assert(!(uint64_t)va_arg(args, uint64_t));

    hatrack_set_init(set, key_type);

    switch (key_type) {
    case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
	hatrack_set_set_hash_offset(set, sizeof(uint64_t) * 2);
	/* fallthrough */
    case HATRACK_DICT_KEY_TYPE_OBJ_PTR:
    case HATRACK_DICT_KEY_TYPE_OBJ_INT:
    case HATRACK_DICT_KEY_TYPE_OBJ_REAL:
	hatrack_set_set_cache_offset(set, (int32_t)(-sizeof(uint64_t) * 2));
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
con4m_set_marshal(set_t *d, stream_t *s, dict_t *memos, int64_t *mid)
{
    uint64_t  length;
    uint8_t   kt = (uint8_t)d->item_type;
    void    **view = hatrack_set_items_sort(d, &length);

    marshal_u32((uint32_t)length, s);
    marshal_u8(kt, s);

    for (uint64_t i = 0; i < length; i++)
    {
	switch (kt) {
	case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
	case HATRACK_DICT_KEY_TYPE_OBJ_PTR:
	    con4m_sub_marshal(view[i], s, memos, mid);
	    break;
	case HATRACK_DICT_KEY_TYPE_CSTR:
	    marshal_cstring(view[i], s);
	    break;
	default:
	    marshal_u64((uint64_t)view[i], s);
	    break;
	}
    }
}

static void
con4m_set_unmarshal(set_t *d, stream_t *s, dict_t *memos)
{
    uint32_t length;
    uint8_t  kt;

    length = unmarshal_u32(s);
    kt     = unmarshal_u8(s);

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
	    key = con4m_sub_unmarshal(s, memos);
	    break;
	case HATRACK_DICT_KEY_TYPE_CSTR:
	    key = unmarshal_cstring(s);
	    break;
	default:
	    key = (void *)unmarshal_u64(s);
	    break;
	}

	hatrack_set_put(d, key);
    }
}

const con4m_vtable set_vtable = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	(con4m_vtable_entry)con4m_set_init,
	NULL,
	NULL,
	(con4m_vtable_entry)con4m_set_marshal,
	(con4m_vtable_entry)con4m_set_unmarshal
    }
};

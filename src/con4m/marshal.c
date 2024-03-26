#include <con4m.h>

STATIC_ASCII_STR(marshal_err, "No marshal implementation is defined for the "
		 "data type: ");

void
marshal_cstring(char *s, stream_t *stream)
{
    uint32_t len = 0;

    if (s == NULL) {

	stream_raw_write(stream, sizeof(uint32_t), (char *)&len);
	return;
    }

    len = strlen(s);

    stream_raw_write(stream, sizeof(uint32_t), (char *)&len);
    stream_raw_write(stream, len, s);
}

char *
unmarshal_cstring(stream_t *stream)
{
    size_t len = 0;
    char  *result;

    stream_raw_read(stream, sizeof(size_t), (char *)&len);

    if (!len) {
	return 0;
    }

    result = con4m_gc_alloc(len + 1, NULL);

    stream_raw_read(stream, len, result);

    return result;
}

void
marshal_i64(int64_t i, stream_t *s)
{
    little_64(i);

    stream_raw_write(s, sizeof(int64_t), (char *)&i);
}

int64_t
unmarshal_i64(stream_t *s)
{
    int64_t result;

    stream_raw_read(s, sizeof(int64_t), (char *)&result);
    little_64(result);

    return result;
}

void
marshal_i32(int32_t i, stream_t *s)
{
    little_32(i);

    stream_raw_write(s, sizeof(int32_t), (char *)&i);
}

int32_t
unmarshal_i32(stream_t *s)
{
    int32_t result;

    stream_raw_read(s, sizeof(int32_t), (char *)&result);
    little_32(result);

    return result;
}

void
marshal_i16(int16_t i, stream_t *s)
{
    little_16(i);

    stream_raw_write(s, sizeof(int16_t), (char *)&i);
}

int16_t
unmarshal_i16(stream_t *s)
{
    int16_t result;

    stream_raw_read(s, sizeof(int16_t), (char *)&result);
    little_16(result);

    return result;
}

void
con4m_sub_marshal(object_t obj, stream_t *s, dict_t *memos, int64_t *mid)
{
    if (obj == NULL) {
	marshal_u64(0ull, s);
	return;
    }

    int     found = 0;
    int64_t memo  = (int64_t)hatrack_dict_get(memos, obj, &found);

    // If we have already processed this object, we will already have
    // a memo, so we write out the ID for the memo only, and do not
    // duplicate the contents.
    if (found) {
	marshal_u64(memo, s);
	return;
    }

    // Here, we still have to write out the memo before the contents, but
    // we also need to add it to the dict.
    memo = *mid;
    *mid = memo + 1;
    marshal_u64(memo, s);
    hatrack_dict_put(memos, obj, (void *)memo);

    con4m_obj_t *hdr = get_object_header(obj);
    marshal_fn   ptr;

    ptr = (marshal_fn)hdr->base_data_type->vtable->methods[CON4M_BI_MARSHAL];

    if (ptr == NULL) {
	utf8_t *type_name = new_utf8(hdr->base_data_type->name);
	utf8_t *msg       = force_utf8(string_concat(marshal_err, type_name));

	RAISE(msg);
    }

    // This captures the actual index of the base type.
    uint16_t diff = (uint16_t)(hdr->base_data_type - &builtin_type_info[0]);
    marshal_u16(diff, s);

    // And now, the concrete type.
    con4m_sub_marshal(hdr->concrete_type, s, memos, mid);

    return (*ptr)(obj, s, memos, mid);
}

object_t
con4m_sub_unmarshal(stream_t *s, dict_t *memos)
{
    int            found = 0;
    uint64_t       memo;
    con4m_obj_t   *obj;

    memo = unmarshal_u64(s);

    if (!memo) {
	return NULL;
    }

    obj = hatrack_dict_get(memos, (void *)memo, &found);

    if (found) {
	return obj->data;
    }

    uint16_t       base_type_id = unmarshal_u16(s);
    dt_info       *dt_entry;
    uint64_t       alloc_len;
    unmarshal_fn   ptr;

    if (base_type_id > CON4M_NUM_BUILTIN_DTS) {
	CRAISE("Invalid marshal format (got invalid data type ID)");
    }
    dt_entry  = (dt_info *)&builtin_type_info[base_type_id];
    alloc_len = sizeof(con4m_obj_t) + dt_entry->alloc_len;

    obj = (con4m_obj_t *)con4m_gc_alloc(alloc_len,
					(uint64_t *)dt_entry->ptr_info);

    // Now that we've allocated the object, we need to fill in the memo
    // before we unmarshal, because cycles happen.
    hatrack_dict_put(memos, (void *)memo, obj);

    obj->base_data_type = dt_entry;
    obj->concrete_type  = con4m_sub_unmarshal(s, memos);

    ptr = (unmarshal_fn)dt_entry->vtable->methods[CON4M_BI_UNMARSHAL];

    if (ptr == NULL) {
	utf8_t *type_name = new_utf8(dt_entry->name);

	RAISE(force_utf8(string_concat(marshal_err, type_name)));
    }

    (*ptr)(obj->data, s, memos);

    return obj->data;
}

thread_local int marshaling = 0;

void
con4m_marshal(object_t obj, stream_t *s)
{

    if (marshaling) {
	CRAISE("Do not recursively call con4m_marshal; "
	       "call con4m_sub_marshal.");
    }

    marshaling = 1;

    // Start w/ 1 as 0 represents the null pointer.
    int64_t next_memo = 1;
    dict_t *memos     = con4m_new(tspec_dict(tspec_ref(), tspec_u64()));


    con4m_sub_marshal(obj, s, memos, &next_memo);
    marshaling = 0;
}


object_t
con4m_unmarshal(stream_t *s)
{

    if (marshaling) {
	CRAISE("Do not recursively call con4m_unmarshal; "
	       "call con4m_sub_unmarshal.");
    }

    dict_t  *memos = con4m_new(tspec_dict(tspec_u64(), tspec_ref()));
    object_t result;

    marshaling = 1;

    result = con4m_sub_unmarshal(s, memos);

    marshaling = 0;

    return result;
}

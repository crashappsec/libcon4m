#include "con4m.h"

C4M_STATIC_ASCII_STR(c4m_marshal_err,
                     "No marshal implementation is defined for the "
                     "data type: ");

void
c4m_marshal_cstring(char *s, c4m_stream_t *stream)
{
    uint32_t len = 0;

    if (s == NULL) {
        c4m_stream_raw_write(stream, sizeof(uint32_t), (char *)&len);
        return;
    }

    len = strlen(s);

    c4m_stream_raw_write(stream, sizeof(uint32_t), (char *)&len);
    c4m_stream_raw_write(stream, len, s);
}

char *
c4m_unmarshal_cstring(c4m_stream_t *stream)
{
    uint32_t len = 0;
    char    *result;

    c4m_stream_raw_read(stream, sizeof(uint32_t), (char *)&len);

    if (len <= 0) {
        return 0;
    }

    result = c4m_gc_raw_alloc(len + 1, NULL);

    c4m_stream_raw_read(stream, len, result);

    return result;
}

void
c4m_marshal_i64(int64_t i, c4m_stream_t *s)
{
    little_64(i);

    c4m_stream_raw_write(s, sizeof(int64_t), (char *)&i);
}

int64_t
c4m_unmarshal_i64(c4m_stream_t *s)
{
    int64_t result;

    c4m_stream_raw_read(s, sizeof(int64_t), (char *)&result);
    little_64(result);

    return result;
}

void
c4m_marshal_i32(int32_t i, c4m_stream_t *s)
{
    little_32(i);

    c4m_stream_raw_write(s, sizeof(int32_t), (char *)&i);
}

int32_t
c4m_unmarshal_i32(c4m_stream_t *s)
{
    int32_t result;

    c4m_stream_raw_read(s, sizeof(int32_t), (char *)&result);
    little_32(result);

    return result;
}

void
c4m_marshal_i16(int16_t i, c4m_stream_t *s)
{
    little_16(i);
    c4m_stream_raw_write(s, sizeof(int16_t), (char *)&i);
}

int16_t
c4m_unmarshal_i16(c4m_stream_t *s)
{
    int16_t result;

    c4m_stream_raw_read(s, sizeof(int16_t), (char *)&result);
    little_16(result);
    return result;
}

void
c4m_marshal_unmanaged_object(void          *addr,
                             c4m_stream_t  *s,
                             c4m_dict_t    *memos,
                             int64_t       *mid,
                             c4m_marshal_fn fn)
{
    if (addr == NULL) {
        c4m_marshal_u64(0ull, s);
        return;
    }

    bool    found = false;
    int64_t memo  = (int64_t)hatrack_dict_get(memos, addr, &found);

    if (found) {
        c4m_marshal_u64(memo, s);
        return;
    }

    memo = *mid;
    *mid = memo + 1;
    c4m_marshal_u64(memo, s);
    hatrack_dict_put(memos, addr, (void *)memo);
    (*fn)(addr, s, memos, mid);
}

void
c4m_marshal_compact_type(c4m_type_t *t, c4m_stream_t *s)
{
    uint16_t param_count;

    c4m_marshal_u16(t->details->base_type->typeid, s);
    c4m_marshal_u64(t->typeid, s);
    switch (t->details->base_type->dt_kind) {
    case C4M_DT_KIND_nil:
    case C4M_DT_KIND_primitive:
    case C4M_DT_KIND_internal:
    case C4M_DT_KIND_maybe:
    case C4M_DT_KIND_oneof:
        return;
    case C4M_DT_KIND_type_var:
        c4m_marshal_cstring(t->details->name, s);
        return;
    case C4M_DT_KIND_func:
        c4m_marshal_u8(t->details->flags, s);
        // Fallthrough.
    case C4M_DT_KIND_list:
    case C4M_DT_KIND_dict:
    case C4M_DT_KIND_tuple:
    case C4M_DT_KIND_object:
        param_count = (uint16_t)c4m_len(t->details->items);
        c4m_marshal_u16(param_count, s);
        for (int i = 0; i < param_count; i++) {
            c4m_marshal_compact_type(c4m_xlist_get(t->details->items, i, NULL),
                                     s);
        }
        return;
    case C4M_DT_KIND_box:
        c4m_marshal_compact_type((c4m_type_t *)t->details->tsi, s);
        return;
    default:
        c4m_unreachable();
    }
}

c4m_type_t *
c4m_unmarshal_compact_type(c4m_stream_t *s)
{
    c4m_builtin_t  base = (c4m_builtin_t)c4m_unmarshal_u16(s);
    uint64_t       tid  = c4m_unmarshal_u64(s);
    c4m_type_t    *result;
    uint8_t        flags = 0;
    uint16_t       param_count;
    c4m_dt_info_t *dtinfo = (c4m_dt_info_t *)&c4m_base_type_info[base];

    switch (dtinfo->dt_kind) {
    case C4M_DT_KIND_nil:
    case C4M_DT_KIND_primitive:
    case C4M_DT_KIND_internal:
    case C4M_DT_KIND_maybe:
    case C4M_DT_KIND_oneof:
        result = c4m_get_builtin_type(base);
        return result;
    case C4M_DT_KIND_type_var:
        result = c4m_new(c4m_type_typespec(),
                         NULL,
                         NULL,
                         NULL);

        result->typeid        = tid;
        result->details->name = c4m_unmarshal_cstring(s);
        break;

    case C4M_DT_KIND_func:
        flags = c4m_unmarshal_u8(s);
        // Fallthrough.
    case C4M_DT_KIND_list:
    case C4M_DT_KIND_dict:
    case C4M_DT_KIND_tuple:
    case C4M_DT_KIND_object:
        param_count            = c4m_unmarshal_u16(s);
        result                 = c4m_new(c4m_type_typespec(), NULL, NULL, 1UL);
        result->typeid         = tid;
        result->details->flags = flags;
        result->details->items = c4m_xlist(c4m_type_typespec());

        for (int i = 0; i < param_count; i++) {
            c4m_xlist_append(result->details->items, c4m_unmarshal_compact_type(s));
        }
        break;
    case C4M_DT_KIND_box:
        result                     = c4m_new(c4m_type_typespec(), C4M_T_BOX);
        result->details->tsi       = c4m_unmarshal_compact_type(s);
        result->details->base_type = (c4m_dt_info_t *)&c4m_base_type_info[base];
        c4m_calculate_type_hash(result);
        break;
    }

    // hatrack_dict_put(c4m_global_type_env->store, (void *)tid, result);
    result->details->base_type = (c4m_dt_info_t *)&c4m_base_type_info[base];
    return result;
}

void
c4m_sub_marshal(c4m_obj_t obj, c4m_stream_t *s, c4m_dict_t *memos, int64_t *mid)
{
    if (obj == NULL) {
        c4m_marshal_u64(0ull, s);
        return;
    }

    bool    found = 0;
    int64_t memo  = (int64_t)hatrack_dict_get(memos, obj, &found);

    // If we have already processed this object, we will already have
    // a memo, so we write out the ID for the memo only, and do not
    // duplicate the contents.
    if (found) {
        c4m_marshal_u64(memo, s);
        return;
    }

    // Here, we still have to write out the memo before the contents, but
    // we also need to add it to the dict.
    memo = *mid;
    *mid = memo + 1;
    c4m_marshal_u64(memo, s);
    hatrack_dict_put(memos, obj, (void *)memo);

    c4m_base_obj_t *hdr = c4m_object_header(obj);
    c4m_marshal_fn  ptr;

    ptr = (c4m_marshal_fn)hdr->base_data_type->vtable->methods[C4M_BI_MARSHAL];

    if (ptr == NULL) {
        c4m_utf8_t *type_name = c4m_new_utf8(hdr->base_data_type->name);
        c4m_utf8_t *msg       = c4m_to_utf8(c4m_str_concat(c4m_marshal_err,
                                                     type_name));

        C4M_RAISE(msg);
    }

    // This captures the actual index of the base type.
    uint16_t diff = (uint16_t)(hdr->base_data_type - &c4m_base_type_info[0]);
    c4m_marshal_u16(diff, s);

    // And now, the concrete type.
    c4m_marshal_compact_type(hdr->concrete_type, s);

    (*ptr)(obj, s, memos, mid);
    return;
}

void *
c4m_unmarshal_unmanaged_object(size_t           len,
                               c4m_stream_t    *s,
                               c4m_dict_t      *memos,
                               c4m_unmarshal_fn fn)
{
    bool     found = false;
    uint64_t memo;
    void    *addr;

    memo = c4m_unmarshal_u64(s);

    if (!memo) {
        return NULL;
    }

    addr = hatrack_dict_get(memos, (void *)memo, &found);

    if (found) {
        return addr;
    }

    addr = c4m_gc_alloc(len);
    hatrack_dict_put(memos, (void *)memo, addr);

    (*fn)(addr, s, memos);

    return addr;
}

c4m_obj_t
c4m_sub_unmarshal(c4m_stream_t *s, c4m_dict_t *memos)
{
    bool            found = false;
    uint64_t        memo;
    c4m_base_obj_t *obj;

    memo = c4m_unmarshal_u64(s);

    if (!memo) {
        return NULL;
    }

    obj = hatrack_dict_get(memos, (void *)memo, &found);

    if (found) {
        return obj->data;
    }

    c4m_builtin_t    base_type_id = (c4m_builtin_t)c4m_unmarshal_u16(s);
    c4m_dt_info_t   *dt_entry;
    uint64_t         alloc_len;
    c4m_unmarshal_fn ptr;

    if (base_type_id > C4M_NUM_BUILTIN_DTS) {
        C4M_CRAISE("Invalid marshal format (got invalid data type ID)");
    }
    dt_entry  = (c4m_dt_info_t *)&c4m_base_type_info[base_type_id];
    alloc_len = sizeof(c4m_base_obj_t) + dt_entry->alloc_len;

    obj = (c4m_base_obj_t *)c4m_gc_raw_alloc(alloc_len,
                                             (uint64_t *)dt_entry->ptr_info);

    // Now that we've allocated the object, we need to fill in the memo
    // before we unmarshal, because cycles happen.
    hatrack_dict_put(memos, (void *)memo, obj);

    obj->base_data_type = dt_entry;
    obj->concrete_type  = c4m_unmarshal_compact_type(s);

    ptr = (c4m_unmarshal_fn)dt_entry->vtable->methods[C4M_BI_UNMARSHAL];

    if (ptr == NULL) {
        c4m_utf8_t *type_name = c4m_new_utf8(dt_entry->name);

        C4M_RAISE(c4m_to_utf8(c4m_str_concat(c4m_marshal_err, type_name)));
    }

    (*ptr)(obj->data, s, memos);

    return obj->data;
}

thread_local int c4m_marshaling = 0;

void
c4m_marshal(c4m_obj_t obj, c4m_stream_t *s)
{
    if (c4m_marshaling) {
        C4M_CRAISE(
            "Do not recursively call c4m_marshal; "
            "call c4m_sub_marshal.");
    }

    c4m_marshaling = 1;

    // Start w/ 1 as 0 represents the null pointer.
    int64_t     next_memo = 1;
    c4m_dict_t *memos     = c4m_alloc_marshal_memos();

    c4m_sub_marshal(obj, s, memos, &next_memo);
    c4m_marshaling = 0;
}

c4m_obj_t
c4m_unmarshal(c4m_stream_t *s)
{
    if (c4m_marshaling) {
        C4M_CRAISE(
            "Do not recursively call c4m_unmarshal; "
            "call c4m_sub_unmarshal.");
    }

    c4m_dict_t *memos = c4m_alloc_unmarshal_memos();
    c4m_obj_t   result;

    c4m_marshaling = 1;

    result = c4m_sub_unmarshal(s, memos);

    c4m_marshaling = 0;

    return result;
}

void
c4m_dump_c_static_instance_code(c4m_obj_t   obj,
                                char       *symbol_name,
                                c4m_utf8_t *filename)
{
    c4m_buf_t    *b = c4m_new(c4m_type_buffer(), c4m_kw("length", c4m_ka(1)));
    c4m_stream_t *s = c4m_new(c4m_type_stream(),
                              c4m_kw("buffer", c4m_ka(b), "write", c4m_ka(1)));

    c4m_marshal(obj, s);
    c4m_stream_close(s);

    s = c4m_new(c4m_type_stream(),
                c4m_kw("filename",
                       c4m_ka(filename),
                       "write",
                       c4m_ka(1),
                       "read",
                       c4m_ka(0)));

    static int   char_per_line = 12;
    static char *decl_start =
        "#include \"c4m.h\"\n\n"
        "static unsigned char _marshaled_";
    static char *mdecl_end   = "\n};\n\n";
    static char *array_start = "[] = {";
    static char *linebreak   = "\n    ";
    static char *map         = "0123456789abcdef";
    static char *hex_prefix  = "0x";
    static char *obj_type    = "c4m_obj_t ";
    static char *obj_init    = " = NULL;\n\n";
    static char *fn_prefix   = "\nget_";
    static char *fn_part1    = "()\n{\n    if (";
    static char *fn_part2 =
        " == NULL) {\n"
        "        c4m_stream_t *s = c4m_new(c4m_type_stream(), \n"
        "                                c4m_kw(\"buffer\", "
        "c4m_new(c4m_type_buffer(),  \"raw\", _marshaled_";
    static char *fn_part3 =
        ", \n"
        "                                             \"length\", c4m_ka(";
    static char *fn_part4 =
        "))));\n        "
        "        c4m_gc_register_root(&";
    static char *fn_part5 = ", 1);\n        ";
    static char *fn_part6 =
        " = c4m_unmarshal(s);\n    }\n"
        "    return ";
    static char *fn_end = ";\n}\n";

    c4m_stream_raw_write(s, strlen(decl_start), decl_start);
    c4m_stream_raw_write(s, strlen(symbol_name), symbol_name);
    c4m_stream_raw_write(s, strlen(array_start), array_start);

    int i = 0;

    goto skip_first_comma;

    for (; i < b->byte_len; i++) {
        c4m_stream_raw_write(s, 2, ", ");

skip_first_comma:
        if (!(i % char_per_line)) {
            c4m_stream_raw_write(s, strlen(linebreak), linebreak);
        }

        c4m_stream_raw_write(s, strlen(hex_prefix), hex_prefix);

        uint8_t byte = b->data[i];

        c4m_stream_raw_write(s, 1, &(map[byte >> 4]));
        c4m_stream_raw_write(s, 1, &(map[byte & 0x0f]));
    }

    c4m_stream_raw_write(s, strlen(mdecl_end), mdecl_end);

    // Declare the actual unmarshaled variable.
    c4m_stream_raw_write(s, strlen(obj_type), obj_type);
    c4m_stream_raw_write(s, strlen(symbol_name), symbol_name);
    c4m_stream_raw_write(s, strlen(obj_init), obj_init);

    // Declare the accessor that initializes and registers the object
    // if needed.
    c4m_stream_raw_write(s, strlen(obj_type), obj_type);
    c4m_stream_raw_write(s, strlen(fn_prefix), fn_prefix);
    c4m_stream_raw_write(s, strlen(symbol_name), symbol_name);
    c4m_stream_raw_write(s, strlen(fn_part1), fn_part1);
    c4m_stream_raw_write(s, strlen(symbol_name), symbol_name);
    c4m_stream_raw_write(s, strlen(fn_part2), fn_part2);
    c4m_stream_raw_write(s, strlen(symbol_name), symbol_name);
    c4m_stream_raw_write(s, strlen(fn_part3), fn_part3);
    c4m_stream_write_object(s, c4m_str_from_int(b->byte_len));
    c4m_stream_raw_write(s, strlen(fn_part4), fn_part4);
    c4m_stream_raw_write(s, strlen(symbol_name), symbol_name);
    c4m_stream_raw_write(s, strlen(fn_part5), fn_part5);
    c4m_stream_raw_write(s, strlen(symbol_name), symbol_name);
    c4m_stream_raw_write(s, strlen(fn_part6), fn_part6);
    c4m_stream_raw_write(s, strlen(symbol_name), symbol_name);
    c4m_stream_raw_write(s, strlen(fn_end), fn_end);

    c4m_stream_close(s);
}

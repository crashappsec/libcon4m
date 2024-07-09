#include "con4m.h"

const c4m_dt_info_t c4m_base_type_info[C4M_NUM_BUILTIN_DTS] = {
    [C4M_T_ERROR] = {
        .name    = "error",
        .typeid  = C4M_T_ERROR,
        .dt_kind = C4M_DT_KIND_nil,
    },
    [C4M_T_VOID] = {
        .name    = "void",
        .typeid  = C4M_T_VOID,
        .dt_kind = C4M_DT_KIND_nil,
    },
    // Should only be used for views on bitfields and similar, where
    // the representation is packed bits. These should be 100%
    // castable back and forth in practice, as long as we know about
    // them.
    [C4M_T_BOOL] = {
        .name      = "bool",
        .typeid    = C4M_T_BOOL,
        .alloc_len = 1,
        .vtable    = &c4m_bool_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    [C4M_T_I8] = {
        .name      = "i8",
        .typeid    = C4M_T_I8,
        .alloc_len = 1,
        .vtable    = &c4m_i8_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    [C4M_T_BYTE] = {
        .name      = "byte",
        .typeid    = C4M_T_BYTE,
        .alloc_len = 1,
        .vtable    = &c4m_u8_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    [C4M_T_I32] = {
        .name      = "i32",
        .typeid    = C4M_T_I32,
        .alloc_len = 4,
        .vtable    = &c4m_i32_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    [C4M_T_CHAR] = {
        .name      = "char",
        .typeid    = C4M_T_CHAR,
        .alloc_len = 4,
        .vtable    = &c4m_u32_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    [C4M_T_U32] = {
        .name      = "u32",
        .typeid    = C4M_T_U32,
        .alloc_len = 4,
        .vtable    = &c4m_u32_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    [C4M_T_INT] = {
        .name      = "int",
        .typeid    = C4M_T_INT,
        .alloc_len = 8,
        .vtable    = &c4m_i64_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    [C4M_T_UINT] = {
        .name      = "uint",
        .typeid    = C4M_T_UINT,
        .alloc_len = 8,
        .vtable    = &c4m_u64_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    [C4M_T_F32] = {
        .name      = "f32",
        .typeid    = C4M_T_F32,
        .alloc_len = 4,
        .vtable    = &c4m_float_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_REAL,
        .by_value  = true,
    },
    [C4M_T_F64] = {
        .name      = "float",
        .typeid    = C4M_T_F64,
        .alloc_len = 8,
        .vtable    = &c4m_float_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_REAL,
        .by_value  = true,
    },
    [C4M_T_UTF8] = {
        .name      = "string",
        .typeid    = C4M_T_UTF8,
        .alloc_len = sizeof(c4m_str_t),
        .vtable    = &c4m_u8str_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_CUSTOM,
    },
    [C4M_T_BUFFER] = {
        .name      = "buffer",
        .typeid    = C4M_T_BUFFER,
        .alloc_len = sizeof(c4m_buf_t),
        .vtable    = &c4m_buffer_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_CUSTOM,
    },
    [C4M_T_UTF32] = {
        .name      = "utf32",
        .typeid    = C4M_T_UTF32,
        .alloc_len = sizeof(c4m_str_t),
        .vtable    = &c4m_u32str_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_CUSTOM,
    },
    [C4M_T_GRID] = {
        .name      = "grid",
        .typeid    = C4M_T_GRID,
        .alloc_len = sizeof(c4m_grid_t),
        .vtable    = &c4m_grid_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_LIST] = {
        .name      = "list",
        .typeid    = C4M_T_XLIST,
        .alloc_len = sizeof(c4m_list_t),
        .vtable    = &c4m_list_vtable,
        .dt_kind   = C4M_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_TUPLE] = {
        .name      = "tuple",
        .typeid    = C4M_T_TUPLE,
        .alloc_len = sizeof(c4m_tuple_t),

        .vtable  = &c4m_tuple_vtable,
        .dt_kind = C4M_DT_KIND_tuple,
        .hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_DICT] = {
        .name      = "dict",
        .typeid    = C4M_T_DICT,
        .alloc_len = sizeof(c4m_dict_t),
        .vtable    = &c4m_dict_vtable,
        .dt_kind   = C4M_DT_KIND_dict,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_SET] = {
        .name      = "set",
        .typeid    = C4M_T_SET,
        .alloc_len = sizeof(c4m_set_t),
        .vtable    = &c4m_set_vtable,
        .dt_kind   = C4M_DT_KIND_dict,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_TYPESPEC] = {
        .name      = "typespec",
        .typeid    = C4M_T_TYPESPEC,
        .alloc_len = sizeof(c4m_type_t),
        .vtable    = &c4m_type_spec_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_IPV4] = {
        .name      = "ipaddr",
        .typeid    = C4M_T_IPV4,
        .vtable    = &c4m_ipaddr_vtable,
        .alloc_len = sizeof(struct sockaddr_in6),
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_IPV6] = {
        .name      = "ipv6_unused", // Going to merge w/ ipv4
        .typeid    = C4M_T_IPV6,
        .vtable    = &c4m_ipaddr_vtable,
        .alloc_len = sizeof(struct sockaddr_in6),
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_DURATION] = {
        .name    = "duration",
        .typeid  = C4M_T_DURATION,
        .dt_kind = C4M_DT_KIND_primitive,
        .hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_SIZE] = {
        .name    = "size",
        .typeid  = C4M_T_SIZE,
        .dt_kind = C4M_DT_KIND_primitive,
        .hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_DATETIME] = {
        .name    = "datetime",
        .typeid  = C4M_T_DATETIME,
        .dt_kind = C4M_DT_KIND_primitive,
        .hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_DATE] = {
        .name    = "date",
        .typeid  = C4M_T_DATE,
        .dt_kind = C4M_DT_KIND_primitive,
        .hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_TIME] = {
        .name    = "time",
        .typeid  = C4M_T_TIME,
        .dt_kind = C4M_DT_KIND_primitive,
        .hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_URL] = {
        .name    = "url",
        .typeid  = C4M_T_URL,
        .dt_kind = C4M_DT_KIND_primitive,
        .hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_FLAGS] = {
        .name      = "flags",
        .typeid    = C4M_T_FLAGS,
        .alloc_len = sizeof(c4m_flags_t),
        .vtable    = &c4m_flags_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_CALLBACK] = {
        .name      = "callback",
        .typeid    = C4M_T_CALLBACK,
        .alloc_len = sizeof(c4m_callback_t),
        .vtable    = &c4m_callback_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_QUEUE] = {
        .name      = "queue",
        .typeid    = C4M_T_QUEUE,
        .alloc_len = sizeof(queue_t),
        .vtable    = &c4m_queue_vtable,
        .dt_kind   = C4M_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_RING] = {
        .name      = "ring",
        .typeid    = C4M_T_RING,
        .alloc_len = sizeof(hatring_t),
        .vtable    = &c4m_ring_vtable,
        .dt_kind   = C4M_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_LOGRING] = {
        .name      = "logring",
        .typeid    = C4M_T_LOGRING,
        .alloc_len = sizeof(logring_t),
        .vtable    = &c4m_logring_vtable,
        .dt_kind   = C4M_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_STACK] = {
        .name      = "stack",
        .typeid    = C4M_T_STACK,
        .alloc_len = sizeof(stack_t),
        .vtable    = &c4m_stack_vtable,
        .dt_kind   = C4M_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_RENDERABLE] = {
        .name      = "renderable",
        .typeid    = C4M_T_RENDERABLE,
        .alloc_len = sizeof(c4m_renderable_t),
        .vtable    = &c4m_renderable_vtable,
        .dt_kind   = C4M_DT_KIND_internal,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_FLIST] = {
        .name      = "flist",
        .typeid    = C4M_T_FLIST,
        .alloc_len = sizeof(flexarray_t),
        .vtable    = &c4m_flexarray_vtable,
        .dt_kind   = C4M_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_RENDER_STYLE] = {
        .name      = "render_style",
        .typeid    = C4M_T_RENDER_STYLE,
        .alloc_len = sizeof(c4m_render_style_t),
        .vtable    = &c4m_render_style_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_SHA] = {
        .name      = "hash",
        .typeid    = C4M_T_SHA,
        .alloc_len = sizeof(c4m_sha_t),
        .vtable    = &c4m_sha_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_EXCEPTION] = {
        .name      = "exception",
        .typeid    = C4M_T_EXCEPTION,
        .alloc_len = sizeof(c4m_exception_t),
        .vtable    = &c4m_exception_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_TREE] = {
        .name      = "tree",
        .typeid    = C4M_T_TREE,
        .alloc_len = sizeof(c4m_tree_node_t),
        .vtable    = &c4m_tree_vtable,
        .dt_kind   = C4M_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_FUNCDEF] = {
        // Non-instantiable.
        .name    = "function_definition",
        .typeid  = C4M_T_FUNCDEF,
        .dt_kind = C4M_DT_KIND_func,
    },
    [C4M_T_REF] = {
        // The idea from the library level behind refs is that they
        // will always be pointers, but perhaps not even to one of our
        // heaps.
        //
        // We need to take this into account if we need to dereference
        // something here. Currently, this is only used for holding
        // non-objects internally.
        //
        // Once we add proper references to the language, we might split
        // out such internal references, IDK.
        .name      = "ref",
        .alloc_len = sizeof(void *),
        .typeid    = C4M_T_REF,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_PTR,
    },
    [C4M_T_GENERIC] = {
        // This is meant for runtime sum types. It's lightly used
        // internally, and we may want to do something more
        // sophisticated when deciding how to support this in the
        // language proper.
        .name      = "mixed",
        .typeid    = C4M_T_GENERIC,
        .alloc_len = sizeof(void *),
        .vtable    = &c4m_mixed_vtable,
        .dt_kind   = C4M_DT_KIND_type_var,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_STREAM] = {
        .name      = "stream",
        .typeid    = C4M_T_STREAM,
        .alloc_len = sizeof(c4m_stream_t),
        .vtable    = &c4m_stream_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_KEYWORD] = {
        .name      = "keyword",
        .typeid    = C4M_T_KEYWORD,
        .alloc_len = sizeof(c4m_karg_info_t),
        .vtable    = &c4m_kargs_vtable,
        .dt_kind   = C4M_DT_KIND_internal,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_VM] = {
        .name      = "vm",
        .typeid    = C4M_T_VM,
        .alloc_len = sizeof(c4m_vm_t),
        .vtable    = &c4m_vm_vtable,
    },
    [C4M_T_PARSE_NODE] = {
        .name      = "parse_node",
        .typeid    = C4M_T_PARSE_NODE,
        .alloc_len = sizeof(c4m_pnode_t),
        .vtable    = &c4m_parse_node_vtable,
        .dt_kind   = C4M_DT_KIND_internal,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [C4M_T_BIT] = {
        .name    = "bit",
        .typeid  = C4M_T_BIT,
        .dt_kind = C4M_DT_KIND_internal,
    },
    // Boxes are implemented by having their tsi field point to the
    // primitive type they are boxing. We only support boxing of
    // primitivate value types, because there's no need to box
    // anything else.
    [C4M_T_BOX] = {
        .name      = "box",
        .typeid    = C4M_T_BOX,
        .alloc_len = sizeof(c4m_box_t),
        .dt_kind   = C4M_DT_KIND_box,
        .vtable    = &c4m_box_vtable,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
};

#if defined(C4M_GC_STATS) || defined(C4M_DEBUG)
c4m_obj_t
_c4m_new(char *file, int line, c4m_type_t *type, ...)
#else
c4m_obj_t
_c4m_new(c4m_type_t *type, ...)
#endif
{
    type = c4m_type_resolve(type);

    c4m_base_obj_t  *obj;
    c4m_obj_t        result;
    va_list          args;
    c4m_dt_info_t   *tinfo     = type->details->base_type;
    uint64_t         alloc_len = tinfo->alloc_len + sizeof(c4m_base_obj_t);
    c4m_vtable_entry init_fn   = tinfo->vtable->methods[C4M_BI_CONSTRUCTOR];

#if defined(C4M_GC_STATS) || defined(C4M_DEBUG)
    if (tinfo->vtable->methods[C4M_BI_FINALIZER] == NULL) {
        obj = _c4m_gc_raw_alloc(alloc_len,
                                (c4m_mem_scan_fn)init_fn,
                                file,
                                line);
    }
    else {
        obj = _c4m_gc_raw_alloc_with_finalizer(alloc_len,
                                               (c4m_mem_scan_fn)init_fn,
                                               file,
                                               line);
    }
#else
    if (tinfo->vtable->methods[C4M_BI_FINALIZER] == NULL) {
        obj = c4m_gc_raw_alloc(alloc_len, (c4m_mem_scan_fn)init_fn);
    }
    else {
        obj = c4m_gc_raw_alloc_with_finalizer(alloc_len,
                                              (c4m_mem_scan_fn)init_fn);
    }
#endif

    c4m_alloc_hdr *hdr = &((c4m_alloc_hdr *)obj)[-1];
    hdr->con4m_obj     = 1;
    hdr->scan_fn       = (c4m_mem_scan_fn)tinfo->vtable->methods[C4M_BI_GC_MAP];

    obj->base_data_type = tinfo;
    obj->concrete_type  = type;
    result              = obj->data;

    switch (tinfo->dt_kind) {
    case C4M_DT_KIND_primitive:
    case C4M_DT_KIND_internal:
    case C4M_DT_KIND_list:
    case C4M_DT_KIND_dict:
    case C4M_DT_KIND_tuple:
    case C4M_DT_KIND_object:
    case C4M_DT_KIND_box:
        if (init_fn != NULL) {
            va_start(args, type);
            (*init_fn)(result, args);
            va_end(args);
        }
        break;
    default:
        C4M_CRAISE(
            "Requested type is non-instantiable or not yet "
            "implemented.");
    }

    return result;
}
c4m_str_t *
c4m_repr(void *item, c4m_type_t *t)
{
    uint64_t    x = c4m_type_get_data_type_info(t)->typeid;
    c4m_repr_fn p;

    p = (c4m_repr_fn)c4m_base_type_info[x].vtable->methods[C4M_BI_REPR];

    if (!p) {
        p = (c4m_repr_fn)c4m_base_type_info[x].vtable->methods[C4M_BI_TO_STR];
        if (!p) {
            return c4m_cstr_format("{}@{:x}",
                                   c4m_new_utf8(c4m_base_type_info[x].name),
                                   c4m_box_u64((uint64_t)item));
        }
    }

    return (*p)(item);
}

c4m_str_t *
c4m_value_obj_repr(c4m_obj_t obj)
{
    c4m_type_t *t = c4m_type_resolve(c4m_get_my_type(obj));

    if (c4m_type_is_box(t)) {
        return c4m_repr(c4m_unbox_obj(obj).v, c4m_type_unbox(t));
    }

    return c4m_repr(obj, t);
}

c4m_str_t *
c4m_value_obj_to_str(c4m_obj_t obj)
{
    c4m_type_t *t = c4m_get_my_type(obj);

    return c4m_to_str(obj, t);
}

c4m_str_t *
c4m_to_str(void *item, c4m_type_t *t)
{
    c4m_dt_info_t *dt = c4m_type_get_data_type_info(t);
    uint64_t       x  = dt->typeid;
    c4m_repr_fn    p;

    switch (x) {
    case 0:
        return c4m_new_utf8("error");
    case 1:
        return c4m_new_utf8("void");
    default:
        break;
    }

    p = (c4m_repr_fn)c4m_base_type_info[x].vtable->methods[C4M_BI_TO_STR];

    if (!p) {
        p = (c4m_repr_fn)c4m_base_type_info[x].vtable->methods[C4M_BI_REPR];

        if (!p) {
            return c4m_cstr_format("{}@{:x}",
                                   c4m_new_utf8(c4m_base_type_info[x].name),
                                   c4m_box_u64((uint64_t)item));
        }
    }

    return (*p)(item);
}

c4m_obj_t
c4m_copy_object(c4m_obj_t obj)
{
    c4m_copy_fn ptr = (c4m_copy_fn)c4m_vtable(obj)->methods[C4M_BI_COPY];

    if (ptr == NULL) {
        c4m_utf8_t *err;

        err = c4m_cstr_format(
            "Copying for '{}' objects is not "
            "currently supported.",
            c4m_get_my_type(obj));
        C4M_RAISE(err);
    }

    return (*ptr)(obj);
}

c4m_obj_t
c4m_copy_object_of_type(c4m_obj_t obj, c4m_type_t *t)
{
    if (c4m_type_is_value_type(t)) {
        return obj;
    }

    c4m_copy_fn ptr = (c4m_copy_fn)c4m_vtable(obj)->methods[C4M_BI_COPY];

    if (ptr == NULL) {
        C4M_CRAISE("Copying for this object type not currently supported.");
    }

    return (*ptr)(obj);
}

c4m_obj_t
c4m_add(c4m_obj_t lhs, c4m_obj_t rhs)
{
    c4m_binop_fn ptr = (c4m_binop_fn)c4m_vtable(lhs)->methods[C4M_BI_ADD];

    if (ptr == NULL) {
        C4M_CRAISE("Addition not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

c4m_obj_t
c4m_sub(c4m_obj_t lhs, c4m_obj_t rhs)
{
    c4m_binop_fn ptr = (c4m_binop_fn)c4m_vtable(lhs)->methods[C4M_BI_SUB];

    if (ptr == NULL) {
        C4M_CRAISE("Subtraction not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

c4m_obj_t
c4m_mul(c4m_obj_t lhs, c4m_obj_t rhs)
{
    c4m_binop_fn ptr = (c4m_binop_fn)c4m_vtable(lhs)->methods[C4M_BI_MUL];

    if (ptr == NULL) {
        C4M_CRAISE("Multiplication not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

c4m_obj_t
c4m_div(c4m_obj_t lhs, c4m_obj_t rhs)
{
    c4m_binop_fn ptr = (c4m_binop_fn)c4m_vtable(lhs)->methods[C4M_BI_DIV];

    if (ptr == NULL) {
        C4M_CRAISE("Division not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

c4m_obj_t
c4m_mod(c4m_obj_t lhs, c4m_obj_t rhs)
{
    c4m_binop_fn ptr = (c4m_binop_fn)c4m_vtable(lhs)->methods[C4M_BI_MOD];

    if (ptr == NULL) {
        C4M_CRAISE("Modulus not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

int64_t
c4m_len(c4m_obj_t container)
{
    if (!container) {
        return 0;
    }

    c4m_len_fn ptr = (c4m_len_fn)c4m_vtable(container)->methods[C4M_BI_LEN];

    if (ptr == NULL) {
        C4M_CRAISE("Cannot call len on a non-container.");
    }

    return (*ptr)(container);
}

c4m_obj_t
c4m_index_get(c4m_obj_t container, c4m_obj_t index)
{
    c4m_index_get_fn ptr;

    ptr = (c4m_index_get_fn)c4m_vtable(container)->methods[C4M_BI_INDEX_GET];

    if (ptr == NULL) {
        C4M_CRAISE("No index operation available.");
    }

    return (*ptr)(container, index);
}

void
c4m_index_set(c4m_obj_t container, c4m_obj_t index, c4m_obj_t value)
{
    c4m_index_set_fn ptr;

    ptr = (c4m_index_set_fn)c4m_vtable(container)->methods[C4M_BI_INDEX_SET];

    if (ptr == NULL) {
        C4M_CRAISE("No index assignment operation available.");
    }

    (*ptr)(container, index, value);
}

c4m_obj_t
c4m_slice_get(c4m_obj_t container, int64_t start, int64_t end)
{
    c4m_slice_get_fn ptr;

    ptr = (c4m_slice_get_fn)c4m_vtable(container)->methods[C4M_BI_SLICE_GET];

    if (ptr == NULL) {
        C4M_CRAISE("No slice operation available.");
    }

    return (*ptr)(container, start, end);
}

void
c4m_slice_set(c4m_obj_t container, int64_t start, int64_t end, c4m_obj_t o)
{
    c4m_slice_set_fn ptr;

    ptr = (c4m_slice_set_fn)c4m_vtable(container)->methods[C4M_BI_SLICE_SET];

    if (ptr == NULL) {
        C4M_CRAISE("No slice assignment operation available.");
    }

    (*ptr)(container, start, end, o);
}

bool
c4m_can_coerce(c4m_type_t *t1, c4m_type_t *t2)
{
    if (c4m_types_are_compat(t1, t2, NULL)) {
        return true;
    }

    c4m_dt_info_t    *info = c4m_type_get_data_type_info(t1);
    c4m_vtable_t     *vtbl = (c4m_vtable_t *)info->vtable;
    c4m_can_coerce_fn ptr  = (c4m_can_coerce_fn)vtbl->methods[C4M_BI_COERCIBLE];

    if (ptr == NULL) {
        return false;
    }

    return (*ptr)(t1, t2);
}

c4m_obj_t
c4m_coerce(void *data, c4m_type_t *t1, c4m_type_t *t2)
{
    t1 = c4m_resolve_and_unbox(t1);
    t2 = c4m_resolve_and_unbox(t2);

    c4m_dt_info_t *info = c4m_type_get_data_type_info(t1);
    c4m_vtable_t  *vtbl = (c4m_vtable_t *)info->vtable;
    c4m_coerce_fn  ptr  = (c4m_coerce_fn)vtbl->methods[C4M_BI_COERCE];

    if (ptr == NULL) {
        C4M_CRAISE("Invalid conversion between types.");
    }

    return (*ptr)(data, t2);
}

c4m_obj_t
c4m_coerce_object(const c4m_obj_t obj, c4m_type_t *to_type)
{
    c4m_type_t    *from_type = c4m_get_my_type(obj);
    c4m_dt_info_t *info      = c4m_type_get_data_type_info(from_type);
    uint64_t       value;

    if (!info->by_value) {
        return c4m_coerce(obj, from_type, to_type);
    }

    switch (info->alloc_len) {
    case 8:
        value = (uint64_t) * (uint8_t *)obj;
        break;
    case 32:
        value = (uint64_t) * (uint32_t *)obj;
        break;
    default:
        value = *(uint64_t *)obj;
    }

    value        = (uint64_t)c4m_coerce((void *)value, from_type, to_type);
    info         = c4m_type_get_data_type_info(to_type);
    void *result = c4m_new(to_type);

    if (info->alloc_len == 8) {
        *(uint8_t *)result = (uint8_t)value;
    }
    else {
        if (info->alloc_len == 32) {
            *(uint32_t *)result = (uint32_t)value;
        }
        else {
            *(uint64_t *)result = value;
        }
    }

    return result;
}

bool
c4m_eq(c4m_type_t *t, c4m_obj_t o1, c4m_obj_t o2)
{
    c4m_dt_info_t *info = c4m_type_get_data_type_info(t);
    c4m_vtable_t  *vtbl = (c4m_vtable_t *)info->vtable;
    c4m_cmp_fn     ptr  = (c4m_cmp_fn)vtbl->methods[C4M_BI_EQ];

    if (!ptr) {
        return o1 == o2;
    }

    return (*ptr)(o1, o2);
}

bool
c4m_lt(c4m_type_t *t, c4m_obj_t o1, c4m_obj_t o2)
{
    c4m_dt_info_t *info = c4m_type_get_data_type_info(t);
    c4m_vtable_t  *vtbl = (c4m_vtable_t *)info->vtable;
    c4m_cmp_fn     ptr  = (c4m_cmp_fn)vtbl->methods[C4M_BI_LT];

    if (!ptr) {
        return o1 < o2;
    }

    return (*ptr)(o1, o2);
}

bool
c4m_gt(c4m_type_t *t, c4m_obj_t o1, c4m_obj_t o2)
{
    c4m_dt_info_t *info = c4m_type_get_data_type_info(t);
    c4m_vtable_t  *vtbl = (c4m_vtable_t *)info->vtable;
    c4m_cmp_fn     ptr  = (c4m_cmp_fn)vtbl->methods[C4M_BI_GT];

    if (!ptr) {
        return o1 > o2;
    }

    return (*ptr)(o1, o2);
}

c4m_type_t *
c4m_get_item_type(c4m_obj_t obj)
{
    c4m_type_t       *t    = c4m_get_my_type(obj);
    c4m_dt_info_t    *info = c4m_type_get_data_type_info(t);
    c4m_vtable_t     *vtbl = (c4m_vtable_t *)info->vtable;
    c4m_ix_item_ty_fn ptr  = (c4m_ix_item_ty_fn)vtbl->methods[C4M_BI_ITEM_TYPE];

    if (!ptr) {
        if (c4m_type_get_num_params(t) == 1) {
            return c4m_type_get_param(t, 0);
        }

        c4m_utf8_t *err = c4m_cstr_format(
            "Type {} is not an indexible container type.",
            t);
        C4M_RAISE(err);
    }

    return (*ptr)(c4m_get_my_type(t));
}

void *
c4m_get_view(c4m_obj_t obj, int64_t *n_items)
{
    c4m_type_t    *t         = c4m_get_my_type(obj);
    c4m_dt_info_t *info      = c4m_type_get_data_type_info(t);
    c4m_vtable_t  *vtbl      = (c4m_vtable_t *)info->vtable;
    void          *itf       = vtbl->methods[C4M_BI_ITEM_TYPE];
    c4m_view_fn    ptr       = (c4m_view_fn)vtbl->methods[C4M_BI_VIEW];
    uint64_t       size_bits = 0x3;

    // If no item type callback is provided, we just assume 8 bytes
    // are produced.  In fact, I currently am not providing callbacks
    // for list types or dicts, since their views are always 64 bits;
    // probably should change the builtin to use an interface that
    // gives back bits, not types (and delete the internal one-bit
    // type).

    if (itf) {
        c4m_type_t    *item_type = c4m_get_item_type(obj);
        c4m_dt_info_t *base      = c4m_type_get_data_type_info(item_type);

        if (base->typeid == C4M_T_BIT) {
            size_bits = 0x7;
        }
        else {
            size_bits = c4m_int_log2((uint64_t)base->alloc_len);
        }
    }

    uint64_t ret_as_int = (uint64_t)(*ptr)(obj, (uint64_t *)n_items);

    ret_as_int |= size_bits;
    return (void *)ret_as_int;
}

c4m_obj_t
c4m_container_literal(c4m_type_t *t, c4m_list_t *items, c4m_utf8_t *mod)
{
    c4m_dt_info_t       *info = c4m_type_get_data_type_info(t);
    c4m_vtable_t        *vtbl = (c4m_vtable_t *)info->vtable;
    c4m_container_lit_fn ptr;

    ptr = (c4m_container_lit_fn)vtbl->methods[C4M_BI_CONTAINER_LIT];

    if (ptr == NULL) {
        c4m_utf8_t *err = c4m_cstr_format(
            "Improper implementation; no literal fn "
            "defined for type '{}'.",
            c4m_new_utf8(info->name));
        C4M_RAISE(err);
    }

    c4m_obj_t result = (*ptr)(t, items, mod);

    if (result == NULL) {
        c4m_utf8_t *err = c4m_cstr_format(
            "Improper implementation; type '{}' did not instantiate "
            "a literal for the literal modifier '{}'",
            c4m_new_utf8(info->name),
            mod);
        C4M_RAISE(err);
    }

    return result;
}

void
c4m_finalize_allocation(c4m_base_obj_t *obj)
{
    c4m_system_finalizer_fn fn;
    return;

    fn = (void *)obj->base_data_type->vtable->methods[C4M_BI_FINALIZER];
    if (fn == NULL) {
    }
    assert(fn != NULL);
    (*fn)(obj->data);
}

void
c4m_scan_header_only(uint64_t *bitfield, int n)
{
    *bitfield = C4M_HEADER_SCAN_CONST;
}

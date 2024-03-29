#include <con4m.h>

const dt_info builtin_type_info[CON4M_NUM_BUILTIN_DTS] = {
    {
	.name      = "error",
	.typeid    = T_TYPE_ERROR,
	.base      = BT_nil,
    },
    {
	.name      = "void",
	.typeid    = T_VOID,
	.base      = BT_nil,
    },
    {
	.name      = "bool",
	.typeid    = T_BOOL,
	.alloc_len = 4,
	.vtable    = &bool_type,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
	.by_value  = true,
    },
    {
	.name      = "i8",
	.typeid    = T_I8,
	.alloc_len = 1,
	.vtable    = &signed_ordinal_type,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
	.by_value  = true,
    },
    {
	.name      = "byte",
	.typeid    = T_BYTE,
	.alloc_len = 1,
	.vtable    = &unsigned_ordinal_type,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
	.by_value  = true,
    },
    {   .name      = "i32",
	.typeid    = T_I32,
	.alloc_len = 4,
	.vtable    = &signed_ordinal_type,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
	.by_value  = true,
    },
    {
	.name      = "char",
	.typeid    = T_CHAR,
	.alloc_len = 4,
	.vtable    = &unsigned_ordinal_type,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
	.by_value  = true,
    },
    {
	.name      = "u32",
	.typeid    = T_U32,
	.alloc_len = 4,
	.vtable    = &unsigned_ordinal_type,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
	.by_value  = true,
    },
    {
	.name      = "int",
	.typeid    = T_INT,
	.alloc_len = 8,
	.vtable    = &signed_ordinal_type,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
	.by_value  = true,
    },
    {
	.name      = "uint",
	.typeid    = T_UINT,
	.alloc_len = 8,
	.vtable    = &signed_ordinal_type,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
	.by_value  = true,
    },
    {
	.name      = "f32",
	.typeid    = T_F32,
	.alloc_len = 4,
	.vtable    = &float_type,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_REAL,
	.by_value  = true,
    },
    {
	.name      = "float",
	.typeid    = T_F64,
	.alloc_len = 8,
	.vtable    = &float_type,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_REAL,
	.by_value  = true,
    },
    {
	.name      = "utf8",
	.typeid    = T_UTF8,
	.alloc_len = sizeof(any_str_t),
	.ptr_info  = (uint64_t *)pmap_str,
	.vtable    = &u8str_vtable,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_CSTR,
    },
    {
	.name      = "buffer",
	.typeid    = T_BUFFER,
	.alloc_len = sizeof(buffer_t),
	.ptr_info  = (uint64_t *)pmap_first_word,
	.vtable    = &buffer_vtable,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_CSTR,
    },
    {
	.name      = "utf32",
	.typeid    = T_UTF32,
	.alloc_len = sizeof(any_str_t),
	.ptr_info  = (uint64_t *)pmap_str,
	.vtable    = &u32str_vtable,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_CSTR,
    },
    {
	.name      = "grid",
	.typeid    = T_GRID,
	.alloc_len = sizeof(grid_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &grid_vtable,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name      = "list",
	.typeid    = T_LIST,
	.alloc_len = sizeof(flexarray_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &list_vtable,
	.base      = BT_list,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name      = "tuple",
	.typeid    = T_TUPLE,
	.alloc_len = sizeof(tuple_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &tuple_vtable,
	.base      = BT_tuple,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name      = "dict",
	.typeid    = T_DICT,
	.alloc_len = sizeof(dict_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &dict_vtable,
	.base      = BT_dict,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name      = "set",
	.typeid    = T_SET,
	.alloc_len = sizeof(set_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &set_vtable,
	.base      = BT_dict,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	 .name      = "typespec",
	 .typeid    = T_TYPESPEC,
	 .alloc_len = sizeof(type_spec_t),
	 .ptr_info  = GC_SCAN_ALL,
	 .vtable    = &type_spec_vtable,
	 .base      = BT_primitive,
	 .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name      = "ipaddr",
	.typeid    = T_IPV4,
	.ptr_info  = NULL,
	.vtable    = &ipaddr_vtable,
	.alloc_len = sizeof(struct sockaddr_in6),
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name      = "ipv6_unused", // Going to merge w/ ipv4
	.typeid    = T_IPV6,
	.ptr_info  = NULL,
	.vtable    = &ipaddr_vtable,
	.alloc_len = sizeof(struct sockaddr_in6),
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name      = "duration",
	.typeid    = T_DURATION,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name      = "size",
	.typeid    = T_SIZE,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name      = "datetime",
	.typeid    = T_DATETIME,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name      = "date",
	.typeid    = T_DATE,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name      = "time",
	.typeid    = T_TIME,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name      = "url",
	.typeid    = T_URL,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name      = "callback",
	.typeid    = T_CALLBACK,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name      = "queue",
	.typeid    = T_QUEUE,
	.alloc_len = sizeof(queue_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &queue_vtable,
	.base      = BT_list,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name      = "ring",
	.typeid    = T_RING,
	.alloc_len = sizeof(hatring_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &ring_vtable,
	.base      = BT_list,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name      = "logring",
	.typeid    = T_LOGRING,
	.alloc_len = sizeof(logring_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &logring_vtable,
	.base      = BT_list,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name      = "stack",
	.typeid    = T_STACK,
	.alloc_len = sizeof(stack_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &stack_vtable,
	.base      = BT_list,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name       = "renderable",
	.typeid     = T_RENDERABLE,
	.alloc_len  = sizeof(renderable_t),
	.ptr_info   = GC_SCAN_ALL,
	.vtable     = &renderable_vtable,
	.base      = BT_internal,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name      = "xlist",
	.typeid    = T_XLIST,
	.alloc_len = sizeof(xlist_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &xlist_vtable,
	.base      = BT_list,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
     },
     {
	 .name      = "render_style",
	 .typeid    = T_RENDER_STYLE,
	 .alloc_len = sizeof(render_style_t),
	 .ptr_info  = (uint64_t *)&rs_pmap,
	 .vtable    = &render_style_vtable,
	 .base      = BT_primitive,
	 .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
     },
     {
	 .name      = "hash",
	 .typeid    = T_SHA,
	 .alloc_len = sizeof(sha_ctx),
	 .ptr_info  = GC_SCAN_ALL,
	 .vtable    = &sha_vtable,
	 .base      = BT_primitive,
	 .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
     },
     {
	 .name      = "exception",
	 .typeid    = T_EXCEPTION,
	 .alloc_len = sizeof(exception_t),
	 .ptr_info  = (uint64_t *)&exception_pmap,
	 .vtable    = &exception_vtable,
	 .base      = BT_primitive,
	 .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
     },
     {
	 .name      = "type_env",
	 .typeid    = T_TYPE_ENV,
	 .alloc_len = sizeof(type_env_t),
	 .ptr_info  = (uint64_t *)pmap_first_word,
	 .vtable    = &type_env_vtable,
	 .base      = BT_internal,
	 .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
     },
     {
	 .name      = "tree",
	 .typeid    = T_TREE,
	 .alloc_len = sizeof(tree_node_t),
	 .ptr_info  = GC_SCAN_ALL, // TODO: set to 6, 7, 8
	 .vtable    = &tree_vtable,
	 .base      = BT_list,
	 .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
     },
     { // Non-instantiable.
	 .name      = "function_definition",
	 .typeid    = T_FUNCDEF,
	 .base      = BT_func,
     },
     {
	 .name      = "ref",
	 .alloc_len = sizeof(void *),
	 .ptr_info  = GC_SCAN_ALL,
	 .typeid    = T_REF,
	 .base      = BT_primitive,
	 .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
     },
     {
	 .name      = "mixed",
	 .typeid    = T_GENERIC,
	 .ptr_info  = GC_SCAN_ALL,
	 .vtable    = &mixed_vtable,
	 .base      = BT_type_var,
	 .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
     },
     {
	 .name      = "stream",
	 .typeid    = T_STREAM,
	 .ptr_info  = GC_SCAN_ALL,
	 .vtable    = &stream_vtable,
	 .base      = BT_primitive,
	 .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
     }

};

object_t
_con4m_new(type_spec_t *type, ...)
{
    con4m_obj_t       *obj;
    object_t           result;
    va_list            args;
    dt_info           *tinfo     = type->details->base_type;
    uint64_t           alloc_len = tinfo->alloc_len + sizeof(con4m_obj_t);
    con4m_vtable_entry init_fn   = tinfo->vtable->methods[CON4M_BI_CONSTRUCTOR];

    obj = con4m_gc_alloc(alloc_len, (uint64_t *)tinfo->ptr_info);

    obj->base_data_type = tinfo;
    obj->concrete_type  = type;
    result              = obj->data;

    switch (tinfo->base) {
    case BT_primitive:
    case BT_internal:
    case BT_list:
    case BT_dict:
    case BT_tuple:
	if (init_fn != NULL) {
	    va_start(args, type);
	    (*init_fn)(result, args);
	    va_end(args);
	}
	break;
    default:
	CRAISE("Requested type is non-instantiable or not yet implemented.");
    }

    return result;
}

uint64_t *
gc_get_ptr_info(con4m_builtin_t dtid)
{
    return (uint64_t *)builtin_type_info[dtid].ptr_info;
}

static const char *repr_err = "Held type does not have a __repr__ function.";

any_str_t *
con4m_value_obj_repr(object_t obj)
{
    // This does NOT work if obj is a value. Use the next fn
    // if that's a possibility.
    repr_fn ptr = (repr_fn)get_vtable(obj)->methods[CON4M_BI_TO_STR];
    if (!ptr) {
	CRAISE(repr_err);
    }
    return (*ptr)(obj, TO_STR_USE_AS_VALUE);
}

any_str_t *
con4m_repr(void *item, type_spec_t *t, to_str_use_t how)
{
    uint64_t x = tspec_get_data_type_info(t)->typeid;
    repr_fn  p = (repr_fn)builtin_type_info[x].vtable->methods[CON4M_BI_TO_STR];

    if (!p) {
	CRAISE(repr_err);
    }

    return (*p)(item, how);
}


object_t
con4m_copy_object(object_t obj)
{
    copy_fn ptr = (copy_fn)get_vtable(obj)->methods[CON4M_BI_COPY];

    if (ptr == NULL) {
	CRAISE("Copying for this object type not currently supported.");
    }

    return (*ptr)(obj);
}

object_t
con4m_add(object_t lhs, object_t rhs)
{
    binop_fn ptr = (binop_fn)get_vtable(lhs)->methods[CON4M_BI_ADD];

    if (ptr == NULL) {
	CRAISE("Addition not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

object_t
con4m_sub(object_t lhs, object_t rhs)
{
    binop_fn ptr = (binop_fn)get_vtable(lhs)->methods[CON4M_BI_SUB];

    if (ptr == NULL) {
	CRAISE("Subtraction not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

object_t
con4m_mul(object_t lhs, object_t rhs)
{
    binop_fn ptr = (binop_fn)get_vtable(lhs)->methods[CON4M_BI_MUL];

    if (ptr == NULL) {
	CRAISE("Multiplication not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

object_t
con4m_div(object_t lhs, object_t rhs)
{
    binop_fn ptr = (binop_fn)get_vtable(lhs)->methods[CON4M_BI_DIV];

    if (ptr == NULL) {
	CRAISE("Division not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

object_t
con4m_mod(object_t lhs, object_t rhs)
{
    binop_fn ptr = (binop_fn)get_vtable(lhs)->methods[CON4M_BI_MOD];

    if (ptr == NULL) {
	CRAISE("Modulus not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

int64_t
con4m_len(object_t container)
{
    len_fn ptr = (len_fn)get_vtable(container)->methods[CON4M_BI_LEN];

    if (ptr == NULL) {
	CRAISE("Cannot call len on a non-container.");
    }

    return (*ptr)(container);
}

object_t
con4m_index_get(object_t container, object_t index)
{
    index_get_fn ptr;

    ptr = (index_get_fn)get_vtable(container)->methods[CON4M_BI_INDEX_GET];

    if (ptr == NULL) {
	CRAISE("No index operation available.");
    }

    return (*ptr)(container, index);
}

void
con4m_index_set(object_t container, object_t index, object_t value)
{
    index_set_fn ptr;

    ptr = (index_set_fn)get_vtable(container)->methods[CON4M_BI_INDEX_SET];

    if (ptr == NULL) {
	CRAISE("No index assignment operation available.");
    }

    (*ptr)(container, index, value);
}

object_t
con4m_slice_get(object_t container, int64_t start, int64_t end)
{
    slice_get_fn ptr;

    ptr = (slice_get_fn)get_vtable(container)->methods[CON4M_BI_SLICE_GET];

    if (ptr == NULL) {
	CRAISE("No slice operation available.");
    }

    return (*ptr)(container, start, end);
}

void
con4m_slice_set(object_t container, int64_t start, int64_t end, object_t o)
{
    slice_set_fn ptr;

    ptr = (slice_set_fn)get_vtable(container)->methods[CON4M_BI_SLICE_SET];

    if (ptr == NULL) {
	CRAISE("No slice assignment operation available.");
    }

    (*ptr)(container, start, end, o);
}

bool
con4m_can_coerce(type_spec_t *t1, type_spec_t *t2)
{
    if (tspecs_are_compat(t1, t2)) {
	return true;
    }

    int64_t       ix   = tspec_get_data_type_info(t1)->typeid;
    con4m_vtable *vtbl = (con4m_vtable *)builtin_type_info[ix].vtable;
    can_coerce_fn ptr  = (can_coerce_fn)vtbl->methods[CON4M_BI_COERCIBLE];

    if (ptr == NULL) {
	return false;
    }

    return (*ptr)(t1, t2);
}

void *
con4m_coerce(void *data, type_spec_t *t1, type_spec_t *t2)
{
    // TODO-- if it's not a primitive type in t1, we should
    // use data's type for extra precaution.

    int64_t       ix   = tspec_get_data_type_info(t1)->typeid;
    con4m_vtable *vtbl = (con4m_vtable *)builtin_type_info[ix].vtable;
    coerce_fn     ptr  = (coerce_fn)vtbl->methods[CON4M_BI_COERCE];

    if (ptr == NULL) {
	CRAISE("Invalid conversion between types.");
    }

    return (*ptr)(data, t2);
}

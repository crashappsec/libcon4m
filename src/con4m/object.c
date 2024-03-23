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
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
	.by_value  = true,
    },
    {
	.name      = "i8",
	.typeid    = T_I8,
	.alloc_len = 1,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
	.by_value  = true,
    },
    {
	.name      = "byte",
	.typeid    = T_BYTE,
	.alloc_len = 1,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
	.by_value  = true,
    },
    {   .name      = "i32",
	.typeid    = T_I32,
	.alloc_len = 4,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
	.by_value  = true,
    },
    {
	.name      = "char",
	.typeid    = T_CHAR,
	.alloc_len = 4,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
	.by_value  = true,
    },
    {
	.name      = "u32",
	.typeid    = T_U32,
	.alloc_len = 4,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
	.by_value  = true,
    },
    {
	.name      = "int",
	.typeid    = T_INT,
	.alloc_len = 8,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
	.by_value  = true,
    },
    {
	.name      = "uint",
	.typeid    = T_UINT,
	.alloc_len = 8,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
	.by_value  = true,
    },
    {
	.name      = "f32",
	.typeid    = T_F32,
	.alloc_len = 4,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_REAL,
	.by_value  = true,
    },
    {
	.name      = "float",
	.typeid    = T_F64,
	.alloc_len = 8,
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
	.name      = "ipv4",
	.typeid    = T_IPV4,
	.base      = BT_primitive,
	.hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
	.name      = "ipv6",
	.typeid    = T_IPV6,
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
	 .base      = BT_type_var,
	 .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
     },

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

any_str_t *
con4m_value_obj_repr(object_t obj)
{
    repr_fn ptr = (repr_fn)get_vtable(obj)->methods[CON4M_BI_TO_STR];
    return (*ptr)(obj, TO_STR_USE_AS_VALUE);
}

#include <con4m.h>

const con4m_dt_info builtin_type_info[CON4M_NUM_BUILTIN_DTS] = {
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
    },
    {
	.name      = "i8",
	.typeid    = T_I8,
	.alloc_len = 1,
	.base      = BT_primitive,
    },
    {
	.name      = "byte",
	.typeid    = T_BYTE,
	.alloc_len = 1,
	.base      = BT_primitive,
    },
    {   .name      = "i32",
	.typeid    = T_I32,
	.alloc_len = 4,
	.base      = BT_primitive,
    },
    {
	.name      = "char",
	.typeid    = T_CHAR,
	.alloc_len = 4,
	.base      = BT_primitive,
    },
    {
	.name      = "u32",
	.typeid    = T_U32,
	.alloc_len = 4,
	.base      = BT_primitive,
    },
    {
	.name      = "int",
	.typeid    = T_INT,
	.alloc_len = 8,
	.base      = BT_primitive,
    },
    {
	.name      = "uint",
	.typeid    = T_UINT,
	.alloc_len = 8,
	.base      = BT_primitive,
    },
    {
	.name      = "f32",
	.typeid    = T_F32,
	.alloc_len = 4,
	.base      = BT_primitive,
    },
    {
	.name      = "float",
	.typeid    = T_F64,
	.alloc_len = 8,
	.base      = BT_primitive,
    },
    {
	.name      = "utf8",
	.typeid    = T_UTF8,
	.alloc_len = sizeof(any_str_t),
	.ptr_info  = (uint64_t *)pmap_str,
	.vtable    = &u8str_vtable,
	.base      = BT_primitive,
    },
    {
	.name      = "buffer",
	.typeid    = T_BUFFER,
	.alloc_len = sizeof(buffer_t),
	.ptr_info  = (uint64_t *)pmap_first_word,
	.vtable    = &buffer_vtable,
	.base      = BT_primitive,
    },
    {
	.name      = "utf32",
	.typeid    = T_UTF32,
	.alloc_len = sizeof(any_str_t),
	.ptr_info  = (uint64_t *)pmap_str,
	.vtable    = &u32str_vtable,
	.base      = BT_primitive,
    },
    {
	.name      = "grid",
	.typeid    = T_GRID,
	.alloc_len = sizeof(grid_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &grid_vtable,
	.base      = BT_primitive,
    },
    {
	.name      = "list",
	.typeid    = T_LIST,
	.alloc_len = sizeof(flexarray_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &list_vtable,
	.base      = BT_list,
    },
    {
	.name      = "tuple",
	.typeid    = T_TUPLE,
	.base      = BT_tuple,
    },

    {
	.name      = "dict",
	.typeid    = T_DICT,
	.alloc_len = sizeof(dict_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &dict_vtable,
	.base      = BT_dict,
    },
     {
	 .name      = "typespec",
	 .typeid    = T_TYPESPEC,
	 .alloc_len = sizeof(type_spec_t),
	 .ptr_info  = GC_SCAN_ALL,
	 .vtable    = &type_spec_vtable,
	 .base      = BT_primitive,
     },
    {
	.name      = "ipv4",
	.typeid    = T_IPV4,
	.base      = BT_primitive,
    },
    {
	.name      = "ipv6",
	.typeid    = T_IPV6,
	.base      = BT_primitive,
    },
    {
	.name      = "duration",
	.typeid    = T_DURATION,
	.base      = BT_primitive,
    },
    {
	.name      = "size",
	.typeid    = T_SIZE,
	.base      = BT_primitive,
    },
    {
	.name      = "datetime",
	.typeid    = T_DATETIME,
	.base      = BT_primitive,
    },
    {
	.name      = "date",
	.typeid    = T_DATE,
	.base      = BT_primitive,
    },
    {
	.name      = "time",
	.typeid    = T_TIME,
	.base      = BT_primitive,
    },
    {
	.name      = "url",
	.typeid    = T_URL,
	.base      = BT_primitive,
    },
    {
	.name      = "callback",
	.typeid    = T_CALLBACK,
	.base      = BT_primitive,
    },
    {
	.name      = "queue",
	.typeid    = T_QUEUE,
	.alloc_len = sizeof(queue_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &queue_vtable,
	.base      = BT_list,
    },
    {
	.name      = "ring",
	.typeid    = T_RING,
	.alloc_len = sizeof(hatring_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &ring_vtable,
	.base      = BT_list,
    },
    {
	.name      = "logring",
	.typeid    = T_LOGRING,
	.alloc_len = sizeof(logring_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &logring_vtable,
	.base      = BT_list,
    },
    {
	.name      = "stack",
	.typeid    = T_STACK,
	.alloc_len = sizeof(stack_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &stack_vtable,
	.base      = BT_list,
    },
    {
	.name       = "renderable",
	.typeid     = T_RENDERABLE,
	.alloc_len  = sizeof(renderable_t),
	.ptr_info   = GC_SCAN_ALL,
	.vtable     = &renderable_vtable,
	.base      = BT_internal,
    },
    {
	.name      = "xlist",
	.typeid    = T_XLIST,
	.alloc_len = sizeof(xlist_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &xlist_vtable,
	.base      = BT_list,
     },
     {
	 .name      = "render_style",
	 .typeid    = T_RENDER_STYLE,
	 .alloc_len = sizeof(render_style_t),
	 .ptr_info  = (uint64_t *)&rs_pmap,
	 .vtable    = &render_style_vtable,
	.base      = BT_primitive,
     },
     {
	 .name      = "hash",
	 .typeid    = T_SHA,
	 .alloc_len = sizeof(sha_ctx),
	 .ptr_info  = GC_SCAN_ALL,
	 .vtable    = &sha_vtable,
	.base      = BT_primitive,
     },
     {
	 .name      = "exception",
	 .typeid    = T_EXCEPTION,
	 .alloc_len = sizeof(exception_t),
	 .ptr_info  = (uint64_t *)&exception_pmap,
	 .vtable    = &exception_vtable,
	 .base      = BT_primitive,
     },
     {
	 .name      = "type_env",
	 .typeid    = T_TYPE_ENV,
	 .alloc_len = sizeof(type_env_t),
	 .ptr_info  = (uint64_t *)pmap_first_word,
	 .vtable    = &type_env_vtable,
	 .base      = BT_internal,
     },
     {
	 .name      = "type_details",
	 .typeid    = T_TYPE_DETAILS,
	 .alloc_len = sizeof(type_details_t),
	 .ptr_info  = GC_SCAN_ALL,
	 .vtable    = &type_details_vtable,
	 .base      = BT_internal,
     },
     {
	 .name      = "mixed",
	 .typeid    = T_GENERIC,
	 .ptr_info  = GC_SCAN_ALL,
	 .base      = BT_type_var,
     },

};

object_t
_con4m_new(con4m_builtin_t typeid, ...)
{
    // With containers, the constructor is expected to update the
    // typeid field.

    con4m_obj_t        *obj;
    object_t            result;
    va_list             args;
    uint64_t            l        = builtin_type_info[typeid].alloc_len;
    uint64_t           *ptr_info = (uint64_t *)builtin_type_info[
	                                                     typeid].ptr_info;
    con4m_vtable_entry  init     = builtin_type_info[typeid].
	                           vtable->methods[CON4M_BI_CONSTRUCTOR];

    va_start(args, typeid);

    obj = (con4m_obj_t *)con4m_gc_alloc(sizeof(con4m_obj_t) + l, ptr_info);

    obj->base_data_type = (con4m_dt_info *)&builtin_type_info[typeid];
    obj->concrete_type  = typeid;

    result = (object_t)obj->data;

    if (init != NULL) {
	(*init)(result, args);
    }

    va_end(args);

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

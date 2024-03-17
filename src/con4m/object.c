#include <con4m.h>

const con4m_dt_info builtin_type_info[CON4M_NUM_BUILTIN_DTS] = {
    {
	.name      = "error",
	.typeid    = T_TYPE_ERROR,
    },
    {
	.name      = "void",
	.typeid    = T_VOID,
    },
    {
	.name      = "bool",
	.typeid    = T_BOOL,
	.alloc_len = 4,
    },
    {
	.name      = "i8",
	.typeid    = T_I8,
	.alloc_len = 1,
    },
    {
	.name      = "byte",
	.typeid    = T_BYTE,
	.alloc_len = 1,
    },
    {   .name      = "i32",
	.typeid    = T_I32,
	.alloc_len = 4,
    },
    {
	.name      = "char",
	.typeid    = T_CHAR,
	.alloc_len = 4,
    },
    {
	.name      = "u32",
	.typeid    = T_U32,
	.alloc_len = 4,
    },
    {
	.name      = "int",
	.typeid    = T_INT,
	.alloc_len = 8,
    },
    {
	.name      = "uint",
	.typeid    = T_UINT,
	.alloc_len = 8,
    },
    {
	.name      = "f32",
	.typeid    = T_F32,
	.alloc_len = 4,
    },
    {
	.name      = "float",
	.typeid    = T_F64,
	.alloc_len = 8,
    },
    {
	.name      = "utf8",
	.typeid    = T_UTF8,
	.alloc_len = sizeof(any_str_t),
	.ptr_info  = (uint64_t *)pmap_str,
	.vtable    = &u8str_vtable
    },
    {
	.name      = "buffer",
	.typeid    = T_BUFFER,
	.alloc_len = sizeof(buffer_t),
	.ptr_info  = (uint64_t *)pmap_first_word,
	.vtable    = &buffer_vtable
    },
    {
	.name      = "utf32",
	.typeid    = T_UTF32,
	.alloc_len = sizeof(any_str_t),
	.ptr_info  = (uint64_t *)pmap_str,
	.vtable    = &u32str_vtable
    },
    {
	.name      = "grid",
	.typeid    = T_GRID,
	.alloc_len = sizeof(grid_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &grid_vtable
    },
    {
	.name      = "list",
	.typeid    = T_LIST,
	.alloc_len = sizeof(flexarray_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &list_vtable
    },
    {
	.name      = "tuple",
	.typeid    = T_TUPLE,
    },

    {
	.name      = "dict",
	.typeid    = T_DICT,
	.alloc_len = sizeof(dict_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &dict_vtable
    },
    {
	.name      = "typespec",
	.typeid    = T_TYPESPEC,
    },
    {
	.name      = "ipv4",
	.typeid    = T_IPV4,
    },
    {
	.name      = "ipv6",
	.typeid    = T_IPV6,
    },
    {
	.name      = "duration",
	.typeid    = T_DURATION,
    },
    {
	.name      = "size",
	.typeid    = T_SIZE,
    },
    {
	.name      = "datetime",
	.typeid    = T_DATETIME,
    },
    {
	.name      = "date",
	.typeid    = T_DATE,
    },
    {
	.name      = "time",
	.typeid    = T_TIME,
    },
    {
	.name      = "url",
	.typeid    = T_URL,
    },
    {
	.name      = "callback",
	.typeid    = T_CALLBACK,
    },
    {
	.name      = "queue",
	.typeid    = T_QUEUE,
	.alloc_len = sizeof(queue_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &queue_vtable
    },
    {
	.name      = "ring",
	.typeid    = T_RING,
	.alloc_len = sizeof(hatring_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &ring_vtable
    },
    {
	.name      = "logring",
	.typeid    = T_LOGRING,
	.alloc_len = sizeof(logring_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &logring_vtable
    },
    {
	.name      = "stack",
	.typeid    = T_STACK,
	.alloc_len = sizeof(stack_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &stack_vtable
    },
    {
	.name       = "renderable",
	.typeid     = T_RENDERABLE,
	.alloc_len  = sizeof(renderable_t),
	.ptr_info   = GC_SCAN_ALL,
	.vtable     = &renderable_vtable,
    },
    {
	.name      = "xlist",
	.typeid    = T_XLIST,
	.alloc_len = sizeof(xlist_t),
	.ptr_info  = GC_SCAN_ALL,
	.vtable    = &xlist_vtable
     },
     {
	 .name      = "render_style",
	 .typeid    = T_RENDER_STYLE,
	 .alloc_len = sizeof(render_style_t),
	 .ptr_info  = (uint64_t *)&rs_pmap,
	 .vtable    = &render_style_vtable
     },
     {
	 .name      = "hash",
	 .typeid    = T_SHA,
	 .alloc_len = sizeof(sha_ctx),
	 .ptr_info  = GC_SCAN_ALL,
	 .vtable    = &sha_vtable
     },
     {
	 .name      = "exception",
	 .typeid    = T_EXCEPTION,
	 .alloc_len = sizeof(exception_t),
	 .ptr_info  = (uint64_t *)&exception_pmap,
	 .vtable    = &exception_vtable
     }
};

object_t
_con4m_new(con4m_builtin_t typeid, ...)
{
    // With containers, the constructor is expected to update the
    // typeid field.

    con4m_obj_t        *obj;
    object_t           result;
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

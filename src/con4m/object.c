#include <con4m.h>

const con4m_dt_info builtin_type_info[CON4M_NUM_BUILTIN_DTS] = {
    { .typeid    = T_TYPE_ERROR, },
    { .typeid    = T_VOID, },
    { .typeid    = T_BOOL,
      .alloc_len = 4,
    },
    { .typeid    = T_I8,
      .alloc_len = 1,
    },
    { .typeid    = T_BYTE,
      .alloc_len = 1,
    },
    { .typeid    = T_I32,
      .alloc_len = 4,
    },
    { .typeid    = T_CHAR,
      .alloc_len = 4,
    },
    { .typeid    = T_U32,
      .alloc_len = 4,
    },
    { .typeid    = T_INT,
      .alloc_len = 8,
    },
    { .typeid    = T_UINT,
      .alloc_len = 8,
    },
    { .typeid    = T_F32,
      .alloc_len = 4,
    },
    { .typeid    = T_F64,
      .alloc_len = 8,
    },
    { .typeid    = T_STR,
      .alloc_len = CON4M_CUSTOM_ALLOC,
      .ptr_info  = (uint64_t *)pmap_str,
      .vtable    = &u8str_vtable
    },
    { .typeid    = T_BUFFER,
      .alloc_len = CON4M_CUSTOM_ALLOC,
      .ptr_info  = (uint64_t *)pmap_str,
      .vtable    = &u8str_vtable
    },
    { .typeid    = T_UTF32,
      .alloc_len = CON4M_CUSTOM_ALLOC,
      .ptr_info  = (uint64_t *)pmap_str,
      .vtable    = &u32str_vtable
    },
    { .typeid    = T_GRID,
      .alloc_len = sizeof(grid_t),
      .ptr_info  = GC_SCAN_ALL,
      .vtable    = &grid_vtable
    },
    { .typeid    = T_LIST,
      .alloc_len = sizeof(flexarray_t),
      .ptr_info  = GC_SCAN_ALL,
      .vtable    = &list_vtable
    },
    { .typeid    = T_TUPLE, },

    { .typeid    = T_DICT,
      .alloc_len = sizeof(hatrack_dict_t),
      .ptr_info  = GC_SCAN_ALL,
      .vtable    = &dict_vtable
    },
    { .typeid    = T_TYPESPEC, },
    { .typeid    = T_IPV4, },
    { .typeid    = T_IPV6, },
    { .typeid    = T_DURATION, },
    { .typeid    = T_SIZE, },
    { .typeid    = T_DATETIME, },
    { .typeid    = T_DATE, },
    { .typeid    = T_TIME, },
    { .typeid    = T_URL, },
    { .typeid    = T_CALLBACK, },
    { .typeid    = T_QUEUE,
      .alloc_len = sizeof(queue_t),
      .ptr_info  = GC_SCAN_ALL,
      .vtable    = &queue_vtable
    },
    { .typeid    = T_RING,
      .alloc_len = sizeof(hatring_t),
      .ptr_info  = GC_SCAN_ALL,
      .vtable    = &ring_vtable
    },
    { .typeid    = T_LOGRING,
      .alloc_len = sizeof(logring_t),
      .ptr_info  = GC_SCAN_ALL,
      .vtable    = &logring_vtable
    },
    { .typeid    = T_STACK,
      .alloc_len = sizeof(stack_t),
      .ptr_info  = GC_SCAN_ALL,
      .vtable    = &stack_vtable
    },
    {.typeid     = T_GRIDPROPS,
     .alloc_len  = sizeof(row_or_col_props_t),
     .ptr_info   = GC_SCAN_ALL,
     .vtable     = &gridprops_vtable,
    },
     {.typeid    = T_RENDERABLE,
     .alloc_len  = sizeof(renderable_t),
     .ptr_info   = GC_SCAN_ALL,
     .vtable     = &renderable_vtable,
     },
    {.typeid     = T_DIMENSIONS,
     .alloc_len  = sizeof(dimspec_t),
     .ptr_info   = GC_SCAN_ALL,
     .vtable     = &dimensions_vtable
    },
     {.typeid    = T_XLIST,
      .alloc_len = sizeof(xlist_t),
      .ptr_info  = GC_SCAN_ALL,
      .vtable    = &xlist_vtable
     },
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

    // Used for objects (strings) that explicitly hide some of their
    // user-defined data.
    if (l != CON4M_CUSTOM_ALLOC) {
	obj = (con4m_obj_t *)con4m_gc_alloc(sizeof(con4m_obj_t) + l, ptr_info);

	obj->base_data_type = (con4m_dt_info *)&builtin_type_info[typeid];
	obj->concrete_type  = typeid;

	result = (object_t)obj->data;

	if (init != NULL) {
	    (*init)(result, args);
	}

    } else {
	result = (*(object_t (*)(va_list))init)(args);
    }

    va_end(args);

    return result;
}

uint64_t *
gc_get_ptr_info(con4m_builtin_t dtid)
{
    return (uint64_t *)builtin_type_info[dtid].ptr_info;
}

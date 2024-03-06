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
      .ptr_info  = (uint64_t *)str_ptr_info,
      .vtable    = &u8str_vtable
    },
    { .typeid    = T_BUFFER,
      .alloc_len = CON4M_CUSTOM_ALLOC,
      .ptr_info  = (uint64_t *)str_ptr_info,
      .vtable    = &u8str_vtable
    },
    { .typeid    = T_UTF32,
      .alloc_len = CON4M_CUSTOM_ALLOC,
      .ptr_info  = (uint64_t *)str_ptr_info,
      .vtable    = &u32str_vtable
    },
    { .typeid = T_GRID, },
    { .typeid = T_LIST, },
    { .typeid = T_TUPLE, },
    { .typeid = T_DICT, },
    { .typeid = T_TYPESPEC, },
    { .typeid = T_IPV4, },
    { .typeid = T_IPV6, },
    { .typeid = T_DURATION, },
    { .typeid = T_SIZE, },
    { .typeid = T_DATETIME, },
    { .typeid = T_DATE, },
    { .typeid = T_TIME, },
    { .typeid = T_URL, },
    { .typeid = T_CALLBACK, }
};

void *
_con4m_new(con4m_builtin_t typeid, ...)
{
    // With containers, the constructor is expected to update the
    // typeid field.

    con4m_obj_t        *obj;
    void               *result;
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

	if (init != NULL) {
	    (*init)(obj, args);
	}

	result = (void *)obj->data;
    } else {
	result = (*(void *(*)(va_list))init)(args);
    }

    va_end(args);

    return result;
}

uint64_t *
gc_get_ptr_info(con4m_builtin_t dtid)
{
    return (uint64_t *)builtin_type_info[dtid].ptr_info;
}

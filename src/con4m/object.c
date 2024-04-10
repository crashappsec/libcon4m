#include "con4m.h"

const c4m_dt_info_t c4m_base_type_info[C4M_NUM_BUILTIN_DTS] = {
    {
        .name    = "error",
        .typeid  = C4M_T_ERROR,
        .dt_kind = C4M_DT_KIND_nil,
    },
    {
        .name    = "void",
        .typeid  = C4M_T_VOID,
        .dt_kind = C4M_DT_KIND_nil,
    },
    {
        .name      = "bool",
        .typeid    = C4M_T_BOOL,
        .alloc_len = 4,
        .vtable    = &c4m_bool_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    {
        .name      = "i8",
        .typeid    = C4M_T_I8,
        .alloc_len = 1,
        .vtable    = &c4m_i8_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    {
        .name      = "byte",
        .typeid    = C4M_T_BYTE,
        .alloc_len = 1,
        .vtable    = &c4m_u8_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    {
        .name      = "i32",
        .typeid    = C4M_T_I32,
        .alloc_len = 4,
        .vtable    = &c4m_i32_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    {
        .name      = "char",
        .typeid    = C4M_T_CHAR,
        .alloc_len = 4,
        .vtable    = &c4m_u32_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    {
        .name      = "u32",
        .typeid    = C4M_T_U32,
        .alloc_len = 4,
        .vtable    = &c4m_u32_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    {
        .name      = "int",
        .typeid    = C4M_T_INT,
        .alloc_len = 8,
        .vtable    = &c4m_i64_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    {
        .name      = "uint",
        .typeid    = C4M_T_UINT,
        .alloc_len = 8,
        .vtable    = &c4m_u64_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    {
        .name      = "f32",
        .typeid    = C4M_T_F32,
        .alloc_len = 4,
        .vtable    = &c4m_float_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_REAL,
        .by_value  = true,
    },
    {
        .name      = "float",
        .typeid    = C4M_T_F64,
        .alloc_len = 8,
        .vtable    = &c4m_float_type,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_REAL,
        .by_value  = true,
    },
    {
        .name      = "utf8",
        .typeid    = C4M_T_UTF8,
        .alloc_len = sizeof(any_str_t),
        .ptr_info  = (uint64_t *)c4m_pmap_str,
        .vtable    = &c4m_u8str_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_CSTR,
    },
    {
        .name      = "buffer",
        .typeid    = C4M_T_BUFFER,
        .alloc_len = sizeof(buffer_t),
        .ptr_info  = (uint64_t *)c4m_pmap_first_word,
        .vtable    = &c4m_buffer_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_CSTR,
    },
    {
        .name      = "utf32",
        .typeid    = C4M_T_UTF32,
        .alloc_len = sizeof(any_str_t),
        .ptr_info  = (uint64_t *)c4m_pmap_str,
        .vtable    = &c4m_u32str_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_CSTR,
    },
    {
        .name      = "grid",
        .typeid    = C4M_T_GRID,
        .alloc_len = sizeof(grid_t),
        .ptr_info  = GC_SCAN_ALL,
        .vtable    = &c4m_grid_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "list",
        .typeid    = C4M_T_LIST,
        .alloc_len = sizeof(flexarray_t),
        .ptr_info  = GC_SCAN_ALL,
        .vtable    = &c4m_list_vtable,
        .dt_kind   = C4M_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "tuple",
        .typeid    = C4M_T_TUPLE,
        .alloc_len = sizeof(tuple_t),
        .ptr_info  = GC_SCAN_ALL,
        .vtable    = &c4m_tuple_vtable,
        .dt_kind   = C4M_DT_KIND_tuple,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "dict",
        .typeid    = C4M_T_DICT,
        .alloc_len = sizeof(dict_t),
        .ptr_info  = GC_SCAN_ALL,
        .vtable    = &c4m_dict_vtable,
        .dt_kind   = C4M_DT_KIND_dict,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "set",
        .typeid    = C4M_T_SET,
        .alloc_len = sizeof(set_t),
        .ptr_info  = GC_SCAN_ALL,
        .vtable    = &c4m_set_vtable,
        .dt_kind   = C4M_DT_KIND_dict,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "typespec",
        .typeid    = C4M_T_TYPESPEC,
        .alloc_len = sizeof(type_spec_t),
        .ptr_info  = GC_SCAN_ALL,
        .vtable    = &c4m_type_spec_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "ipaddr",
        .typeid    = C4M_T_IPV4,
        .ptr_info  = NULL,
        .vtable    = &c4m_ipaddr_vtable,
        .alloc_len = sizeof(struct sockaddr_in6),
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "ipv6_unused", // Going to merge w/ ipv4
        .typeid    = C4M_T_IPV6,
        .ptr_info  = NULL,
        .vtable    = &c4m_ipaddr_vtable,
        .alloc_len = sizeof(struct sockaddr_in6),
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name    = "duration",
        .typeid  = C4M_T_DURATION,
        .dt_kind = C4M_DT_KIND_primitive,
        .hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name    = "size",
        .typeid  = C4M_T_SIZE,
        .dt_kind = C4M_DT_KIND_primitive,
        .hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name    = "datetime",
        .typeid  = C4M_T_DATETIME,
        .dt_kind = C4M_DT_KIND_primitive,
        .hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name    = "date",
        .typeid  = C4M_T_DATE,
        .dt_kind = C4M_DT_KIND_primitive,
        .hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name    = "time",
        .typeid  = C4M_T_TIME,
        .dt_kind = C4M_DT_KIND_primitive,
        .hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name    = "url",
        .typeid  = C4M_T_URL,
        .dt_kind = C4M_DT_KIND_primitive,
        .hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name    = "callback",
        .typeid  = C4M_T_CALLBACK,
        .dt_kind = C4M_DT_KIND_primitive,
        .hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "queue",
        .typeid    = C4M_T_QUEUE,
        .alloc_len = sizeof(queue_t),
        .ptr_info  = GC_SCAN_ALL,
        .vtable    = &c4m_queue_vtable,
        .dt_kind   = C4M_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "ring",
        .typeid    = C4M_T_RING,
        .alloc_len = sizeof(hatring_t),
        .ptr_info  = GC_SCAN_ALL,
        .vtable    = &c4m_ring_vtable,
        .dt_kind   = C4M_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "logring",
        .typeid    = C4M_T_LOGRING,
        .alloc_len = sizeof(logring_t),
        .ptr_info  = GC_SCAN_ALL,
        .vtable    = &c4m_logring_vtable,
        .dt_kind   = C4M_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "stack",
        .typeid    = C4M_T_STACK,
        .alloc_len = sizeof(stack_t),
        .ptr_info  = GC_SCAN_ALL,
        .vtable    = &c4m_stack_vtable,
        .dt_kind   = C4M_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "renderable",
        .typeid    = C4M_T_RENDERABLE,
        .alloc_len = sizeof(renderable_t),
        .ptr_info  = GC_SCAN_ALL,
        .vtable    = &c4m_renderable_vtable,
        .dt_kind   = C4M_DT_KIND_internal,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "xlist",
        .typeid    = C4M_T_XLIST,
        .alloc_len = sizeof(xlist_t),
        .ptr_info  = GC_SCAN_ALL,
        .vtable    = &c4m_xlist_vtable,
        .dt_kind   = C4M_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "render_style",
        .typeid    = C4M_T_RENDER_STYLE,
        .alloc_len = sizeof(c4m_render_style_t),
        .ptr_info  = (uint64_t *)&c4m_rs_pmap,
        .vtable    = &c4m_render_style_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "hash",
        .typeid    = C4M_T_SHA,
        .alloc_len = sizeof(sha_ctx),
        .ptr_info  = GC_SCAN_ALL,
        .vtable    = &c4m_sha_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "exception",
        .typeid    = C4M_T_EXCEPTION,
        .alloc_len = sizeof(exception_t),
        .ptr_info  = (uint64_t *)&c4m_exception_pmap,
        .vtable    = &c4m_exception_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "type_env",
        .typeid    = C4M_T_TYPE_ENV,
        .alloc_len = sizeof(type_env_t),
        .ptr_info  = (uint64_t *)c4m_pmap_first_word,
        .vtable    = &c4m_type_env_vtable,
        .dt_kind   = C4M_DT_KIND_internal,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "tree",
        .typeid    = C4M_T_TREE,
        .alloc_len = sizeof(tree_node_t),
        .ptr_info  = GC_SCAN_ALL, // TODO: set to 6, 7, 8
        .vtable    = &c4m_tree_vtable,
        .dt_kind   = C4M_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        // Non-instantiable.
        .name    = "function_definition",
        .typeid  = C4M_T_FUNCDEF,
        .dt_kind = C4M_DT_KIND_func,
    },
    {
        .name      = "ref",
        .alloc_len = sizeof(void *),
        .ptr_info  = GC_SCAN_ALL,
        .typeid    = C4M_T_REF,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "mixed",
        .typeid    = C4M_T_GENERIC,
        .alloc_len = sizeof(mixed_t),
        .ptr_info  = GC_SCAN_ALL,
        .vtable    = &c4m_mixed_vtable,
        .dt_kind   = C4M_DT_KIND_type_var,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "stream",
        .typeid    = C4M_T_STREAM,
        .alloc_len = sizeof(stream_t),
        .ptr_info  = GC_SCAN_ALL,
        .vtable    = &c4m_stream_vtable,
        .dt_kind   = C4M_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    {
        .name      = "keyword",
        .typeid    = C4M_T_KEYWORD,
        .alloc_len = sizeof(c4m_karg_info_t),
        .ptr_info  = GC_SCAN_ALL,
        .vtable    = &c4m_kargs_vtable,
        .dt_kind   = C4M_DT_KIND_internal,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    }};

object_t
_c4m_new(type_spec_t *type, ...)
{
    c4m_obj_t       *obj;
    object_t         result;
    va_list          args;
    c4m_dt_info_t   *tinfo     = type->details->base_type;
    uint64_t         alloc_len = tinfo->alloc_len + sizeof(c4m_obj_t);
    c4m_vtable_entry init_fn   = tinfo->vtable->methods[C4M_BI_CONSTRUCTOR];

    obj = c4m_gc_raw_alloc(alloc_len, (uint64_t *)tinfo->ptr_info);

    obj->base_data_type = tinfo;
    obj->concrete_type  = type;
    result              = obj->data;

    switch (tinfo->dt_kind) {
    case C4M_DT_KIND_primitive:
    case C4M_DT_KIND_internal:
    case C4M_DT_KIND_list:
    case C4M_DT_KIND_dict:
    case C4M_DT_KIND_tuple:
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

uint64_t *
c4m_gc_ptr_info(c4m_builtin_t dtid)
{
    return (uint64_t *)c4m_base_type_info[dtid].ptr_info;
}

static const char *repr_err = "Held type does not have a __repr__ function.";

any_str_t *
c4m_value_obj_repr(object_t obj)
{
    // This does NOT work on direct values.
    c4m_repr_fn ptr = (c4m_repr_fn)c4m_vtable(obj)->methods[C4M_BI_TO_STR];
    if (!ptr) {
        C4M_CRAISE(repr_err);
    }
    return (*ptr)(obj, C4M_REPR_VALUE);
}

any_str_t *
c4m_repr(void *item, type_spec_t *t, to_str_use_t how)
{
    uint64_t    x = c4m_tspec_get_data_type_info(t)->typeid;
    c4m_repr_fn p = (c4m_repr_fn)c4m_base_type_info[x].vtable->methods[C4M_BI_TO_STR];

    if (!p) {
        C4M_CRAISE(repr_err);
    }

    return (*p)(item, how);
}

object_t
c4m_copy_object(object_t obj)
{
    c4m_copy_fn ptr = (c4m_copy_fn)c4m_vtable(obj)->methods[C4M_BI_COPY];

    if (ptr == NULL) {
        C4M_CRAISE("Copying for this object type not currently supported.");
    }

    return (*ptr)(obj);
}

object_t
c4m_add(object_t lhs, object_t rhs)
{
    c4m_binop_fn ptr = (c4m_binop_fn)c4m_vtable(lhs)->methods[C4M_BI_ADD];

    if (ptr == NULL) {
        C4M_CRAISE("Addition not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

object_t
c4m_sub(object_t lhs, object_t rhs)
{
    c4m_binop_fn ptr = (c4m_binop_fn)c4m_vtable(lhs)->methods[C4M_BI_SUB];

    if (ptr == NULL) {
        C4M_CRAISE("Subtraction not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

object_t
c4m_mul(object_t lhs, object_t rhs)
{
    c4m_binop_fn ptr = (c4m_binop_fn)c4m_vtable(lhs)->methods[C4M_BI_MUL];

    if (ptr == NULL) {
        C4M_CRAISE("Multiplication not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

object_t
c4m_div(object_t lhs, object_t rhs)
{
    c4m_binop_fn ptr = (c4m_binop_fn)c4m_vtable(lhs)->methods[C4M_BI_DIV];

    if (ptr == NULL) {
        C4M_CRAISE("Division not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

object_t
c4m_mod(object_t lhs, object_t rhs)
{
    c4m_binop_fn ptr = (c4m_binop_fn)c4m_vtable(lhs)->methods[C4M_BI_MOD];

    if (ptr == NULL) {
        C4M_CRAISE("Modulus not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

int64_t
c4m_len(object_t container)
{
    c4m_len_fn ptr = (c4m_len_fn)c4m_vtable(container)->methods[C4M_BI_LEN];

    if (ptr == NULL) {
        C4M_CRAISE("Cannot call len on a non-container.");
    }

    return (*ptr)(container);
}

object_t
c4m_index_get(object_t container, object_t index)
{
    c4m_index_get_fn ptr;

    ptr = (c4m_index_get_fn)c4m_vtable(container)->methods[C4M_BI_INDEX_GET];

    if (ptr == NULL) {
        C4M_CRAISE("No index operation available.");
    }

    return (*ptr)(container, index);
}

void
c4m_index_set(object_t container, object_t index, object_t value)
{
    c4m_index_set_fn ptr;

    ptr = (c4m_index_set_fn)c4m_vtable(container)->methods[C4M_BI_INDEX_SET];

    if (ptr == NULL) {
        C4M_CRAISE("No index assignment operation available.");
    }

    (*ptr)(container, index, value);
}

object_t
c4m_slice_get(object_t container, int64_t start, int64_t end)
{
    c4m_slice_get_fn ptr;

    ptr = (c4m_slice_get_fn)c4m_vtable(container)->methods[C4M_BI_SLICE_GET];

    if (ptr == NULL) {
        C4M_CRAISE("No slice operation available.");
    }

    return (*ptr)(container, start, end);
}

void
c4m_slice_set(object_t container, int64_t start, int64_t end, object_t o)
{
    c4m_slice_set_fn ptr;

    ptr = (c4m_slice_set_fn)c4m_vtable(container)->methods[C4M_BI_SLICE_SET];

    if (ptr == NULL) {
        C4M_CRAISE("No slice assignment operation available.");
    }

    (*ptr)(container, start, end, o);
}

bool
c4m_can_coerce(type_spec_t *t1, type_spec_t *t2)
{
    if (c4m_tspecs_are_compat(t1, t2)) {
        return true;
    }

    c4m_dt_info_t    *info = c4m_tspec_get_data_type_info(t1);
    c4m_vtable_t     *vtbl = (c4m_vtable_t *)info->vtable;
    c4m_can_coerce_fn ptr  = (c4m_can_coerce_fn)vtbl->methods[C4M_BI_COERCIBLE];

    if (ptr == NULL) {
        return false;
    }

    return (*ptr)(t1, t2);
}

void *
c4m_coerce(void *data, type_spec_t *t1, type_spec_t *t2)
{
    // TODO-- if it's not a primitive type in t1, we should
    // use data's type for extra precaution.

    c4m_dt_info_t *info = c4m_tspec_get_data_type_info(t1);
    c4m_vtable_t  *vtbl = (c4m_vtable_t *)info->vtable;
    c4m_coerce_fn  ptr  = (c4m_coerce_fn)vtbl->methods[C4M_BI_COERCE];

    if (ptr == NULL) {
        C4M_CRAISE("Invalid conversion between types.");
    }

    return (*ptr)(data, t2);
}

bool
c4m_eq(type_spec_t *t, object_t o1, object_t o2)
{
    c4m_dt_info_t *info = c4m_tspec_get_data_type_info(t);
    c4m_vtable_t  *vtbl = (c4m_vtable_t *)info->vtable;
    c4m_cmp_fn     ptr  = (c4m_cmp_fn)vtbl->methods[C4M_BI_EQ];

    if (!ptr) {
        return o1 == o2;
    }

    return (*ptr)(o1, o2);
}

bool
c4m_lt(type_spec_t *t, object_t o1, object_t o2)
{
    c4m_dt_info_t *info = c4m_tspec_get_data_type_info(t);
    c4m_vtable_t  *vtbl = (c4m_vtable_t *)info->vtable;
    c4m_cmp_fn     ptr  = (c4m_cmp_fn)vtbl->methods[C4M_BI_LT];

    if (!ptr) {
        return o1 < o2;
    }

    return (*ptr)(o1, o2);
}

bool
c4m_gt(type_spec_t *t, object_t o1, object_t o2)
{
    c4m_dt_info_t *info = c4m_tspec_get_data_type_info(t);
    c4m_vtable_t  *vtbl = (c4m_vtable_t *)info->vtable;
    c4m_cmp_fn     ptr  = (c4m_cmp_fn)vtbl->methods[C4M_BI_GT];

    if (!ptr) {
        return o1 > o2;
    }

    return (*ptr)(o1, o2);
}

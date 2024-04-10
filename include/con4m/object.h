#pragma once

#include "con4m.h"

static inline c4m_obj_t *
c4m_object_header(const object_t user_object)
{
    return &((c4m_obj_t *)user_object)[-1];
}

static inline c4m_vtable_t *
c4m_vtable(const object_t user_object)
{
    c4m_obj_t *obj = c4m_object_header(user_object);
    return (c4m_vtable_t *)obj->base_data_type->vtable;
}

static inline uint64_t
c4m_base_type(const object_t user_object)
{
    c4m_obj_t *obj = c4m_object_header(user_object);
    return obj->base_data_type->typeid;
}

static inline const char *
c4m_base_type_name(const object_t user_object)
{
    c4m_obj_t *obj = c4m_object_header(user_object);
    return obj->base_data_type->name;
}

// in object.c
extern const c4m_dt_info_t c4m_base_type_info[C4M_NUM_BUILTIN_DTS];

#define c4m_new(tid, ...) _c4m_new(tid, KFUNC(__VA_ARGS__))

extern object_t   _c4m_new(type_spec_t *type, ...);
extern uint64_t  *c4m_gc_ptr_info(c4m_builtin_t);
extern any_str_t *c4m_repr(void *, type_spec_t *, to_str_use_t);
extern bool       c4m_can_coerce(type_spec_t *, type_spec_t *);
extern object_t   c4m_coerce(void *, type_spec_t *, type_spec_t *);
extern object_t   c4m_copy_object(object_t);
extern object_t   c4m_add(object_t, object_t);
extern object_t   c4m_sub(object_t, object_t);
extern object_t   c4m_mul(object_t, object_t);
extern object_t   c4m_div(object_t, object_t);
extern object_t   c4m_mod(object_t, object_t);
extern bool       c4m_eq(type_spec_t *, object_t, object_t);
extern bool       c4m_lt(type_spec_t *, object_t, object_t);
extern bool       c4m_gt(type_spec_t *, object_t, object_t);
extern int64_t    c4m_len(object_t);
extern object_t   c4m_index_get(object_t, object_t);
extern void       c4m_index_set(object_t, object_t, object_t);
extern object_t   c4m_slice_get(object_t, int64_t, int64_t);
extern void       c4m_slice_set(object_t, int64_t, int64_t, object_t);
extern any_str_t *c4m_value_obj_repr(object_t);

extern const uint64_t     str_ptr_info[];
extern const c4m_vtable_t c4m_i8_type;
extern const c4m_vtable_t c4m_u8_type;
extern const c4m_vtable_t c4m_i32_type;
extern const c4m_vtable_t c4m_u32_type;
extern const c4m_vtable_t c4m_i64_type;
extern const c4m_vtable_t c4m_u64_type;
extern const c4m_vtable_t c4m_bool_type;
extern const c4m_vtable_t c4m_float_type;
extern const c4m_vtable_t c4m_u8str_vtable;
extern const c4m_vtable_t c4m_u32str_vtable;
extern const c4m_vtable_t c4m_buffer_vtable;
extern const c4m_vtable_t c4m_grid_vtable;
extern const c4m_vtable_t c4m_gridprops_vtable;
extern const c4m_vtable_t c4m_renderable_vtable;
extern const c4m_vtable_t c4m_list_vtable;
extern const c4m_vtable_t c4m_queue_vtable;
extern const c4m_vtable_t c4m_ring_vtable;
extern const c4m_vtable_t c4m_logring_vtable;
extern const c4m_vtable_t c4m_stack_vtable;
extern const c4m_vtable_t c4m_dict_vtable;
extern const c4m_vtable_t c4m_set_vtable;
extern const c4m_vtable_t c4m_xlist_vtable;
extern const c4m_vtable_t c4m_sha_vtable;
extern const c4m_vtable_t c4m_render_style_vtable;
extern const c4m_vtable_t c4m_exception_vtable;
extern const c4m_vtable_t c4m_type_env_vtable;
extern const c4m_vtable_t c4m_type_details_vtable;
extern const c4m_vtable_t c4m_type_spec_vtable;
extern const c4m_vtable_t c4m_tree_vtable;
extern const c4m_vtable_t c4m_tuple_vtable;
extern const c4m_vtable_t c4m_mixed_vtable;
extern const c4m_vtable_t c4m_ipaddr_vtable;
extern const c4m_vtable_t c4m_stream_vtable;
extern const c4m_vtable_t c4m_kargs_vtable;

extern const uint64_t c4m_pmap_first_word[2];
extern const uint64_t c4m_rs_pmap[2];
extern const uint64_t c4m_exception_pmap[2];

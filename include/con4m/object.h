#pragma once

#include "con4m.h"

static inline c4m_base_obj_t *
c4m_object_header(const c4m_obj_t *user_object)
{
    return &((c4m_base_obj_t *)user_object)[-1];
}

static inline c4m_vtable_t *
c4m_vtable(const c4m_obj_t *user_object)
{
    c4m_base_obj_t *obj = c4m_object_header(user_object);
    return (c4m_vtable_t *)obj->base_data_type->vtable;
}

static inline uint64_t
c4m_base_type(const c4m_obj_t *user_object)
{
    c4m_base_obj_t *obj = c4m_object_header(user_object);
    return obj->base_data_type->typeid;
}

static inline const char *
c4m_base_type_name(const c4m_obj_t *user_object)
{
    c4m_base_obj_t *obj = c4m_object_header(user_object);
    return obj->base_data_type->name;
}

// in object.c
extern const c4m_dt_info_t c4m_base_type_info[C4M_NUM_BUILTIN_DTS];

#if defined(C4M_GC_STATS) || defined(C4M_DEBUG)
extern c4m_obj_t _c4m_new(char *, int, c4m_type_t *, ...);

#define c4m_new(tid, ...) _c4m_new(__FILE__, __LINE__, tid, KFUNC(__VA_ARGS__))
#else
extern c4m_obj_t _c4m_new(c4m_type_t *type, ...);

#define c4m_new(tid, ...) _c4m_new(tid, KFUNC(__VA_ARGS__))
#endif

extern uint64_t   *c4m_gc_ptr_info(c4m_builtin_t);
extern c4m_str_t  *c4m_repr(void *, c4m_type_t *);
extern c4m_str_t  *c4m_to_str(void *, c4m_type_t *);
extern bool        c4m_can_coerce(c4m_type_t *, c4m_type_t *);
extern c4m_obj_t   c4m_coerce(void *, c4m_type_t *, c4m_type_t *);
extern c4m_obj_t   c4m_coerce_object(const c4m_obj_t, c4m_type_t *);
extern c4m_obj_t   c4m_copy_object(c4m_obj_t);
extern c4m_obj_t   c4m_copy_object_of_type(c4m_obj_t, c4m_type_t *);
extern c4m_obj_t   c4m_add(c4m_obj_t, c4m_obj_t);
extern c4m_obj_t   c4m_sub(c4m_obj_t, c4m_obj_t);
extern c4m_obj_t   c4m_mul(c4m_obj_t, c4m_obj_t);
extern c4m_obj_t   c4m_div(c4m_obj_t, c4m_obj_t);
extern c4m_obj_t   c4m_mod(c4m_obj_t, c4m_obj_t);
extern bool        c4m_eq(c4m_type_t *, c4m_obj_t, c4m_obj_t);
extern bool        c4m_lt(c4m_type_t *, c4m_obj_t, c4m_obj_t);
extern bool        c4m_gt(c4m_type_t *, c4m_obj_t, c4m_obj_t);
extern int64_t     c4m_len(c4m_obj_t);
extern c4m_obj_t   c4m_index_get(c4m_obj_t, c4m_obj_t);
extern void        c4m_index_set(c4m_obj_t, c4m_obj_t, c4m_obj_t);
extern c4m_obj_t   c4m_slice_get(c4m_obj_t, int64_t, int64_t);
extern void        c4m_slice_set(c4m_obj_t, int64_t, int64_t, c4m_obj_t);
extern c4m_str_t  *c4m_value_obj_repr(c4m_obj_t);
extern c4m_str_t  *c4m_value_obj_to_str(c4m_obj_t);
extern c4m_type_t *c4m_get_item_type(c4m_obj_t);
extern void       *c4m_get_view(c4m_obj_t, int64_t *);
extern c4m_obj_t   c4m_container_literal(c4m_type_t *,
                                         c4m_xlist_t *,
                                         c4m_utf8_t *);
extern void        c4m_finalize_allocation(c4m_base_obj_t *);

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
extern const c4m_vtable_t c4m_type_spec_vtable;
extern const c4m_vtable_t c4m_tree_vtable;
extern const c4m_vtable_t c4m_tuple_vtable;
extern const c4m_vtable_t c4m_mixed_vtable;
extern const c4m_vtable_t c4m_ipaddr_vtable;
extern const c4m_vtable_t c4m_stream_vtable;
extern const c4m_vtable_t c4m_kargs_vtable;
extern const c4m_vtable_t c4m_vm_vtable;
extern const c4m_vtable_t c4m_parse_node_vtable;
extern const c4m_vtable_t c4m_callback_vtable;
extern const c4m_vtable_t c4m_flags_vtable;
extern const c4m_vtable_t c4m_box_vtable;
extern const uint64_t     c4m_pmap_first_word[2];
extern const uint64_t     c4m_rs_pmap[2];
extern const uint64_t     c4m_exception_pmap[2];

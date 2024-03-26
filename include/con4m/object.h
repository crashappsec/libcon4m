#pragma once

#include <con4m.h>

static inline con4m_obj_t *
get_object_header(const object_t user_object)
{
    return &((con4m_obj_t *)user_object)[-1];
}

static inline con4m_vtable *
get_vtable(const object_t user_object)
{
    con4m_obj_t *obj = get_object_header(user_object);
    return (con4m_vtable *)obj->base_data_type->vtable;
}

static inline uint64_t
get_base_type(const object_t user_object)
{
    con4m_obj_t *obj = get_object_header(user_object);
    return obj->base_data_type->typeid;
}

static inline const char *
get_base_type_name(const object_t user_object)
{
    con4m_obj_t *obj = get_object_header(user_object);
    return obj->base_data_type->name;
}

// in object.c
extern const dt_info builtin_type_info[CON4M_NUM_BUILTIN_DTS];

#define con4m_new(tid, ...) _con4m_new(tid, KFUNC(__VA_ARGS__))

extern object_t _con4m_new(type_spec_t *type,  ...);
extern uint64_t *gc_get_ptr_info(con4m_builtin_t);

extern any_str_t *con4m_repr(void *, type_spec_t *, to_str_use_t);
extern bool       con4m_can_coerce(type_spec_t *, type_spec_t *);
extern object_t   con4m_coerce(void *, type_spec_t *, type_spec_t *);

extern object_t con4m_copy_object(object_t);
extern object_t con4m_copy_object(object_t);
extern object_t con4m_add(object_t, object_t);
extern object_t con4m_sub(object_t, object_t);
extern object_t con4m_mul(object_t, object_t);
extern object_t con4m_div(object_t, object_t);
extern object_t con4m_mod(object_t, object_t);
extern int64_t con4m_len(object_t);
extern object_t con4m_index_get(object_t, object_t);
extern void con4m_index_set(object_t, object_t, object_t);
extern object_t con4m_slice_get(object_t, int64_t, int64_t);
extern void con4m_slice_set(object_t, int64_t, int64_t, object_t);

extern any_str_t *con4m_value_obj_repr(object_t);

extern const uint64_t str_ptr_info[];

extern const con4m_vtable signed_ordinal_type;
extern const con4m_vtable unsigned_ordinal_type;
extern const con4m_vtable bool_type;
extern const con4m_vtable float_type;
extern const con4m_vtable u8str_vtable;
extern const con4m_vtable u32str_vtable;
extern const con4m_vtable buffer_vtable;
extern const con4m_vtable grid_vtable;
extern const con4m_vtable dimensions_vtable;
extern const con4m_vtable gridprops_vtable;
extern const con4m_vtable renderable_vtable;
extern const con4m_vtable list_vtable;
extern const con4m_vtable queue_vtable;
extern const con4m_vtable ring_vtable;
extern const con4m_vtable logring_vtable;
extern const con4m_vtable stack_vtable;
extern const con4m_vtable dict_vtable;
extern const con4m_vtable set_vtable;
extern const con4m_vtable xlist_vtable;
extern const con4m_vtable sha_vtable;
extern const con4m_vtable render_style_vtable;
extern const con4m_vtable exception_vtable;
extern const con4m_vtable type_env_vtable;
extern const con4m_vtable type_details_vtable;
extern const con4m_vtable type_spec_vtable;
extern const con4m_vtable tree_vtable;
extern const con4m_vtable tuple_vtable;
extern const con4m_vtable mixed_vtable;
extern const con4m_vtable ipaddr_vtable;
extern const con4m_vtable stream_vtable;

extern const uint64_t     pmap_first_word[2];
extern const uint64_t     rs_pmap[2];
extern const uint64_t     exception_pmap[2];

#pragma once

#include <con4m.h>

static inline con4m_obj_t *
get_object_header(const object_t user_object)
{
    return &((con4m_obj_t *)user_object)[-1];
}

// This produces a proper con4m type that encodes the type info known
// about the entire contents. For instance, for a dictionary, the
// value would encode not just the fact that it's a dictionary, but
// the type of the key and the type of the value as well.
static inline uint64_t
get_concrete_type(const object_t user_object)
{
    con4m_obj_t *obj = get_object_header(user_object);
    return obj->concrete_type;
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
extern const con4m_dt_info builtin_type_info[CON4M_NUM_BUILTIN_DTS];

#define con4m_new(tid, ...) _con4m_new(tid, KFUNC(__VA_ARGS__))

extern object_t _con4m_new(con4m_builtin_t typeid,  ...);
extern uint64_t *gc_get_ptr_info(con4m_builtin_t);

typedef any_str_t *(*repr_fn)(object_t, to_str_use_t);

static inline any_str_t *
con4m_raw_repr(con4m_builtin_t tinfo, object_t val, to_str_use_t kind)
{
    const con4m_vtable *vtbl = builtin_type_info[tinfo].vtable;
    repr_fn             ptr  = (repr_fn)vtbl->methods[CON4M_BI_TO_STR];

    return (*ptr)(val, kind);
}

static inline any_str_t *
con4m_value_obj_repr(object_t obj)
{
    repr_fn ptr = (repr_fn)get_vtable(obj)->methods[CON4M_BI_TO_STR];
    return (*ptr)(obj, TO_STR_USE_AS_VALUE);
}


extern const uint64_t str_ptr_info[];
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
extern const con4m_vtable xlist_vtable;
extern const con4m_vtable sha_vtable;
extern const con4m_vtable render_style_vtable;
extern const con4m_vtable exception_vtable;

extern const uint64_t pmap_first_word[2];
extern const uint64_t rs_pmap[2];
extern const uint64_t exception_pmap[2];

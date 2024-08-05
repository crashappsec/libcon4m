#pragma once

#include "con4m.h"

// in object.c
extern const c4m_dt_info_t c4m_base_type_info[C4M_NUM_BUILTIN_DTS];

extern c4m_type_t *c4m_type_resolve(c4m_type_t *);
extern c4m_utf8_t *c4m_new_utf8(char *);

static inline c4m_type_t *
c4m_get_my_type(c4m_obj_t user_object)
{
    c4m_mem_ptr p = (c4m_mem_ptr){.v = user_object};
    p.alloc -= 1;
    return p.alloc->type;
}

static inline c4m_dt_info_t *
c4m_object_type_info(c4m_obj_t user_object)
{
    uint64_t n = c4m_get_my_type(user_object)->base_index;
    return (c4m_dt_info_t *)&c4m_base_type_info[n];
}

static inline c4m_vtable_t *
c4m_vtable(c4m_obj_t user_object)
{
    return (c4m_vtable_t *)c4m_object_type_info(user_object)->vtable;
}

static inline uint64_t
c4m_base_type(c4m_obj_t user_object)
{
    return c4m_get_my_type(user_object)->base_index;
}

static inline c4m_utf8_t *
c4m_base_type_name(c4m_obj_t user_object)
{
    return c4m_new_utf8((char *)c4m_object_type_info(user_object)->name);
}

// The first 2 words are pointers, but the first one is static.
#define C4M_HEADER_SCAN_CONST 0x02ULL

static inline void
c4m_set_bit(uint64_t *bitfield, int ix)
{
    int word = ix / 64;
    int bit  = ix % 64;

    bitfield[word] |= (1ULL << bit);
}

static inline void
c4m_mark_address(uint64_t *bitfield, void *base, void *address)
{
    c4m_set_bit(bitfield, c4m_ptr_diff(base, address));
}

static inline void
c4m_restore(c4m_obj_t user_object)
{
    c4m_vtable_t  *vt = c4m_vtable(user_object);
    c4m_restore_fn fn = (void *)vt->methods[C4M_BI_RESTORE];

    if (fn != NULL) {
        (*fn)(user_object);
    }
}

extern void c4m_scan_header_only(uint64_t *, int);

#if defined(C4M_GC_STATS) || defined(C4M_DEBUG)
extern c4m_obj_t _c4m_new(char *, int, c4m_type_t *, ...);

#define c4m_new(tid, ...) _c4m_new(__FILE__, __LINE__, tid, C4M_VA(__VA_ARGS__))
#else
extern c4m_obj_t _c4m_new(c4m_type_t *type, ...);

#define c4m_new(tid, ...) _c4m_new(tid, C4M_VA(__VA_ARGS__))
#endif

extern c4m_str_t  *c4m_repr(void *, c4m_type_t *);
extern c4m_str_t  *c4m_to_str(void *, c4m_type_t *);
extern bool        c4m_can_coerce(c4m_type_t *, c4m_type_t *);
extern c4m_obj_t   c4m_coerce(void *, c4m_type_t *, c4m_type_t *);
extern c4m_obj_t   c4m_coerce_object(const c4m_obj_t, c4m_type_t *);
extern c4m_obj_t   c4m_copy(c4m_obj_t);
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
                                         c4m_list_t *,
                                         c4m_utf8_t *);
extern void        c4m_finalize_allocation(void *);
extern c4m_obj_t   c4m_shallow(c4m_obj_t);

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
extern const c4m_vtable_t c4m_flexarray_vtable;
extern const c4m_vtable_t c4m_queue_vtable;
extern const c4m_vtable_t c4m_ring_vtable;
extern const c4m_vtable_t c4m_logring_vtable;
extern const c4m_vtable_t c4m_stack_vtable;
extern const c4m_vtable_t c4m_dict_vtable;
extern const c4m_vtable_t c4m_set_vtable;
extern const c4m_vtable_t c4m_list_vtable;
extern const c4m_vtable_t c4m_sha_vtable;
extern const c4m_vtable_t c4m_render_style_vtable;
extern const c4m_vtable_t c4m_exception_vtable;
extern const c4m_vtable_t c4m_type_spec_vtable;
extern const c4m_vtable_t c4m_tree_vtable;
extern const c4m_vtable_t c4m_tuple_vtable;
extern const c4m_vtable_t c4m_mixed_vtable;
extern const c4m_vtable_t c4m_ipaddr_vtable;
extern const c4m_vtable_t c4m_stream_vtable;
extern const c4m_vtable_t c4m_vm_vtable;
extern const c4m_vtable_t c4m_parse_node_vtable;
extern const c4m_vtable_t c4m_callback_vtable;
extern const c4m_vtable_t c4m_flags_vtable;
extern const c4m_vtable_t c4m_box_vtable;
extern const c4m_vtable_t c4m_basic_http_vtable;
extern const c4m_vtable_t c4m_datetime_vtable;
extern const c4m_vtable_t c4m_date_vtable;
extern const c4m_vtable_t c4m_time_vtable;
extern const c4m_vtable_t c4m_size_vtable;
extern const c4m_vtable_t c4m_duration_vtable;

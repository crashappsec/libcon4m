#pragma once

#include "con4m.h"

extern type_spec_t *c4m_resolve_type_aliases(type_spec_t *, type_env_t *);
extern bool         c4m_tspec_is_concrete(type_spec_t *);
extern type_spec_t *c4m_tspec_copy(type_spec_t *node, type_env_t *env);
extern type_spec_t *c4m_get_builtin_type(c4m_builtin_t);
extern type_spec_t *c4m_unify(type_spec_t *, type_spec_t *, type_env_t *);
extern type_spec_t *c4m_lookup_tspec(type_t tid, type_env_t *env);
extern type_spec_t *c4m_tspec_list(type_spec_t *);
extern type_spec_t *c4m_tspec_xlist(type_spec_t *);
extern type_spec_t *c4m_tspec_tree(type_spec_t *);
extern type_spec_t *c4m_tspec_queue(type_spec_t *);
extern type_spec_t *c4m_tspec_ring(type_spec_t *);
extern type_spec_t *c4m_tspec_stack(type_spec_t *);
extern type_spec_t *c4m_tspec_dict(type_spec_t *, type_spec_t *);
extern type_spec_t *c4m_tspec_set(type_spec_t *);
extern type_spec_t *c4m_tspec_tuple(int64_t, ...);
extern type_spec_t *c4m_tspec_fn(type_spec_t *, xlist_t *, bool);
extern type_spec_t *c4m_tspec_varargs_fn_va(type_spec_t *, int64_t, ...);
extern type_spec_t *c4m_tspec_varargs_fn(type_spec_t *, int64_t, ...);
extern type_spec_t *c4m_global_resolve_type(type_spec_t *);
extern type_spec_t *c4m_global_copy(type_spec_t *);
extern type_spec_t *c4m_global_type_check(type_spec_t *, type_spec_t *);
extern void         c4m_lock_type(type_spec_t *);
extern type_spec_t *c4m_get_promotion_type(type_spec_t *, type_spec_t *, int *);
extern void         c4m_initialize_global_types();
extern type_t       c4m_type_hash(type_spec_t *node, type_env_t *env);

static inline c4m_dt_kind_t
c4m_tspec_get_base(type_spec_t *n)
{
    return n->details->base_type->dt_kind;
}

static inline c4m_dt_kind_t
c4m_tspec_get_type_kind(type_spec_t *n)
{
    return n->details->base_type->dt_kind;
}

static inline xlist_t *
c4m_tspec_get_params(type_spec_t *n)
{
    return n->details->items;
}

static inline int
c4m_tspec_get_num_params(type_spec_t *n)
{
    return c4m_xlist_len(n->details->items);
}

static inline bool
c4m_tspec_is_error(type_spec_t *n)
{
    return n->typeid == C4M_T_ERROR;
}

static inline bool
c4m_tspec_is_locked(type_spec_t *t)
{
    return t->details->flags & FN_TY_LOCK;
}

static inline void
c4m_tspec_lock(type_spec_t *t)
{
    t->details->flags |= FN_TY_LOCK;
}

static inline void
c4m_tspec_unlock(type_spec_t *t)
{
    t->details->flags &= ~FN_TY_LOCK;
}

static inline type_t
c4m_tenv_next_tid(type_env_t *env)
{
    return atomic_fetch_add(&env->next_tid, 1);
}

extern type_env_t *c4m_global_type_env;

static inline type_spec_t *
c4m_merge_types(type_spec_t *t1, type_spec_t *t2)
{
    return c4m_unify(t1, t2, c4m_global_type_env);
}

static inline xlist_t *
c4m_tspec_get_parameters(type_spec_t *t)
{
    return t->details->items;
}

static inline type_spec_t *
c4m_tspec_get_param(type_spec_t *t, int i)
{
    return c4m_xlist_get(t->details->items, i, NULL);
}

static inline c4m_dt_info_t *
c4m_tspec_get_data_type_info(type_spec_t *t)
{
    return t->details->base_type;
}

static inline type_spec_t *
c4m_get_my_type(const object_t user_object)
{
    c4m_obj_t *hdr = c4m_object_header(user_object);

    return hdr->concrete_type;
}

static inline int64_t
c4m_get_base_type_id(const object_t obj)
{
    return c4m_tspec_get_data_type_info(c4m_get_my_type(obj))->typeid;
}

static inline c4m_builtin_t
c4m_tspec_get_base_tid(type_spec_t *n)
{
    return n->details->base_type->typeid;
}

extern type_spec_t *c4m_bi_types[C4M_NUM_BUILTIN_DTS];

static inline type_spec_t *
c4m_tspec_error()
{
    return c4m_bi_types[C4M_T_ERROR];
}

static inline type_spec_t *
c4m_tspec_void()
{
    return c4m_bi_types[C4M_T_VOID];
}

static inline type_spec_t *
c4m_tspec_bool()
{
    return c4m_bi_types[C4M_T_BOOL];
}

static inline type_spec_t *
c4m_tspec_i8()
{
    return c4m_bi_types[C4M_T_I8];
}

static inline type_spec_t *
c4m_tspec_u8()
{
    return c4m_bi_types[C4M_T_BYTE];
}

static inline type_spec_t *
c4m_tspec_byte()
{
    return c4m_bi_types[C4M_T_BYTE];
}

static inline type_spec_t *
c4m_tspec_i32()
{
    return c4m_bi_types[C4M_T_I32];
}

static inline type_spec_t *
c4m_tspec_u32()
{
    return c4m_bi_types[C4M_T_CHAR];
}

static inline type_spec_t *
c4m_tspec_char()
{
    return c4m_bi_types[C4M_T_CHAR];
}

static inline type_spec_t *
c4m_tspec_i64()
{
    return c4m_bi_types[C4M_T_INT];
}

static inline type_spec_t *
c4m_tspec_int()
{
    return c4m_bi_types[C4M_T_INT];
}

static inline type_spec_t *
c4m_tspec_u64()
{
    return c4m_bi_types[C4M_T_UINT];
}

static inline type_spec_t *
c4m_tspec_uint()
{
    return c4m_bi_types[C4M_T_UINT];
}

static inline type_spec_t *
c4m_tspec_f32()
{
    return c4m_bi_types[C4M_T_F32];
}

static inline type_spec_t *
c4m_tspec_f64()
{
    return c4m_bi_types[C4M_T_F64];
}

static inline type_spec_t *
c4m_tspec_float()
{
    return c4m_bi_types[C4M_T_F64];
}

static inline type_spec_t *
c4m_tspec_utf8()
{
    return c4m_bi_types[C4M_T_UTF8];
}

static inline type_spec_t *
c4m_tspec_buffer()
{
    return c4m_bi_types[C4M_T_BUFFER];
}

static inline type_spec_t *
c4m_tspec_utf32()
{
    return c4m_bi_types[C4M_T_UTF32];
}

static inline type_spec_t *
c4m_tspec_grid()
{
    return c4m_bi_types[C4M_T_GRID];
}

static inline type_spec_t *
c4m_tspec_typespec()
{
    return c4m_bi_types[C4M_T_TYPESPEC];
}

static inline type_spec_t *
c4m_tspec_ipv4()
{
    return c4m_bi_types[C4M_T_IPV4];
}

static inline type_spec_t *
c4m_tspec_ipv6()
{
    return c4m_bi_types[C4M_T_IPV6];
}

static inline type_spec_t *
c4m_tspec_duration()
{
    return c4m_bi_types[C4M_T_DURATION];
}

static inline type_spec_t *
c4m_tspec_size()
{
    return c4m_bi_types[C4M_T_SIZE];
}

static inline type_spec_t *
c4m_tspec_datetime()
{
    return c4m_bi_types[C4M_T_DATETIME];
}

static inline type_spec_t *
c4m_tspec_date()
{
    return c4m_bi_types[C4M_T_DATE];
}

static inline type_spec_t *
c4m_tspec_time()
{
    return c4m_bi_types[C4M_T_TIME];
}

static inline type_spec_t *
c4m_tspec_url()
{
    return c4m_bi_types[C4M_T_URL];
}

static inline type_spec_t *
c4m_tspec_callback()
{
    return c4m_bi_types[C4M_T_CALLBACK];
}

static inline type_spec_t *
c4m_tspec_renderable()
{
    return c4m_bi_types[C4M_T_RENDERABLE];
}

static inline type_spec_t *
c4m_tspec_render_style()
{
    return c4m_bi_types[C4M_T_RENDER_STYLE];
}

static inline type_spec_t *
c4m_tspec_hash()
{
    return c4m_bi_types[C4M_T_SHA];
}

static inline type_spec_t *
c4m_tspec_exception()
{
    return c4m_bi_types[C4M_T_EXCEPTION];
}

static inline type_spec_t *
c4m_tspec_type_env()
{
    return c4m_bi_types[C4M_T_TYPE_ENV];
}

static inline type_spec_t *
c4m_tspec_logring()
{
    return c4m_bi_types[C4M_T_LOGRING];
}

static inline type_spec_t *
c4m_tspec_mixed()
{
    return c4m_bi_types[C4M_T_GENERIC];
}

static inline type_spec_t *
c4m_tspec_ref()
{
    return c4m_bi_types[C4M_T_REF];
}

static inline type_spec_t *
c4m_tspec_stream()
{
    return c4m_bi_types[C4M_T_STREAM];
}

static inline type_spec_t *
c4m_tspec_kargs()
{
    return c4m_bi_types[C4M_T_KEYWORD];
}

static inline type_spec_t *
c4m_new_typevar(type_env_t *env)
{
    type_spec_t *result = c4m_new(c4m_tspec_typespec(), env, C4M_T_GENERIC);

    return result;
}

static inline type_spec_t *
c4m_tspec_typevar()
{
    return c4m_new_typevar(c4m_global_type_env);
}

static inline bool
c4m_tspecs_are_compat(type_spec_t *t1, type_spec_t *t2)
{
    t1 = c4m_global_copy(t1);
    t2 = c4m_global_copy(t2);

    return !c4m_tspec_is_error(c4m_global_type_check(t1, t2));
}

static inline type_spec_t *
c4m_tspec_tuple_from_xlist(xlist_t *item_types)
{
    type_spec_t *result   = c4m_new(c4m_tspec_typespec(),
                                  c4m_global_type_env,
                                  C4M_T_TUPLE);
    xlist_t     *res_list = result->details->items;

    int n = c4m_xlist_len(item_types);

    for (int i = 0; i < n; i++) {
        c4m_xlist_append(res_list, c4m_xlist_get(item_types, i, NULL));
    }

    return result;
}

static inline bool
c4m_tspec_is_int_type(type_spec_t *t)
{
    if (t == NULL) {
        return false;
    }

    switch (t->typeid) {
    case C4M_T_I8:
    case C4M_T_BYTE:
    case C4M_T_I32:
    case C4M_T_CHAR:
    case C4M_T_U32:
    case C4M_T_INT:
    case C4M_T_UINT:
        return true;
    default:
        return false;
    }
}

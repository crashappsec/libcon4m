#pragma once

#include "con4m.h"

extern c4m_type_t     *c4m_resolve_type_aliases(c4m_type_t *, c4m_type_env_t *);
extern bool            c4m_tspec_is_concrete(c4m_type_t *);
extern c4m_type_t     *c4m_tspec_copy(c4m_type_t *, c4m_type_env_t *);
extern c4m_type_t     *c4m_get_builtin_type(c4m_builtin_t);
extern c4m_type_t     *c4m_unify(c4m_type_t *, c4m_type_t *, c4m_type_env_t *);
extern c4m_type_t     *c4m_lookup_tspec(c4m_type_hash_t, c4m_type_env_t *);
extern c4m_type_t     *c4m_tspec_list(c4m_type_t *);
extern c4m_type_t     *c4m_tspec_xlist(c4m_type_t *);
extern c4m_type_t     *c4m_tspec_tree(c4m_type_t *);
extern c4m_type_t     *c4m_tspec_queue(c4m_type_t *);
extern c4m_type_t     *c4m_tspec_ring(c4m_type_t *);
extern c4m_type_t     *c4m_tspec_stack(c4m_type_t *);
extern c4m_type_t     *c4m_tspec_box(c4m_type_t *);
extern c4m_type_t     *c4m_tspec_dict(c4m_type_t *, c4m_type_t *);
extern c4m_type_t     *c4m_tspec_set(c4m_type_t *);
extern c4m_type_t     *c4m_tspec_tuple(int64_t, ...);
extern c4m_type_t     *c4m_tspec_tuple_from_xlist(c4m_xlist_t *);
extern c4m_type_t     *c4m_tspec_fn(c4m_type_t *, c4m_xlist_t *, bool);
extern c4m_type_t     *c4m_tspec_fn_va(c4m_type_t *, int64_t, ...);
extern c4m_type_t     *c4m_tspec_varargs_fn(c4m_type_t *, int64_t, ...);
extern c4m_type_t     *c4m_global_resolve_type(c4m_type_t *);
extern c4m_type_t     *c4m_global_copy(c4m_type_t *);
extern c4m_type_t     *c4m_global_type_check(c4m_type_t *, c4m_type_t *);
extern c4m_type_t     *c4m_resolve_and_unbox(c4m_type_t *);
extern void            c4m_lock_type(c4m_type_t *);
extern c4m_type_t     *c4m_get_promotion_type(c4m_type_t *,
                                              c4m_type_t *,
                                              int *);
extern void            c4m_initialize_global_types();
extern c4m_type_hash_t c4m_type_hash(c4m_type_t *node, c4m_type_env_t *env);

extern uint64_t *c4m_get_list_bitfield();
extern uint64_t *c4m_get_dict_bitfield();
extern uint64_t *c4m_get_set_bitfield();
extern uint64_t *c4m_get_tuple_bitfield();
extern uint64_t *c4m_get_all_containers_bitfield();
extern uint64_t *c4m_get_no_containers_bitfield();
extern int       c4m_get_num_bitfield_words();
extern bool      c4m_partial_inference(c4m_type_t *);
extern bool      c4m_list_syntax_possible(c4m_type_t *);
extern bool      c4m_dict_syntax_possible(c4m_type_t *);
extern bool      c4m_set_syntax_possible(c4m_type_t *);
extern bool      c4m_tuple_syntax_possible(c4m_type_t *);
extern void      c4m_remove_list_options(c4m_type_t *);
extern void      c4m_remove_dict_options(c4m_type_t *);
extern void      c4m_remove_set_options(c4m_type_t *);
extern void      c4m_remove_tuple_options(c4m_type_t *);
extern bool      c4m_type_has_list_syntax(c4m_type_t *);
extern bool      c4m_type_has_dict_syntax(c4m_type_t *);
extern bool      c4m_type_has_set_syntax(c4m_type_t *);
extern bool      c4m_type_has_tuple_syntax(c4m_type_t *);

static inline void
c4m_remove_all_container_options(c4m_type_t *t)
{
    c4m_remove_list_options(t);
    c4m_remove_dict_options(t);
    c4m_remove_set_options(t);
    c4m_remove_tuple_options(t);
}

extern c4m_type_env_t *c4m_global_type_env;

extern c4m_type_exact_result_t
c4m_type_cmp_exact_env(c4m_type_t *,
                       c4m_type_t *,
                       c4m_type_env_t *);

static inline c4m_type_exact_result_t
c4m_type_cmp_exact(c4m_type_t *t1, c4m_type_t *t2)
{
    return c4m_type_cmp_exact_env(t1, t2, c4m_global_type_env);
}

static inline c4m_dt_kind_t
c4m_tspec_get_base(c4m_type_t *n)
{
    return c4m_global_resolve_type(n)->details->base_type->dt_kind;
}

static inline c4m_xlist_t *
c4m_tspec_get_params(c4m_type_t *n)
{
    return c4m_global_resolve_type(n)->details->items;
}

static inline int
c4m_tspec_get_num_params(c4m_type_t *n)
{
    return c4m_xlist_len(c4m_global_resolve_type(n)->details->items);
}

static inline bool
c4m_tspec_is_bool(c4m_type_t *n)
{
    return c4m_global_resolve_type(n)->typeid == C4M_T_BOOL;
}

static inline bool
c4m_tspec_is_error(c4m_type_t *n)
{
    return c4m_global_resolve_type(n)->typeid == C4M_T_ERROR;
}

static inline bool
c4m_tspec_is_locked(c4m_type_t *t)
{
    return c4m_global_resolve_type(t)->details->flags & C4M_FN_TY_LOCK;
}

static inline void
c4m_tspec_lock(c4m_type_t *t)
{
    c4m_global_resolve_type(t)->details->flags |= C4M_FN_TY_LOCK;
}

static inline void
c4m_tspec_unlock(c4m_type_t *t)
{
    t->details->flags &= ~C4M_FN_TY_LOCK;
}

static inline c4m_type_hash_t
c4m_tenv_next_tid(c4m_type_env_t *env)
{
    return atomic_fetch_add(&env->next_tid, 1);
}

static inline c4m_type_t *
c4m_merge_types(c4m_type_t *t1, c4m_type_t *t2)
{
    return c4m_unify(t1, t2, c4m_global_type_env);
}

static inline c4m_xlist_t *
c4m_tspec_get_parameters(c4m_type_t *t)
{
    return c4m_global_resolve_type(t)->details->items;
}

static inline c4m_type_t *
c4m_tspec_get_param(c4m_type_t *t, int i)
{
    return c4m_xlist_get(t->details->items, i, NULL);
}

static inline c4m_dt_info_t *
c4m_tspec_get_data_type_info(c4m_type_t *t)
{
    t = c4m_global_resolve_type(t);

    return t->details->base_type;
}

static inline c4m_type_t *
c4m_get_my_type(const c4m_obj_t user_object)
{
    c4m_base_obj_t *hdr = c4m_object_header(user_object);

    return hdr->concrete_type;
}

static inline int64_t
c4m_get_base_type_id(const c4m_obj_t obj)
{
    return c4m_tspec_get_data_type_info(c4m_get_my_type(obj))->typeid;
}

static inline c4m_builtin_t
c4m_tspec_get_base_tid(c4m_type_t *n)
{
    return n->details->base_type->typeid;
}

extern c4m_type_t *c4m_bi_types[C4M_NUM_BUILTIN_DTS];

static inline c4m_type_t *
c4m_tspec_error()
{
    return c4m_bi_types[C4M_T_ERROR];
}

static inline c4m_type_t *
c4m_tspec_void()
{
    return c4m_bi_types[C4M_T_VOID];
}

static inline c4m_type_t *
c4m_tspec_bool()
{
    return c4m_bi_types[C4M_T_BOOL];
}

static inline c4m_type_t *
c4m_tspec_i8()
{
    return c4m_bi_types[C4M_T_I8];
}

static inline c4m_type_t *
c4m_tspec_u8()
{
    return c4m_bi_types[C4M_T_BYTE];
}

static inline c4m_type_t *
c4m_tspec_byte()
{
    return c4m_bi_types[C4M_T_BYTE];
}

static inline c4m_type_t *
c4m_tspec_i32()
{
    return c4m_bi_types[C4M_T_I32];
}

static inline c4m_type_t *
c4m_tspec_u32()
{
    return c4m_bi_types[C4M_T_CHAR];
}

static inline c4m_type_t *
c4m_tspec_char()
{
    return c4m_bi_types[C4M_T_CHAR];
}

static inline c4m_type_t *
c4m_tspec_i64()
{
    return c4m_bi_types[C4M_T_INT];
}

static inline c4m_type_t *
c4m_tspec_int()
{
    return c4m_bi_types[C4M_T_INT];
}

static inline c4m_type_t *
c4m_tspec_u64()
{
    return c4m_bi_types[C4M_T_UINT];
}

static inline c4m_type_t *
c4m_tspec_uint()
{
    return c4m_bi_types[C4M_T_UINT];
}

static inline c4m_type_t *
c4m_tspec_f32()
{
    return c4m_bi_types[C4M_T_F32];
}

static inline c4m_type_t *
c4m_tspec_f64()
{
    return c4m_bi_types[C4M_T_F64];
}

static inline c4m_type_t *
c4m_tspec_float()
{
    return c4m_bi_types[C4M_T_F64];
}

static inline c4m_type_t *
c4m_tspec_utf8()
{
    return c4m_bi_types[C4M_T_UTF8];
}

static inline c4m_type_t *
c4m_tspec_buffer()
{
    return c4m_bi_types[C4M_T_BUFFER];
}

static inline c4m_type_t *
c4m_tspec_utf32()
{
    return c4m_bi_types[C4M_T_UTF32];
}

static inline c4m_type_t *
c4m_tspec_grid()
{
    return c4m_bi_types[C4M_T_GRID];
}

static inline c4m_type_t *
c4m_tspec_typespec()
{
    return c4m_bi_types[C4M_T_TYPESPEC];
}

static inline c4m_type_t *
c4m_tspec_ipv4()
{
    return c4m_bi_types[C4M_T_IPV4];
}

static inline c4m_type_t *
c4m_tspec_ipv6()
{
    return c4m_bi_types[C4M_T_IPV6];
}

static inline c4m_type_t *
c4m_tspec_duration()
{
    return c4m_bi_types[C4M_T_DURATION];
}

static inline c4m_type_t *
c4m_tspec_size()
{
    return c4m_bi_types[C4M_T_SIZE];
}

static inline c4m_type_t *
c4m_tspec_datetime()
{
    return c4m_bi_types[C4M_T_DATETIME];
}

static inline c4m_type_t *
c4m_tspec_date()
{
    return c4m_bi_types[C4M_T_DATE];
}

static inline c4m_type_t *
c4m_tspec_time()
{
    return c4m_bi_types[C4M_T_TIME];
}

static inline c4m_type_t *
c4m_tspec_url()
{
    return c4m_bi_types[C4M_T_URL];
}

static inline c4m_type_t *
c4m_tspec_flags()
{
    return c4m_bi_types[C4M_T_FLAGS];
}

static inline c4m_type_t *
c4m_tspec_callback()
{
    return c4m_bi_types[C4M_T_CALLBACK];
}

static inline c4m_type_t *
c4m_tspec_renderable()
{
    return c4m_bi_types[C4M_T_RENDERABLE];
}

static inline c4m_type_t *
c4m_tspec_render_style()
{
    return c4m_bi_types[C4M_T_RENDER_STYLE];
}

static inline c4m_type_t *
c4m_tspec_hash()
{
    return c4m_bi_types[C4M_T_SHA];
}

static inline c4m_type_t *
c4m_tspec_exception()
{
    return c4m_bi_types[C4M_T_EXCEPTION];
}

static inline c4m_type_t *
c4m_tspec_type_env()
{
    return c4m_bi_types[C4M_T_TYPE_ENV];
}

static inline c4m_type_t *
c4m_tspec_logring()
{
    return c4m_bi_types[C4M_T_LOGRING];
}

static inline c4m_type_t *
c4m_tspec_mixed()
{
    return c4m_bi_types[C4M_T_GENERIC];
}

static inline c4m_type_t *
c4m_tspec_ref()
{
    return c4m_bi_types[C4M_T_REF];
}

static inline c4m_type_t *
c4m_tspec_stream()
{
    return c4m_bi_types[C4M_T_STREAM];
}

static inline c4m_type_t *
c4m_tspec_kargs()
{
    return c4m_bi_types[C4M_T_KEYWORD];
}

static inline c4m_type_t *
c4m_tspec_parse_node()
{
    return c4m_bi_types[C4M_T_PARSE_NODE];
}

static inline c4m_type_t *
c4m_tspec_bit()
{
    return c4m_bi_types[C4M_T_BIT];
}

static inline c4m_type_t *
c4m_new_typevar(c4m_type_env_t *env)
{
    c4m_type_t   *result = c4m_new(c4m_tspec_typespec(), env, C4M_T_GENERIC);
    tv_options_t *tsi    = c4m_gc_alloc(tv_options_t);

    result->details->tsi   = tsi;
    tsi->container_options = c4m_get_all_containers_bitfield();
    result->details->items = c4m_new(c4m_tspec_xlist(c4m_tspec_typespec()));
    result->details->flags = C4M_FN_UNKNOWN_TV_LEN;

    return result;
}

static inline c4m_type_t *
c4m_tspec_typevar()
{
    return c4m_new_typevar(c4m_global_type_env);
}

static inline c4m_type_t *
c4m_tspec_any_list(c4m_type_t *item_type)
{
    c4m_type_t   *result = c4m_new(c4m_tspec_typespec(),
                                 c4m_global_type_env,
                                 C4M_T_GENERIC);
    tv_options_t *tsi    = c4m_gc_alloc(tv_options_t);

    result->details->tsi   = tsi;
    tsi->container_options = c4m_get_list_bitfield();
    result->details->items = c4m_new(c4m_tspec_xlist(c4m_tspec_typespec()));

    if (item_type == NULL) {
        item_type = c4m_tspec_typevar();
    }

    c4m_xlist_append(result->details->items, item_type);

    return result;
}

static inline c4m_type_t *
c4m_tspec_any_dict(c4m_type_t *key, c4m_type_t *value)
{
    c4m_type_t   *result = c4m_new(c4m_tspec_typespec(),
                                 c4m_global_type_env,
                                 C4M_T_GENERIC);
    tv_options_t *tsi    = c4m_gc_alloc(tv_options_t);

    result->details->tsi   = tsi;
    tsi->container_options = c4m_get_dict_bitfield();
    result->details->items = c4m_new(c4m_tspec_xlist(c4m_tspec_typespec()));

    if (key == NULL) {
        key = c4m_tspec_typevar();
    }

    if (value == NULL) {
        value = c4m_tspec_typevar();
    }

    c4m_xlist_append(result->details->items, key);
    c4m_xlist_append(result->details->items, value);

    return result;
}

static inline c4m_type_t *
c4m_tspec_any_set(c4m_type_t *item_type)
{
    c4m_type_t   *result = c4m_new(c4m_tspec_typespec(),
                                 c4m_global_type_env,
                                 C4M_T_GENERIC);
    tv_options_t *tsi    = c4m_gc_alloc(tv_options_t);

    result->details->tsi   = tsi;
    tsi->container_options = c4m_get_set_bitfield();
    result->details->items = c4m_new(c4m_tspec_xlist(c4m_tspec_typespec()));

    if (item_type == NULL) {
        item_type = c4m_tspec_typevar();
    }

    c4m_xlist_append(result->details->items, item_type);

    return result;
}

static inline bool
c4m_tspecs_are_compat(c4m_type_t *t1, c4m_type_t *t2)
{
    t1 = c4m_global_copy(t1);
    t2 = c4m_global_copy(t2);

    return !c4m_tspec_is_error(c4m_global_type_check(t1, t2));
}

static inline bool
c4m_obj_type_check(const c4m_obj_t *obj, c4m_type_t *t2)
{
    return c4m_tspecs_are_compat(c4m_object_type(obj), t2);
}

static inline bool
c4m_tspec_is_box(c4m_type_t *t)
{
    t = c4m_global_resolve_type(t);
    return t->details->base_type->typeid == C4M_T_BOX;
}

static inline bool
c4m_tspec_is_int_type(c4m_type_t *t)
{
    if (t == NULL) {
        return false;
    }

    t = c4m_resolve_and_unbox(t);

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

static inline bool
c4m_tspec_is_signed(c4m_type_t *t)
{
    if (t == NULL) {
        return false;
    }

    t = c4m_global_resolve_type(t);

    switch (t->typeid) {
    case C4M_T_I8:
    case C4M_T_I32:
    case C4M_T_CHAR:
    case C4M_T_INT:
        return true;
    default:
        return false;
    }
}

static inline bool
c4m_tspec_is_tvar(c4m_type_t *t)
{
    t = c4m_global_resolve_type(t);
    return (c4m_tspec_get_base(t) == C4M_DT_KIND_type_var);
}

static inline bool
c4m_obj_is_int_type(const c4m_obj_t *obj)
{
    c4m_base_obj_t *base = (c4m_base_obj_t *)c4m_object_header(obj);

    return c4m_tspec_is_int_type(base->concrete_type);
}

static inline bool
c4m_type_is_value_type(c4m_type_t *t)
{
    // This should NOT unbox; check c4m_tspec_is_box() too if needed.
    t                 = c4m_global_resolve_type(t);
    c4m_dt_info_t *dt = c4m_tspec_get_data_type_info(t);

    return dt->by_value;
}

static inline bool
c4m_type_is_boxed_value_type(c4m_type_t *t)
{
    return c4m_type_is_value_type(c4m_resolve_and_unbox(t));
}

// Once we add objects, this will be a dynamic number.
static inline int
c4m_number_concrete_types()
{
    return C4M_NUM_BUILTIN_DTS;
}

static inline int
c4m_get_alloc_len(c4m_type_t *t)
{
    return c4m_tspec_get_data_type_info(t)->alloc_len;
}

#ifdef C4M_USE_INTERNAL_API
extern c4m_grid_t *c4m_format_global_type_environment();
extern void        c4m_clean_environment();

#ifdef C4M_TYPE_LOG
extern void type_log_on();
extern void type_log_off();
#else
#define type_log_on()
#define type_log_off()
#endif
#endif

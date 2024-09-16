#pragma once

#include "con4m.h"

extern c4m_type_t *c4m_type_resolve(c4m_type_t *);
extern bool        c4m_type_is_concrete(c4m_type_t *);
extern c4m_type_t *c4m_type_copy(c4m_type_t *);
extern c4m_type_t *c4m_get_builtin_type(c4m_builtin_t);
extern c4m_type_t *c4m_unify(c4m_type_t *, c4m_type_t *);

#if defined(C4M_GC_STATS) || defined(C4M_DEBUG)
extern c4m_type_t *_c4m_type_flist(c4m_type_t *, char *, int);
extern c4m_type_t *_c4m_type_list(c4m_type_t *, char *, int);
extern c4m_type_t *_c4m_type_tree(c4m_type_t *, char *, int);
extern c4m_type_t *_c4m_type_queue(c4m_type_t *, char *, int);
extern c4m_type_t *_c4m_type_ring(c4m_type_t *, char *, int);
extern c4m_type_t *_c4m_type_stack(c4m_type_t *, char *, int);
extern c4m_type_t *_c4m_type_set(c4m_type_t *, char *, int);
#define c4m_type_flist(x) _c4m_type_flist(x, __FILE__, __LINE__)
#define c4m_type_list(x)  _c4m_type_list(x, __FILE__, __LINE__)
#define c4m_type_tree(x)  _c4m_type_tree(x, __FILE__, __LINE__)
#define c4m_type_queue(x) _c4m_type_queue(x, __FILE__, __LINE__)
#define c4m_type_ring(x)  _c4m_type_ring(x, __FILE__, __LINE__)
#define c4m_type_stack(x) _c4m_type_stack(x, __FILE__, __LINE__)
#define c4m_type_set(x)   _c4m_type_set(x, __FILE__, __LINE__)
#else
extern c4m_type_t *c4m_type_flist(c4m_type_t *);
extern c4m_type_t *c4m_type_list(c4m_type_t *);
extern c4m_type_t *c4m_type_tree(c4m_type_t *);
extern c4m_type_t *c4m_type_queue(c4m_type_t *);
extern c4m_type_t *c4m_type_ring(c4m_type_t *);
extern c4m_type_t *c4m_type_stack(c4m_type_t *);
extern c4m_type_t *c4m_type_set(c4m_type_t *);
#endif
extern c4m_type_t *c4m_type_box(c4m_type_t *);
extern c4m_type_t *c4m_type_dict(c4m_type_t *, c4m_type_t *);

extern c4m_type_t     *c4m_type_tuple(int64_t, ...);
extern c4m_type_t     *c4m_type_tuple_from_xlist(c4m_list_t *);
extern c4m_type_t     *c4m_type_fn(c4m_type_t *, c4m_list_t *, bool);
extern c4m_type_t     *c4m_type_fn_va(c4m_type_t *, int64_t, ...);
extern c4m_type_t     *c4m_type_varargs_fn(c4m_type_t *, int64_t, ...);
extern void            c4m_lock_type(c4m_type_t *);
extern c4m_type_t     *c4m_get_promotion_type(c4m_type_t *,
                                              c4m_type_t *,
                                              int *);
extern c4m_type_t     *c4m_new_typevar();
extern void            c4m_initialize_global_types();
extern c4m_type_hash_t c4m_calculate_type_hash(c4m_type_t *node);
extern uint64_t       *c4m_get_list_bitfield();
extern uint64_t       *c4m_get_dict_bitfield();
extern uint64_t       *c4m_get_set_bitfield();
extern uint64_t       *c4m_get_tuple_bitfield();
extern uint64_t       *c4m_get_all_containers_bitfield();
extern uint64_t       *c4m_get_no_containers_bitfield();
extern int             c4m_get_num_bitfield_words();
extern bool            c4m_partial_inference(c4m_type_t *);
extern bool            c4m_list_syntax_possible(c4m_type_t *);
extern bool            c4m_dict_syntax_possible(c4m_type_t *);
extern bool            c4m_set_syntax_possible(c4m_type_t *);
extern bool            c4m_tuple_syntax_possible(c4m_type_t *);
extern void            c4m_remove_list_options(c4m_type_t *);
extern void            c4m_remove_dict_options(c4m_type_t *);
extern void            c4m_remove_set_options(c4m_type_t *);
extern void            c4m_remove_tuple_options(c4m_type_t *);
extern bool            c4m_type_has_list_syntax(c4m_type_t *);
extern bool            c4m_type_has_dict_syntax(c4m_type_t *);
extern bool            c4m_type_has_set_syntax(c4m_type_t *);
extern bool            c4m_type_has_tuple_syntax(c4m_type_t *);

static inline void
c4m_remove_all_container_options(c4m_type_t *t)
{
    c4m_remove_list_options(t);
    c4m_remove_dict_options(t);
    c4m_remove_set_options(t);
    c4m_remove_tuple_options(t);
}

extern c4m_type_exact_result_t
c4m_type_cmp_exact(c4m_type_t *, c4m_type_t *);

static inline c4m_dt_info_t *
c4m_internal_type_base(c4m_type_t *n)
{
    return (c4m_dt_info_t *)&c4m_base_type_info[n->base_index];
}

static inline char *
c4m_internal_type_name(c4m_type_t *n)
{
    return (char *)c4m_base_type_info[n->base_index].name;
}

static inline c4m_dt_kind_t
c4m_type_get_kind(c4m_type_t *n)
{
    return c4m_internal_type_base(c4m_type_resolve(n))->dt_kind;
}

static inline c4m_list_t *
c4m_type_get_params(c4m_type_t *n)
{
    return c4m_type_resolve(n)->items;
}

static inline int
c4m_type_get_num_params(c4m_type_t *n)
{
    return c4m_list_len(c4m_type_resolve(n)->items);
}

static inline bool
c4m_type_is_bool(c4m_type_t *n)
{
    return c4m_type_resolve(n)->typeid == C4M_T_BOOL;
}

static inline bool
c4m_type_is_ref(c4m_type_t *n)
{
    return c4m_type_resolve(n)->typeid == C4M_T_REF;
}

static inline bool
c4m_type_is_locked(c4m_type_t *t)
{
    return c4m_type_resolve(t)->flags & C4M_FN_TY_LOCK;
}

static inline bool
c4m_type_is_tuple(c4m_type_t *t)
{
    return c4m_type_get_kind(t) == C4M_DT_KIND_tuple;
}

static inline bool
c4m_type_is_list(c4m_type_t *t)
{
    return c4m_type_resolve(t)->base_index == C4M_T_LIST;
}

static inline bool
c4m_type_is_set(c4m_type_t *t)
{
    return c4m_type_resolve(t)->base_index == C4M_T_SET;
}

static inline void
c4m_type_lock(c4m_type_t *t)
{
    c4m_type_resolve(t)->flags |= C4M_FN_TY_LOCK;
}

static inline void
c4m_type_unlock(c4m_type_t *t)
{
    t->flags &= ~C4M_FN_TY_LOCK;
}

static inline c4m_type_t *
c4m_type_get_param(c4m_type_t *t, int i)
{
    if (t && t->items) {
        return c4m_list_get(t->items, i, NULL);
    }
    else {
        return NULL;
    }
}

static inline c4m_type_t *
c4m_type_get_last_param(c4m_type_t *n)
{
    return c4m_type_get_param(n, c4m_type_get_num_params(n) - 1);
}

static inline c4m_dt_info_t *
c4m_type_get_data_type_info(c4m_type_t *t)
{
    t = c4m_type_resolve(t);
    return c4m_internal_type_base(t);
}

static inline bool
c4m_type_is_mutable(c4m_type_t *t)
{
    return c4m_type_get_data_type_info(t)->mutable;
}

static inline int64_t
c4m_get_base_type_id(const c4m_obj_t obj)
{
    return c4m_get_my_type(obj)->base_index;
}

extern c4m_type_t *c4m_bi_types[C4M_NUM_BUILTIN_DTS];

static inline bool
c4m_type_is_error(c4m_type_t *n)
{
    return c4m_type_resolve(n)->typeid == C4M_T_ERROR;
}

static inline c4m_type_t *
c4m_type_error()
{
    return c4m_bi_types[C4M_T_ERROR];
}

static inline c4m_type_t *
c4m_type_void()
{
    return c4m_bi_types[C4M_T_VOID];
}

static inline c4m_type_t *
c4m_type_nil()
{
    return c4m_bi_types[C4M_T_NIL];
}

static inline c4m_type_t *
c4m_type_bool()
{
    return c4m_bi_types[C4M_T_BOOL];
}

static inline c4m_type_t *
c4m_type_i8()
{
    return c4m_bi_types[C4M_T_I8];
}

static inline c4m_type_t *
c4m_type_u8()
{
    return c4m_bi_types[C4M_T_BYTE];
}

static inline c4m_type_t *
c4m_type_byte()
{
    return c4m_bi_types[C4M_T_BYTE];
}

static inline c4m_type_t *
c4m_type_i32()
{
    return c4m_bi_types[C4M_T_I32];
}

static inline c4m_type_t *
c4m_type_u32()
{
    return c4m_bi_types[C4M_T_CHAR];
}

static inline c4m_type_t *
c4m_type_char()
{
    return c4m_bi_types[C4M_T_CHAR];
}

static inline c4m_type_t *
c4m_type_i64()
{
    return c4m_bi_types[C4M_T_INT];
}

static inline c4m_type_t *
c4m_type_int()
{
    return c4m_bi_types[C4M_T_INT];
}

static inline c4m_type_t *
c4m_type_u64()
{
    return c4m_bi_types[C4M_T_UINT];
}

static inline c4m_type_t *
c4m_type_uint()
{
    return c4m_bi_types[C4M_T_UINT];
}

static inline c4m_type_t *
c4m_type_f32()
{
    return c4m_bi_types[C4M_T_F32];
}

static inline c4m_type_t *
c4m_type_f64()
{
    return c4m_bi_types[C4M_T_F64];
}

static inline c4m_type_t *
c4m_type_float()
{
    return c4m_bi_types[C4M_T_F64];
}

static inline c4m_type_t *
c4m_type_utf8()
{
    return c4m_bi_types[C4M_T_UTF8];
}

static inline c4m_type_t *
c4m_type_buffer()
{
    return c4m_bi_types[C4M_T_BUFFER];
}

static inline c4m_type_t *
c4m_type_utf32()
{
    return c4m_bi_types[C4M_T_UTF32];
}

static inline c4m_type_t *
c4m_type_grid()
{
    return c4m_bi_types[C4M_T_GRID];
}

static inline c4m_type_t *
c4m_type_typespec()
{
    return c4m_bi_types[C4M_T_TYPESPEC];
}

static inline c4m_type_t *
c4m_type_ip()
{
    return c4m_bi_types[C4M_T_IPV4];
}

static inline c4m_type_t *
c4m_type_ipv4()
{
    return c4m_bi_types[C4M_T_IPV4];
}

static inline c4m_type_t *
c4m_type_duration()
{
    return c4m_bi_types[C4M_T_DURATION];
}

static inline c4m_type_t *
c4m_type_size()
{
    return c4m_bi_types[C4M_T_SIZE];
}

static inline c4m_type_t *
c4m_type_datetime()
{
    return c4m_bi_types[C4M_T_DATETIME];
}

static inline c4m_type_t *
c4m_type_date()
{
    return c4m_bi_types[C4M_T_DATE];
}

static inline c4m_type_t *
c4m_type_time()
{
    return c4m_bi_types[C4M_T_TIME];
}

static inline c4m_type_t *
c4m_type_url()
{
    return c4m_bi_types[C4M_T_URL];
}

static inline c4m_type_t *
c4m_type_flags()
{
    return c4m_bi_types[C4M_T_FLAGS];
}

static inline c4m_type_t *
c4m_type_callback()
{
    return c4m_bi_types[C4M_T_CALLBACK];
}

static inline c4m_type_t *
c4m_type_renderable()
{
    return c4m_bi_types[C4M_T_RENDERABLE];
}

static inline c4m_type_t *
c4m_type_render_style()
{
    return c4m_bi_types[C4M_T_RENDER_STYLE];
}

static inline c4m_type_t *
c4m_type_hash()
{
    return c4m_bi_types[C4M_T_SHA];
}

static inline c4m_type_t *
c4m_type_exception()
{
    return c4m_bi_types[C4M_T_EXCEPTION];
}

static inline c4m_type_t *
c4m_type_logring()
{
    return c4m_bi_types[C4M_T_LOGRING];
}

static inline c4m_type_t *
c4m_type_mixed()
{
    return c4m_bi_types[C4M_T_GENERIC];
}

static inline c4m_type_t *
c4m_type_ref()
{
    return c4m_bi_types[C4M_T_REF];
}

static inline c4m_type_t *
c4m_type_true_ref()
{
    return c4m_bi_types[C4M_T_TRUE_REF];
}

static inline c4m_type_t *
c4m_type_internal()
{
    return c4m_bi_types[C4M_T_INTERNAL];
}

static inline c4m_type_t *
c4m_type_stream()
{
    return c4m_bi_types[C4M_T_STREAM];
}

static inline c4m_type_t *
c4m_type_kargs()
{
    return c4m_bi_types[C4M_T_KEYWORD];
}

static inline c4m_type_t *
c4m_type_parse_node()
{
    return c4m_bi_types[C4M_T_PARSE_NODE];
}

static inline c4m_type_t *
c4m_type_bit()
{
    return c4m_bi_types[C4M_T_BIT];
}

static inline c4m_type_t *
c4m_type_http()
{
    return c4m_bi_types[C4M_T_HTTP];
}

static inline c4m_type_t *
c4m_type_parser()
{
    return c4m_bi_types[C4M_T_PARSER];
}

static inline c4m_type_t *
c4m_type_grammar()
{
    return c4m_bi_types[C4M_T_GRAMMAR];
}

static inline c4m_type_t *
c4m_type_terminal()
{
    return c4m_bi_types[C4M_T_TERMINAL];
}

static inline c4m_type_t *
c4m_type_ruleset()
{
    return c4m_bi_types[C4M_T_RULESET];
}

static inline c4m_type_t *
c4m_type_gopt_parser()
{
    return c4m_bi_types[C4M_T_GOPT_PARSER];
}

static inline c4m_type_t *
c4m_type_gopt_command()
{
    return c4m_bi_types[C4M_T_GOPT_COMMAND];
}

static inline c4m_type_t *
c4m_type_gopt_option()
{
    return c4m_bi_types[C4M_T_GOPT_OPTION];
}

static inline c4m_type_t *
c4m_merge_types(c4m_type_t *t1, c4m_type_t *t2, int *warning)
{
    c4m_type_t *result = c4m_unify(t1, t2);

    if (!c4m_type_is_error(result)) {
        return result;
    }

    if (warning != NULL) {
        return c4m_get_promotion_type(t1, t2, warning);
    }

    return c4m_type_error();
}

static inline c4m_type_t *
c4m_type_any_list(c4m_type_t *item_type)
{
    c4m_type_t *result                = c4m_new(c4m_type_typespec(),
                                 C4M_T_GENERIC);
    result->options.container_options = c4m_get_list_bitfield();
    result->items                     = c4m_list(c4m_type_typespec());

    if (item_type == NULL) {
        item_type = c4m_new_typevar();
    }

    c4m_list_append(result->items, item_type);

    return result;
}

static inline c4m_type_t *
c4m_type_any_dict(c4m_type_t *key, c4m_type_t *value)
{
    c4m_type_t *result = c4m_new(c4m_type_typespec(),
                                 C4M_T_GENERIC);

    result->options.container_options = c4m_get_dict_bitfield();
    result->items                     = c4m_new(c4m_type_list(c4m_type_typespec()));

    if (key == NULL) {
        key = c4m_new_typevar();
    }

    if (value == NULL) {
        value = c4m_new_typevar();
    }

    c4m_list_append(result->items, key);
    c4m_list_append(result->items, value);

    return result;
}

static inline c4m_type_t *
c4m_type_any_set(c4m_type_t *item_type)
{
    c4m_type_t *result                = c4m_new(c4m_type_typespec(),
                                 C4M_T_GENERIC);
    result->options.container_options = c4m_get_set_bitfield();
    result->items                     = c4m_new(c4m_type_list(c4m_type_typespec()));

    if (item_type == NULL) {
        item_type = c4m_new_typevar();
    }

    c4m_list_append(result->items, item_type);

    return result;
}

static inline bool
c4m_types_are_compat(c4m_type_t *t1, c4m_type_t *t2, int *warning)
{
    t1 = c4m_type_copy(t1);
    t2 = c4m_type_copy(t2);

    if (!c4m_type_is_error(c4m_unify(t1, t2))) {
        return true;
    }

    if (!warning) {
        return false;
    }

    if (!c4m_type_is_error(c4m_get_promotion_type(t1, t2, warning))) {
        return true;
    }

    return false;
}

static inline bool
c4m_obj_type_check(const c4m_obj_t *obj, c4m_type_t *t2, int *warn)
{
    return c4m_types_are_compat(c4m_get_my_type((c4m_type_t *)obj), t2, warn);
}

static inline bool
c4m_type_is_box(c4m_type_t *t)
{
    t = c4m_type_resolve(t);
    switch (t->base_index) {
    case C4M_T_BOX_BOOL:
    case C4M_T_BOX_I8:
    case C4M_T_BOX_BYTE:
    case C4M_T_BOX_I32:
    case C4M_T_BOX_CHAR:
    case C4M_T_BOX_U32:
    case C4M_T_BOX_INT:
    case C4M_T_BOX_UINT:
    case C4M_T_BOX_F32:
    case C4M_T_BOX_F64:
        return true;
    default:
        return false;
    }
}

static inline c4m_type_t *
c4m_type_unbox(c4m_type_t *t)
{
    switch (t->base_index) {
    case C4M_T_BOX_BOOL:
        return c4m_bi_types[C4M_T_BOOL];
    case C4M_T_BOX_I8:
        return c4m_bi_types[C4M_T_I8];
    case C4M_T_BOX_BYTE:
        return c4m_bi_types[C4M_T_BYTE];
    case C4M_T_BOX_I32:
        return c4m_bi_types[C4M_T_I32];
    case C4M_T_BOX_CHAR:
        return c4m_bi_types[C4M_T_CHAR];
    case C4M_T_BOX_U32:
        return c4m_bi_types[C4M_T_U32];
    case C4M_T_BOX_INT:
        return c4m_bi_types[C4M_T_INT];
    case C4M_T_BOX_UINT:
        return c4m_bi_types[C4M_T_UINT];
    case C4M_T_BOX_F32:
        return c4m_bi_types[C4M_T_F32];
    case C4M_T_BOX_F64:
        return c4m_bi_types[C4M_T_F64];
    default:
        return t;
    }
}

static inline bool
c4m_type_is_int_type(c4m_type_t *t)
{
    if (t == NULL) {
        return false;
    }

    t = c4m_type_unbox(c4m_type_resolve(t));

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
c4m_type_is_float_type(c4m_type_t *t)
{
    if (t == NULL) {
        return false;
    }

    t = c4m_type_unbox(c4m_type_resolve(t));

    switch (t->typeid) {
    case C4M_T_F64:
    case C4M_T_F32:
        return true;
    default:
        return false;
    }
}

static inline bool
c4m_type_is_signed(c4m_type_t *t)
{
    if (t == NULL) {
        return false;
    }

    t = c4m_type_unbox(c4m_type_resolve(t));

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
c4m_type_is_tvar(c4m_type_t *t)
{
    t = c4m_type_resolve(t);
    return (c4m_type_get_kind(t) == C4M_DT_KIND_type_var);
}

static inline bool
c4m_type_is_void(c4m_type_t *t)
{
    t = c4m_type_resolve(t);
    return t->typeid == C4M_T_VOID;
}

static inline bool
c4m_type_is_nil(c4m_type_t *t)
{
    t = c4m_type_resolve(t);
    return t->typeid == C4M_T_VOID;
}

static inline bool
c4m_type_is_string(c4m_type_t *t)
{
    t = c4m_type_resolve(t);
    return t->typeid == C4M_T_UTF8 || t->typeid == C4M_T_UTF32;
}

static inline bool
c4m_type_is_value_type(c4m_type_t *t)
{
    // This should NOT unbox; check c4m_type_is_box() too if needed.
    t                 = c4m_type_resolve(t);
    c4m_dt_info_t *dt = c4m_type_get_data_type_info(t);

    return dt->by_value;
}

static inline bool
c4m_obj_item_type_is_value(c4m_obj_t obj)
{
    c4m_type_t *t         = c4m_get_my_type(obj);
    c4m_type_t *item_type = c4m_type_get_param(t, 0);

    return c4m_type_is_value_type(item_type);
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
    return c4m_type_get_data_type_info(t)->alloc_len;
}

static inline c4m_type_t *
c4m_resolve_and_unbox(c4m_type_t *t)
{
    t = c4m_type_resolve(t);

    if (c4m_type_is_box(t)) {
        return c4m_type_unbox(t);
    }

    return t;
}

static inline bool
c4m_obj_is_int_type(const c4m_obj_t *obj)
{
    c4m_type_t *t = c4m_get_my_type((c4m_obj_t *)obj);

    return c4m_type_is_int_type(c4m_resolve_and_unbox(t));
}

static inline bool
c4m_type_requires_gc_scan(c4m_type_t *t)
{
    t                 = c4m_type_resolve(t);
    c4m_dt_info_t *dt = c4m_type_get_data_type_info(t);

    if (dt->by_value) {
        return false;
    }

    return true;
}

void c4m_set_next_typevar_fn(c4m_next_typevar_fn);

#ifdef C4M_USE_INTERNAL_API
extern c4m_grid_t *c4m_format_global_type_environment(c4m_type_universe_t *);
extern void        c4m_clean_environment();

#ifdef C4M_TYPE_LOG
extern void type_log_on();
extern void type_log_off();
#else
#define type_log_on()
#define type_log_off()
#endif
#endif

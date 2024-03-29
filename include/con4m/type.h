#pragma once

#include <con4m.h>


extern type_spec_t *resolve_type_aliases(type_spec_t *, type_env_t *);
extern bool type_spec_is_concrete(type_spec_t *);
extern type_spec_t *type_spec_copy(type_spec_t *node, type_env_t *env);
extern type_spec_t *get_builtin_type(con4m_builtin_t);
extern type_spec_t *unify(type_spec_t *, type_spec_t *, type_env_t *);
extern type_spec_t *lookup_type_spec(type_t tid, type_env_t *env);
extern type_spec_t *tspec_list(type_spec_t *);
extern type_spec_t *tspec_xlist(type_spec_t *);
extern type_spec_t *tspec_tree(type_spec_t *);
extern type_spec_t *tspec_queue(type_spec_t *);
extern type_spec_t *tspec_ring(type_spec_t *);
extern type_spec_t *tspec_stack(type_spec_t *);
extern type_spec_t *tspec_dict(type_spec_t *, type_spec_t *);
extern type_spec_t *tspec_set(type_spec_t *);
extern type_spec_t *tspec_tuple(int64_t, ...);
extern type_spec_t *tspec_fn(type_spec_t *, int64_t, ...);
extern type_spec_t *tspec_varargs_fn(type_spec_t *, int64_t, ...);
extern type_spec_t *global_resolve_type(type_spec_t *);
extern type_spec_t *global_copy(type_spec_t *);
extern type_spec_t *global_type_check(type_spec_t *, type_spec_t *);
extern void         lock_type(type_spec_t *);

static inline bool
typeid_is_concrete(type_t tid)
{
    return !(tid & (1ULL << 63));
}

static inline bool
typeid_is_generic(type_t tid)
{
    return !(typeid_is_concrete(tid));
}

static inline base_t
type_spec_get_base(type_spec_t *n)
{
    return n->details->base_type->base;
}

static inline xlist_t *
type_spec_get_params(type_spec_t *n)
{
    return n->details->items;
}

static inline int
type_spec_get_num_params(type_spec_t *n)
{
    return xlist_len(n->details->items);
}

static inline int
typeid_get_num_params(type_t tid, type_env_t *env)
{
    type_spec_t *t = lookup_type_spec(tid, env);
    return type_spec_get_num_params(t);
}

static inline type_t
typeid_get_param(type_t tid, int64_t ix, type_env_t *env)
{
    type_spec_t *n   = lookup_type_spec(tid, env);
    type_spec_t *kid = xlist_get(n->details->items, ix, NULL);

    if (kid == NULL) {
	return T_TYPE_ERROR;
    }

    return kid->typeid;
}

static inline bool
type_spec_is_error(type_spec_t *n)
{
    return n->typeid == T_TYPE_ERROR;
}

static inline bool
type_spec_is_locked(type_spec_t *t)
{
    return t->details->flags & FN_TY_LOCK;
}

static inline void
type_spec_lock(type_spec_t *t)
{
    t->details->flags |= FN_TY_LOCK;
}

static inline void
type_spec_unlock(type_spec_t *t)
{
    t->details->flags &= ~FN_TY_LOCK;
}

static inline type_t
tenv_next_tid(type_env_t *env)
{
    return atomic_fetch_add(&env->next_tid, 1);
}

extern type_env_t *global_type_env;

static inline type_spec_t *
merge_types(type_spec_t *t1, type_spec_t *t2)
{
    return unify(t1, t2, global_type_env);
}

static inline xlist_t *
tspec_get_parameters(type_spec_t *t)
{
    return t->details->items;
}

static inline type_spec_t *
tspec_get_param(type_spec_t *t, int i)
{
    return xlist_get(t->details->items, i, NULL);
}

static inline dt_info *
tspec_get_data_type_info(type_spec_t *t)
{
    return t->details->base_type;
}

static inline type_spec_t *
get_my_type(const object_t user_object)
{
    con4m_obj_t *hdr = get_object_header(user_object);

    return hdr->concrete_type;
}

static inline int64_t
get_base_type_id(const object_t obj)
{
    return tspec_get_data_type_info(get_my_type(obj))->typeid;
}

extern type_spec_t *builtin_types[CON4M_NUM_BUILTIN_DTS];

static inline type_spec_t *
tspec_error()
{
    return builtin_types[T_TYPE_ERROR];
}

static inline type_spec_t *
tspec_void()
{
    return builtin_types[T_VOID];
}

static inline type_spec_t *
tspec_bool()
{
    return builtin_types[T_BOOL];
}

static inline type_spec_t *
tspec_i8()
{
    return builtin_types[T_I8];
}

static inline type_spec_t *
tspec_u8()
{
    return builtin_types[T_BYTE];
}

static inline type_spec_t *
tspec_byte()
{
    return builtin_types[T_BYTE];
}

static inline type_spec_t *
tspec_i32()
{
    return builtin_types[T_I32];
}

static inline type_spec_t *
tspec_u32()
{
    return builtin_types[T_CHAR];
}

static inline type_spec_t *
tspec_char()
{
    return builtin_types[T_CHAR];
}

static inline type_spec_t *
tspec_i64()
{
    return builtin_types[T_INT];
}

static inline type_spec_t *
tspec_int()
{
    return builtin_types[T_INT];
}

static inline type_spec_t *
tspec_u64()
{
    return builtin_types[T_UINT];
}

static inline type_spec_t *
tspec_uint()
{
    return builtin_types[T_UINT];
}

static inline type_spec_t *
tspec_f32()
{
    return builtin_types[T_F32];
}

static inline type_spec_t *
tspec_f64()
{
    return builtin_types[T_F64];
}

static inline type_spec_t *
tspec_float()
{
    return builtin_types[T_F64];
}

static inline type_spec_t *
tspec_utf8()
{
    return builtin_types[T_UTF8];
}

static inline type_spec_t *
tspec_buffer()
{
    return builtin_types[T_BUFFER];
}

static inline type_spec_t *
tspec_utf32()
{
    return builtin_types[T_UTF32];
}

static inline type_spec_t *
tspec_grid()
{
    return builtin_types[T_GRID];
}

static inline type_spec_t *
tspec_typespec()
{
    return builtin_types[T_TYPESPEC];
}

static inline type_spec_t *
tspec_ipv4()
{
    return builtin_types[T_IPV4];
}

static inline type_spec_t *
tspec_ipv6()
{
    return builtin_types[T_IPV6];
}

static inline type_spec_t *
tspec_duration()
{
    return builtin_types[T_DURATION];
}

static inline type_spec_t *
tspec_size()
{
    return builtin_types[T_SIZE];
}

static inline type_spec_t *
tspec_datetime()
{
    return builtin_types[T_DATETIME];
}

static inline type_spec_t *
tspec_date()
{
    return builtin_types[T_DATE];
}

static inline type_spec_t *
tspec_time()
{
    return builtin_types[T_TIME];
}

static inline type_spec_t *
tspec_url()
{
    return builtin_types[T_URL];
}

static inline type_spec_t *
tspec_callback()
{
    return builtin_types[T_CALLBACK];
}

static inline type_spec_t *
tspec_renderable()
{
    return builtin_types[T_RENDERABLE];
}

static inline type_spec_t *
tspec_render_style()
{
    return builtin_types[T_RENDER_STYLE];
}

static inline type_spec_t *
tspec_hash()
{
    return builtin_types[T_SHA];
}

static inline type_spec_t *
tspec_exception()
{
    return builtin_types[T_EXCEPTION];
}

static inline type_spec_t *
tspec_type_env()
{
    return builtin_types[T_TYPE_ENV];
}

static inline type_spec_t *
tspec_logring()
{
    return builtin_types[T_LOGRING];
}

static inline type_spec_t *
tspec_mixed()
{
    return builtin_types[T_GENERIC];
}

static inline type_spec_t *
tspec_ref()
{
    return builtin_types[T_REF];
}

static inline type_spec_t *
tspec_stream()
{
    return builtin_types[T_STREAM];
}


static inline type_spec_t *
tspec_kargs()
{
    return builtin_types[T_KEYWORD];
}

static inline type_spec_t *
type_spec_new_typevar(type_env_t *env)
{
    type_spec_t *result = con4m_new(tspec_typespec(), env, T_GENERIC);

    return result;
}

static inline type_t
type_new_typevar(type_env_t *env)
{
    return type_spec_new_typevar(env)->typeid;
}

static inline type_spec_t *
tspec_typevar()
{
    return type_spec_new_typevar(global_type_env);
}

static inline bool
tspecs_are_compat(type_spec_t *t1, type_spec_t *t2)
{
    t1 = global_copy(t1);
    t2 = global_copy(t2);

    return ! type_spec_is_error(global_type_check(t1, t2));
}

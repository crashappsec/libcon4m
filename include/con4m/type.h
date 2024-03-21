#pragma once

#include <con4m.h>


extern type_spec_t *resolve_type_aliases(type_spec_t *, type_env_t *);
extern bool type_spec_is_concrete(type_spec_t *);
extern type_spec_t *type_spec_copy(type_spec_t *node, type_env_t *env);
extern type_spec_t *get_builtin_type(type_env_t *, con4m_builtin_t);
extern type_spec_t *unify(type_spec_t *, type_spec_t *, type_env_t *);
extern type_spec_t *lookup_type_spec(type_t tid, type_env_t *env);
extern type_spec_t *tspec_error();
extern type_spec_t *tspec_void();
extern type_spec_t *tspec_i8();
extern type_spec_t *tspec_u8();
extern type_spec_t *tspec_byte();
extern type_spec_t *tspec_i32();
extern type_spec_t *tspec_u32();
extern type_spec_t *tspec_char();
extern type_spec_t *tspec_i64();
extern type_spec_t *tspec_int();
extern type_spec_t *tspec_u64();
extern type_spec_t *tspec_uint();
extern type_spec_t *tspec_f32();
extern type_spec_t *tspec_f64();
extern type_spec_t *tspec_float();
extern type_spec_t *tspec_utf8();
extern type_spec_t *tspec_buffer();
extern type_spec_t *tspec_utf32();
extern type_spec_t *tspec_grid();
extern type_spec_t *tspec_typespec();
extern type_spec_t *tspec_ipv4();
extern type_spec_t *tspec_ipv6();
extern type_spec_t *tspec_duration();
extern type_spec_t *tspec_size();
extern type_spec_t *tspec_datetime();
extern type_spec_t *tspec_date();
extern type_spec_t *tspec_time();
extern type_spec_t *tspec_url();
extern type_spec_t *tspec_callback();
extern type_spec_t *tspec_renderable();
extern type_spec_t *tspec_render_style();
extern type_spec_t *tspec_hash();
extern type_spec_t *tspec_exception();
extern type_spec_t *tspec_type_env();
extern type_spec_t *tspec_type_details();
extern type_spec_t *tspec_logring();
extern type_spec_t *tspec_mixed();
extern type_spec_t *tspec_list(type_spec_t *);
extern type_spec_t *tspec_xlist(type_spec_t *);
extern type_spec_t *tspec_queue(type_spec_t *);
extern type_spec_t *tspec_ring(type_spec_t *);
extern type_spec_t *tspec_stack(type_spec_t *);
extern type_spec_t *tspec_dict(type_spec_t *, type_spec_t *);
extern type_spec_t *tspec_set(type_spec_t *);
extern type_spec_t *tspec_tuple(int, ...);
extern type_spec_t *tspec_fn(type_spec_t *, int64_t, ...);
extern type_spec_t *tspec_varargs_fn(type_spec_t *, int64_t, ...);

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

static inline type_t
tenv_next_tid(type_env_t *env)
{
    return atomic_fetch_add(&env->next_tid, 1);
}

static inline type_spec_t *
type_spec_new_typevar(type_env_t *env)
{
    type_spec_t *result = con4m_new(T_TYPESPEC, env, T_GENERIC);

    return result;
}

static inline type_t
type_new_typevar(type_env_t *env)
{
    return type_spec_new_typevar(env)->typeid;
}

extern type_env_t *global_type_env;

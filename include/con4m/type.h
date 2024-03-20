#pragma once

#include <con4m.h>


extern type_spec_t *resolve_type_aliases(type_spec_t *, type_env_t *);
extern bool type_spec_is_concrete(type_spec_t *);
extern type_spec_t *type_spec_new_container(con4m_builtin_t, type_env_t *,
					    uint64_t, ...);
extern type_spec_t *type_spec_copy(type_spec_t *node, type_env_t *env);
extern type_spec_t *get_builtin_type(type_env_t *, con4m_builtin_t);

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

// Need to fix the headers.
extern void *hatrack_dict_get    (struct dict_t *, void *, int *);
extern void  hatrack_dict_put    (struct dict_t *, void *, void *);

static inline type_spec_t *
lookup_type_spec(type_t tid, type_env_t *env)
{
    type_spec_t *node = hatrack_dict_get(env->store, (void *)tid, NULL);

    if (!node || type_spec_is_concrete(node)) {
	return node;
    }

    return resolve_type_aliases(node, env);
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

extern type_spec_t *unify(type_spec_t *, type_spec_t *, type_env_t *);

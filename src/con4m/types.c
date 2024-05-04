#include "con4m.h"

// Hashing a tree-based data structure down to a single integer can
// save a lot of work if the user does want to do any dynamic type
// checking at all.
//
// Generally, at run-time, objects mostly have instantiated types, but
// we still have to check against generic function parameters.
//
// And, I'd like to have a repl where we're incrementally typing,
// which also requires generic typing.
//
// The biggest challenge is that at time unit T, you might have two
// types that are (`x, `y) -> `z; sometimes those will be the same
// type, and sometimes they will be the same representation of
// different types. We can't have a hash value that say's they're
// always equal.
//
// The first c4m hash has been working pretty well, and I'm going to
// continue to use it for the time being. But at some point, I am
// going to revisit it, as I know how to do a lot better. The
// fundamental problen is that the hashes are almost fully one-way, so
// you still have to keep around the full graph for anything with
// generics in it. Not that big a deal, but eventually I wouldn't mind
// not having to keep full trees around at run-time.
//
//
// Here is the high-level on the current state of the type system,
// btw, covering the current, planned and possible "kinds" of types in
// c4m.

// SOLID type kinds that will never go away (tho might evolve a bit).
// -----------------------------------------------------------------
// 1. Primitive Types. These are built-in, non-structured types with
//    no directly exposed fields, like int, string, etc. Currently, a
//    lot of our config-related data types all fall in this bucket. It
//    also can include things like the 'void' type, although I currently
//    treat 'type' 'error' and 'void' as their own thing.
//
// 2. List Types. These are container types that take a single type
//    parameter (generally their their contents). Items must all have
//    a unified type, but in exchange you can have an arbitrary number
//    of items.  Con4m is close to supporting multiple built-in list
//    types, including queues, stacks, and rings, with single-threaded
//    and multi-threaded variants. All lists use traditional [] syntax
//    for literals.
//
// 3. Dictionary Types. These are container types that take two, or
//    sometimes one type parameter (e.g., Sets), and use { } for syntax.
//
// 4. Tuple Types. These are container types that take an arbitrary
//    but fixed number of type parameters. Importantly, the parameters
//    are positional, and currently anonymous (so they're structurally
//    typed).
//
// 5. Function types. I'm going to want to extend these to handle
//    keyword arguments soon, so the representation here is probably
//    going to change. Right now, conceptually there are the
//    parameters, which are effectively a tuple, and the return value,
//    which gets special treatment. When I add keyword parameters,
//    those parameters will essentially be a third 'part' of the
//    signature, and will work similar to how objects will end up
//    working (see below).
//
// 6. Type variables. These stand in for any information we haven't
//    resolved yet. These are key to how type checking works
//    statically, but we need them when running too.
//
//    This is particularly the case because I am taking a different
//    implementation approach to most languages, which generate
//    implementations for each instance type. I currently rely on
//    run-time boxing, and don't plan to change that unless it's
//    someday incredibly obviously important.
//
// Basically, type variables are the only 'unknowns', but can easily
// live as components of anything other than a primitive type.
//
//
// Stuff that's there that I'm thinking about ditching.
// -----------------------------------------------------------------
// 1. Currently, TypeSpecs are their own kind. Types can basically be
//    run-time values that people can assign, print, etc. The reason
//    they're not a primitive type is that they are actually
//    parameterized currently. Essentially, you want people to be able
//    to specify their own type to go along with some data field, you
//    can parameterize it to narrow what is allowed. For instance, you
//    can declare a field to be typespec[list[`x]], and values assigned
//    to it MUST be in that form.
//
// 2. I also have "OneOf", which is basically an algebraic sum
//    type. It's mainly meant for making typespec constraints more
//    useful. I find unrestructed types like this in a language have a
//    lot of practical issues that don't lend themselves well to the
//    type of language c4m is becoming.
//
// Basically, since we have explicit post-execution validation
// checking of fields, #1 isn't really necessary, which makes #2
// unnecessary. I haven't made a firm decision yet, though.
//
//
// Stuff that I'm planning that I haven't gotten to yet.
// -----------------------------------------------------------------
// 1. Object types. Once Chalk doesn't need anything specific from
// c4m, this will probably end up my top priority, and it isn't too
// much work. From a type system perspective, this will be kind of a
// dual of tuples... they will be structs of data fields, but order
// will be unimportant, and will have name equivolence. Functions will
// probably *not* be considered data objects, since I have no plans
// for inheritance.
//
// 2. Reference types. Every type will have an associated reference
//    type. Go, aliasing.
//
// 3. "Maybe" types. Basically, this is an highly constrained
// algabraic type that 'extends' a type by adding null (the `void`
// type) as an optional value. This one should be easy relative to
// more generic algabraic types.
//
// Stuff that I'm considering, but am in no hurry to decide.
// -----------------------------------------------------------------
// Most of the stuff here is possible to do well statically, depending
// on constraints, but I really want to wait and see if I can nail the
// usability.
//
// 1. Interfaces; especially if they basically give the feeling of
//    allowing statically checkable duck typing.  These are
//    essentially constraints on generic types. But, it does have some
//    implications because all types, including primitive types
//    suddenly, if we think of types as sets, primitive types can all
//    the sudden be in multiple sets. I think I need to get objects to
//    the point where I'm happy before I can play around and decide if
//    this will work.
//
// 2. 'Either' types. While I'd like to do this for return values instead of
//    exception handling, given we're doing garbage collection, exception
//    handling is pretty easy, whereas this is pretty hard to do well.
//    I think I'm going to start w/ exceptions, and then once Maybe is in,
//    I'll consider this in more detail.

// Type hash requires a type environment.

typedef struct {
    c4m_type_env_t *env;
    c4m_sha_t      *sha;
    int             tv_count;
    c4m_dict_t     *memos;
} type_hash_ctx;

static inline bool
typeid_is_concrete(c4m_type_hash_t tid)
{
    return !(tid & (1ULL << 63));
}

c4m_type_env_t *c4m_global_type_env               = NULL;
c4m_type_t     *c4m_bi_types[C4M_NUM_BUILTIN_DTS] = {
    0,
};
c4m_type_t *type_node_for_list_of_type_objects;

c4m_type_t *
c4m_resolve_type_aliases(c4m_type_t *node, c4m_type_env_t *env)
{
    while (true) {
        c4m_type_t *tmp = hatrack_dict_get(env->store,
                                           (void *)node->typeid,
                                           NULL);

        if (tmp == NULL) {
            hatrack_dict_put(env->store, (void *)node->typeid, node);
            return node;
        }
        if (c4m_tspec_is_concrete(node)) {
            return node;
        }
        if (tmp->typeid == node->typeid) {
            return node;
        }
        node = tmp;
    }
}

c4m_type_t *
c4m_global_resolve_type(c4m_type_t *t)
{
    return c4m_resolve_type_aliases(t, c4m_global_type_env);
}

c4m_type_t *
c4m_global_copy(c4m_type_t *t)
{
    return c4m_tspec_copy(t, c4m_global_type_env);
}

c4m_type_t *
c4m_global_type_check(c4m_type_t *t1, c4m_type_t *t2)
{
    return c4m_unify(t1, t2, c4m_global_type_env);
}

void
c4m_lock_type(c4m_type_t *t)
{
    t->details->flags &= C4M_FN_TY_LOCK;
}

static void
internal_type_hash(c4m_type_t *node, type_hash_ctx *ctx)
{
    if (node == NULL) {
        return;
    }

    node = c4m_resolve_type_aliases(node, ctx->env);

    c4m_type_info_t *deets = node->details;
    uint64_t         num_tvars;

    c4m_sha_int_update(ctx->sha, (uint64_t)deets->base_type->typeid);

    switch (node->details->base_type->dt_kind) {
        // Currently not hashing for future things.
    case C4M_DT_KIND_func:
        c4m_sha_int_update(ctx->sha, (uint64_t)deets->flags);
        break;
    case C4M_DT_KIND_type_var:
        num_tvars = (uint64_t)hatrack_dict_get(ctx->memos,
                                               (void *)node->typeid,
                                               NULL);

        if (num_tvars == 0) {
            num_tvars = ++ctx->tv_count;
            hatrack_dict_put(ctx->memos,
                             (void *)node->typeid,
                             (void *)num_tvars);
        }

        c4m_sha_int_update(ctx->sha, num_tvars);
        break;
    default:; // Do nothing.
    }

    size_t n = c4m_xlist_len(deets->items);

    c4m_sha_int_update(ctx->sha, n);

    for (size_t i = 0; i < n; i++) {
        internal_type_hash(c4m_xlist_get(deets->items, i, NULL), ctx);
    }
}

static c4m_type_hash_t
type_hash_and_dedupe(c4m_type_t **nodeptr, c4m_type_env_t *env)
{
    c4m_buf_t    *buf;
    uint64_t      result;
    c4m_type_t   *node = *nodeptr;
    type_hash_ctx ctx;

    if (node->typeid != 0) {
        return node->typeid;
    }

    switch (node->details->base_type->dt_kind) {
    case C4M_DT_KIND_nil:
    case C4M_DT_KIND_primitive:
    case C4M_DT_KIND_internal:
        node->typeid = node->details->base_type->typeid;
        return node->typeid;
    case C4M_DT_KIND_type_var:
        assert(false); // unreachable; typeid should always be set.
    default:
        ctx.env      = env;
        ctx.sha      = c4m_new(c4m_tspec_hash());
        ctx.tv_count = 0;

        internal_type_hash(node, &ctx);

        buf    = c4m_sha_finish(ctx.sha);
        result = ((uint64_t *)buf->data)[0];

        little_64(result);

        if (ctx.tv_count == 0) {
            result &= ~(1LLU << 63);
        }
        else {
            result |= (1LLU << 63);
        }

        node->typeid = result;
        if (!hatrack_dict_add(env->store, (void *)result, node)) {
            *nodeptr = hatrack_dict_get(env->store, (void *)result, NULL);
            *nodeptr = c4m_resolve_type_aliases(*nodeptr, env);
        }
    }
    return result;
}

c4m_type_hash_t
c4m_type_hash(c4m_type_t *node, c4m_type_env_t *env)
{
    return type_hash_and_dedupe(&node, env);
}

static void
c4m_type_env_init(c4m_type_env_t *env, va_list args)
{
    env->store = c4m_new(c4m_tspec_dict(c4m_tspec_u64(), c4m_tspec_typespec()));
    atomic_store(&env->next_tid, 1LLU << 63);

    for (int i = 0; i < C4M_NUM_BUILTIN_DTS; i++) {
        c4m_type_t *s = c4m_bi_types[i];

        if (s == NULL) {
            continue;
        }
        c4m_type_hash_t id = c4m_tspec_get_data_type_info(s)->typeid;
        hatrack_dict_put(env->store, (void *)id, s);
    }
}

static void
c4m_type_env_marshal(c4m_type_env_t *env, c4m_stream_t *s, c4m_dict_t *memos, int64_t *mid)
{
    c4m_sub_marshal(env->store, s, memos, mid);
    c4m_marshal_u64(atomic_load(&env->next_tid), s);
}

static void
c4m_type_env_unmarshal(c4m_type_env_t *env, c4m_stream_t *s, c4m_dict_t *memos)
{
    env->store = c4m_sub_unmarshal(s, memos);
    atomic_store(&env->next_tid, c4m_unmarshal_u64(s));
}

static void
internal_add_items_array(c4m_type_t *n)
{
    // Avoid infinite recursion by manually constructing the list.
    size_t          sz    = sizeof(c4m_base_obj_t) + sizeof(c4m_xlist_t);
    c4m_base_obj_t *alloc = c4m_gc_raw_alloc(sz, GC_SCAN_ALL);

    c4m_xlist_t *items    = (c4m_xlist_t *)alloc->data;
    alloc->base_data_type = (c4m_dt_info_t *)&c4m_base_type_info[C4M_T_XLIST];
    alloc->concrete_type  = type_node_for_list_of_type_objects;
    items->data           = c4m_gc_array_alloc(uint64_t *, 16);
    items->length         = 16;

    n->details->items = items;
}

static void
c4m_tspec_init(c4m_type_t *n, va_list args)
{
    c4m_type_env_t *env     = va_arg(args, c4m_type_env_t *);
    c4m_builtin_t   base_id = va_arg(args, c4m_builtin_t);

    n->details = c4m_gc_alloc(c4m_type_info_t);

    if (env == NULL && base_id == 0) {
        // This short circuit should be used when unmarshaling only.
        // We basically only jump down to add the items xlist, if
        // needed, which is controlled by a 3rd param after the
        // two typical params.
        if (va_arg(args, int64_t)) {
            internal_add_items_array(n);
        }
        return;
    }

    if (base_id > C4M_T_GENERIC || base_id < C4M_T_ERROR) {
        C4M_CRAISE("Invalid type ID.");
    }

    if (base_id == C4M_T_GENERIC) {
        n->typeid = c4m_tenv_next_tid(env);
    }

    c4m_dt_info_t *info = (c4m_dt_info_t *)&c4m_base_type_info[base_id];

    n->details->base_type = info;

    switch (info->dt_kind) {
    case C4M_DT_KIND_nil:
    case C4M_DT_KIND_primitive:
    case C4M_DT_KIND_internal:
        n->typeid = base_id;
        if (hatrack_dict_get(env->store, (void *)base_id, NULL)) {
            C4M_CRAISE("Call get_builtin_type(), not c4m_new().");
        }
        if ((n->typeid = info->typeid) == C4M_T_TYPESPEC) {
            internal_add_items_array(n);
        }
        break;
    case C4M_DT_KIND_type_var:
        n->typeid = c4m_tenv_next_tid(env);
        break;
    case C4M_DT_KIND_list:
    case C4M_DT_KIND_tuple:
    case C4M_DT_KIND_dict:
    case C4M_DT_KIND_func:
        internal_add_items_array(n);
        c4m_type_t *arg = va_arg(args, c4m_type_t *);

        while (arg != NULL) {
            c4m_xlist_append(n->details->items, arg);
            arg = va_arg(args, c4m_type_t *);
        }

        break;
    default:
        assert(false);
    }

    c4m_type_hash(n, env);
    assert(n->details != NULL && (((int64_t)n->details) & 0x07) == 0);
}

c4m_type_t *
c4m_tspec_copy(c4m_type_t *node, c4m_type_env_t *env)
{
    // Copies, anything that might be mutated, returning an unlocked
    // instance where mutation is okay.

    node = c4m_resolve_type_aliases(node, env);

    if (c4m_tspec_is_concrete(node)) {
        return node;
    }

    c4m_type_info_t *ts_from = node->details;

    if (ts_from->base_type->dt_kind == C4M_DT_KIND_type_var) {
        return c4m_new_typevar(env);
    }

    c4m_type_t      *result  = c4m_new(c4m_tspec_typespec(),
                                 c4m_kw("name",
                                        c4m_ka(ts_from->name),
                                        "base_id",
                                        c4m_ka(ts_from->base_type->typeid)));
    int              n       = c4m_tspec_get_num_params(node);
    c4m_xlist_t     *to_copy = c4m_tspec_get_params(node);
    c4m_type_info_t *ts_to   = result->details;
    ts_to->flags             = ts_from->flags & ~C4M_FN_TY_LOCK;

    for (int i = 0; i < n; i++) {
        c4m_type_t *original = c4m_xlist_get(to_copy, i, NULL);
        c4m_type_t *copy     = c4m_tspec_copy(original, env);

        c4m_xlist_append(ts_to->items, copy);
    }

    c4m_type_hash(result, env);
    return result;
}

static c4m_type_t *
copy_if_needed(c4m_type_t *t, c4m_type_env_t *env)
{
    if (c4m_tspec_is_concrete(t)) {
        return t;
    }

    if (!c4m_tspec_is_locked(t)) {
        return t;
    }

    return c4m_tspec_copy(t, env);
}

bool
c4m_tspec_is_concrete(c4m_type_t *node)
{
    int          n;
    c4m_xlist_t *param;

    // Fast path most of the time; just if we decide to check
    // before an initial tid is set, take the long road.
    if (node->typeid != 0) {
        return typeid_is_concrete(node->typeid);
    }

    switch (c4m_tspec_get_base(node)) {
    case C4M_DT_KIND_nil:
    case C4M_DT_KIND_primitive:
    case C4M_DT_KIND_internal:
        return true;
    case C4M_DT_KIND_type_var:
        return false;
    case C4M_DT_KIND_list:
    case C4M_DT_KIND_dict:
    case C4M_DT_KIND_tuple:
    case C4M_DT_KIND_func:
        n     = c4m_tspec_get_num_params(node);
        param = c4m_tspec_get_params(node);

        for (int i = 0; i < n; i++) {
            if (!c4m_tspec_is_concrete(c4m_xlist_get(param, i, NULL))) {
                return false;
            }
        }
        return true;
    default:
        assert(false);
    }
}

// Just makes things a bit more clear.
#define type_error c4m_tspec_error

c4m_type_t *
c4m_unify(c4m_type_t *t1, c4m_type_t *t2, c4m_type_env_t *env)
{
    c4m_type_t  *result;
    c4m_type_t  *sub1;
    c4m_type_t  *sub2;
    c4m_type_t  *sub_result;
    c4m_xlist_t *p1;
    c4m_xlist_t *p2;
    c4m_xlist_t *new_subs;
    int          num_params;

    t1 = c4m_resolve_type_aliases(t1, env);
    t2 = c4m_resolve_type_aliases(t1, env);

    t1 = copy_if_needed(t1, env);
    t2 = copy_if_needed(t2, env);

    if (t1->typeid == t2->typeid) {
        return t1;
    }

    // Currently, treat utf8 and utf32 as the same type, until all ops
    // are available on each. We'll just always return C4M_T_UTF8 in
    // these cases.
    if (t1->typeid == C4M_T_UTF8 && t2->typeid == C4M_T_UTF32) {
        return t1;
    }
    if (t1->typeid == C4M_T_UTF32 && t2->typeid == C4M_T_UTF8) {
        return t2;
    }

    if (t1->typeid == C4M_T_ERROR || t2->typeid == C4M_T_ERROR) {
        return type_error();
    }

    if (c4m_tspec_is_concrete(t1) && c4m_tspec_is_concrete(t2)) {
        // Concrete, but not the same. Types are not equivolent.
        // While casting may be possible, that doesn't happen here;
        // unification is about type equivolence, not coercion!
        return type_error();
    }

    c4m_dt_kind_t b1 = c4m_tspec_get_base(t1);
    c4m_dt_kind_t b2 = c4m_tspec_get_base(t2);

    if (b1 != b2) {
        if (b1 != C4M_DT_KIND_type_var) {
            if (b2 != C4M_DT_KIND_type_var) {
                return type_error();
            }

            c4m_type_t   *tswap;
            c4m_dt_kind_t bswap;

            tswap = t1;
            t1    = t2;
            t2    = tswap;
            bswap = b1;
            b1    = b2;
            b2    = bswap;
        }
    }
    else {
        if (b1 != b2) {
            // Lists and queues are not type compatable, for example.
            return type_error();
        }
    }

    switch (b1) {
    case C4M_DT_KIND_type_var:
        t1->typeid = t2->typeid; // Forward t1 to t2.
        result     = t2;
        break;

    case C4M_DT_KIND_list:
    case C4M_DT_KIND_dict:
    case C4M_DT_KIND_tuple:
        num_params = c4m_tspec_get_num_params(t1);

        if (num_params != c4m_tspec_get_num_params(t2)) {
            return type_error();
        }

unify_sub_nodes:
        p1       = c4m_tspec_get_params(t1);
        p2       = c4m_tspec_get_params(t2);
        new_subs = c4m_new(c4m_tspec_xlist(c4m_tspec_typespec()),
                           c4m_kw("length", c4m_ka(num_params)));

        for (int i = 0; i < num_params; i++) {
            sub1       = c4m_xlist_get(p1, i, NULL);
            sub2       = c4m_xlist_get(p2, i, NULL);
            sub_result = c4m_unify(sub1, sub2, env);

            if (c4m_tspec_is_error(sub_result)) {
                return sub_result;
            }
            c4m_xlist_append(new_subs, sub_result);
        }

        result                 = t1;
        result->details->items = new_subs;
        break;
    case C4M_DT_KIND_func: {
        int f1_params = c4m_tspec_get_num_params(t1);
        int f2_params = c4m_tspec_get_num_params(t2);

        if (f2_params == 0) {
            result = t1;
            break;
        }

        if (f1_params == 0) {
            result = t2;
            break;
        }
        // Actuals will never be varargs, so if we have two vararg
        // functions, it's only because we're trying to unify two formals.
        if ((t1->details->flags ^ t2->details->flags) & C4M_FN_TY_VARARGS) {
            if (f1_params != f2_params) {
                return type_error();
            }

            num_params = f1_params;
            goto unify_sub_nodes;
        }

        // Below here, we got varargs; put the vararg param on the left.
        if ((t2->details->flags & C4M_FN_TY_VARARGS) != 0) {
            c4m_type_t *swap;

            swap       = t1;
            t1         = t2;
            t2         = swap;
            num_params = f1_params;
            f1_params  = f2_params;
            f2_params  = num_params;
        }

        // -1 here because varargs params are optional.
        if (f2_params < f1_params - 1) {
            return type_error();
        }

        // The last item is always the return type, so we have to
        // unify the last items, plus any items before varargs.  Then,
        // if there are any items in type2, they each need to unify
        // with t1's varargs parameter.
        p1       = c4m_tspec_get_params(t1);
        p2       = c4m_tspec_get_params(t2);
        new_subs = c4m_new(c4m_tspec_xlist(c4m_tspec_typespec()),
                           c4m_kw("length", c4m_ka(num_params)));

        for (int i = 0; i < f1_params - 2; i++) {
            sub1       = c4m_xlist_get(p1, i, NULL);
            sub2       = c4m_xlist_get(p2, i, NULL);
            sub_result = c4m_unify(sub1, sub2, env);

            c4m_xlist_append(new_subs, sub_result);
        }

        // This checks the return type.
        sub1       = c4m_xlist_get(p1, f1_params - 1, NULL);
        sub2       = c4m_xlist_get(p2, f2_params - 1, NULL);
        sub_result = c4m_unify(sub1, sub2, env);

        if (c4m_tspec_is_error(sub_result)) {
            return sub_result;
        }
        // Now, check any varargs.
        sub1 = c4m_xlist_get(p1, f1_params - 2, NULL);

        for (int i = max(f1_params - 2, 0); i < f2_params - 1; i++) {
            sub2       = c4m_xlist_get(p2, i, NULL);
            sub_result = c4m_unify(sub1, sub2, env);
            if (c4m_tspec_is_error(sub_result)) {
                return sub_result;
            }
        }

        result = t1;

        break;
    }
    case C4M_DT_KIND_nil:
    case C4M_DT_KIND_primitive:
    case C4M_DT_KIND_internal:
    case C4M_DT_KIND_maybe:
    case C4M_DT_KIND_object:
    case C4M_DT_KIND_oneof:
    default:
        // Either not implemented yet or covered before the switch.
        // These are all implemented in the Nim checker but won't
        // be moved until Con4m is using them.
        assert(false);
    }

    type_hash_and_dedupe(&result, env);

    return result;
}

static c4m_str_t *internal_type_repr(c4m_type_t *, c4m_dict_t *, int64_t *);

// This is just hex w/ a different char set; max size would be 18 digits.
static const char tv_letters[] = "jtvwxyzabcdefghi";

static inline c4m_str_t *
create_typevar_name(int64_t num)
{
    char buf[19] = {
        0,
    };
    int i = 0;

    while (num) {
        buf[i++] = tv_letters[num & 0x0f];
        num >>= 4;
    }

    return c4m_new(c4m_tspec_utf8(), c4m_kw("cstring", c4m_ka(buf)));
}

static inline c4m_str_t *
internal_repr_tv(c4m_type_t *t, c4m_dict_t *memos, int64_t *nexttv)
{
    c4m_str_t *s = hatrack_dict_get(memos, t, NULL);

    if (s != NULL) {
        return s;
    }

    if (t->details->name != NULL) {
        s = c4m_new(c4m_tspec_utf8(),
                    c4m_kw("cstring",
                           c4m_ka(t->details->name)));
    }
    else {
        int64_t v = *nexttv;
        s         = create_typevar_name(++v);
        *nexttv   = v;
    }

    s = c4m_str_concat(c4m_get_backtick_const(), s);

    hatrack_dict_put(memos, t, s);

    return s;
}

static inline c4m_str_t *
internal_repr_container(c4m_type_info_t *info, c4m_dict_t *memos, int64_t *nexttv)
{
    int          num_types = c4m_xlist_len(info->items);
    c4m_xlist_t *to_join   = c4m_new(c4m_tspec_xlist(c4m_tspec_utf8()));
    int          i         = 0;
    c4m_type_t  *subnode;
    c4m_str_t   *substr;

    c4m_xlist_append(to_join,
                     c4m_new(c4m_tspec_utf8(),
                             c4m_kw("cstring",
                                    c4m_ka(info->base_type->name))));
    c4m_xlist_append(to_join, c4m_get_lbrak_const());
    goto first_loop_start;

    for (; i < num_types; i++) {
        c4m_xlist_append(to_join, c4m_get_comma_const());

first_loop_start:
        subnode = c4m_xlist_get(info->items, i, NULL);

        substr = internal_type_repr(subnode, memos, nexttv);

        c4m_xlist_append(to_join, substr);
    }

    c4m_xlist_append(to_join, c4m_get_rbrak_const());

    return c4m_str_join(to_join, NULL);
}

// This will get more complicated when we add keyword parameter sypport.

static inline c4m_str_t *
internal_repr_func(c4m_type_info_t *info, c4m_dict_t *memos, int64_t *nexttv)
{
    int          num_types = c4m_xlist_len(info->items);
    c4m_xlist_t *to_join   = c4m_new(c4m_tspec_xlist(c4m_tspec_utf8()));
    int          i         = 0;
    c4m_type_t  *subnode;
    c4m_str_t   *substr;

    c4m_xlist_append(to_join, c4m_get_lparen_const());

    // num_types - 1 will be 0 if there are no args, but there is a
    // return value. So the below loop won't run in all cases.  But
    // only need to do the test for comma once, not every time through
    // the loop.

    if (num_types > 1) {
        goto first_loop_start;

        for (; i < num_types - 1; i++) {
            c4m_xlist_append(to_join, c4m_get_comma_const());

first_loop_start:

            if ((i == num_types - 2) && info->flags & C4M_FN_TY_VARARGS) {
                c4m_xlist_append(to_join, c4m_get_asterisk_const());
            }

            subnode = c4m_xlist_get(info->items, i, NULL);
            substr  = internal_type_repr(subnode, memos, nexttv);
            c4m_xlist_append(to_join, substr);
        }
    }

    c4m_xlist_append(to_join, c4m_get_rparen_const());
    c4m_xlist_append(to_join, c4m_get_arrow_const());

    subnode = c4m_xlist_get(info->items, num_types - 1, NULL);
    substr  = internal_type_repr(subnode, memos, nexttv);

    c4m_xlist_append(to_join, substr);

    return c4m_str_join(to_join, NULL);
}

static c4m_str_t *
internal_type_repr(c4m_type_t *t, c4m_dict_t *memos, int64_t *nexttv)
{
    c4m_type_info_t *info = t->details;

    switch (info->base_type->dt_kind) {
    case C4M_DT_KIND_nil:
    case C4M_DT_KIND_primitive:
        return c4m_new(c4m_tspec_utf8(),
                       c4m_kw("cstring", c4m_ka(info->base_type->name)));
    case C4M_DT_KIND_type_var:
        return internal_repr_tv(t, memos, nexttv);
    case C4M_DT_KIND_list:
    case C4M_DT_KIND_dict:
    case C4M_DT_KIND_tuple:
        return internal_repr_container(info, memos, nexttv);
    case C4M_DT_KIND_func:
        return internal_repr_func(info, memos, nexttv);
    default:
        assert(false);
    }

    return NULL;
}

static c4m_str_t *
c4m_tspec_repr(c4m_type_t *t, to_str_use_t how)
{
    c4m_dict_t *memos = c4m_new(c4m_tspec_dict(c4m_tspec_ref(), c4m_tspec_utf8()));
    int64_t     n     = 0;

    return internal_type_repr(t, memos, &n);
}

extern void        c4m_marshal_compact_type(c4m_type_t *t, c4m_stream_t *s);
extern c4m_type_t *c4m_unmarshal_compact_type(c4m_stream_t *s);

static void
c4m_tspec_marshal(c4m_type_t *n, c4m_stream_t *s, c4m_dict_t *m, int64_t *mid)
{
    c4m_marshal_compact_type(n, s);
}

static void
c4m_tspec_unmarshal(c4m_type_t *n, c4m_stream_t *s, c4m_dict_t *m)
{
    c4m_type_t *r = c4m_unmarshal_compact_type(s);

    n->details = r->details;
    n->typeid  = r->typeid;
}

const c4m_vtable_t c4m_type_env_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        (c4m_vtable_entry)c4m_type_env_init,
        NULL,
        NULL,
        NULL,
        (c4m_vtable_entry)c4m_type_env_marshal,
        (c4m_vtable_entry)c4m_type_env_unmarshal,
        NULL,
    }};

const c4m_vtable_t c4m_type_spec_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        (c4m_vtable_entry)c4m_tspec_init,
        (c4m_vtable_entry)c4m_tspec_repr,
        NULL,
        NULL,
        (c4m_vtable_entry)c4m_tspec_marshal,
        (c4m_vtable_entry)c4m_tspec_unmarshal,
        NULL, // Nothing to coerce to.
        NULL,
        NULL, // from-lit still handled in Nim.
        (c4m_vtable_entry)c4m_global_copy,
        NULL,
        // Nothing else is appropriate.
    }};

void
c4m_initialize_global_types()
{
    if (c4m_global_type_env == NULL) {
        c4m_dt_info_t   *tspec = (c4m_dt_info_t *)&c4m_base_type_info[C4M_T_TYPESPEC];
        int              tslen = tspec->alloc_len + sizeof(c4m_base_obj_t);
        c4m_base_obj_t  *tobj  = c4m_gc_raw_alloc(tslen,
                                                (uint64_t *)tspec->ptr_info);
        c4m_type_info_t *info  = c4m_gc_alloc(c4m_type_info_t);
        c4m_base_obj_t  *one;
        c4m_type_t      *ts;

        ts          = (c4m_type_t *)tobj->data;
        ts->typeid  = C4M_T_TYPESPEC;
        ts->details = info;

        internal_add_items_array(ts);

        tobj->base_data_type         = tspec;
        tobj->concrete_type          = ts;
        info->name                   = (char *)tspec->name;
        info->base_type              = tspec;
        c4m_bi_types[C4M_T_TYPESPEC] = ts;

        for (int i = 0; i < C4M_NUM_BUILTIN_DTS; i++) {
            if (i == C4M_T_TYPESPEC) {
                continue;
            }
            c4m_dt_info_t *one_spec = (c4m_dt_info_t *)&c4m_base_type_info[i];

            switch (one_spec->dt_kind) {
            case C4M_DT_KIND_nil:
            case C4M_DT_KIND_primitive:
            case C4M_DT_KIND_internal:
                one         = c4m_gc_raw_alloc(tslen,
                                       (uint64_t *)tspec->ptr_info);
                ts          = (c4m_type_t *)one->data;
                ts->typeid  = i;
                info        = c4m_gc_alloc(c4m_type_info_t);
                ts->details = info;

                one->base_data_type = tspec;
                one->concrete_type  = (c4m_type_t *)tobj->data;

                info->name      = (char *)one_spec->name;
                info->base_type = one_spec;

                c4m_bi_types[i] = ts;
                assert(ts->details != NULL && (((int64_t)ts->details) & 0x07) == 0);
                continue;
            default:
                continue;
            }
        }

        c4m_base_obj_t *envobj;
        c4m_base_obj_t *envstore;
        // This needs to not be c4m_new'd.
        envobj = c4m_gc_raw_alloc(
            sizeof(c4m_type_env_t) + sizeof(c4m_base_obj_t),
            GC_SCAN_ALL);
        envstore = c4m_gc_alloc(c4m_dict_t);

        c4m_global_type_env = (c4m_type_env_t *)envobj->data;
        c4m_dict_t *store   = (c4m_dict_t *)envstore->data;

        c4m_global_type_env->store = store;

        // We don't set the heading info up fully, so this dict
        // won't be directly marshalable unless / until we do.
        hatrack_dict_init(store, HATRACK_DICT_KEY_TYPE_INT);
        c4m_gc_register_root(&c4m_global_type_env, 1);

        // Set up the type we need internally for containers.

        tobj = c4m_gc_raw_alloc(tslen, (uint64_t *)tspec->ptr_info);

        tobj->base_data_type = tspec;
        tobj->concrete_type  = c4m_bi_types[C4M_T_TYPESPEC];

        ts                     = (c4m_type_t *)tobj->data;
        ts->details            = c4m_gc_alloc(c4m_type_info_t);
        ts->details->base_type = (c4m_dt_info_t *)&c4m_base_type_info[C4M_T_XLIST];
        ts->details->name      = (char *)ts->details->base_type->name;

        // This type hash won't really be right because there's
        // a recursive loop.
        c4m_type_hash(ts, c4m_global_type_env);
        type_node_for_list_of_type_objects = ts;

        // Now that that's set up, we can go back and fill in the store's
        // details for good measure.
        envobj->base_data_type   = (c4m_dt_info_t *)&c4m_base_type_info[C4M_T_TYPE_ENV];
        envobj->concrete_type    = c4m_tspec_type_env();
        envstore->base_data_type = (c4m_dt_info_t *)&c4m_base_type_info[C4M_T_DICT];
        envstore->concrete_type  = c4m_tspec_dict(c4m_tspec_int(),
                                                 c4m_tspec_typespec());

        // Now, we have to manually set up an xlist.
        size_t          sz   = sizeof(c4m_xlist_t) + sizeof(c4m_base_obj_t);
        c4m_base_obj_t *xobj = c4m_gc_raw_alloc(sz, GC_SCAN_ALL);
        xobj->base_data_type = ts->details->base_type;

        c4m_xlist_t *list  = (c4m_xlist_t *)xobj->data;
        list->data         = c4m_gc_alloc(int64_t *);
        list->data[0]      = (int64_t *)ts;
        list->append_ix    = 1;
        list->length       = 1;
        ts->details->items = list;

        // Theoretically, we should be able to marshal these now.
    }
}

c4m_type_t *
c4m_tspec_list(c4m_type_t *sub)
{
    c4m_type_t  *result = c4m_new(c4m_tspec_typespec(),
                                 c4m_global_type_env,
                                 C4M_T_LIST);
    c4m_xlist_t *items  = result->details->items;

    c4m_xlist_append(items, sub);

    type_hash_and_dedupe(&result, c4m_global_type_env);

    return result;
}

c4m_type_t *
c4m_tspec_xlist(c4m_type_t *sub)
{
    c4m_type_t  *result = c4m_new(c4m_tspec_typespec(),
                                 c4m_global_type_env,
                                 C4M_T_XLIST);
    c4m_xlist_t *items  = result->details->items;

    c4m_xlist_append(items, sub);

    type_hash_and_dedupe(&result, c4m_global_type_env);

    return result;
}

c4m_type_t *
c4m_tspec_tree(c4m_type_t *sub)
{
    c4m_type_t  *result = c4m_new(c4m_tspec_typespec(),
                                 c4m_global_type_env,
                                 C4M_T_TREE);
    c4m_xlist_t *items  = result->details->items;

    c4m_xlist_append(items, sub);

    type_hash_and_dedupe(&result, c4m_global_type_env);

    return result;
}

c4m_type_t *
c4m_tspec_queue(c4m_type_t *sub)
{
    c4m_type_t  *result = c4m_new(c4m_tspec_typespec(),
                                 c4m_global_type_env,
                                 C4M_T_QUEUE);
    c4m_xlist_t *items  = result->details->items;

    c4m_xlist_append(items, sub);

    type_hash_and_dedupe(&result, c4m_global_type_env);

    return result;
}

c4m_type_t *
c4m_tspec_ring(c4m_type_t *sub)
{
    c4m_type_t  *result = c4m_new(c4m_tspec_typespec(),
                                 c4m_global_type_env,
                                 C4M_T_RING);
    c4m_xlist_t *items  = result->details->items;

    c4m_xlist_append(items, sub);

    type_hash_and_dedupe(&result, c4m_global_type_env);

    return result;
}

c4m_type_t *
c4m_tspec_stack(c4m_type_t *sub)
{
    c4m_type_t  *result = c4m_new(c4m_tspec_typespec(),
                                 c4m_global_type_env,
                                 C4M_T_STACK);
    c4m_xlist_t *items  = result->details->items;

    c4m_xlist_append(items, sub);

    type_hash_and_dedupe(&result, c4m_global_type_env);

    return result;
}

c4m_type_t *
c4m_tspec_dict(c4m_type_t *sub1, c4m_type_t *sub2)
{
    c4m_type_t  *result = c4m_new(c4m_tspec_typespec(),
                                 c4m_global_type_env,
                                 C4M_T_DICT);
    c4m_xlist_t *items  = result->details->items;

    c4m_xlist_append(items, sub1);
    c4m_xlist_append(items, sub2);

    type_hash_and_dedupe(&result, c4m_global_type_env);

    return result;
}

c4m_type_t *
c4m_tspec_set(c4m_type_t *sub1)
{
    c4m_type_t  *result = c4m_new(c4m_tspec_typespec(),
                                 c4m_global_type_env,
                                 C4M_T_SET);
    c4m_xlist_t *items  = result->details->items;

    c4m_xlist_append(items, sub1);

    type_hash_and_dedupe(&result, c4m_global_type_env);

    return result;
}

c4m_type_t *
c4m_tspec_tuple(int64_t nitems, ...)
{
    va_list      args;
    c4m_type_t  *result = c4m_new(c4m_tspec_typespec(),
                                 c4m_global_type_env,
                                 C4M_T_TUPLE);
    c4m_xlist_t *items  = result->details->items;

    va_start(args, nitems);

    if (nitems <= 1) {
        C4M_CRAISE("Tuples must contain 2 or more items.");
    }

    for (int i = 0; i < nitems; i++) {
        c4m_type_t *sub = va_arg(args, c4m_type_t *);
        c4m_xlist_append(items, sub);
    }

    type_hash_and_dedupe(&result, c4m_global_type_env);

    return result;
}

c4m_type_t *
c4m_tspec_fn_va(c4m_type_t *return_type, int64_t nparams, ...)
{
    va_list      args;
    c4m_type_t  *result = c4m_new(c4m_tspec_typespec(),
                                 c4m_global_type_env,
                                 C4M_T_FUNCDEF);
    c4m_xlist_t *items  = result->details->items;

    va_start(args, nparams);

    for (int i = 0; i < nparams; i++) {
        c4m_type_t *sub = va_arg(args, c4m_type_t *);
        c4m_xlist_append(items, sub);
    }
    c4m_xlist_append(items, return_type);

    type_hash_and_dedupe(&result, c4m_global_type_env);

    return result;
}

c4m_type_t *
c4m_tspec_varargs_fn(c4m_type_t *return_type, int64_t nparams, ...)
{
    va_list      args;
    c4m_type_t  *result = c4m_new(c4m_tspec_typespec(),
                                 c4m_global_type_env,
                                 C4M_T_FUNCDEF);
    c4m_xlist_t *items  = result->details->items;

    va_start(args, nparams);

    if (nparams < 1) {
        C4M_CRAISE("Varargs functions require at least one argument.");
    }

    for (int i = 0; i < nparams; i++) {
        c4m_type_t *sub = va_arg(args, c4m_type_t *);
        c4m_xlist_append(items, sub);
    }
    c4m_xlist_append(items, return_type);
    result->details->flags |= C4M_FN_TY_VARARGS;

    type_hash_and_dedupe(&result, c4m_global_type_env);

    return result;
}

c4m_type_t *
c4m_tspec_fn(c4m_type_t *ret, c4m_xlist_t *params, bool va)
{
    c4m_type_t  *result = c4m_new(c4m_tspec_typespec(),
                                 c4m_global_type_env,
                                 C4M_T_FUNCDEF);
    c4m_xlist_t *items  = result->details->items;
    int          n      = c4m_xlist_len(params);

    for (int i = 0; i < n; i++) {
        c4m_xlist_append(items, c4m_xlist_get(params, i, NULL));
    }
    c4m_xlist_append(items, ret);

    if (va) {
        result->details->flags |= C4M_FN_TY_VARARGS;
    }
    type_hash_and_dedupe(&result, c4m_global_type_env);

    return result;
}

c4m_type_t *
c4m_lookup_tspec(c4m_type_hash_t tid, c4m_type_env_t *env)
{
    c4m_type_t *node = hatrack_dict_get(env->store, (void *)tid, NULL);

    if (!node || c4m_tspec_is_concrete(node)) {
        return node;
    }

    return c4m_resolve_type_aliases(node, env);
}

c4m_type_t *
c4m_get_builtin_type(c4m_builtin_t base_id)
{
    return c4m_bi_types[base_id];
}

c4m_type_t *
c4m_get_promotion_type(c4m_type_t *t1, c4m_type_t *t2, int *warning)
{
    *warning = 0;

    t1 = c4m_resolve_type_aliases(t1, c4m_global_type_env);
    t2 = c4m_resolve_type_aliases(t2, c4m_global_type_env);

    c4m_type_hash_t id1 = c4m_tspec_get_data_type_info(t1)->typeid;
    c4m_type_hash_t id2 = c4m_tspec_get_data_type_info(t2)->typeid;

    if (id1 < C4M_T_I8 || id1 > C4M_T_UINT || id2 < C4M_T_I8 || id2 > C4M_T_UINT) {
        *warning = -1;
        return type_error();
    }

    if (id2 > id1) {
        c4m_type_hash_t swap = id1;
        id1                  = id2;
        id2                  = swap;
    }

    switch (id1) {
    case C4M_T_UINT:
        switch (id2) {
        case C4M_T_INT:
            *warning = 1; // Might wrap.;
                          // fallthrough
        case C4M_T_I32:
        case C4M_T_I8:
            return c4m_tspec_i64();
        default:
            return c4m_tspec_u64();
        }
    case C4M_T_U32:
    case C4M_T_CHAR:
        switch (id2) {
        case C4M_T_I32:
            *warning = 1;
            // fallthrough
        case C4M_T_I8:
            return c4m_tspec_i32();
        default:
            return c4m_tspec_u32();
        }
    case C4M_T_BYTE:
        if (id2 != C4M_T_BYTE) {
            *warning = 1;
            return c4m_tspec_i8();
        }
        return c4m_tspec_byte();
    case C4M_T_INT:
        return c4m_tspec_i64();
    case C4M_T_I32:
        return c4m_tspec_i32();
    default:
        return c4m_tspec_i8();
    }
}

#include <con4m.h>

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
// The first con4m hash has been working pretty well, and I'm going to
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
// con4m.


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
//    type of language con4m is becoming.
//
// Basically, since we have explicit post-execution validation
// checking of fields, #1 isn't really necessary, which makes #2
// unnecessary. I haven't made a firm decision yet, though.
//
//
// Stuff that I'm planning that I haven't gotten to yet.
// -----------------------------------------------------------------
// 1. Object types. Once Chalk doesn't need anything specific from
// con4m, this will probably end up my top priority, and it isn't too
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
    type_env_t *env;
    sha_ctx    *sha;
    int         tv_count;
    dict_t     *memos;
} type_hash_ctx;

type_spec_t *
resolve_type_aliases(type_spec_t *node, type_env_t *env)
{

    while (true) {
	type_spec_t *tmp = hatrack_dict_get(env->store, (void *)node->typeid,
					    NULL);

	if (tmp == NULL) {
	    hatrack_dict_put(env->store, (void *)node->typeid, node);
	    return node;
	}
	if (type_spec_is_concrete(node)) {
	    return node;
	}
	if (tmp->typeid == node->typeid) {
	    return node;
	}
	node = tmp;
    }
}

static void
internal_type_hash(type_spec_t *node, type_hash_ctx *ctx)
{
    if (node == NULL) {
	return;
    }

    node = resolve_type_aliases(node, ctx->env);

    type_details_t *deets = node->details;
    uint64_t        num_tvars;

    sha_int_update(ctx->sha, deets->base_type->typeid);

    switch(node->details->base_type->base) {
	// Currently not hashing for future things.
    case BT_func:
	sha_int_update(ctx->sha, (uint64_t)deets->flags);

    case BT_type_var:
	num_tvars = (uint64_t)hatrack_dict_get(ctx->memos,
					       (void *)node->typeid, NULL);

	if (num_tvars == 0) {
	    num_tvars = ++ctx->tv_count;
	    hatrack_dict_put(ctx->memos, (void *)node->typeid,
			     (void *)num_tvars);
	}

	sha_int_update(ctx->sha, num_tvars);
	break;
    default:
	; // Do nothing.
    }

    size_t n = xlist_len(deets->items);

    sha_int_update(ctx->sha, n);

    for (size_t i = 0; i < n; i++) {
	internal_type_hash(xlist_get(deets->items, i, NULL), ctx);
    }
}

type_t
type_hash(type_spec_t *node, type_env_t *env)
{
    buffer_t *buf;
    uint64_t  result;

    if (node->typeid != 0) {
	return node->typeid;
    }

    switch (node->details->base_type->base) {
    case BT_nil:
    case BT_primitive:
    case BT_internal:
	node->typeid = node->details->base_type->typeid;
	return node->typeid;
    case BT_type_var:
	assert(false); // unreachable; typeid should always be set.
    default:
    {
	type_hash_ctx ctx = {
	    .env      = env,
	    .sha      = con4m_new(T_SHA),
	    .tv_count = 0
	};

	internal_type_hash(node, &ctx);

	buf    = sha_finish(ctx.sha);
	result = ((uint64_t *)buf->data)[0];

	little_64(result);

	if (ctx.tv_count == 0) {
	    result &= ~(1LLU << 63);
	}
	else {
	    result |= (1LLU << 63);
	}

	node->typeid = result;
	hatrack_dict_put(env->store, (void *)result, node);
    }
    }
    return result;
}

static void
con4m_type_env_init(type_env_t *env, va_list args)
{
    env->store = con4m_new(T_DICT, HATRACK_DICT_KEY_TYPE_INT);
    atomic_store(&env->next_tid, 1LLU << 63);
}

static void
con4m_type_env_marshal(type_env_t *env, FILE *f, dict_t *memos, int64_t *mid)
{
    con4m_sub_marshal(env->store, f, memos, mid);
    marshal_u64(atomic_load(&env->next_tid), f);
}

static void
con4m_type_env_unmarshal(type_env_t *env, FILE *f, dict_t *memos)
{
    env->store = con4m_sub_unmarshal(f, memos);
    atomic_store(&env->next_tid, unmarshal_u64(f));
}

static void
con4m_type_details_marshal(type_details_t *d, FILE *f, dict_t *m, int64_t *mid)
{
    con4m_sub_marshal(d->name, f, m, mid);
    con4m_sub_marshal(d->base_type, f, m, mid);
    con4m_sub_marshal(d->items, f, m, mid);
    con4m_sub_marshal(d->props, f, m, mid);
    marshal_u8(d->flags, f);
}

static void
con4m_type_details_unmarshal(type_details_t *d, FILE *f, dict_t *m)
{
    d->name      = con4m_sub_unmarshal(f, m);
    d->base_type = con4m_sub_unmarshal(f, m);
    d->items     = con4m_sub_unmarshal(f, m);
    d->props     = con4m_sub_unmarshal(f, m);
    d->flags     = unmarshal_u8(f);
}

static void
con4m_type_spec_init(type_spec_t *n, va_list args)
{
    type_env_t     *env     = va_arg(args, type_env_t *);
    con4m_builtin_t base_id = va_arg(args, con4m_builtin_t);

    if (base_id > T_GENERIC || base_id < T_TYPE_ERROR) {
	CRAISE("Invalid type ID.");
    }

    if (base_id == T_GENERIC) {
	n->typeid = tenv_next_tid(env);
    }

    con4m_dt_info  *info  = (con4m_dt_info *)&builtin_type_info[base_id];
    n->details            = con4m_new(T_TYPE_DETAILS);
    n->details->base_type = info;

    switch(info->base) {
    case BT_nil:
    case BT_primitive:
    case BT_internal:
	if (hatrack_dict_get(env->store, (void *)base_id, NULL)) {
		CRAISE("Call get_builtin_type(), not con4m_new().");
	}
	n->typeid = info->typeid;
	break;
    case BT_type_var:
	n->typeid = tenv_next_tid(env);
	break;
    case BT_list:
    case BT_tuple:
    case BT_dict:
    case BT_func:
    {
	xlist_t *items = con4m_new(T_XLIST);
	type_spec_t *arg = va_arg(args, type_spec_t *);
	while (arg != NULL) {
	    xlist_append(items, arg);
	    arg = va_arg(args, type_spec_t *);
	}
	n->details->items = items;
	break;
    }
    default:
	assert(false);
    }

    type_hash(n, env);
}

type_spec_t *
get_builtin_type(type_env_t *env, con4m_builtin_t base_id)
{
    type_spec_t *result = hatrack_dict_get(env->store, (void *)base_id, NULL);

    if (result != NULL) {
	return result;
    }

    return con4m_new(T_TYPESPEC, env, base_id);
}

type_spec_t *
type_spec_new_container(con4m_builtin_t base_id, type_env_t *env,
			uint64_t num, ...)
{
    va_list      args;
    type_spec_t *result = con4m_new(T_TYPESPEC, "base_id", base_id);

    va_start(args, num);

    for (uint64_t i = 0; i < num; i++) {
	xlist_append(result->details->items, va_arg(args, type_spec_t *));
    }

    va_end(args);

    type_hash(result, env);

    return result;
}

type_spec_t *
type_spec_copy(type_spec_t *node, type_env_t *env)
{
    // Copies, anything that might be mutated, returning an unlocked
    // instance where mutation is okay.

    node = resolve_type_aliases(node, env);

    if (type_spec_is_concrete(node)) {
	return node;
    }

    type_details_t *ts_from = node->details;

    if (ts_from->base_type->base == BT_type_var)
    {
	return type_spec_new_typevar(env);
    }

    type_spec_t    *result  = con4m_new(T_TYPESPEC, "name", ts_from->name,
			 	        "base_id", ts_from->base_type->typeid);
    int             n       = type_spec_get_num_params(node);
    xlist_t        *to_copy = type_spec_get_params(node);
    type_details_t *ts_to   = result->details;
    ts_to->flags            = ts_from->flags & ~FN_TY_LOCK;

    for (int i = 0; i < n; i++) {
	type_spec_t *original = xlist_get(to_copy, i, NULL);
	type_spec_t *copy     = type_spec_copy(original, env);

	xlist_append(ts_to->items, copy);
    }

    type_hash(result, env);
    return result;
}

static type_spec_t *
copy_if_needed(type_spec_t *t, type_env_t *env)
{
    if (type_spec_is_concrete(t)) {
	return t;
    }

    if (!type_spec_is_locked(t)) {
	return t;
    }

    return type_spec_copy(t, env);
}

bool
type_spec_is_concrete(type_spec_t *node)
{
    int      n;
    xlist_t *param;

    // Fast path most of the time; just if we decide to check
    // before an initial tid is set, take the long road.
    if (node->typeid != 0) {
	return typeid_is_concrete(node->typeid);
    }

    switch(type_spec_get_base(node)) {
    case BT_nil:
    case BT_primitive:
    case BT_internal:
	return true;
    case BT_type_var:
	return false;
    case BT_list:
    case BT_dict:
    case BT_tuple:
    case BT_func:
	n     = type_spec_get_num_params(node);
	param = type_spec_get_params(node);

	for (int i = 0; i < n; i++) {
	    if (!type_spec_is_concrete(xlist_get(param, i, NULL))) {
		return false;
	    }
	}
	return true;
    default:
	assert(false);
    }
}

static inline type_spec_t *
type_error(type_env_t *env)
{
    return get_builtin_type(env, T_TYPE_ERROR);
}

type_spec_t *
unify(type_spec_t *t1, type_spec_t *t2, type_env_t *env)
{
    type_spec_t *result;
    type_spec_t *sub1;
    type_spec_t *sub2;
    type_spec_t *sub_result;
    xlist_t     *p1;
    xlist_t     *p2;
    xlist_t     *new_subs;
    int         num_params;


    t1 = resolve_type_aliases(t1, env);
    t2 = resolve_type_aliases(t1, env);

    t1 = copy_if_needed(t1, env);
    t2 = copy_if_needed(t2, env);

    if (t1->typeid == t2->typeid) {
	return t1;
    }

    if (t1->typeid == T_TYPE_ERROR || t2->typeid == T_TYPE_ERROR) {
	return type_error(env);
    }

    if (type_spec_is_concrete(t1) && type_spec_is_concrete(t2)) {
	// Concrete, but not the same. Types are not equivolent.
	// While casting may be possible, that doesn't happen here;
	// unification is about type equivolence, not coercion!
	return type_error(env);
    }

    base_t b1 = type_spec_get_base(t1);
    base_t b2 = type_spec_get_base(t2);

    if (b1 != b2) {
	if (b1 != BT_type_var) {
	    if (b2 != BT_type_var) {
		return type_error(env);
	    }

	    type_spec_t *tswap;
	    base_t       bswap;

	    tswap = t1;
	    t1    = t2;
	    t2    = tswap;
	    bswap = b1;
	    b1    = b2;
	    b2    = bswap;
	}
    } else {
	if (b1 != b2) {
	    // Lists and queues are not type compatable, for example.
	    return type_error(env);
	}
    }

    switch(b1) {
    case BT_type_var:
	t1->typeid = t2->typeid; // Forward t1 to t2.
	result = t2;
	break;

    case BT_list:
    case BT_dict:
    case BT_tuple:
	num_params = type_spec_get_num_params(t1);

	if (num_params != type_spec_get_num_params(t2)) {
	    return type_error(env);
	}

    unify_sub_nodes:
	p1       = type_spec_get_params(t1);
	p2       = type_spec_get_params(t2);
	new_subs = con4m_new(T_XLIST, "length", num_params);

	for (int i = 0; i < num_params; i++) {
	    sub1       = xlist_get(p1, i, NULL);
	    sub2       = xlist_get(p2, i, NULL);
	    sub_result = unify(sub1, sub2, env);

	    if (type_spec_is_error(sub_result)) {
		return sub_result;
	    }
	    xlist_append(new_subs, sub_result);
	}

	result                 = t1;
	result->details->items = new_subs;
	break;
    case BT_func:
    {
	int f1_params = type_spec_get_num_params(t1);
	int f2_params = type_spec_get_num_params(t2);

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
	if ((t1->details->flags ^ t2->details->flags) & FN_TY_VARARGS) {
	    if (f1_params != f2_params) {
		return type_error(env);
	    }

	    num_params = f1_params;
	    goto unify_sub_nodes;
	}

	// Below here, we got varargs; put the vararg param on the left.
	if ((t2->details->flags & FN_TY_VARARGS) != 0) {
	    type_spec_t *swap;

	    swap       = t1;
	    t1         = t2;
	    t2         = swap;
	    num_params = f1_params;
	    f1_params  = f2_params;
	    f2_params  = num_params;
	}

	// -1 here because varargs params are optional.
	if (f2_params < f1_params - 1) {
	    return type_error(env);
	}

	// The last item is always the return type, so we have to
	// unify the last items, plus any items before varargs.  Then,
	// if there are any items in type2, they each need to unify
	// with t1's varargs parameter.
	p1       = type_spec_get_params(t1);
	p2       = type_spec_get_params(t2);
	new_subs = con4m_new(T_XLIST, "length", num_params);

	for (int i = 0; i < f1_params - 2; i++) {
	    sub1       = xlist_get(p1, i, NULL);
	    sub2       = xlist_get(p2, i, NULL);
	    sub_result = unify(sub1, sub2, env);

	    xlist_append(new_subs, sub_result);
	}

	// This checks the return type.
	sub1       = xlist_get(p1, f1_params - 1, NULL);
	sub2       = xlist_get(p2, f2_params - 1, NULL);
	sub_result = unify(sub1, sub2, env);

	if (type_spec_is_error(sub_result)) {
	    return sub_result;
	}
	// Now, check any varargs.
	sub1 = xlist_get(p1, f1_params - 2, NULL);

	for (int i = max(f1_params - 2, 0); i < f2_params - 1; i++) {
	    sub2       = xlist_get(p2, i, NULL);
	    sub_result = unify(sub1, sub2, env);
	    if (type_spec_is_error(sub_result)) {
		return sub_result;
	    }
	}

	result = t1;

	break;
    }
    case BT_nil:
    case BT_primitive:
    case BT_internal:
    case BT_ref:
    case BT_maybe:
    case BT_object:
    case BT_oneof:
    default:
	// Either not implemented yet or covered before the switch.
	// These are all implemented in the Nim checker but won't
	// be moved until Con4m is using them.
	assert(false);
    }

    type_hash(result, env);
    return result;
}

static any_str_t *internal_type_repr(type_spec_t *, dict_t *, int64_t *);

enum {
    LBRAK_IX  = 0,
    COMMA_IX  = 1,
    RBRAK_IX  = 2,
    LPAREN_IX = 3,
    RPAREN_IX = 4,
    ARROW_IX  = 5,
    BTICK_IX  = 6,
    STAR_IX   = 7,
    PUNC_MAX  = 8
};

static any_str_t *type_punct[PUNC_MAX] = {0, };


static inline void
init_punctuation()
{
    if (type_punct[0] == NULL) {
	type_punct[LBRAK_IX]  = utf8_repeat('[', 1);
	type_punct[COMMA_IX]  = con4m_new(T_UTF8, "cstring", ", ");
	type_punct[RBRAK_IX]  = utf8_repeat(']', 1);
	type_punct[LPAREN_IX] = utf8_repeat('(', 1);
	type_punct[RPAREN_IX] = utf8_repeat(')', 1);
	type_punct[ARROW_IX]  = con4m_new(T_UTF8, "cstring", " -> ");
	type_punct[BTICK_IX]  = utf8_repeat('`', 1);
	type_punct[STAR_IX]   = utf8_repeat('*', 1);
    }
    con4m_gc_register_root(&type_punct[0], PUNC_MAX);
}


// This is just hex w/ a different char set; max size would be 18 digits.
static const char tv_letters[] = "jtvwxyzabcdefghi";

static inline any_str_t *
create_typevar_name(int64_t num)
{
    char buf[19] = {0, };
    int  i  = 0;

    while (num) {
	buf[i++] = tv_letters[num & 0x0f];
	num >>= 4;
    }

    return con4m_new(T_UTF8, "cstring", buf);
}

static inline any_str_t *
internal_repr_tv(type_details_t *info, dict_t *memos, int64_t *nexttv)
{
    any_str_t *s = hatrack_dict_get(memos, info, NULL);

    if (s != NULL) {
	return s;
    }

    if (info->name != NULL) {
	s = con4m_new(T_UTF8, "cstring", info->name);
    }
    else {
	int64_t v = *nexttv;
	s         = create_typevar_name(++v);
	*nexttv   = v;
    }

    s = string_concat(type_punct[BTICK_IX], s);

    hatrack_dict_put(memos, info, s);

    return s;
}

static inline any_str_t *
internal_repr_container(type_details_t *info, dict_t *memos, int64_t *nexttv)
{
    int          num_types = xlist_len(info->items);
    xlist_t     *to_join   = con4m_new(T_XLIST);
    int          i         = 0;
    type_spec_t *subnode;
    any_str_t   *substr;

    xlist_append(to_join, con4m_new(T_UTF8, "cstring", info->base_type->name));
    xlist_append(to_join, type_punct[LBRAK_IX]);
    goto first_loop_start;

    for (; i < num_types; i++) {
	xlist_append(to_join, type_punct[COMMA_IX]);

    first_loop_start:
	subnode = xlist_get(info->items, i, NULL);
	substr  = internal_type_repr(subnode, memos, nexttv);

	xlist_append(to_join, substr);
    }

    xlist_append(to_join, type_punct[RBRAK_IX]);

    return string_join(to_join, NULL);
}

// This will get more complicated when we add keyword parameter sypport.

static inline any_str_t *
internal_repr_func(type_details_t *info, dict_t *memos, int64_t *nexttv)
{
    int      num_types = xlist_len(info->items);
    xlist_t *to_join   = con4m_new(T_XLIST);
    int      i         = 0;
    type_spec_t *subnode;
    any_str_t   *substr;



    xlist_append(to_join, type_punct[LPAREN_IX]);

    // num_types - 1 will be 0 if there are no args, but there is a
    // return value. So the below loop won't run in all cases.  But
    // only need to do the test for comma once, not every time through
    // the loop.

    if (num_types > 1) {
	goto first_loop_start;

	for (; i < num_types - 1; i++) {
	    xlist_append(to_join, type_punct[COMMA_IX]);

	first_loop_start:

	    if ((i == num_types - 2) && info->flags & FN_TY_VARARGS) {
		xlist_append(to_join, type_punct[STAR_IX]);
	    }

	    subnode = xlist_get(info->items, i, NULL);
	    substr  = internal_type_repr(subnode, memos, nexttv);
	    xlist_append(to_join, substr);
	}
    }

    xlist_append(to_join, type_punct[RPAREN_IX]);
    xlist_append(to_join, type_punct[ARROW_IX]);

    subnode = xlist_get(info->items, num_types - 1, NULL);
    substr  = internal_type_repr(subnode, memos, nexttv);

    xlist_append(to_join, substr);


    return string_join(to_join, NULL);
}

static any_str_t *
internal_type_repr(type_spec_t *t, dict_t *memos, int64_t *nexttv)
{
    type_details_t *info = t->details;

    init_punctuation();

    switch(info->base_type->base) {
    case BT_nil:
    case BT_primitive:
	return con4m_new(T_UTF8, "cstring", info->base_type->name);
    case BT_type_var:
	return internal_repr_tv(info, memos, nexttv);
    case BT_list:
    case BT_dict:
    case BT_tuple:
	return internal_repr_container(info, memos, nexttv);
    case BT_func:
	return internal_repr_func(info, memos, nexttv);
    default:
	assert(false);
    }

    return NULL;
}

static any_str_t *
con4m_type_spec_repr(type_spec_t *t, to_str_use_t how)
{
    dict_t *memos = con4m_new(T_DICT, HATRACK_DICT_KEY_TYPE_OBJ_PTR);
    int64_t n = 0;

    return internal_type_repr(t, memos, &n);
}

static void
con4m_type_spec_marshal(type_spec_t *n, FILE *f, dict_t *m, int64_t *mid)
{
    marshal_u64(n->typeid, f);
    con4m_sub_marshal(n->details, f, m, mid);
}

static void
con4m_type_spec_unmarshal(type_spec_t *n, FILE *f, dict_t *m)
{
    n->typeid  = unmarshal_u64(f);
    n->details = con4m_sub_unmarshal(f, m);
}

const con4m_vtable type_env_vtable = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	(con4m_vtable_entry)con4m_type_env_init,
	NULL,
	NULL,
	(con4m_vtable_entry)con4m_type_env_marshal,
	(con4m_vtable_entry)con4m_type_env_unmarshal
    }
};

const con4m_vtable type_details_vtable = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	NULL,
	NULL,
	NULL,
	(con4m_vtable_entry)con4m_type_details_marshal,
	(con4m_vtable_entry)con4m_type_details_unmarshal
    }
};

const con4m_vtable type_spec_vtable = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	(con4m_vtable_entry)con4m_type_spec_init,
	(con4m_vtable_entry)con4m_type_spec_repr,
	NULL,
	(con4m_vtable_entry)con4m_type_spec_marshal,
	(con4m_vtable_entry)con4m_type_spec_unmarshal
    }
};

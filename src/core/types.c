#define C4M_USE_INTERNAL_API
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

// Note: because of the type hashing, concrete types in the type
// environment are compatable across type environemnts / universes.
//
// Instead of parameterizing all of the APIs to take a separate
// parameter to the type environment, I've changed the API so that you
// can parameterize when need be, but avoid the extra complexity. All
// we need is to be able to issue type variables that never collide.
//
// To this end, you can register a callback to provide the next type
// identifier. The default callback initializes per-thread, and
// chooses a random 64 bit value. Note that the high bit will always
// be forcefully set by the type system.
c4m_type_universe_t c4m_type_universe;
c4m_type_t         *c4m_bi_types[C4M_NUM_BUILTIN_DTS] = {
    0,
};
c4m_type_t  *c4m_type_node_for_list_of_type_objects;
c4m_arena_t *c4m_early_type_arena = NULL;

static int tspec_len = 0;

#ifdef C4M_TYPE_LOG

static bool log_types = true;

#define type_log(x, y)                               \
    if (log_types) {                                 \
        c4m_printf("[h2]{}:[/] [h1]{}[/] (line {})", \
                   c4m_new_utf8(x),                  \
                   c4m_type_resolve(y),              \
                   c4m_box_i64(__LINE__));           \
    }

void
type_start_log()
{
    log_types = true;
}

void
type_end_log()
{
    log_types = false;
}

#else
#define type_log(x, y)
#endif

#define BASE_OBJ_ALLOC_SZ sizeof(c4m_base_obj_t)
#define TINFO_SZ          sizeof(c4m_type_info_t)

typedef struct {
    c4m_sha_t  *sha;
    c4m_dict_t *memos;
    int         tv_count;
} type_hash_ctx;

static uint64_t            c4m_default_next_typevar(void);
static c4m_next_typevar_fn next_typevar_fn = c4m_default_next_typevar;

void
c4m_type_set_gc_bits(uint64_t *bitfield, c4m_base_obj_t *alloc)
{
    c4m_set_bit(bitfield, c4m_ptr_diff(alloc, alloc->data));
}

void
c4m_type_info_gc_bits(uint64_t *bitfield, c4m_type_info_t *info)
{
    c4m_mark_raw_to_addr(bitfield, info, &info->tsi);
}

void
c4m_set_next_typevar_vn(c4m_next_typevar_fn f)
{
    next_typevar_fn = f ? f : c4m_default_next_typevar;
}

static inline uint64_t
c4m_acquire_typevar_id(void)
{
    return (1ULL << 63) | next_typevar_fn();
}

static pthread_key_t c4m_tvar_start_key;
static pthread_key_t c4m_tvar_next_key;

static void
c4m_thread_release_typevar(void *arg)
{
    pthread_setspecific(c4m_tvar_start_key, NULL);
    pthread_setspecific(c4m_tvar_next_key, NULL);
}

static void
c4m_initialize_next_typevar_fn(void)
{
    pthread_key_create(&c4m_tvar_start_key, c4m_thread_release_typevar);
    pthread_key_create(&c4m_tvar_next_key, NULL);
    uint64_t start = c4m_rand64();
    pthread_setspecific(c4m_tvar_start_key, (void *)start);
    pthread_setspecific(c4m_tvar_next_key, (void *)start);
}

static uint64_t
c4m_default_next_typevar(void)
{
    static pthread_once_t init = PTHREAD_ONCE_INIT;
    pthread_once(&init, c4m_initialize_next_typevar_fn);

    uint64_t tid = (uint64_t)pthread_getspecific(c4m_tvar_next_key);
    pthread_setspecific(c4m_tvar_next_key, (void *)(tid + 1));

    return tid;
}

static inline bool
typeid_is_concrete(c4m_type_hash_t tid)
{
    return !(tid & (1ULL << 63));
}

static c4m_type_t *
c4m_early_alloc_type(c4m_base_obj_t **bptr)
{
    c4m_dt_info_t *tspec;
    tspec = (c4m_dt_info_t *)&c4m_base_type_info[C4M_T_TYPESPEC];

    if (!tspec_len) {
        tspec_len = tspec->alloc_len + BASE_OBJ_ALLOC_SZ;
    }

    c4m_mem_ptr      obj      = (c4m_mem_ptr){.v = c4m_early_alloc(tspec_len)};
    c4m_mem_ptr      hdr      = (c4m_mem_ptr){.alloc = obj.alloc - 1};
    c4m_type_t      *result   = (c4m_type_t *)obj.object->data;
    c4m_mem_ptr      info     = (c4m_mem_ptr){.v = c4m_early_alloc(TINFO_SZ)};
    c4m_mem_ptr      info_hdr = (c4m_mem_ptr){.alloc = info.alloc - 1};
    c4m_type_info_t *ti       = info.v;

    *bptr                      = obj.v;
    hdr.alloc->con4m_obj       = true;
    hdr.alloc->type_obj        = true;
    obj.object->base_data_type = tspec;
    obj.object->concrete_type  = c4m_bi_types[C4M_T_TYPESPEC];
    ti->base_type              = tspec;
    info_hdr.alloc->type_obj   = true; // Mark to have Marshal fix ptrs
    result->details            = ti;

    return result;
}

static inline c4m_list_t *
early_type_list()
{
    size_t      sz     = BASE_OBJ_ALLOC_SZ + sizeof(c4m_list_t);
    c4m_mem_ptr obj    = (c4m_mem_ptr){.v = c4m_early_alloc(sz)};
    c4m_mem_ptr hdr    = (c4m_mem_ptr){.alloc = obj.alloc - 1};
    c4m_list_t *result = (c4m_list_t *)obj.object->data;

    hdr.alloc->con4m_obj = true;

    // The object we're returning is a list of types, so we need to set the
    // object header.
    obj.object->base_data_type = (c4m_dt_info_t *)&c4m_base_type_info[C4M_T_LIST];
    obj.object->concrete_type  = c4m_type_node_for_list_of_type_objects;

    // Create the empty list to hold up to two types.
    result->data      = c4m_early_alloc(sizeof(uint64_t *) * 2);
    result->append_ix = 0;
    result->length    = 2;

    return result;
}

static inline void
setup_list_of_types()
{
    // Does not perform the type hash; do it once the type environment
    // is set up.

    c4m_base_obj_t *base;
    c4m_type_t     *l          = c4m_early_alloc_type(&base);
    l->details->items          = early_type_list();
    l->details->items->data[0] = (void *)c4m_bi_types[C4M_T_TYPESPEC];
    base->concrete_type        = l;
    l->details->base_type      = base->base_data_type;

    c4m_type_node_for_list_of_type_objects = l;
}

// Since types are represented as a graph, as we 'solve' for type
// variables, we often will need to make sure we are looking at the
// resolved type, not variables.
//
// We take the lookup opportunity to add ourselves to the type store
// if we aren't already there.c4m_type_t *
c4m_type_t *
c4m_type_resolve(c4m_type_t *node)
{
    c4m_type_t *next;

    if (!node->typeid) {
        return node;
    }

    if (c4m_universe_add(&c4m_type_universe, node)) {
        return node;
    }

    while (true) {
        next = c4m_universe_get(&c4m_type_universe, node->typeid);

        if (!next) {
            return node;
        }

        if (next == node || node->typeid == next->typeid) {
            return next;
        }

        node = next;
    }
}

void
c4m_lock_type(c4m_type_t *t)
{
    t->details->flags &= C4M_FN_TY_LOCK;
}

static void
internal_type_hash(c4m_type_t *node, type_hash_ctx *ctx)
{
    // This is the recursive bits of the hash... it won't finalize
    // the hash context ever.
    if (node == NULL) {
        return;
    }

    if (node->typeid != 0) {
        node = c4m_type_resolve(node);
    }

    c4m_type_info_t *deets = node->details;
    uint64_t         num_tvars;

    c4m_sha_int_update(ctx->sha, (uint64_t)deets->base_type->typeid);
    c4m_sha_int_update(ctx->sha, (uint64_t)node->details->base_type->dt_kind);

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
        c4m_sha_int_update(ctx->sha, node->typeid);
        break;
    case C4M_DT_KIND_primitive:
    case C4M_DT_KIND_box:
    case C4M_DT_KIND_nil:
    case C4M_DT_KIND_internal:
        return;
    default:; // Do nothing.
    }

    if (!deets->items) {
        return;
    }

    size_t n = c4m_list_len(deets->items);

    c4m_sha_int_update(ctx->sha, n);

    for (size_t i = 0; i < n; i++) {
        c4m_type_t *t = c4m_list_get(deets->items, i, NULL);
        assert(t != node);
        internal_type_hash(t, ctx);
    }
}

static c4m_type_hash_t
type_hash_and_dedupe(c4m_type_t **nodeptr)
{
    // This is only used internally when requesting type objects; if
    // someone requests a concrete type, we keep yielding shared
    // instances, since there is no more resolution that can happen.

    c4m_buf_t    *buf;
    uint64_t      result;
    c4m_type_t   *node = *nodeptr;
    type_hash_ctx ctx;

    switch (node->details->base_type->dt_kind) {
    case C4M_DT_KIND_nil:
    case C4M_DT_KIND_primitive:
    case C4M_DT_KIND_internal:
        node->typeid = node->details->base_type->typeid;
        return c4m_universe_attempt_to_add(&c4m_type_universe, node)->typeid;

    case C4M_DT_KIND_box:
        ctx.sha      = c4m_new(c4m_type_hash());
        ctx.tv_count = 0;
        ctx.memos    = c4m_new_unmanaged_dict(HATRACK_DICT_KEY_TYPE_PTR,
                                           false,
                                           false);

        c4m_type_info_t *deets = node->details;
        c4m_type_t      *base  = (c4m_type_t *)deets->tsi;

        c4m_sha_int_update(ctx.sha, (uint64_t)deets->base_type->dt_kind);
        deets = base->details;
        c4m_sha_int_update(ctx.sha, (uint64_t)deets->base_type->typeid);

        buf    = c4m_sha_finish(ctx.sha);
        result = ((uint64_t *)buf->data)[0];

        little_64(result);
        result &= ~(1LLU << 63);

        node->typeid = result;

        return c4m_universe_attempt_to_add(&c4m_type_universe, node)->typeid;

    case C4M_DT_KIND_type_var:
        c4m_universe_put(&c4m_type_universe, node);
        return node->typeid;
    default:
        node->typeid = 0;
        ctx.sha      = c4m_new(c4m_type_hash());
        ctx.tv_count = 0;
        ctx.memos    = c4m_new_unmanaged_dict(HATRACK_DICT_KEY_TYPE_PTR,
                                           false,
                                           false);

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

        if (!c4m_universe_add(&c4m_type_universe, node)) {
            c4m_type_t *o = c4m_universe_get(&c4m_type_universe, node->typeid);
            if (o->typeid == 0) {
                c4m_universe_put(&c4m_type_universe, node);
            }
            else {
                *nodeptr = c4m_type_resolve(o);
            }
        }
    }
    return result;
}

c4m_type_hash_t
c4m_calculate_type_hash(c4m_type_t *node)
{
    return type_hash_and_dedupe(&node);
}

static inline c4m_type_hash_t
type_rehash(c4m_type_t *node)
{
    node->typeid = 0;
    return c4m_calculate_type_hash(node);
}

static void
internal_add_items_array(c4m_type_t *n)
{
    // Avoid infinite recursion by manually constructing the list.
    // Similar to the `early_type_list()` call, but uses the GC.
    size_t      sz  = BASE_OBJ_ALLOC_SZ + sizeof(c4m_list_t);
    c4m_mem_ptr obj = (c4m_mem_ptr){
        .v = c4m_gc_raw_alloc(sz, C4M_GC_SCAN_ALL),
    };

    c4m_mem_ptr hdr   = {.alloc = obj.alloc - 1};
    c4m_list_t *items = (c4m_list_t *)obj.object->data;

    hdr.alloc->con4m_obj       = true;
    obj.object->base_data_type = (c4m_dt_info_t *)&c4m_base_type_info[C4M_T_XLIST];
    obj.object->concrete_type  = c4m_type_node_for_list_of_type_objects;
    items->data                = c4m_gc_array_alloc(uint64_t *, 4);
    items->length              = 4;
    items->append_ix           = 0;

    n->details->items = items;
}

static void
c4m_type_init(c4m_type_t *n, va_list args)
{
    c4m_builtin_t base_id = va_arg(args, c4m_builtin_t);
    c4m_mem_ptr   p       = (c4m_mem_ptr){.v = n};
    c4m_mem_ptr   ti;

    --p.object; // Back the pointer up by its object header.
    --p.alloc;  // Now back it up to the beginning of the allocation.
    p.alloc->type_obj = true;

    ti.v       = c4m_gc_alloc_mapped(c4m_type_info_t, c4m_type_info_gc_bits);
    n->details = ti.v;

    ti.alloc -= 1;
    ti.alloc->type_obj = true;

    if (base_id == 0) {
        // This short circuit should be used when unmarshaling or
        // when creating an internal container inference type.
        //
        // We basically only jump down to add the items xlist, if
        // needed, which is controlled by a 3rd param after the
        // two typical params.
        if (va_arg(args, int64_t)) {
            internal_add_items_array(n);
        }
        return;
    }

    if (base_id >= C4M_NUM_BUILTIN_DTS || base_id < C4M_T_ERROR) {
        C4M_CRAISE("Invalid type ID.");
    }

    if (base_id >= C4M_T_GENERIC) {
        n->typeid = c4m_acquire_typevar_id();
    }

    c4m_dt_info_t *info = (c4m_dt_info_t *)&c4m_base_type_info[base_id];

    n->details->base_type = info;

    switch (info->dt_kind) {
    case C4M_DT_KIND_nil:
    case C4M_DT_KIND_primitive:
    case C4M_DT_KIND_internal:
        abort();
        break;
    case C4M_DT_KIND_type_var:
    case C4M_DT_KIND_list:
    case C4M_DT_KIND_tuple:
    case C4M_DT_KIND_dict:
    case C4M_DT_KIND_func:
    case C4M_DT_KIND_object:
        internal_add_items_array(n);
        break;
    case C4M_DT_KIND_box:
        // The item will be put in tsi, not the array.
        break;
    default:
        c4m_unreachable();
    }

    assert(n->details != NULL && (((int64_t)n->details) & 0x07) == 0);
}

static c4m_type_t *
tspec_copy_internal(c4m_type_t *node, c4m_dict_t *dupes)
{
    // Copies, anything that might be mutated, returning an unlocked
    // instance where mutation is okay.
    //
    // TODO: this is wrong for general purpose; uses global environment.
    // Need to do a base version that works for any type env.

    node             = c4m_type_resolve(node);
    c4m_type_t *dupe = hatrack_dict_get(dupes, node, NULL);

    if (dupe != NULL) {
        return dupe;
    }

    if (c4m_type_is_concrete(node) || c4m_type_is_box(node)) {
        return node;
    }

    int              n       = c4m_type_get_num_params(node);
    c4m_type_info_t *ts_from = node->details;
    c4m_type_t      *result;

    if (ts_from->base_type->dt_kind == C4M_DT_KIND_type_var) {
        result                 = c4m_new_typevar();
        tv_options_t *oldtsi   = ts_from->tsi;
        tv_options_t *newtsi   = result->details->tsi;
        uint64_t     *old_opts = oldtsi->container_options;
        uint64_t     *new_opts = newtsi->container_options;

        for (int i = 0; i < c4m_get_num_bitfield_words(); i++) {
            new_opts[i] = old_opts[i];
        }

        hatrack_dict_put(dupes, node, result);
        return result;
    }
    else {
        result = c4m_new(c4m_type_typespec(), ts_from->base_type->typeid);
    }

    c4m_list_t *to_copy = c4m_type_get_params(node);
    c4m_list_t *ts_dst  = result->details->items;

    for (int i = 0; i < n; i++) {
        c4m_type_t *original = c4m_list_get(to_copy, i, NULL);
        c4m_type_t *copy     = tspec_copy_internal(original, dupes);

        c4m_list_append(ts_dst, copy);
    }

    c4m_calculate_type_hash(result);

    hatrack_dict_put(dupes, node, result);
    return result;
}

c4m_type_t *
c4m_type_copy(c4m_type_t *node)
{
    c4m_dict_t *dupes = c4m_dict(c4m_type_ref(), c4m_type_ref());

    return tspec_copy_internal(node, dupes);
}

static c4m_type_t *
copy_if_needed(c4m_type_t *t)
{
    if (c4m_type_is_concrete(t)) {
        return t;
    }

    if (!c4m_type_is_locked(t)) {
        return t;
    }

    return c4m_type_copy(t);
}

bool
c4m_type_is_concrete(c4m_type_t *node)
{
    int         n;
    c4m_list_t *param;

    node = c4m_type_resolve(node);

    // Fast path most of the time; just if we decide to check
    // before an initial tid is set, take the long road.
    if (node->typeid != 0) {
        return typeid_is_concrete(node->typeid);
    }

    switch (c4m_type_get_base(node)) {
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
        n     = c4m_type_get_num_params(node);
        param = c4m_type_get_params(node);

        for (int i = 0; i < n; i++) {
            if (!c4m_type_is_concrete(c4m_list_get(param, i, NULL))) {
                return false;
            }
        }
        return true;
    default:
        assert(false);
    }
}

// Just makes things a bit more clear.
#define type_error c4m_type_error

static c4m_type_t *
unify_type_variables(c4m_type_t *t1, c4m_type_t *t2)
{
    int           bf_words    = c4m_get_num_bitfield_words();
    int64_t       num_options = 0;
    c4m_type_t   *result      = c4m_new_typevar();
    tv_options_t *t1_opts     = t1->details->tsi;
    tv_options_t *t2_opts     = t2->details->tsi;
    tv_options_t *res_opts    = result->details->tsi;
    int           t1_arg_ct   = c4m_type_get_num_params(t1);
    bool          t1_fixed_ct = !(t1->details->flags & C4M_FN_UNKNOWN_TV_LEN);
    int           t2_arg_ct   = c4m_type_get_num_params(t2);
    bool          t2_fixed_ct = !(t2->details->flags & C4M_FN_UNKNOWN_TV_LEN);

    for (int i = 0; i < bf_words; i++) {
        uint64_t one    = t1_opts->container_options[i];
        uint64_t other  = t2_opts->container_options[i];
        uint64_t merged = one & other;

        num_options += __builtin_popcountll(merged);
        res_opts->container_options[i] = merged;
    }

    if (num_options == 0) {
        type_log("unify(t1, t2)", type_error());
        return type_error();
    }

    // Okay, now we need to look at any info we have on type
    // parameters.

    int nparams = c4m_max(t1_arg_ct, t2_arg_ct);

    for (int i = 0; i < nparams; i++) {
        if (i >= t1_arg_ct) {
            c4m_list_append(result->details->items,
                            c4m_type_get_param(t2, i));
        }
        else {
            if (i >= t2_arg_ct) {
                c4m_list_append(result->details->items,
                                c4m_type_get_param(t1, i));
            }
            else {
                c4m_type_t *sub1 = c4m_type_get_param(t1, i);
                c4m_type_t *sub2 = c4m_type_get_param(t2, i);
                c4m_type_t *sub3 = c4m_unify(sub1, sub2);
                if (c4m_type_is_error(sub3)) {
                    type_log("unify(t1, t2)", type_error());
                    return type_error();
                }

                c4m_list_append(result->details->items, sub3);
            }
        }
    }

    bool list_ok      = c4m_tuple_syntax_possible(result);
    bool set_ok       = c4m_set_syntax_possible(result);
    bool dict_ok      = c4m_dict_syntax_possible(result);
    bool tuple_ok     = c4m_tuple_syntax_possible(result);
    bool known_arglen = t1_fixed_ct | t2_fixed_ct;
    bool recal_pop    = false;

    if (known_arglen) {
        switch (nparams) {
        case 1:
            if (dict_ok) {
                c4m_remove_dict_options(result);
                recal_pop = true;
            }
            if (tuple_ok) {
                c4m_remove_tuple_options(result);
                recal_pop = true;
            }
            break;
        case 2:
            if (list_ok) {
                c4m_remove_tuple_options(result);
                recal_pop = true;
            }
            if (set_ok) {
                c4m_remove_set_options(result);
                recal_pop = true;
            }
            break;
        default:
            // Has to be tuple syntax.
            if (list_ok) {
                c4m_remove_tuple_options(result);
                recal_pop = true;
            }
            if (set_ok) {
                c4m_remove_set_options(result);
                recal_pop = true;
            }
            if (dict_ok) {
                c4m_remove_dict_options(result);
                recal_pop = true;
            }
            break;
        }
    }

    if (!dict_ok && !tuple_ok) {
        known_arglen = true;

        if (nparams > 1) {
            type_log("unify(t1, t2)", type_error());
            return type_error();
        }
        if (nparams == 0) {
            c4m_list_append(result->details->items, c4m_new_typevar());
        }
    }

    if (!tuple_ok && !list_ok && !set_ok) {
        known_arglen = true;

        if (nparams > 2) {
            type_log("unify(t1, t2)", type_error());
            return type_error();
        }
        while (nparams < 2) {
            c4m_list_append(result->details->items, c4m_new_typevar());
        }
    }

    if (known_arglen) {
        result->details->flags = 0;
    }
    else {
        result->details->flags = C4M_FN_UNKNOWN_TV_LEN;
    }

    if (recal_pop) {
        num_options = 0;

        for (int i = 0; i < bf_words; i++) {
            num_options += __builtin_popcountll(res_opts->container_options[i]);
        }
    }

    if (num_options == 0) {
        if (t1_arg_ct || t2_arg_ct) {
            type_log("unify(t1, t2)", type_error());
            return type_error();
        }
        // Otherwise, it's a type variable that MUST point to a concrete type.
    }
    if (num_options == 1) {
        uint64_t baseid = 0;

        for (int i = 0; i < bf_words; i++) {
            uint64_t val = res_opts->container_options[i];

            // It at least used to be the case that clzll on an empty
            // word gives arch dependent results (either 64 or 63).
            // So special case to be safe.
            if (!val) {
                baseid += 64;
                continue;
            }

            // If the 0th bit is set, we want to return 0.
            // If the 64th bit is set, we want to return 63.
            baseid += 63 - __builtin_clzll(val);
            break;
        }

        c4m_type_t *last = c4m_type_get_param(result, nparams - 1);

        if (t1_opts->value_type != 0) {
            // This is supposed to be caught by the index node not
            // being a constant, but that's not done yet.
            if (tuple_ok) {
                type_log("unify(t1, t2)", type_error());
                return type_error();
            }

            if (c4m_type_is_error(c4m_unify(t1_opts->value_type, last))) {
                type_log("unify(t1, t2)", type_error());
                return type_error();
            }
        }

        if (t2_opts->value_type != 0) {
            if (tuple_ok) {
                type_log("unify(t1, t2)", type_error());
                return type_error();
            }

            if (c4m_type_is_error(c4m_unify(t1_opts->value_type, last))) {
                type_log("unify(t1, t2)", type_error());
                return type_error();
            }
        }
        c4m_dt_info_t *dt_info = (c4m_dt_info_t *)&c4m_base_type_info[baseid];

        result->details->base_type = dt_info;
        type_rehash(result);
    }
    else {
        if (!t1_opts->value_type && t2_opts->value_type) {
            t1_opts->value_type = c4m_new_typevar();
        }

        if (!t2_opts->value_type && t1_opts->value_type) {
            t2_opts->value_type = c4m_new_typevar();
        }

        if (t1_opts->value_type) {
            if (c4m_type_is_error(c4m_unify(t1_opts->value_type,
                                            t2_opts->value_type))) {
                type_log("unify(t1, t2)", type_error());
                return type_error();
            }
        }
    }

    c4m_universe_forward(&c4m_type_universe, t1, result);
    c4m_universe_forward(&c4m_type_universe, t2, result);

    if (nparams != 0 && res_opts->value_type != NULL) {
        if (c4m_type_is_error(c4m_unify(t1_opts->value_type,
                                        t2_opts->value_type))) {
            if (c4m_type_is_error(
                    c4m_unify(res_opts->value_type,
                              c4m_type_get_param(result,
                                                 nparams - 1)))) {
                type_log("unify(t1, t2)", type_error());
                return type_error();
            }
        }
    }

    type_log("unify(t1, t2)", result);
    return result;
}

static c4m_type_t *
unify_tv_with_concrete_type(c4m_type_t *t1,
                            c4m_type_t *t2)
{
    switch (t2->details->base_type->dt_kind) {
    case C4M_DT_KIND_primitive:
    case C4M_DT_KIND_internal:
    case C4M_DT_KIND_func:
    case C4M_DT_KIND_nil:
        c4m_universe_forward(&c4m_type_universe, t1, t2);
        type_log("unify(t1, t2)", t2);
        return t2;
    default:
        break;
    }

    uint32_t      baseid  = (uint32_t)t2->details->base_type->typeid;
    int           word    = ((int)baseid) / 64;
    int           bit     = ((int)baseid) % 64;
    tv_options_t *t1_opts = t1->details->tsi;

    if (!(t1_opts->container_options[word] & (1UL << bit))) {
        type_log("unify(t1, t2)", type_error());
        return type_error();
    }

    for (int i = 0; i < c4m_type_get_num_params(t1); i++) {
        c4m_type_t *sub = c4m_unify(c4m_type_get_param(t1, i),
                                    c4m_type_get_param(t2, i));

        if (c4m_type_is_error(sub)) {
            type_log("unify(t1, t2)", type_error());
            return sub;
        }
    }

    if (t1_opts->value_type != 0) {
        if (c4m_type_has_tuple_syntax(t2)) {
            // This is supposed to be caught by the index node not
            // being a constant, but that's not done yet.
            return type_error();
        }
        int         n   = c4m_type_get_num_params(t2);
        c4m_type_t *sub = c4m_unify(c4m_type_get_param(t2, n - 1),
                                    t1_opts->value_type);

        if (c4m_type_is_error(sub)) {
            type_log("unify(t1, t2)", type_error());
            return sub;
        }
    }

    type_rehash(t2);
    c4m_universe_forward(&c4m_type_universe, t1, t2);
    type_log("unify(t1, t2)", t2);
    return t2;
}

c4m_type_t *
c4m_unify(c4m_type_t *t1, c4m_type_t *t2)
{
    c4m_type_t *result;
    c4m_type_t *sub1;
    c4m_type_t *sub2;
    c4m_type_t *sub_result;
    c4m_list_t *p1;
    c4m_list_t *p2;
    c4m_list_t *new_subs;
    int         num_params = 0;

    if (!t1 || !t2) {
        return c4m_type_error();
    }

    t1 = c4m_type_resolve(t1);
    t2 = c4m_type_resolve(t2);

    t1 = copy_if_needed(t1);
    t2 = copy_if_needed(t2);

    // If comparing types w/ something boxed, ignored the box.
    if (c4m_type_is_box(t1)) {
        t1 = c4m_type_unbox(t1);
        if (c4m_type_is_box(t2)) {
            t2 = c4m_type_unbox(t2);
        }
        return c4m_unify(t1, t2);
    }

    if (c4m_type_is_box(t2)) {
        t2 = c4m_type_unbox(t2);
        return c4m_unify(t1, t2);
    }

    if (t1->typeid == 0 && t1->details->base_type->typeid == 0) {
        return type_error();
    }

    if (t2->typeid == 0 && t2->details->base_type->typeid == 0) {
        return type_error();
    }

    // This is going to re-check the structure, just to cover any
    // cases where we didn't or couldn't update it before.
    //
    // This needs to be here, for reasons I don't even understand;
    // I'm clearly missing a needed rehash in a previous call.
    if (c4m_type_is_concrete(t1)) {
        type_rehash(t1);
    }
    if (c4m_type_is_concrete(t2)) {
        type_rehash(t2);
    }

    type_log("t1", t1);
    type_log("t2", t2);

    if (t1->typeid == t2->typeid) {
        type_log("unify(t1, t2)", t1);
        return t1;
    }

    // Currently, treat utf8 and utf32 as the same type, until all ops
    // are available on each. We'll just always return C4M_T_UTF8 in
    // these cases.
    if (t1->typeid == C4M_T_UTF8 && t2->typeid == C4M_T_UTF32) {
        type_log("unify(t1, t2)", t1);
        return t1;
    }
    if (t1->typeid == C4M_T_UTF32 && t2->typeid == C4M_T_UTF8) {
        type_log("unify(t1, t2)", t2);
        return t2;
    }

    if (t1->typeid == C4M_T_ERROR || t2->typeid == C4M_T_ERROR) {
        type_log("unify(t1, t2)", type_error());
        return type_error();
    }

    if (c4m_type_is_concrete(t1) && c4m_type_is_concrete(t2)) {
        // Concrete, but not the same. Types are not equivolent.
        // While casting may be possible, that doesn't happen here;
        // unification is about type equivolence, not coercion!
        type_log("unify(t1, t2)", type_error());
        return type_error();
    }

    c4m_dt_kind_t b1 = c4m_type_get_base(t1);
    c4m_dt_kind_t b2 = c4m_type_get_base(t2);

    if (b1 != b2) {
        if (b1 != C4M_DT_KIND_type_var) {
            if (b2 != C4M_DT_KIND_type_var) {
                type_log("unify(t1, t2)", type_error());
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

    switch (b1) {
    case C4M_DT_KIND_type_var:

        if (b2 == C4M_DT_KIND_type_var) {
            return unify_type_variables(t1, t2);
        }
        else {
            return unify_tv_with_concrete_type(t1, t2);
        }

    case C4M_DT_KIND_list:
    case C4M_DT_KIND_dict:
    case C4M_DT_KIND_tuple:
        num_params = c4m_type_get_num_params(t1);

        if (num_params != c4m_type_get_num_params(t2)) {
            type_log("unify(t1, t2)", type_error());
            return type_error();
        }

unify_sub_nodes:
        p1       = c4m_type_get_params(t1);
        p2       = c4m_type_get_params(t2);
        new_subs = c4m_new(c4m_type_list(c4m_type_typespec()),
                           c4m_kw("length", c4m_ka(num_params)));

        for (int i = 0; i < num_params; i++) {
            sub1 = c4m_list_get(p1, i, NULL);
            sub2 = c4m_list_get(p2, i, NULL);

            if (sub1 == NULL) {
                sub1 = c4m_new_typevar();
                c4m_list_set(p1, i, sub1);
            }
            if (sub2 == NULL) {
                sub2 = c4m_new_typevar();
                c4m_list_set(p1, i, sub2);
            }

            sub_result = c4m_unify(sub1, sub2);

            if (c4m_type_is_error(sub_result)) {
                type_log("unify(t1, t2)", sub_result);
                return sub_result;
            }
            c4m_list_append(new_subs, sub_result);
        }

        result                 = t1;
        result->details->items = new_subs;
        break;
    case C4M_DT_KIND_func: {
        int f1_params = c4m_type_get_num_params(t1);
        int f2_params = c4m_type_get_num_params(t2);

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
        if (!((t1->details->flags & t2->details->flags) & C4M_FN_TY_VARARGS)) {
            if (f1_params != f2_params) {
                type_log("unify(t1, t2)", type_error());
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
            type_log("unify(t1, t2)", type_error());
            return type_error();
        }

        // The last item is always the return type, so we have to
        // unify the last items, plus any items before varargs.  Then,
        // if there are any items in type2, they each need to unify
        // with t1's va;l;rargs parameter.
        p1       = c4m_type_get_params(t1);
        p2       = c4m_type_get_params(t2);
        new_subs = c4m_new(c4m_type_list(c4m_type_typespec()),
                           c4m_kw("length", c4m_ka(num_params)));

        for (int i = 0; i < f1_params - 2; i++) {
            sub1       = c4m_list_get(p1, i, NULL);
            sub2       = c4m_list_get(p2, i, NULL);
            sub_result = c4m_unify(sub1, sub2);

            c4m_list_append(new_subs, sub_result);
        }

        // This checks the return type.
        sub1 = c4m_list_get(p1, f1_params - 1, NULL);
        sub2 = c4m_list_get(p2, f2_params - 1, NULL);

        if (!sub1) {
            sub1 = c4m_type_void();
        }
        if (!sub2) {
            sub2 = c4m_type_void();
        }

        sub_result = c4m_unify(sub1, sub2);

        if (c4m_type_is_error(sub_result)) {
            type_log("unify(t1, t2)", sub_result);
            return sub_result;
        }
        // Now, check any varargs.
        sub1 = c4m_list_get(p1, f1_params - 2, NULL);

        for (int i = c4m_max(f1_params - 2, 0); i < f2_params - 1; i++) {
            sub2       = c4m_list_get(p2, i, NULL);
            sub_result = c4m_unify(sub1, sub2);
            if (c4m_type_is_error(sub_result)) {
                type_log("unify(t1, t2)", sub_result);
                return sub_result;
            }
        }

        result = t1;

        break;
    }
    case C4M_DT_KIND_object:
    case C4M_DT_KIND_nil:
    case C4M_DT_KIND_primitive:
    case C4M_DT_KIND_internal:
    case C4M_DT_KIND_maybe:
    case C4M_DT_KIND_oneof:
    default:
        // Either not implemented yet or covered before the switch.
        // These are all implemented in the Nim checker but won't
        // be moved until Con4m is using them.
        c4m_unreachable();
    }

    type_hash_and_dedupe(&result);

    type_log("unify(t1, t2)", result);
    return result;
}

// 'exact' match is mainly used for comparing declarations to
// other types. It needs to ignore boxes though.
//
// TODO: Need to revisit this with the new generics.
//
c4m_type_exact_result_t
c4m_type_cmp_exact(c4m_type_t *t1, c4m_type_t *t2)
{
    t1 = c4m_type_resolve(t1);
    t2 = c4m_type_resolve(t2);

    if (c4m_type_is_box(t1)) {
        t1 = c4m_type_unbox(t1);
    }

    if (c4m_type_is_box(t2)) {
        t2 = c4m_type_unbox(t2);
    }

    if (t1->typeid == t2->typeid) {
        return c4m_type_match_exact;
    }

    if (t1->typeid == C4M_T_UTF8 && t2->typeid == C4M_T_UTF32) {
        return c4m_type_match_exact;
    }
    if (t1->typeid == C4M_T_UTF32 && t2->typeid == C4M_T_UTF8) {
        return c4m_type_match_exact;
    }

    if (t1->typeid == C4M_T_ERROR || t2->typeid == C4M_T_ERROR) {
        return c4m_type_cant_match;
    }

    c4m_dt_kind_t b1 = c4m_type_get_base(t1);
    c4m_dt_kind_t b2 = c4m_type_get_base(t2);

    if (b1 != b2) {
        if (b1 == C4M_DT_KIND_type_var) {
            return c4m_type_match_right_more_specific;
        }
        if (b2 == C4M_DT_KIND_type_var) {
            return c4m_type_match_left_more_specific;
        }

        return c4m_type_cant_match;
    }

    int  n1    = c4m_type_get_num_params(t1);
    int  n2    = c4m_type_get_num_params(t2);
    bool err   = false;
    bool left  = false;
    bool right = false;

    if (n1 != n2) {
        return c4m_type_cant_match;
    }

    if ((t1->details->flags ^ t2->details->flags) & C4M_FN_TY_VARARGS) {
        return c4m_type_cant_match;
    }

    for (int i = 0; i < n1; i++) {
        c4m_list_t *p1;
        c4m_list_t *p2;
        c4m_type_t *sub1;
        c4m_type_t *sub2;

        p1   = c4m_type_get_params(t1);
        p2   = c4m_type_get_params(t2);
        sub1 = c4m_list_get(p1, i, NULL);
        sub2 = c4m_list_get(p2, i, NULL);

        switch (c4m_type_cmp_exact(sub1, sub2)) {
        case c4m_type_match_exact:
            continue;
        case c4m_type_cant_match:
            return c4m_type_cant_match;
        case c4m_type_match_left_more_specific:
            err  = true;
            left = true;
            continue;
        case c4m_type_match_right_more_specific:
            err   = true;
            right = true;
            continue;
        case c4m_type_match_both_have_more_generic_bits:
            err   = true;
            left  = true;
            right = true;
            continue;
        }
    }

    if (!err) {
        return c4m_type_match_exact;
    }
    if (left && right) {
        return c4m_type_match_both_have_more_generic_bits;
    }
    if (left) {
        return c4m_type_match_left_more_specific;
    }
    else { // right only
        return c4m_type_match_right_more_specific;
    }
}

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

    return c4m_new(c4m_type_utf8(), c4m_kw("cstring", c4m_ka(buf)));
}

static inline c4m_str_t *
internal_repr_tv(c4m_type_t *t, c4m_dict_t *memos, int64_t *nexttv)
{
    c4m_str_t *s = hatrack_dict_get(memos, t, NULL);

    if (s != NULL) {
        return s;
    }

    if (c4m_partial_inference(t)) {
        bool        list_ok  = c4m_list_syntax_possible(t);
        bool        set_ok   = c4m_set_syntax_possible(t);
        bool        dict_ok  = c4m_dict_syntax_possible(t);
        bool        tuple_ok = c4m_tuple_syntax_possible(t);
        int         num_ok   = 0;
        c4m_list_t *parts    = c4m_list(c4m_type_utf8());
        c4m_utf8_t *res;

        if (list_ok) {
            num_ok++;
            c4m_list_append(parts, c4m_new_utf8("some_list"));
        }
        // For now, hardcode knowing we don't expose other options.
        if (dict_ok) {
            num_ok++;
            c4m_list_append(parts, c4m_new_utf8("dict"));
        }
        if (set_ok) {
            num_ok++;
            c4m_list_append(parts, c4m_new_utf8("set"));
        }
        if (tuple_ok) {
            c4m_list_append(parts, c4m_new_utf8("tuple"));
            num_ok++;
        }

        switch (num_ok) {
        case 0:
            return c4m_new_utf8("$non_container");
        case 1:
            if (list_ok) {
                res = c4m_new_utf8("$some_list[");
            }
            else {
                res = c4m_cstr_format("{}{}",
                                      c4m_list_get(parts, 0, NULL),
                                      c4m_new_utf8("["));
            }
            break;
        default:
            res = c4m_cstr_format("${}{}",
                                  c4m_str_join(parts,
                                               c4m_new_utf8("_or_")),
                                  c4m_new_utf8("["));
            break;
        }

        int num = c4m_type_get_num_params(t);

        if (num) {
            c4m_type_t *sub = c4m_type_get_param(t, 0);
            c4m_utf8_t *one = c4m_internal_type_repr(sub, memos, nexttv);

            res = c4m_cstr_format("{}{}", res, one);
        }

        for (int i = 1; i < num; i++) {
            c4m_utf8_t *one = c4m_internal_type_repr(c4m_type_get_param(t, i),
                                                     memos,
                                                     nexttv);

            res = c4m_cstr_format("{}, {}", res, one);
        }

        if (t->details->flags & C4M_FN_UNKNOWN_TV_LEN) {
            res = c4m_cstr_format("{}...]", res);
        }
        else {
            res = c4m_cstr_format("{}]", res);
        }
        return res;
    }

    if (t->details->name != NULL) {
        s = c4m_new(c4m_type_utf8(),
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
internal_repr_container(c4m_type_info_t *info,
                        c4m_dict_t      *memos,
                        int64_t         *nexttv)
{
    int         num_types = c4m_list_len(info->items);
    c4m_list_t *to_join   = c4m_list(c4m_type_utf8());
    int         i         = 0;
    c4m_type_t *subnode;
    c4m_str_t  *substr;

    c4m_list_append(to_join,
                    c4m_new(c4m_type_utf8(),
                            c4m_kw("cstring",
                                   c4m_ka(info->base_type->name))));
    c4m_list_append(to_join, c4m_get_lbrak_const());
    goto first_loop_start;

    for (; i < num_types; i++) {
        c4m_list_append(to_join, c4m_get_comma_const());

first_loop_start:
        subnode = c4m_list_get(info->items, i, NULL);

        substr = c4m_internal_type_repr(subnode, memos, nexttv);

        c4m_list_append(to_join, substr);
    }

    c4m_list_append(to_join, c4m_get_rbrak_const());

    return c4m_str_join(to_join, c4m_empty_string());
}

// This will get more complicated when we add keyword parameter sypport.

static inline c4m_str_t *
internal_repr_func(c4m_type_info_t *info, c4m_dict_t *memos, int64_t *nexttv)
{
    int         num_types = c4m_list_len(info->items);
    c4m_list_t *to_join   = c4m_list(c4m_type_utf8());
    int         i         = 0;
    c4m_type_t *subnode;
    c4m_str_t  *substr;

    c4m_list_append(to_join, c4m_get_lparen_const());

    // num_types - 1 will be 0 if there are no args, but there is a
    // return value. So the below loop won't run in all cases.  But
    // only need to do the test for comma once, not every time through
    // the loop.

    if (num_types > 1) {
        goto first_loop_start;

        for (; i < num_types - 1; i++) {
            c4m_list_append(to_join, c4m_get_comma_const());

first_loop_start:

            if ((i == num_types - 2) && info->flags & C4M_FN_TY_VARARGS) {
                c4m_list_append(to_join, c4m_get_asterisk_const());
            }

            subnode = c4m_list_get(info->items, i, NULL);
            substr  = c4m_internal_type_repr(subnode, memos, nexttv);
            c4m_list_append(to_join, substr);
        }
    }

    c4m_list_append(to_join, c4m_get_rparen_const());
    c4m_list_append(to_join, c4m_get_arrow_const());

    subnode = c4m_list_get(info->items, num_types - 1, NULL);

    if (subnode) {
        substr = c4m_internal_type_repr(subnode, memos, nexttv);
        c4m_list_append(to_join, substr);
    }

    return c4m_str_join(to_join, c4m_empty_string());
}

static c4m_str_t *c4m_type_repr(c4m_type_t *);

c4m_str_t *
c4m_internal_type_repr(c4m_type_t *t, c4m_dict_t *memos, int64_t *nexttv)
{
    t = c4m_type_resolve(t);

    c4m_type_info_t *info = t->details;

    switch (info->base_type->dt_kind) {
    case C4M_DT_KIND_nil:
    case C4M_DT_KIND_primitive:
    case C4M_DT_KIND_internal:
        return c4m_new_utf8(info->base_type->name);
    case C4M_DT_KIND_type_var:
        return internal_repr_tv(t, memos, nexttv);
    case C4M_DT_KIND_list:
    case C4M_DT_KIND_dict:
    case C4M_DT_KIND_tuple:
        return internal_repr_container(info, memos, nexttv);
    case C4M_DT_KIND_func:
        return internal_repr_func(info, memos, nexttv);
    case C4M_DT_KIND_box:
        return c4m_cstr_format("{}(boxed)",
                               c4m_internal_type_repr((c4m_type_t *)info->tsi,
                                                      memos,
                                                      nexttv));
    default:
        assert(false);
    }

    return NULL;
}

static c4m_str_t *
c4m_type_repr(c4m_type_t *t)
{
    c4m_dict_t *memos = c4m_dict(c4m_type_ref(), c4m_type_utf8());
    int64_t     n     = 0;

    return c4m_internal_type_repr(c4m_type_resolve(t), memos, &n);
}

const c4m_vtable_t c4m_type_spec_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)c4m_type_init,
        [C4M_BI_REPR]        = (c4m_vtable_entry)c4m_type_repr,
        [C4M_BI_COPY]        = (c4m_vtable_entry)c4m_type_copy,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)c4m_type_set_gc_bits,
        [C4M_BI_FINALIZER]   = NULL,
    },
};

static void
setup_primitive_types()
{
    c4m_base_obj_t *base;
    c4m_type_t     *origin = c4m_early_alloc_type(&base);

    base->concrete_type          = origin;
    origin->typeid               = C4M_T_TYPESPEC;
    c4m_bi_types[C4M_T_TYPESPEC] = origin;

    c4m_universe_put(&c4m_type_universe, origin);

    for (int i = 0; i < C4M_NUM_BUILTIN_DTS; i++) {
        if (i == C4M_T_TYPESPEC) {
            continue;
        }

        c4m_dt_info_t *one_spec = (c4m_dt_info_t *)&c4m_base_type_info[i];

        switch (one_spec->dt_kind) {
        case C4M_DT_KIND_nil:
        case C4M_DT_KIND_primitive:
        case C4M_DT_KIND_internal:;
            c4m_type_t *t         = c4m_early_alloc_type(&base);
            t->typeid             = i;
            t->details->base_type = one_spec;
            t->details->items     = NULL;
            c4m_bi_types[i]       = t;

            if (i) {
                c4m_universe_put(&c4m_type_universe, t);
            }
            continue;
        default:
            continue;
        }
    }
}

void
c4m_initialize_global_types()
{
    c4m_early_type_arena = c4m_new_arena(1 << 15, NULL);
    c4m_add_static_segment(c4m_early_type_arena,
                           c4m_early_type_arena->heap_end);
    c4m_gc_register_root(&c4m_type_universe.store, 1);
    c4m_universe_init(&c4m_type_universe);

    setup_primitive_types();
    setup_list_of_types();
    c4m_calculate_type_hash(c4m_type_node_for_list_of_type_objects);
}

#if defined(C4M_GC_STATS) || defined(C4M_DEBUG)
#define DECLARE_ONE_PARAM_FN(tname, idnumber)                       \
    c4m_type_t *                                                    \
        _c4m_type_##tname(c4m_type_t *sub, char *file, int line)    \
    {                                                               \
        c4m_type_t *ts     = c4m_type_typespec();                   \
        c4m_type_t *result = _c4m_new(file, line, ts, idnumber, 0); \
        c4m_list_t *items  = result->details->items;                \
        c4m_list_append(items, sub);                                \
                                                                    \
        type_hash_and_dedupe(&result);                              \
                                                                    \
        return result;                                              \
    }

#else
#define DECLARE_ONE_PARAM_FN(tname, idnumber)        \
    c4m_type_t *                                     \
        c4m_type_##tname(c4m_type_t *sub)            \
    {                                                \
        c4m_type_t *ts     = c4m_type_typespec();    \
        c4m_type_t *result = c4m_new(ts, idnumber);  \
        c4m_list_t *items  = result->details->items; \
        c4m_list_append(items, sub);                 \
                                                     \
        type_hash_and_dedupe(&result);               \
                                                     \
        return result;                               \
    }
#endif

extern int c4m_current_test_case;
extern int c4m_watch_case;
extern int TMP_DEBUG;

c4m_type_t *
_c4m_type_list(c4m_type_t *sub, char *file, int line)
{
    c4m_type_t *result = _c4m_new(file,
                                  line,
                                  c4m_type_typespec(),
                                  C4M_T_LIST);

    c4m_list_t *items = result->details->items;
    c4m_list_append(items, sub);
    type_hash_and_dedupe(&result);

    return result;
}

DECLARE_ONE_PARAM_FN(flist, C4M_T_FLIST);
// DECLARE_ONE_PARAM_FN(list, C4M_T_XLIST);
DECLARE_ONE_PARAM_FN(xlist, C4M_T_XLIST);
DECLARE_ONE_PARAM_FN(tree, C4M_T_TREE);
DECLARE_ONE_PARAM_FN(queue, C4M_T_QUEUE);
DECLARE_ONE_PARAM_FN(ring, C4M_T_RING);
DECLARE_ONE_PARAM_FN(stack, C4M_T_STACK);
DECLARE_ONE_PARAM_FN(set, C4M_T_SET);

c4m_type_t *
c4m_type_box(c4m_type_t *sub)
{
    c4m_type_t *result = c4m_new(c4m_type_typespec(), C4M_T_BOX);

    result->details->tsi = sub;

    return result;
}

c4m_type_t *
c4m_type_dict(c4m_type_t *sub1, c4m_type_t *sub2)
{
    c4m_type_t *result = c4m_new(c4m_type_typespec(),
                                 C4M_T_DICT);
    c4m_list_t *items  = result->details->items;

    c4m_list_append(items, sub1);
    c4m_list_append(items, sub2);

    type_hash_and_dedupe(&result);

    return result;
}

c4m_type_t *
c4m_type_tuple(int64_t nitems, ...)
{
    va_list     args;
    c4m_type_t *result = c4m_new(c4m_type_typespec(),
                                 C4M_T_TUPLE);
    c4m_list_t *items  = result->details->items;

    va_start(args, nitems);

    if (nitems <= 1) {
        C4M_CRAISE("Tuples must contain 2 or more items.");
    }

    for (int i = 0; i < nitems; i++) {
        c4m_type_t *sub = va_arg(args, c4m_type_t *);
        c4m_list_append(items, sub);
    }

    type_hash_and_dedupe(&result);

    return result;
}

c4m_type_t *
c4m_type_tuple_from_xlist(c4m_list_t *items)
{
    c4m_type_t *result = c4m_new(c4m_type_typespec(),
                                 C4M_T_TUPLE);

    result->details->items = items;

    type_hash_and_dedupe(&result);

    return result;
}

c4m_type_t *
c4m_type_fn_va(c4m_type_t *return_type, int64_t nparams, ...)
{
    va_list     args;
    c4m_type_t *result = c4m_new(c4m_type_typespec(),
                                 C4M_T_FUNCDEF);
    c4m_list_t *items  = result->details->items;

    va_start(args, nparams);

    for (int i = 0; i < nparams; i++) {
        c4m_type_t *sub = va_arg(args, c4m_type_t *);
        c4m_list_append(items, sub);
    }
    c4m_list_append(items, return_type);

    type_hash_and_dedupe(&result);

    return result;
}

// This one explicitly sets the varargs flag, as opposed to the one above that
// simply just takes variable # of args as an input.
c4m_type_t *
c4m_type_varargs_fn(c4m_type_t *return_type, int64_t nparams, ...)
{
    va_list     args;
    c4m_type_t *result = c4m_new(c4m_type_typespec(),
                                 C4M_T_FUNCDEF);
    c4m_list_t *items  = result->details->items;

    va_start(args, nparams);

    if (nparams < 1) {
        C4M_CRAISE("Varargs functions require at least one argument.");
    }

    for (int i = 0; i < nparams; i++) {
        c4m_type_t *sub = va_arg(args, c4m_type_t *);
        c4m_list_append(items, sub);
    }
    c4m_list_append(items, return_type);
    result->details->flags |= C4M_FN_TY_VARARGS;

    type_hash_and_dedupe(&result);

    return result;
}

c4m_type_t *
c4m_type_fn(c4m_type_t *ret, c4m_list_t *params, bool va)
{
    c4m_type_t *result = c4m_new(c4m_type_typespec(),
                                 C4M_T_FUNCDEF);
    c4m_list_t *items  = result->details->items;
    int         n      = c4m_list_len(params);

    // Copy the list to be safe.
    for (int i = 0; i < n; i++) {
        c4m_list_append(items, c4m_list_get(params, i, NULL));
    }
    c4m_list_append(items, ret);

    if (va) {
        result->details->flags |= C4M_FN_TY_VARARGS;
    }
    type_hash_and_dedupe(&result);

    return result;
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

    t1 = c4m_type_resolve(t1);
    t2 = c4m_type_resolve(t2);

    if (c4m_type_is_box(t1)) {
        t1 = c4m_type_unbox(t1);
    }
    if (c4m_type_is_box(t2)) {
        t2 = c4m_type_unbox(t2);
    }

    c4m_type_hash_t id1 = c4m_type_get_data_type_info(t1)->typeid;
    c4m_type_hash_t id2 = c4m_type_get_data_type_info(t2)->typeid;

    // clang-format off
     if (id1 < C4M_T_I8 || id1 > C4M_T_UINT ||
         id2 < C4M_T_I8 || id2 > C4M_T_UINT) {
        // clang-format on
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
            return c4m_type_i64();
        default:
            return c4m_type_u64();
        }
    case C4M_T_U32:
    case C4M_T_CHAR:
        switch (id2) {
        case C4M_T_I32:
            *warning = 1;
            // fallthrough
        case C4M_T_I8:
            return c4m_type_i32();
        default:
            return c4m_type_u32();
        }
    case C4M_T_BYTE:
        if (id2 != C4M_T_BYTE) {
            *warning = 1;
            return c4m_type_i8();
        }
        return c4m_type_byte();
    case C4M_T_INT:
        return c4m_type_i64();
    case C4M_T_I32:
        return c4m_type_i32();
    default:
        return c4m_type_i8();
    }
}

/* void */
/* c4m_clean_environment() */
/* { */
/*     c4m_type_universe_t *new_env = c4m_new_type_universe(); */
/*     hatrack_dict_item_t *items; */
/*     uint64_t             len; */

/*     items = hatrack_dict_items_sort(c4m_type_universe->store, &len); */

/*     for (uint64_t i = 0; i < len; i++) { */
/*         c4m_type_t *t = items[i].value; */

/*         if (t->typeid != (uint64_t)items[i].key) { */
/*             continue; */
/*         } */

/*         if (c4m_type_get_base(t) != C4M_DT_KIND_type_var) { */
/*             hatrack_dict_put(new_env->store, (void *)t->typeid, t); */
/*         } */

/*         int nparams = c4m_list_len(t->details->items); */
/*         for (int i = 0; i < nparams; i++) { */
/*             c4m_type_t *it = c4m_type_get_param(t, i); */
/*             it             = c4m_type_resolve(it); */

/*             if (c4m_type_get_base(it) == C4M_DT_KIND_type_var) { */
/*                 hatrack_dict_put(new_env->store, (void *)t->typeid, t); */
/*             } */
/*         } */
/*     } */

/*     c4m_type_universe = new_env; */
/* } */

c4m_grid_t *
c4m_format_global_type_environment()
{
    uint64_t        len;
    hatrack_view_t *view;
    c4m_grid_t     *grid  = c4m_new(c4m_type_grid(),
                               c4m_kw("start_cols",
                                      c4m_ka(3),
                                      "header_rows",
                                      c4m_ka(1),
                                      "stripe",
                                      c4m_ka(true)));
    c4m_list_t     *row   = c4m_new_table_row();
    c4m_dict_t     *memos = c4m_dict(c4m_type_ref(),
                                 c4m_type_utf8());
    int64_t         n     = 0;

    view = crown_view(&c4m_type_universe.store, &len, true);

    c4m_list_append(row, c4m_new_utf8("Id"));
    c4m_list_append(row, c4m_new_utf8("Value"));
    c4m_list_append(row, c4m_new_utf8("Base Type"));
    c4m_grid_add_row(grid, row);

    for (uint64_t i = 0; i < len; i++) {
        c4m_type_t *t = (c4m_type_t *)view[i].item;

        // Skip forwarded nodes.
        if (t->fw) {
            continue;
        }

        c4m_utf8_t *base_name;

        switch (c4m_type_get_base(t)) {
        case C4M_DT_KIND_nil:
            base_name = c4m_new_utf8("nil");
            break;
        case C4M_DT_KIND_primitive:
            base_name = c4m_new_utf8("primitive");
            break;
        case C4M_DT_KIND_internal: // Internal primitives.
            base_name = c4m_new_utf8("internal");
            break;
        case C4M_DT_KIND_type_var:
            base_name = c4m_new_utf8("var");
            break;
        case C4M_DT_KIND_list:
            base_name = c4m_new_utf8("list");
            break;
        case C4M_DT_KIND_dict:
            base_name = c4m_new_utf8("dict");
            break;
        case C4M_DT_KIND_tuple:
            base_name = c4m_new_utf8("tuple");
            break;
        case C4M_DT_KIND_func:
            base_name = c4m_new_utf8("func");
            break;
        case C4M_DT_KIND_maybe:
            base_name = c4m_new_utf8("maybe");
            break;
        case C4M_DT_KIND_object:
            base_name = c4m_new_utf8("object");
            break;
        case C4M_DT_KIND_oneof:
            base_name = c4m_new_utf8("oneof");
            break;
        case C4M_DT_KIND_box:
            base_name = c4m_new_utf8("box");
            break;
        default:
            c4m_unreachable();
        }

        row = c4m_new_table_row();
        c4m_list_append(row, c4m_cstr_format("{:x}", c4m_box_i64(t->typeid)));
        c4m_list_append(row,
                        c4m_internal_type_repr(t, memos, &n));
        c4m_list_append(row, base_name);
        c4m_grid_add_row(grid, row);
    }
    c4m_set_column_style(grid, 0, "snap");
    c4m_set_column_style(grid, 1, "snap");
    c4m_set_column_style(grid, 2, "snap");
    return grid;
}

c4m_type_t *
c4m_new_typevar()
{
    c4m_type_t   *result = c4m_new(c4m_type_typespec(),
                                 C4M_T_GENERIC);
    tv_options_t *tsi    = c4m_gc_alloc_mapped(tv_options_t, C4M_GC_SCAN_ALL);

    result->details->tsi   = tsi;
    tsi->container_options = c4m_get_all_containers_bitfield();
    result->details->items = c4m_list(c4m_type_typespec());
    result->details->flags = C4M_FN_UNKNOWN_TV_LEN;

    return result;
}

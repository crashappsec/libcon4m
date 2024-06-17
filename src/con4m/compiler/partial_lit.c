// An internal type for literals, where we keep what we need to be
// able to either fold them into a single object, or generate code
// from them.
#define C4M_USE_INTERNAL_API
#include "con4m.h"

static void
partial_literal_init(c4m_partial_lit_t *partial, va_list args)
{
    int              n         = va_arg(args, int);
    c4m_builtin_t    base_type = va_arg(args, c4m_builtin_t);
    c4m_tree_node_t *node      = va_arg(args, c4m_tree_node_t *);

    partial->num_items = n;
    partial->items     = c4m_gc_array_alloc(c4m_obj_t, n);
    partial->type      = c4m_new(c4m_tspec_typespec(),
                            c4m_global_type_env,
                            base_type);
    partial->node      = node;

    if (!n) {
        partial->empty_container = 1;
        if (base_type == C4M_T_VOID) {
            partial->empty_dict_or_set = 1;
        }
        return;
    }

    int flagwords_needed = (n + 63) / 64;

    if (flagwords_needed > 1) {
        partial->cached_state = c4m_gc_array_alloc(uint64_t, flagwords_needed);
        for (int i = 0; i < flagwords_needed - 1; i++) {
            partial->cached_state[0] = ~(0ULL);
        }

        partial->cached_state[flagwords_needed - 1] = (1 << (n % 64)) - 1;
    }
    else {
        uint64_t tmp          = (1 << n) - 1;
        partial->cached_state = (void *)tmp;
    }
}

const c4m_vtable_t c4m_partial_lit_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)partial_literal_init,
    },
};

static inline bool
this_node_can_fold(c4m_obj_t o)
{
    c4m_type_t *t = c4m_get_my_type(o);

    switch (c4m_tspec_get_base_tid(t)) {
    case C4M_T_PARTIAL_LIT:
    case C4M_T_PARSE_NODE:
        return false;
    default:
        return true;
    }
}

c4m_obj_t
c4m_fold_partial(c4m_compile_ctx *cctx, c4m_obj_t o)
{
    c4m_partial_lit_t *partial;
    bool               fully_foldable = true;

    // Partial will be a parse tree node if it's not straightforward
    // to fold.

    switch (c4m_tspec_get_base_tid(c4m_get_my_type(o))) {
    case C4M_T_PARTIAL_LIT:
        break;
    default:
        return o;
    }

    partial = o;

    for (int i = 0; i < partial->num_items; i++) {
        c4m_obj_t replacement = c4m_fold_partial(cctx, partial->items[i]);
        fully_foldable &= this_node_can_fold(replacement);
        partial->items[i] = replacement;
    }

    if (!fully_foldable) {
        return o;
    }

    c4m_type_t  *t       = c4m_resolve_and_unbox(partial->type);
    c4m_xlist_t *params  = c4m_tspec_get_params(t);
    int          nparams = c4m_tspec_get_num_params(t);

    assert(c4m_tspec_is_concrete(t));
    assert(!(partial->num_items % nparams));

    c4m_xlist_t *items;

    if (nparams == 1) {
        c4m_type_t *item_type = c4m_xlist_get(params, 0, NULL);
        item_type             = c4m_resolve_and_unbox(item_type);

        items = c4m_new(c4m_tspec_xlist(item_type));

        if (c4m_type_is_boxed_value_type(item_type)) {
            for (int i = 0; i < partial->num_items; i++) {
                c4m_xlist_append(items, (void *)c4m_unbox(partial->items[i]));
            }
        }
        else {
            for (int i = 0; i < partial->num_items; i++) {
                c4m_xlist_append(items, partial->items[i]);
            }
        }
    }
    else {
        c4m_type_t *tuple_type = c4m_tspec_tuple_from_xlist(params);
        items                  = c4m_new(c4m_tspec_xlist(tuple_type));

        assert(!(partial->num_items % nparams));

        for (int i = 0; i < partial->num_items;) {
            c4m_tuple_t *tuple = c4m_new(tuple_type);
            c4m_obj_t    obj;

            for (int j = 0; j < partial->num_items; j++) {
                c4m_type_t *item_type = c4m_xlist_get(params, j, NULL);

                if (c4m_type_is_boxed_value_type(item_type)) {
                    obj = (void *)c4m_unbox(partial->items[i++]);
                }
                else {
                    obj = partial->items[i++];
                }
                c4m_tuple_set(tuple, j, obj);
            }

            c4m_xlist_append(items, tuple);
        }
    }

    return c4m_container_literal(t, items, partial->litmod);
}

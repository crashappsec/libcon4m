#define C4M_USE_INTERNAL_API
#include "con4m.h"

typedef struct {
    c4m_utf8_t        *name;
    c4m_type_t        *sig;
    c4m_tree_node_t   *loc;
    c4m_scope_entry_t *resolution;
    unsigned int       polymorphic : 1;
    unsigned int       deferred    : 1;
} call_resolution_info_t;

typedef struct {
    c4m_utf8_t        *label;
    c4m_utf8_t        *label_ix;
    c4m_utf8_t        *label_last;
    c4m_tree_node_t   *prelude;
    c4m_tree_node_t   *test;
    c4m_tree_node_t   *body;
    c4m_scope_entry_t *shadowed_ix;
    c4m_scope_entry_t *loop_ix;
    c4m_scope_entry_t *named_loop_ix;
    c4m_scope_entry_t *shadowed_last;
    c4m_scope_entry_t *loop_last;
    c4m_scope_entry_t *named_loop_last;
    c4m_scope_entry_t *lvar_1;
    c4m_scope_entry_t *lvar_2;
    c4m_scope_entry_t *shadowed_lvar_1;
    c4m_scope_entry_t *shadowed_lvar_2;
    bool               ranged;
} c4m_loop_info_t;

typedef struct {
    c4m_scope_t          *attr_scope;
    c4m_scope_t          *global_scope;
    c4m_spec_t           *spec;
    c4m_compile_ctx      *compile;
    c4m_file_compile_ctx *file_ctx;
    __uint128_t           du_stack;
    int                   du_stack_ix;
    // The above get initialized only once when we start processing a module.
    // Everything below this comment gets updated for each function entry too.
    c4m_scope_t          *local_scope;
    c4m_tree_node_t      *node;
    c4m_cfg_node_t       *cfg; // Current control-flow-graph node.
    c4m_cfg_node_t       *fn_exit_node;
    c4m_xlist_t          *func_nodes;
    // Current fn decl object when in a fn. It's NULL in a module context.
    c4m_fn_decl_t        *fn_decl;
    c4m_xlist_t          *current_rhs_uses;
    c4m_utf8_t           *current_section_prefix;
    c4m_xlist_t          *loop_stack;
    c4m_xlist_t          *deferred_calls;
} pass2_ctx;

static inline void
next_branch(pass2_ctx *ctx, c4m_cfg_node_t *branch_node)
{
    ctx->cfg = branch_node;
}

static inline bool
base_node_tcheck(c4m_pnode_t *pnode, c4m_type_t *type)
{
    return !c4m_tspec_is_error(c4m_merge_types(pnode->type, type));
}

static inline c4m_type_t *
get_pnode_type(c4m_tree_node_t *node)
{
    c4m_pnode_t *pnode = get_pnode(node);
    return c4m_resolve_type_aliases(pnode->type, c4m_global_type_env);
}

static inline c4m_type_t *
merge_or_err(pass2_ctx *ctx, c4m_type_t *new_t, c4m_type_t *old_t)
{
    c4m_type_t *result = c4m_merge_types(new_t, old_t);
    if (c4m_tspec_is_error(result)) {
        if (!c4m_tspec_is_error(new_t) && !c4m_tspec_is_error(old_t)) {
            c4m_add_error(ctx->file_ctx,
                          c4m_err_inconsistent_infer_type,
                          ctx->node,
                          new_t,
                          old_t);
        }
    }

    return result;
}

static inline void
set_node_type(pass2_ctx *ctx, c4m_tree_node_t *node, c4m_type_t *type)
{
    c4m_pnode_t *pnode = get_pnode(node);
    if (pnode->type == NULL) {
        pnode->type = type;
    }
    else {
        merge_or_err(ctx, pnode->type, type);
    }
}

static void
add_def(pass2_ctx *ctx, c4m_scope_entry_t *sym, bool finish_flow)
{
    ctx->cfg = c4m_cfg_add_def(ctx->cfg, ctx->node, sym, ctx->current_rhs_uses);

    if (finish_flow) {
        ctx->current_rhs_uses = NULL;
    }

    if (sym->sym_defs == NULL) {
        sym->sym_defs = c4m_new(c4m_tspec_xlist(c4m_tspec_ref()));
    }

    c4m_xlist_append(sym->sym_defs, ctx->node);
}

static void
add_use(pass2_ctx *ctx, c4m_scope_entry_t *sym)
{
    ctx->cfg = c4m_cfg_add_use(ctx->cfg, ctx->node, sym);
    if (ctx->current_rhs_uses) {
        c4m_xlist_append(ctx->current_rhs_uses, sym);
    }

    if (sym->sym_uses == NULL) {
        sym->sym_uses = c4m_new(c4m_tspec_xlist(c4m_tspec_ref()));
    }

    c4m_xlist_append(sym->sym_uses, ctx->node);
}

static inline void
start_data_flow(pass2_ctx *ctx)
{
    ctx->current_rhs_uses = c4m_new(c4m_tspec_xlist(c4m_tspec_ref()));
}

static inline c4m_xlist_t *
use_pattern(pass2_ctx *ctx, c4m_tpat_node_t *pat)
{
    return apply_pattern_on_node(ctx->node, pat);
}

static inline void
type_check_node_against_sym(pass2_ctx         *ctx,
                            c4m_pnode_t       *pnode,
                            c4m_scope_entry_t *sym)
{
    if (pnode->type == NULL) {
        pnode->type = c4m_tspec_typevar();
    }

    else {
        if (pnode->type && c4m_tspec_is_error(pnode->type)) {
            return; // Already errored for this node.
        }
    }

    if (!c4m_tspec_is_error(sym->type)) {
        if (base_node_tcheck(pnode, sym->type)) {
            return;
        }
        c4m_add_error(ctx->file_ctx,
                      c4m_err_decl_mismatch,
                      ctx->node,
                      pnode->type,
                      sym->type,
                      c4m_node_get_loc_str(sym->declaration_node));
    }
    else {
        if (base_node_tcheck(pnode, sym->type)) {
            return;
        }

        c4m_add_error(ctx->file_ctx,
                      c4m_err_inconsistent_type,
                      ctx->node,
                      pnode->type,
                      sym->name,
                      sym->type);

        // Maybe make this an option; supress further type errors for
        // this symbol.
        sym->type = c4m_tspec_error();
    }
}

static inline c4m_type_t *
type_check_nodes(c4m_tree_node_t *n1, c4m_tree_node_t *n2)
{
    c4m_pnode_t *pnode1 = get_pnode(n1);
    c4m_pnode_t *pnode2 = get_pnode(n2);

    return c4m_merge_types(pnode1->type, pnode2->type);
}

static inline c4m_type_t *
type_check_node_against_type(c4m_tree_node_t *n, c4m_type_t *t)
{
    c4m_pnode_t *pnode = get_pnode(n);

    return c4m_merge_types(pnode->type, t);
}

// Extract type info from partials.
#define extract_type(obj, varname)                  \
    if (c4m_is_partial_lit(obj)) {                  \
        varname = ((c4m_partial_lit_t *)obj)->type; \
    }                                               \
    else {                                          \
        varname = c4m_get_my_type(obj);             \
    }

static inline void
calculate_partial_dict_type(pass2_ctx *ctx, c4m_partial_lit_t *partial)
{
    c4m_type_t *key_type   = c4m_tspec_get_param(partial->type, 0);
    c4m_type_t *value_type = c4m_tspec_get_param(partial->type, 1);
    int         n          = partial->num_items;
    int         i          = 0;

    while (i < n) {
        c4m_obj_t   obj = partial->items[i++];
        c4m_type_t *one_type;

        extract_type(obj, one_type);
        merge_or_err(ctx, one_type, key_type);

        obj = partial->items[i++];
        extract_type(obj, one_type);
        merge_or_err(ctx, one_type, value_type);
    }
}

static inline void
calculate_partial_tuple_type(pass2_ctx *ctx, c4m_partial_lit_t *partial)
{
    int n = partial->num_items;

    for (int i = 0; i < n; i++) {
        c4m_obj_t   obj = partial->items[i];
        c4m_type_t *t;

        extract_type(obj, t);

        merge_or_err(ctx, t, c4m_tspec_get_param(partial->type, i));
    }
}

static inline void
calculate_partial_list_or_set_type(pass2_ctx *ctx, c4m_partial_lit_t *partial)
{
    int         n         = partial->num_items;
    c4m_type_t *item_type = c4m_tspec_get_param(partial->type, 0);

    for (int i = 0; i < n; i++) {
        c4m_obj_t   obj = partial->items[i];
        c4m_type_t *t;

        extract_type(obj, t);

        merge_or_err(ctx, t, item_type);
    }
}

static void
c4m_calculate_partial_type(pass2_ctx *ctx, c4m_partial_lit_t *partial)
{
    if (partial->empty_container) {
        if (partial->empty_dict_or_set) {
            partial->type = c4m_tspec_typevar();
            c4m_remove_list_options(partial->type);
            c4m_remove_tuple_options(partial->type);
        }
        return;
    }

    if (c4m_type_has_dict_syntax(partial->type)) {
        calculate_partial_dict_type(ctx, partial);
        return;
    }
    if (c4m_type_has_tuple_syntax(partial->type)) {
        calculate_partial_tuple_type(ctx, partial);
        return;
    }
    calculate_partial_list_or_set_type(ctx, partial);
}

// This maps names to how many arguments the function takes.  If
// there's no return type, it'll be a negative number.  For now, the
// type constraints will be handled by the caller, and the # of args
// should already be checked.

static c4m_dict_t *polymorphic_fns = NULL;

static void
setup_polymorphic_fns()
{
    polymorphic_fns = c4m_new(c4m_tspec_dict(c4m_tspec_utf8(),
                                             c4m_tspec_ref()));

    hatrack_dict_put(polymorphic_fns, c4m_new_utf8("__slice__"), (void *)~3);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8("__index__"), (void *)~2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8("__plus__"), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8("__minus__"), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8("__mul__"), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8("__mod__"), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8("__div__"), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8("__fdiv__"), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8("__shl__"), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8("__shr__"), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8("__bitand__"), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8("__bitor__"), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8("__bitxor__"), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8("__cmp__"), (void *)2);
    hatrack_dict_put(polymorphic_fns,
                     c4m_new_utf8("__set_slice__"),
                     (void *)~4);
    hatrack_dict_put(polymorphic_fns,
                     c4m_new_utf8("__set_index__"),
                     (void *)~3);
}

static call_resolution_info_t *
initial_function_resolution(pass2_ctx       *ctx,
                            c4m_utf8_t      *call_name,
                            c4m_type_t      *called_type,
                            c4m_tree_node_t *call_loc)
{
    // For now, (maybe will change when we add in objects), the only
    // overloading we allow for function names is for builtin names.
    // For such things `overload` will be true. And for these, we do
    // NOT make any serious attempt to do resolution when the first
    // argument is not "concrete enough", meaning the only type
    // variables we would allow would be in *parameters* to function
    // calls.
    //
    // We'll try to statically bind at the end of processing though.
    // And if there's an ambiguity, we'll generate a run-time
    // dispatch.
    //
    // For any other function, we unify immediately, and if there is
    // no symbol at this point, we know it's a miss, since all
    // declarations have been processed by this point.
    //
    // However, if the function we've looked up hasn't been fully
    // inferenced yet, we won't have enough information yet to bind
    // the types yet (the functions params are often all going to just
    // be type variables).
    //
    // If a function hasn't been processed, comparing vs. its type sig
    // is worthless, because we don't want concrete types at call
    // sites to influence the actual type. So in those cases, we defer
    // until the end of the pass, just like w/ overloads.

    if (polymorphic_fns == NULL) {
        setup_polymorphic_fns();
    }

    call_resolution_info_t *info = c4m_gc_alloc(call_resolution_info_t);

    info->name = call_name;
    info->loc  = call_loc;
    info->sig  = called_type;

    // Result will be non-zero in all cases we allow polymorphism,
    // thus the null check, even though we were putting in ints.
    if (hatrack_dict_get(polymorphic_fns, call_name, NULL) != NULL) {
        info->polymorphic = 1;
        info->deferred    = 1;

        c4m_xlist_append(ctx->deferred_calls, info);

        return info;
    }

    // Otherwise, at this point we must be able to statically bind
    // from our static scope, and currently the function can only live
    // in the module or global scope.
    c4m_scope_entry_t *sym = c4m_symbol_lookup(NULL,
                                               ctx->file_ctx->module_scope,
                                               ctx->global_scope,
                                               NULL,
                                               call_name);

    if (sym == NULL) {
        c4m_add_error(ctx->file_ctx,
                      c4m_err_fn_not_found,
                      call_loc,
                      call_name);
        return NULL;
    }

    switch (sym->kind) {
    case sk_func:
    case sk_extern_func:
        info->resolution = sym;

        ctx->cfg = c4m_cfg_add_call(ctx->cfg,
                                    call_loc,
                                    sym,
                                    ctx->current_rhs_uses);

        int sig_params = c4m_tspec_get_num_params(info->sig);
        int sym_params = c4m_tspec_get_num_params(sym->type);

        if (sig_params != sym_params) {
            c4m_add_error(ctx->file_ctx,
                          c4m_err_num_params,
                          call_loc,
                          call_name,
                          c4m_box_u64(sig_params - 1),
                          c4m_box_u64(sym_params - 1));
            return NULL;
        }

        if (!(sym->flags & C4M_F_FN_PASS_DONE)) {
            info->deferred = 1;
            set_node_type(ctx,
                          call_loc,
                          c4m_tspec_get_param(info->sig, sig_params - 1));
            return info;
        }

        merge_or_err(ctx, c4m_global_copy(sym->type), info->sig);

        set_node_type(ctx,
                      call_loc,
                      c4m_tspec_get_param(info->sig, sig_params - 1));

        return info;

    default:
        c4m_add_error(ctx->file_ctx,
                      c4m_err_calling_non_fn,
                      call_loc,
                      call_name,
                      c4m_sym_kind_name(sym));

        return NULL;
    }
}

static void base_check_pass_dispatch(pass2_ctx *);

static inline void
process_node(pass2_ctx *ctx, c4m_tree_node_t *n)
{
    c4m_tree_node_t *saved = ctx->node;
    ctx->node              = n;
    base_check_pass_dispatch(ctx);
    ctx->node = saved;
}

static inline void
def_context_enter(pass2_ctx *ctx)
{
    ctx->du_stack <<= 1;
    ctx->du_stack |= 1;

    if (++ctx->du_stack_ix == 128) {
        C4M_CRAISE("Stack overflow in def/use tracker.");
    }
}

static inline void
def_use_context_exit(pass2_ctx *ctx)
{
    ctx->du_stack >>= 1;
    ctx->du_stack_ix--;
}

static inline void
use_context_enter(pass2_ctx *ctx)
{
    ctx->du_stack <<= 1;

    if (++ctx->du_stack_ix == 128) {
        C4M_CRAISE("Stack overflow in def/use tracker.");
    }
}

static void
process_children(pass2_ctx *ctx)
{
    c4m_tree_node_t *saved = ctx->node;
    int              n     = saved->num_kids;

    for (int i = 0; i < n; i++) {
        ctx->node = saved->children[i];
        base_check_pass_dispatch(ctx);
    }

    // Handle propogations automatically for unary nodes.
    // binary nodes are on their own.
    if (n == 1) {
        c4m_pnode_t *my_pnode  = get_pnode(saved);
        c4m_pnode_t *kid_pnode = get_pnode(saved->children[0]);

        my_pnode->value      = kid_pnode->value;
        my_pnode->type       = kid_pnode->type;
        my_pnode->extra_info = kid_pnode->extra_info;
    }

    ctx->node = saved;
}

static inline int
num_children(pass2_ctx *ctx)
{
    return ctx->node->num_kids;
}

static inline void
process_child(pass2_ctx *ctx, int i)
{
    c4m_tree_node_t *saved = ctx->node;
    ctx->node              = saved->children[i];
    base_check_pass_dispatch(ctx);
    ctx->node = saved;
}

static inline bool
is_def_context(pass2_ctx *ctx)
{
    return (bool)ctx->du_stack & 0x1;
}

static c4m_scope_entry_t *
sym_lookup(pass2_ctx *ctx, c4m_utf8_t *name)
{
    c4m_scope_entry_t *result;
    c4m_spec_t        *spec = ctx->spec;
    c4m_utf8_t        *dot  = c4m_new_utf8(".");

    // First, if it's specified in the spec, we consider it there,
    // even if it's not in the symbol table.

    if (spec != NULL) {
        c4m_xlist_t     *parts     = c4m_str_xsplit(name, dot);
        c4m_attr_info_t *attr_info = c4m_get_attr_info(spec, parts);

        switch (attr_info->kind) {
        case c4m_attr_user_def_field:
            if (c4m_xlist_len(parts) == 1) {
                break;
            }
            // fallthrough
        case c4m_attr_field:

            result = hatrack_dict_get(ctx->attr_scope->symbols, name, NULL);
            if (result == NULL) {
                result             = c4m_add_inferred_symbol(ctx->file_ctx,
                                                 ctx->attr_scope,
                                                 name);
                result->other_info = attr_info;
            }
            return result;

        case c4m_attr_object_type:
            c4m_add_error(ctx->file_ctx,
                          c4m_err_spec_needs_field,
                          ctx->node,
                          name,
                          c4m_new_utf8("named section"));
            return NULL;
        case c4m_attr_singleton:
            c4m_add_error(ctx->file_ctx,
                          c4m_err_spec_needs_field,
                          ctx->node,
                          name,
                          c4m_new_utf8("singleton (unnamed) section"));
            return NULL;

        case c4m_attr_instance:
            c4m_add_error(ctx->file_ctx,
                          c4m_err_spec_needs_field,
                          ctx->node,
                          name,
                          c4m_new_utf8("section instance"));
            return NULL;

        case c4m_attr_invalid:
            if (c4m_xlist_len(parts) == 1) {
                // If it's not explicitly by the spec, then we
                // treat it as a variable not an attribute.
                break;
            }
            switch (attr_info->err) {
            case c4m_attr_err_sec_under_field:
                c4m_add_error(ctx->file_ctx,
                              c4m_err_field_not_spec,
                              ctx->node,
                              name,
                              attr_info->err_arg);
                break;
            case c4m_attr_err_field_not_allowed:
                c4m_add_error(ctx->file_ctx,
                              c4m_err_field_not_spec,
                              ctx->node,
                              name,
                              attr_info->err_arg);
                break;
            case c4m_attr_err_no_such_sec:
                c4m_add_error(ctx->file_ctx,
                              c4m_err_undefined_section,
                              ctx->node,
                              attr_info->err_arg);
                break;
            case c4m_attr_err_sec_not_allowed:
                c4m_add_error(ctx->file_ctx,
                              c4m_err_section_not_allowed,
                              ctx->node,
                              attr_info->err_arg);
            default:
                unreachable();
            }

            return NULL;
        }
    }
    else {
        // TODO: Object resolution.
        if (c4m_str_find(name, dot) != -1) {
            result = c4m_symbol_lookup(NULL, NULL, NULL, ctx->attr_scope, name);

            if (!result) {
                result = c4m_add_inferred_symbol(ctx->file_ctx,
                                                 ctx->attr_scope,
                                                 name);
            }

            return result;
        }
    }

    result = c4m_symbol_lookup(ctx->local_scope,
                               ctx->file_ctx->module_scope,
                               ctx->global_scope,
                               ctx->attr_scope,
                               name);

    return result;
}

static c4m_scope_entry_t *
lookup_or_add(pass2_ctx *ctx, c4m_utf8_t *name)
{
    c4m_scope_entry_t *result = sym_lookup(ctx, name);

    if (!result) {
        result = c4m_add_inferred_symbol(ctx->file_ctx,
                                         ctx->local_scope,
                                         name);
    }

    if (is_def_context(ctx)) {
        add_def(ctx, result, true);
    }
    else {
        add_use(ctx, result);
    }

    return result;
}

static void
handle_index(pass2_ctx *ctx)
{
    c4m_pnode_t *pnode     = get_pnode(ctx->node);
    int          num_kids  = num_children(ctx);
    c4m_type_t  *node_type = c4m_tspec_typevar();
    c4m_type_t  *container_type;
    c4m_type_t  *ix1_type;
    c4m_type_t  *ix2_type;

    process_child(ctx, 0);
    container_type = get_pnode_type(ctx->node->children[0]);
    use_context_enter(ctx);
    process_child(ctx, 1);
    ix1_type    = get_pnode_type(ctx->node->children[1]);
    pnode->type = node_type;

    if (!c4m_tspec_is_int_type(ix1_type)) {
        if (num_kids == 3) {
            c4m_add_error(ctx->file_ctx,
                          c4m_err_slice_on_dict,
                          ctx->node);
            return;
        }

        c4m_type_t *tmp = c4m_tspec_any_dict(ix1_type, NULL);
        container_type  = merge_or_err(ctx, tmp, container_type);
    }

    call_resolution_info_t *info = NULL;

    if (num_kids == 3) {
        process_child(ctx, 2);
        ix2_type = get_pnode_type(ctx->node->children[2]);

        // This would give too generic an error if we just try to
        // merge first.
        if (!c4m_tspec_is_int_type(ix2_type) && !c4m_tspec_is_tvar(ix2_type)) {
            c4m_add_error(ctx->file_ctx,
                          c4m_err_bad_slice_ix,
                          ctx->node);
            return;
        }

        if (c4m_tspec_is_tvar(ix1_type)) {
            merge_or_err(ctx, ix1_type, c4m_tspec_i32());
        }

        if (c4m_tspec_is_tvar(ix2_type)) {
            merge_or_err(ctx, ix2_type, c4m_tspec_i32());
        }

        info = initial_function_resolution(
            ctx,
            c4m_new_utf8("__slice__"),
            c4m_tspec_varargs_fn(node_type,
                                 3,
                                 container_type,
                                 ix1_type,
                                 ix2_type),
            ctx->node);
    }
    else {
        info = initial_function_resolution(
            ctx,
            c4m_new_utf8("__index__"),
            c4m_tspec_varargs_fn(node_type,
                                 2,
                                 container_type,
                                 ix1_type),
            ctx->node);
    }

    if (c4m_tspec_is_tvar(container_type)) {
        tv_options_t *tsi = container_type->details->tsi;

        if (tsi->value_type == NULL) {
            tsi->value_type = c4m_tspec_typevar();
        }

        merge_or_err(ctx, node_type, tsi->value_type);
    }

    else {
        int         nparams = c4m_tspec_get_num_params(container_type);
        c4m_type_t *tmp     = c4m_tspec_get_param(container_type, nparams - 1);

        merge_or_err(ctx, node_type, tmp);
    }

    c4m_xlist_append(ctx->deferred_calls, info);

    def_use_context_exit(ctx);
    pnode->extra_info = info;
}

static void
handle_call(pass2_ctx *ctx)
{
    c4m_xlist_t *stashed_uses = ctx->current_rhs_uses;

    use_context_enter(ctx);
    start_data_flow(ctx);

    c4m_tree_node_t *saved    = ctx->node;
    int              n        = saved->num_kids;
    c4m_xlist_t     *argtypes = c4m_new(c4m_tspec_xlist(c4m_tspec_typespec()));
    c4m_utf8_t      *fname    = node_text(saved->children[0]);
    c4m_pnode_t     *pnode;

    for (int i = 1; i < n; i++) {
        ctx->node = saved->children[i];
        pnode     = get_pnode(ctx->node);
        base_check_pass_dispatch(ctx);
        c4m_xlist_append(argtypes, pnode->type);
    }

    c4m_type_t *fn_type;

    pnode       = get_pnode(saved);
    pnode->type = c4m_tspec_typevar();
    fn_type     = c4m_tspec_fn(pnode->type, argtypes, false);

    initial_function_resolution(ctx, fname, fn_type, saved);

    ctx->current_rhs_uses = stashed_uses;
    def_use_context_exit(ctx);
}

static void
handle_break(pass2_ctx *ctx)
{
    c4m_tree_node_t *n     = ctx->node;
    c4m_utf8_t      *label = NULL;

    if (c4m_tree_get_number_children(n) != 0) {
        label = node_text(n->children[0]);
    }

    ctx->cfg = c4m_cfg_add_break(ctx->cfg, n, label);
    int i    = c4m_xlist_len(ctx->loop_stack);

    while (i--) {
        c4m_loop_info_t *li = c4m_xlist_get(ctx->loop_stack, i, NULL);
        if (!label || (li->label && !strcmp(label->data, li->label->data))) {
            c4m_pnode_t *npnode = get_pnode(n);
            npnode->extra_info  = li;
            return;
        }
    }

    c4m_add_error(ctx->file_ctx,
                  c4m_err_label_target,
                  n,
                  label,
                  c4m_new_utf8("break"));
}

static void
handle_return(pass2_ctx *ctx)
{
    c4m_tree_node_t *n = ctx->node;

    process_children(ctx);

    if (c4m_tree_get_number_children(n) != 0) {
        c4m_scope_entry_t *sym = c4m_symbol_lookup(NULL,
                                                   ctx->local_scope,
                                                   NULL,
                                                   NULL,
                                                   c4m_new_utf8("$result"));
        type_check_node_against_sym(ctx,
                                    get_pnode(n->children[0]),
                                    sym);
        add_def(ctx, sym, true);
    }

    ctx->cfg = c4m_cfg_add_return(ctx->cfg, n, ctx->fn_exit_node);
}

static void
handle_continue(pass2_ctx *ctx)
{
    c4m_tree_node_t *n     = ctx->node;
    c4m_utf8_t      *label = NULL;

    if (c4m_tree_get_number_children(n) != 0) {
        label = node_text(n->children[0]);
    }

    ctx->cfg = c4m_cfg_add_continue(ctx->cfg, n, label);
    int i    = c4m_xlist_len(ctx->loop_stack);

    while (i--) {
        c4m_loop_info_t *li = c4m_xlist_get(ctx->loop_stack, i, NULL);
        if (!label || (li->label && !strcmp(label->data, li->label->data))) {
            c4m_pnode_t *npnode = get_pnode(n);
            npnode->extra_info  = li;
            return;
        }
    }

    c4m_add_error(ctx->file_ctx,
                  c4m_err_label_target,
                  n,
                  label,
                  c4m_new_utf8("continue"));
}

static c4m_utf8_t *ix_var_name   = NULL;
static c4m_utf8_t *last_var_name = NULL;

static void
loop_push_ix_var(pass2_ctx *ctx, c4m_loop_info_t *li)
{
    if (ix_var_name == NULL) {
        ix_var_name   = c4m_new_utf8("$i");
        last_var_name = c4m_new_utf8("$last");
        c4m_gc_register_root(&ix_var_name, 1);
    }

    li->shadowed_ix = c4m_symbol_lookup(ctx->local_scope,
                                        NULL,
                                        NULL,
                                        NULL,
                                        ix_var_name);
    li->loop_ix     = c4m_add_or_replace_symbol(ctx->file_ctx,
                                            ctx->local_scope,
                                            ix_var_name);

    li->loop_ix->type = c4m_tspec_u32();
    li->loop_ix->flags |= C4M_F_USER_IMMUTIBLE;

    add_def(ctx, li->loop_ix, false);

    if (li->label != NULL) {
        li->label_ix = c4m_str_concat(li->label, ix_var_name);

        if (c4m_symbol_lookup(ctx->local_scope,
                              NULL,
                              NULL,
                              NULL,
                              li->label_ix)) {
            c4m_add_error(ctx->file_ctx,
                          c4m_err_dupe_label,
                          ctx->node,
                          li->label);
            return;
        }

        li->named_loop_ix       = c4m_add_inferred_symbol(ctx->file_ctx,
                                                    ctx->local_scope,
                                                    li->label_ix);
        li->named_loop_ix->type = c4m_tspec_u32();
        li->named_loop_ix->flags |= C4M_F_USER_IMMUTIBLE;

        add_def(ctx, li->named_loop_ix, false);
    }
}

static void
loop_pop_ix_var(pass2_ctx *ctx, c4m_loop_info_t *li)
{
    hatrack_dict_remove(ctx->local_scope->symbols, ix_var_name);

    if (li->label_ix != NULL) {
        hatrack_dict_remove(ctx->local_scope->symbols, li->label_ix);
    }

    if (li->shadowed_ix != NULL) {
        hatrack_dict_put(ctx->local_scope->symbols,
                         ix_var_name,
                         li->shadowed_ix);
    }
}

typedef struct {
    c4m_tree_node_t *condition_node;
    c4m_tree_node_t *body_node;
} condition_chain_t;

static void
do_recursive_if(pass2_ctx *ctx, condition_chain_t *info, int cond)
{
    c4m_cfg_node_t *res;
    c4m_cfg_node_t *branch;

    if (!info->body_node && !info->condition_node) {
        res      = c4m_cfg_enter_block(ctx->cfg, info->condition_node);
        ctx->cfg = res;
        c4m_cfg_exit_block(ctx->cfg, info->condition_node);
        return;
    }

    res      = c4m_cfg_enter_block(ctx->cfg, info->condition_node);
    ctx->cfg = res;

    if (info->condition_node) {
        ctx->node = info->condition_node;
        base_check_pass_dispatch(ctx);
        branch = c4m_cfg_block_new_branch_node(ctx->cfg,
                                               2,
                                               NULL,
                                               info->body_node);

        next_branch(ctx, branch);
    }

    c4m_cfg_node_t *b1 = c4m_cfg_enter_block(ctx->cfg, info->body_node);
    ctx->cfg           = b1;
    ctx->node          = info->body_node;
    base_check_pass_dispatch(ctx);
    c4m_cfg_exit_block(ctx->cfg, info->body_node);

    if (info->condition_node) {
        next_branch(ctx, branch);
        do_recursive_if(ctx, info + 1, cond + 1);
    }

    ctx->cfg = c4m_cfg_exit_block(ctx->cfg, info->condition_node);

    return;
}

// Currently looks like the merging in tree capture isn't consistent,
// resulting in an ordering problem.
// Tmp fix. The assign to elif below really wants to be:
// elifs = use_pattern(ctx, c4m_elif_branches);

static c4m_xlist_t *
get_elifs(c4m_tree_node_t *t)
{
    c4m_xlist_t *result = c4m_new(c4m_tspec_xlist(c4m_tspec_ref()));

    for (int i = 2; i < t->num_kids - 1; i++) {
        c4m_xlist_append(result, t->children[i]);
    }

    c4m_tree_node_t *last  = t->children[t->num_kids - 1];
    c4m_pnode_t     *plast = get_pnode(last);

    if (plast->kind == c4m_nt_elif) {
        c4m_xlist_append(result, last);
    }

    return result;
}

static void
handle_if(pass2_ctx *ctx)
{
    c4m_tree_node_t   *saved = ctx->node;
    c4m_xlist_t       *elifs = get_elifs(ctx->node);
    int                n     = c4m_xlist_len(elifs);
    int                l     = 0;
    int                len   = sizeof(condition_chain_t) * (n + 2);
    condition_chain_t *chain = alloca(len);

    chain[l].condition_node = saved->children[0];
    chain[l++].body_node    = saved->children[1];

    for (int i = 0; i < n; i++) {
        c4m_tree_node_t *elif_branch = c4m_xlist_get(elifs, i, NULL);
        chain[l].condition_node      = elif_branch->children[0];
        chain[l++].body_node         = elif_branch->children[1];
    }

    chain[l].body_node      = get_match_on_node(saved, c4m_else_condition);
    chain[l].condition_node = NULL;

    do_recursive_if(ctx, chain, 1);
}

static void
handle_for(pass2_ctx *ctx)
{
    c4m_loop_info_t *li    = c4m_gc_alloc(c4m_loop_info_t);
    c4m_pnode_t     *pnode = get_pnode(ctx->node);
    c4m_tree_node_t *n     = ctx->node;
    c4m_xlist_t     *vars  = use_pattern(ctx, c4m_loop_vars);
    c4m_cfg_node_t  *entrance;
    c4m_cfg_node_t  *branch;
    int              expr_ix = 0;

    start_data_flow(ctx);

    li->ranged = pnode->extra_info == NULL;

    pnode->extra_info = li;

    if (node_has_type(n->children[0], c4m_nt_label)) {
        expr_ix++;
        li->label = node_text(n->children[0]);
    }

    c4m_xlist_append(ctx->loop_stack, li);

    // First, process either the container to unpack or the range
    // expressions. We do this before the CFG entrance node, as when
    // this isn't already a literal, we only want to do it once before
    // we loop.

    ctx->node                    = n->children[expr_ix + 1];
    c4m_pnode_t *container_pnode = get_pnode(ctx->node);
    base_check_pass_dispatch(ctx);

    // Now we start the loop's CFG block. The flow graph will evaluate
    // this part every time we get back to the top of the loop.  We
    // actually skip adding a 'use' here for the container (as if we
    // just moved the contents to a temporary). Instead, we need to
    // add defs for any iteration variables after we start the block,
    // but before we branch.

    entrance = c4m_cfg_enter_block(ctx->cfg, n);
    ctx->cfg = entrance;

    // At this point, set up the iteration variables...  `push_ix_var`
    // sets up `$i` and `label$i` (if a label is provided), so that
    // code in the block below can reference it.
    //
    // We manually set up `$last`, which is only available in for
    // loops. It's set to the last iteration number we'll do if
    // there's no early exit. So when `$i == $last`, you are on the
    // last iteration of the loop.
    //
    // If used, `$i` gets updated each iteration with the number of
    // iterations through the loop that have successfully
    // completed. The lifetime of this variable is only the lifetime
    // of the loop; you cannot use it outside the loop.
    //
    // `$last` also cannot be used outside the loop. And it is not
    // reevaluated each loop, just once when starting the loop.
    //
    // If the variable isn't referenced, we'll just ignore the symbol
    // later.
    //
    // Note that we also track here when loops are nested, so that if
    // $i is used when nested, we can give a warning. But we only give
    // that warning if we see a use of $i once there is definitely
    // nesting.
    //
    // It might be worth making that an error, since the alternative
    // is still available: add a label and use `label$i` or `label$last`
    //
    // Additionally, I'm thinking about disallowing `$` in all
    // user-defined symbols.

    loop_push_ix_var(ctx, li);

    li->shadowed_last = c4m_symbol_lookup(ctx->local_scope,
                                          NULL,
                                          NULL,
                                          NULL,
                                          last_var_name);

    li->loop_last = c4m_add_or_replace_symbol(ctx->file_ctx,
                                              ctx->local_scope,
                                              last_var_name);
    li->loop_last->flags |= C4M_F_USER_IMMUTIBLE;
    li->loop_last->type = c4m_tspec_u32();

    add_def(ctx, li->loop_last, true);

    if (li->label != NULL) {
        li->label_last            = c4m_to_utf8(c4m_str_concat(li->label,
                                                    last_var_name));
        li->named_loop_last       = c4m_add_inferred_symbol(ctx->file_ctx,
                                                      ctx->local_scope,
                                                      li->label_last);
        li->named_loop_last->type = c4m_tspec_u32();
        li->named_loop_last->flags |= C4M_F_USER_IMMUTIBLE;

        add_def(ctx, li->named_loop_last, false);
    }

    // Now, process the assignment of the variable(s) to put per-loop
    // values into.  Instead of keeping nested scopes, if we see
    // shadowing, we will stash the old symbol until the loop's
    // done. We also will warn about the shadowing.

    c4m_tree_node_t *var_node1 = c4m_xlist_get(vars, 0, NULL);
    c4m_tree_node_t *var_node2;
    c4m_utf8_t      *var1_name = node_text(var_node1);
    c4m_utf8_t      *var2_name = NULL;

    if (c4m_xlist_len(vars) == 2) {
        var_node2 = c4m_xlist_get(vars, 1, NULL);
        var2_name = node_text(var_node2);

        if (!strcmp(var1_name->data, var2_name->data)) {
            c4m_add_error(ctx->file_ctx,
                          c4m_err_iter_name_conflict,
                          var_node2);
            return;
        }
    }

    li->shadowed_lvar_1          = c4m_symbol_lookup(ctx->local_scope,
                                            NULL,
                                            NULL,
                                            NULL,
                                            var1_name);
    li->lvar_1                   = c4m_add_or_replace_symbol(ctx->file_ctx,
                                           ctx->local_scope,
                                           var1_name);
    li->lvar_1->declaration_node = var_node1;
    li->lvar_1->flags |= C4M_F_USER_IMMUTIBLE;

    if (li->shadowed_lvar_1 != NULL) {
        c4m_add_warning(ctx->file_ctx,
                        c4m_warn_shadowed_var,
                        ctx->node,
                        var1_name,
                        c4m_sym_kind_name(li->shadowed_lvar_1),
                        c4m_sym_get_best_ref_loc(li->lvar_1));
    }

    if (var2_name) {
        li->shadowed_lvar_2          = c4m_symbol_lookup(ctx->local_scope,
                                                NULL,
                                                NULL,
                                                NULL,
                                                var2_name);
        li->lvar_2                   = c4m_add_or_replace_symbol(ctx->file_ctx,
                                               ctx->local_scope,
                                               var2_name);
        li->lvar_2->declaration_node = var_node2;
        li->lvar_2->flags |= C4M_F_USER_IMMUTIBLE;

        if (li->shadowed_lvar_2 != NULL) {
            c4m_add_warning(ctx->file_ctx,
                            c4m_warn_shadowed_var,
                            ctx->node,
                            var2_name,
                            c4m_sym_kind_name(li->shadowed_lvar_2),
                            c4m_sym_get_best_ref_loc(li->lvar_2));
        }

        add_def(ctx, li->lvar_1, false);
        add_def(ctx, li->lvar_2, true);
    }
    else {
        add_def(ctx, li->lvar_1, true);
    }

    // Okay, now we need to add the branch node to the CFG.

    branch   = c4m_cfg_block_new_branch_node(ctx->cfg, 2, li->label, ctx->node);
    ctx->cfg = c4m_cfg_enter_block(branch, n->children[expr_ix]);

    // Now, process that body. The body gets its own block in the CFG,
    // which makes this a bit clunkier than it needs to be.
    expr_ix += 2;
    ctx->node = n->children[expr_ix];

    base_check_pass_dispatch(ctx);
    c4m_cfg_exit_block(ctx->cfg, n->children[expr_ix]);
    ctx->cfg = branch;

    // This sets up an empty block for the code that runs when the
    // loop condition is false.
    ctx->cfg = c4m_cfg_enter_block(ctx->cfg, n);
    ctx->cfg = c4m_cfg_exit_block(ctx->cfg, n);

    // Here, it's time to clean up. We need to reset the tree and the
    // loop stack, and remove our iteration variable(s) from the
    // scope.  Plus, if any variables were shadowed, we need to
    // restore them.

    c4m_xlist_pop(ctx->loop_stack);
    ctx->node = n;

    loop_pop_ix_var(ctx, li);

    hatrack_dict_remove(ctx->local_scope->symbols, last_var_name);

    if (li->label_last != NULL) {
        hatrack_dict_remove(ctx->local_scope->symbols, li->label_last);
    }

    if (li->shadowed_last != NULL) {
        hatrack_dict_put(ctx->local_scope->symbols,
                         last_var_name,
                         li->shadowed_last);
    }
    if (li->shadowed_lvar_1 != NULL) {
        hatrack_dict_put(ctx->local_scope->symbols,
                         var1_name,
                         li->shadowed_lvar_1);
    }
    if (li->shadowed_lvar_2 != NULL) {
        hatrack_dict_put(ctx->local_scope->symbols,
                         var2_name,
                         li->shadowed_lvar_2);
    }

    // Finally, do type checking based on the type of loop. The type
    // will always be contained in the second non-label subtree of the
    // loop node. We stashed that above as `container_pnode`.

    //
    //
    if (li->ranged) {
        // Ranged fors are easy enough. The assigned variable is
        // always based on the range type.
        merge_or_err(ctx, li->lvar_1->type, container_pnode->type);
    }
    else {
        // If there are two variables, we can infer that the container is
        // a dictionary.
        //
        // Otherwise, if we have no info, it can be any set or list type.
        //
        // We never allow loops over tuples.

        c4m_type_t *cinfo = NULL;

        if (container_pnode->type == NULL) {
            container_pnode->type = c4m_tspec_typevar();
        }

        cinfo = c4m_tspec_typevar();

        if (li->lvar_2 != NULL) {
            c4m_remove_list_options(cinfo);
            c4m_remove_set_options(cinfo);
            c4m_remove_tuple_options(cinfo);
            merge_or_err(ctx, cinfo, container_pnode->type);
        }
        else {
            cinfo = c4m_tspec_typevar();
            c4m_remove_tuple_options(cinfo);
            c4m_remove_dict_options(cinfo);
            merge_or_err(ctx, container_pnode->type, cinfo);
        }
    }

    ctx->cfg = c4m_cfg_exit_node(entrance);
}

static void
handle_while(pass2_ctx *ctx)
{
    int              expr_ix = 0;
    c4m_tree_node_t *n       = ctx->node;
    c4m_pnode_t     *p       = get_pnode(n);
    c4m_loop_info_t *li      = c4m_gc_alloc(c4m_loop_info_t);
    c4m_cfg_node_t  *entrance;
    c4m_cfg_node_t  *branch;

    p->extra_info = li;
    c4m_xlist_append(ctx->loop_stack, li);

    if (node_has_type(n->children[0], c4m_nt_label)) {
        expr_ix++;
        li->label = node_text(n->children[0]);
    }

    loop_push_ix_var(ctx, li);

    entrance  = c4m_cfg_enter_block(ctx->cfg, n);
    ctx->cfg  = entrance;
    ctx->node = n->children[expr_ix++];
    base_check_pass_dispatch(ctx);
    branch    = c4m_cfg_block_new_branch_node(ctx->cfg,
                                           2,
                                           li->label,
                                           ctx->node);
    ctx->cfg  = c4m_cfg_enter_block(branch, n->children[expr_ix]);
    ctx->node = n->children[expr_ix];

    base_check_pass_dispatch(ctx);
    ctx->cfg = c4m_cfg_exit_block(ctx->cfg, n);

    next_branch(ctx, branch);

    ctx->cfg = c4m_cfg_enter_block(branch, n->children[expr_ix]);
    c4m_cfg_exit_block(ctx->cfg, n);

    c4m_xlist_pop(ctx->loop_stack);
    ctx->node = n;
    loop_pop_ix_var(ctx, li);

    ctx->cfg = c4m_cfg_exit_node(entrance);
}

static void
handle_range(pass2_ctx *ctx)
{
    process_children(ctx);
    set_node_type(ctx,
                  ctx->node,
                  type_check_nodes(ctx->node->children[0],
                                   ctx->node->children[1]));

    if (c4m_tspec_is_error(
            c4m_merge_types(get_pnode_type(ctx->node), c4m_tspec_i64()))) {
        c4m_add_error(ctx->file_ctx,
                      c4m_err_range_type,
                      ctx->node);
    }
}

static void
handle_typeof_statement(pass2_ctx *ctx)
{
    c4m_tree_node_t *saved    = ctx->node;
    c4m_xlist_t     *branches = use_pattern(ctx, c4m_case_branches);
    c4m_tree_node_t *elsenode = get_match_on_node(saved, c4m_case_else);
    c4m_tree_node_t *variant  = saved->children[0];
    int              ncases   = c4m_xlist_len(branches);
    c4m_xlist_t     *prev_types;

    prev_types = c4m_new(c4m_tspec_xlist(c4m_tspec_typespec()));

    ctx->node = variant;
    base_check_pass_dispatch(ctx);

    c4m_pnode_t       *variant_p    = get_pnode(variant);
    c4m_type_t        *type_to_test = (c4m_type_t *)variant_p->type;
    c4m_scope_entry_t *sym          = variant_p->extra_info;
    c4m_cfg_node_t    *entrance     = c4m_cfg_enter_block(ctx->cfg, saved);
    c4m_scope_entry_t *saved_sym;
    c4m_cfg_node_t    *cfgbranch;
    c4m_scope_entry_t *tmp;

    saved_sym = hatrack_dict_get(ctx->local_scope->symbols, sym->name, NULL);
    ctx->cfg  = entrance;

    if (c4m_tspec_is_concrete(type_to_test)) {
        c4m_add_error(ctx->file_ctx,
                      c4m_err_concrete_typeof,
                      variant,
                      type_to_test);
        return;
    }

    add_use(ctx, sym);

    cfgbranch = c4m_cfg_block_new_branch_node(ctx->cfg,
                                              ncases + 1,
                                              NULL,
                                              ctx->node);

    for (int i = 0; i < ncases; i++) {
        c4m_tree_node_t *branch   = c4m_xlist_get(branches, i, NULL);
        c4m_dict_t      *type_ctx = c4m_new(c4m_tspec_dict(c4m_tspec_utf8(),
                                                      c4m_tspec_ref()));

        c4m_type_t *casetype = c4m_node_to_type(ctx->file_ctx,
                                                branch->children[0],
                                                type_ctx);

        for (int j = 0; j < c4m_xlist_len(prev_types); j++) {
            c4m_type_t *oldcase = c4m_xlist_get(prev_types, j, NULL);

            if (c4m_tspecs_are_compat(oldcase, casetype)) {
                c4m_add_warning(ctx->file_ctx,
                                c4m_warn_type_overlap,
                                branch->children[0],
                                casetype,
                                oldcase);
                goto next_branch;
            }
            if (!c4m_tspecs_are_compat(casetype, type_to_test)) {
                c4m_add_error(ctx->file_ctx,
                              c4m_err_dead_branch,
                              branch->children[0],
                              casetype);
                goto next_branch;
            }

            // Alias absolutely everything except the
            // type. It's the only thing that should change.
            tmp                   = c4m_add_or_replace_symbol(ctx->file_ctx,
                                            ctx->local_scope,
                                            sym->name);
            tmp->flags            = sym->flags;
            tmp->kind             = sym->kind;
            tmp->declaration_node = sym->declaration_node;
            tmp->path             = sym->path;
            tmp->value            = sym->value;
            tmp->my_scope         = sym->my_scope;
            tmp->other_info       = sym->other_info;
            tmp->sym_defs         = sym->sym_defs;
            tmp->sym_uses         = sym->sym_uses;
            tmp->type             = casetype;

            ctx->node = branch->children[1];
            next_branch(ctx, cfgbranch);
            ctx->cfg = c4m_cfg_enter_block(ctx->cfg, ctx->node);

            base_check_pass_dispatch(ctx);
            ctx->cfg = c4m_cfg_exit_block(ctx->cfg, ctx->node);
        }

        next_branch(ctx, cfgbranch);

        if (elsenode != NULL) {
            ctx->node = elsenode;
            ctx->cfg  = c4m_cfg_enter_block(ctx->cfg, ctx->node);

            base_check_pass_dispatch(ctx);
            ctx->cfg = c4m_cfg_exit_block(ctx->cfg, ctx->node);
        }
        else {
            // Dummy CFG node for when we don't match any case.
            ctx->cfg = c4m_cfg_enter_block(ctx->cfg, saved);

            ctx->cfg = c4m_cfg_exit_block(ctx->cfg, saved);
        }

        ctx->cfg  = c4m_cfg_exit_node(entrance);
        ctx->node = saved;

        if (saved_sym != NULL) {
            hatrack_dict_put(ctx->local_scope->symbols, sym->name, saved_sym);
        }
        else {
            hatrack_dict_remove(ctx->local_scope->symbols, sym->name);
        }

        c4m_xlist_append(prev_types, c4m_global_copy(casetype));

next_branch: /* nothing */;
    }
}

static void
handle_switch_statement(pass2_ctx *ctx)
{
    c4m_tree_node_t *saved        = ctx->node;
    c4m_xlist_t     *branches     = use_pattern(ctx, c4m_case_branches);
    c4m_tree_node_t *elsenode     = get_match_on_node(saved, c4m_case_else);
    c4m_tree_node_t *variant_node = saved->children[0];
    int              ncases       = c4m_xlist_len(branches);
    c4m_cfg_node_t  *entrance;
    c4m_cfg_node_t  *cfgbranch;

    ctx->cfg  = c4m_cfg_enter_block(ctx->cfg, saved);
    entrance  = ctx->cfg;
    ctx->node = variant_node;
    base_check_pass_dispatch(ctx);

    // For the CFG's purposes, pretend we evaluate all case exprs w/o
    // short circuit, before the branch. Since we're going to
    // lazy-evaluate branches, we maybe should break this out into
    // nested ifs, like we do w/ if/else chains.
    //
    // I haven't gone through the pain though, because it is
    // impossible to define anything in the case condition, except
    // indirectly through a function call. And the worst case there
    // would be a spurious use-before-def that won't even seem
    // spurious generally.

    for (int i = 0; i < ncases; i++) {
        c4m_tree_node_t *branch = c4m_xlist_get(branches, i, NULL);
        c4m_pnode_t     *pcond  = get_pnode(branch);
        ctx->node               = branch->children[0];

        base_check_pass_dispatch(ctx);

        pcond->type = type_check_nodes(variant_node, ctx->node);

        if (c4m_tspec_is_error(pcond->type)) {
            c4m_add_error(ctx->file_ctx,
                          c4m_err_switch_case_type,
                          ctx->node,
                          get_pnode_type(ctx->node),
                          get_pnode_type(variant_node));
            return;
        }
    }

    cfgbranch = c4m_cfg_block_new_branch_node(ctx->cfg,
                                              ncases + 1,
                                              NULL,
                                              ctx->node);

    for (int i = 0; i < ncases; i++) {
        next_branch(ctx, cfgbranch);
        c4m_tree_node_t *branch = c4m_xlist_get(branches, i, NULL);
        ctx->node               = branch->children[1];
        ctx->cfg                = c4m_cfg_enter_block(ctx->cfg, ctx->node);

        base_check_pass_dispatch(ctx);
        c4m_cfg_exit_block(ctx->cfg, ctx->node);
    }

    next_branch(ctx, cfgbranch);

    if (elsenode != NULL) {
        ctx->node = elsenode;
        ctx->cfg  = c4m_cfg_enter_block(ctx->cfg, ctx->node);

        base_check_pass_dispatch(ctx);
        ctx->cfg = c4m_cfg_exit_block(ctx->cfg, ctx->node);
    }
    else {
        // Dummy CFG node for when we don't match any case.
        ctx->cfg = c4m_cfg_enter_block(ctx->cfg, saved);
        c4m_cfg_exit_block(ctx->cfg, saved);
    }

    ctx->cfg  = c4m_cfg_exit_node(entrance);
    ctx->node = saved;
}

static void
builtin_bincall(pass2_ctx *ctx)
{
    // Currently, this routine type checks based on the operation, and
    // sets the expected return type.
    //
    // Here:
    //
    // 1. Comparison ops must have the same type, and the return type is
    //    always bool.
    //
    // 2. All math ops take the same input type and returns the
    //    same output type.
    //
    // 3. Eventually we will add a float div that is a special case,
    //    but I'm still thinking about the syntax on that one, so it's
    //    not implemented yet.
    //
    // Currently, we don't bother adding the call to the CFG, since in
    // most cases this will be inlined, and not a real call.
    //
    // Once we add objects, we probably should add the call in for objects
    // (TODO).

    c4m_type_t    *tleft  = get_pnode_type(ctx->node->children[0]);
    c4m_type_t    *tright = get_pnode_type(ctx->node->children[1]);
    c4m_pnode_t   *pn     = get_pnode(ctx->node);
    c4m_operator_t op     = (c4m_operator_t)pn->extra_info;

    switch (op) {
    case c4m_op_plus:
    case c4m_op_minus:
    case c4m_op_mul:
    case c4m_op_mod:
    case c4m_op_div:
    case c4m_op_fdiv:
    case c4m_op_shl:
    case c4m_op_shr:
    case c4m_op_bitand:
    case c4m_op_bitor:
    case c4m_op_bitxor:
        set_node_type(ctx, ctx->node, merge_or_err(ctx, tleft, tright));
        return;
    case c4m_op_lt:
    case c4m_op_lte:
    case c4m_op_gt:
    case c4m_op_gte:
    case c4m_op_eq:
    case c4m_op_neq:
        merge_or_err(ctx, tleft, tright);
        set_node_type(ctx, ctx->node, c4m_tspec_bool());
        return;
    default:
        unreachable();
    }
}

static void
handle_binary_op(pass2_ctx *ctx)
{
    process_children(ctx);
    builtin_bincall(ctx);
}

static void
base_handle_assign(pass2_ctx *ctx, bool binop)
{
    c4m_tree_node_t *saved = ctx->node;

    if (binop) {
        ctx->node = saved->children[0];
        // With binops, we process the LHS twice; once in a use context
        // and once in a def context (below, after processing the RHS).
        base_check_pass_dispatch(ctx);
    }

    ctx->node = saved->children[1];
    start_data_flow(ctx);
    base_check_pass_dispatch(ctx);

    if (binop) {
        ctx->node = saved;
        builtin_bincall(ctx);
    }

    ctx->node = saved->children[0];

    def_context_enter(ctx);
    base_check_pass_dispatch(ctx);
    def_use_context_exit(ctx);

    if (!binop) {
        merge_or_err(ctx,
                     get_pnode_type(saved->children[0]),
                     get_pnode_type(saved->children[1]));
    }

    ctx->node = saved;
}

static inline void
handle_assign(pass2_ctx *ctx)
{
    base_handle_assign(ctx, false);
}

static inline void
handle_binop_assign(pass2_ctx *ctx)
{
    base_handle_assign(ctx, true);
}

static void
handle_section_decl(pass2_ctx *ctx)
{
    c4m_tree_node_t *saved      = ctx->node;
    int              n          = saved->num_kids - 1;
    c4m_utf8_t      *saved_path = ctx->current_section_prefix;

    if (saved_path == NULL) {
        ctx->current_section_prefix = node_text(saved->children[0]);
    }
    else {
        ctx->current_section_prefix = c4m_cstr_format(
            "{}.{}",
            saved_path,
            node_text(saved->children[0]));
    }

    if (n == 2) {
        ctx->current_section_prefix = c4m_cstr_format(
            "{}.{}",
            ctx->current_section_prefix,
            node_text(saved->children[1]));
    }

    ctx->node = saved->children[n];
    base_check_pass_dispatch(ctx);
    ctx->current_section_prefix = saved_path;
    ctx->node                   = saved;
}

static void
handle_identifier(pass2_ctx *ctx)
{
    c4m_pnode_t *pnode = get_pnode(ctx->node);
    c4m_utf8_t  *id    = node_text(ctx->node);

    if (ctx->current_section_prefix != NULL) {
        id = c4m_cstr_format("{}.{}", ctx->current_section_prefix, id);
    }

    c4m_scope_entry_t *sym = (void *)lookup_or_add(ctx, id);
    pnode->extra_info      = (void *)sym;
    set_node_type(ctx, ctx->node, sym->type);
}

static void
check_literal(pass2_ctx *ctx)
{
    // Right now, we don't try to fold sub-items.
    c4m_pnode_t *pnode = get_pnode(ctx->node);

    if (!c4m_is_partial_lit(pnode->value)) {
        pnode->type = c4m_get_my_type(pnode->value);
    }
    else {
        c4m_partial_lit_t *partial = (c4m_partial_lit_t *)pnode->value;

        c4m_calculate_partial_type(ctx, partial);
        pnode->type = partial->type;
    }
}

static void
handle_member(pass2_ctx *ctx)
{
    c4m_tree_node_t **kids     = ctx->node->children;
    c4m_utf8_t       *sym_name = node_text(kids[0]);
    c4m_utf8_t       *dot      = c4m_new_utf8(".");
    c4m_pnode_t      *pnode    = get_pnode(ctx->node);

    for (int i = 1; i < ctx->node->num_kids; i++) {
        sym_name = c4m_str_concat(sym_name, dot);
        sym_name = c4m_str_concat(sym_name, node_text(kids[i]));
    }

    if (ctx->current_section_prefix != NULL) {
        sym_name = c4m_cstr_format("{}.{}", sym_name);
    }

    c4m_scope_entry_t *sym = (void *)lookup_or_add(ctx, sym_name);
    pnode->extra_info      = (void *)sym;
    set_node_type(ctx, ctx->node, sym->type);
}

static void
handle_binary_logical_op(pass2_ctx *ctx)
{
    c4m_type_t  *btype = c4m_tspec_bool();
    c4m_pnode_t *kid1  = get_pnode(ctx->node->children[0]);
    c4m_pnode_t *kid2  = get_pnode(ctx->node->children[1]);
    c4m_pnode_t *pn    = get_pnode(ctx->node);

    process_children(ctx);
    if (!(base_node_tcheck(kid1, btype) && base_node_tcheck(kid2, btype))) {
        pn->type = btype;
    }
    else {
        pn->type = c4m_tspec_error();
    }
}

static void
handle_cmp(pass2_ctx *ctx)
{
    c4m_tree_node_t *tn = ctx->node;
    c4m_pnode_t     *pn = get_pnode(tn);

    process_children(ctx);

    if (c4m_tspec_is_error(
            type_check_nodes(tn->children[0], tn->children[1]))) {
        pn->type = c4m_tspec_error();
        c4m_add_error(ctx->file_ctx,
                      c4m_err_cannot_cmp,
                      tn,
                      get_pnode_type(tn->children[0]),
                      get_pnode_type(tn->children[1]));
    }
    else {
        pn->type = c4m_tspec_bool();
    }
}

static void
handle_unary_op(pass2_ctx *ctx)
{
    process_children(ctx);

    c4m_utf8_t *text = node_text(ctx->node);

    if (!strcmp(text->data, "-")) {
        if (c4m_tspec_is_error(
                type_check_node_against_type(ctx->node, c4m_tspec_i64()))) {
            c4m_add_error(ctx->file_ctx,
                          c4m_err_unary_minus_type,
                          ctx->node);
        }
    }
}

static void
base_check_pass_dispatch(pass2_ctx *ctx)
{
    c4m_pnode_t *pnode = c4m_tree_get_contents(ctx->node);

    switch (pnode->kind) {
    case c4m_nt_global_enum:
    case c4m_nt_enum:
    case c4m_nt_func_def:
    case c4m_nt_variable_decls:
    case c4m_nt_config_spec:
    case c4m_nt_section_spec:
    case c4m_nt_param_block:
    case c4m_nt_extern_block:
    case c4m_nt_use:
        return;
    case c4m_nt_section:
        handle_section_decl(ctx);
        break;

    case c4m_nt_identifier:
        handle_identifier(ctx);
        break;

    case c4m_nt_member:
        handle_member(ctx);
        break;

    case c4m_nt_simple_lit:
    case c4m_nt_lit_list:
    case c4m_nt_lit_dict:
    case c4m_nt_lit_set:
    case c4m_nt_lit_empty_dict_or_set:
    case c4m_nt_lit_tuple:
    case c4m_nt_lit_unquoted:
    case c4m_nt_lit_callback:
    case c4m_nt_lit_tspec:
        check_literal(ctx);
        break;

    case c4m_nt_index:
        handle_index(ctx);
        return;

    case c4m_nt_call:
        handle_call(ctx);
        break;

    case c4m_nt_break:
        handle_break(ctx);
        break;

    case c4m_nt_continue:
        handle_continue(ctx);
        break;

    case c4m_nt_if:
        handle_if(ctx);
        break;

    case c4m_nt_for:
        handle_for(ctx);
        break;

    case c4m_nt_while:
        handle_while(ctx);
        break;

    case c4m_nt_range:
        handle_range(ctx);
        break;

    case c4m_nt_typeof:
        handle_typeof_statement(ctx);
        break;

    case c4m_nt_switch:
        handle_switch_statement(ctx);
        break;

    case c4m_nt_assign:
        handle_assign(ctx);
        break;

    case c4m_nt_binary_assign_op:
        handle_binop_assign(ctx);
        break;

    case c4m_nt_or:
    case c4m_nt_and:
        handle_binary_logical_op(ctx);
        break;

    case c4m_nt_cmp:
        handle_cmp(ctx);
        break;

    case c4m_nt_binary_op:
        handle_binary_op(ctx);
        break;

    case c4m_nt_unary_op:
        handle_unary_op(ctx);
        break;

    case c4m_nt_return:
        handle_return(ctx);
        break;

    default:
        process_children(ctx);
        break;
    }
}

static void check_pass_toplevel_dispatch(pass2_ctx *);

static inline void
process_toplevel_children(pass2_ctx *ctx)
{
    c4m_tree_node_t *saved = ctx->node;
    int              n     = saved->num_kids;

    for (int i = 0; i < n; i++) {
        ctx->node = saved->children[i];
        check_pass_toplevel_dispatch(ctx);
    }

    ctx->node = saved;
}

static void
process_var_decls(pass2_ctx *ctx)
{
    c4m_tree_node_t  *cur  = ctx->node;
    int               num  = cur->num_kids;
    c4m_tree_node_t **kids = cur->children;

    for (int i = 1; i < num; i++) {
        c4m_tree_node_t *decl_node       = kids[i];
        int              last            = decl_node->num_kids - 1;
        c4m_tree_node_t *possible_assign = decl_node->children[last];
        c4m_pnode_t     *pnode           = get_pnode(possible_assign);

        if (pnode->kind == c4m_nt_assign) {
            ctx->node                 = possible_assign;
            c4m_tree_node_t *var_node = decl_node->children[--last];
            c4m_pnode_t     *vpnode   = get_pnode(var_node);

            if (vpnode->kind != c4m_nt_identifier && last) {
                var_node = decl_node->children[last - 1];
                vpnode   = get_pnode(var_node);
            }

            c4m_scope_entry_t *sym    = (c4m_scope_entry_t *)vpnode->value;
            c4m_tree_node_t   *tnode  = possible_assign->children[0];
            c4m_pnode_t       *tpnode = get_pnode(tnode);

            start_data_flow(ctx);
            process_node(ctx, tnode);
            add_def(ctx, sym, true);

            type_check_node_against_sym(ctx, tpnode, sym);

            ctx->node = cur;
        }
    }
}

static void
check_pass_toplevel_dispatch(pass2_ctx *ctx)
{
    c4m_pnode_t *pnode = c4m_tree_get_contents(ctx->node);

    switch (pnode->kind) {
    case c4m_nt_module:
        process_toplevel_children(ctx);
        return;
    case c4m_nt_variable_decls:
        process_var_decls(ctx);
        return;
    case c4m_nt_func_def:
        c4m_xlist_append(ctx->func_nodes, ctx->node);
        return;
    case c4m_nt_enum:
    case c4m_nt_global_enum:
    case c4m_nt_param_block:
    case c4m_nt_extern_block:
    case c4m_nt_use:
        // Absolutely 0 to do for these until we generate code.
        return;
    default:
        base_check_pass_dispatch(ctx);
        return;
    }
}

typedef struct {
    int                num_defs;
    int                num_uses;
    c4m_fn_decl_t     *decl;
    c4m_sig_info_t    *si;
    c4m_tree_node_t   *node;
    pass2_ctx         *pass_ctx;
    c4m_scope_entry_t *sym;
    c4m_scope_entry_t *formal;
    bool               delete_result_var;
} fn_check_ctx;

static void
check_return_type(fn_check_ctx *ctx)
{
    c4m_type_t *decl_type;
    c4m_type_t *cmp_type;

    if (ctx->num_defs == 0) {
        if (ctx->num_uses == 0) {
            ctx->delete_result_var = true;

            if (ctx->si->return_info.type != NULL) {
                c4m_add_error(ctx->pass_ctx->file_ctx,
                              c4m_err_no_ret,
                              ctx->node,
                              ctx->si->return_info.type);
                return;
            }
            else {
                ctx->si->return_info.type = c4m_tspec_void();
                // Drop below to set this on the fn.
            }
        }
        else {
            c4m_tree_node_t *first_use = c4m_xlist_get(ctx->sym->sym_uses,
                                                       0,
                                                       NULL);
            c4m_add_error(ctx->pass_ctx->file_ctx,
                          c4m_err_use_no_def,
                          first_use);
            return;
        }
    }

    if (ctx->si->return_info.type != NULL) {
        decl_type = c4m_global_copy(ctx->si->return_info.type);
        cmp_type  = c4m_merge_types(decl_type, ctx->sym->type);

        if (c4m_tspec_is_error(cmp_type)) {
            c4m_add_error(ctx->pass_ctx->file_ctx,
                          c4m_err_declared_incompat,
                          ctx->sym->declaration_node->children[2],
                          decl_type,
                          ctx->sym->type);
            return;
        }
        if (c4m_type_cmp_exact(decl_type, cmp_type) != c4m_type_match_exact) {
            c4m_add_error(ctx->pass_ctx->file_ctx,
                          c4m_err_too_general,
                          ctx->sym->declaration_node->children[2],
                          decl_type,
                          ctx->sym->type);
            return;
        }

        c4m_merge_types(ctx->si->return_info.type, ctx->sym->type);
    }
    else {
        // Formal was never used; just merge to be safe.
        cmp_type = c4m_merge_types(ctx->sym->type, ctx->formal->type);

        ctx->si->return_info.type = cmp_type;
        assert(!c4m_tspec_is_error(cmp_type));
    }
}

static void
check_formal_param(fn_check_ctx *ctx)
{
    c4m_fn_param_info_t *param   = ctx->si->param_info;
    c4m_utf8_t          *symname = ctx->sym->name;
    c4m_type_t          *decl_type;
    c4m_type_t          *cmp_type;

    if (ctx->num_uses == 0) {
        c4m_add_warning(ctx->pass_ctx->file_ctx,
                        c4m_warn_unused_param,
                        ctx->sym->declaration_node,
                        ctx->sym->name);
    }

    for (int i = 0; i < ctx->si->num_params; i++) {
        if (!strcmp(param->name->data, symname->data)) {
            break;
        }
        param++;
    }

    if (param->type != NULL) {
        decl_type = c4m_global_copy(param->type);
        cmp_type  = c4m_merge_types(decl_type, ctx->sym->type);

        if (c4m_tspec_is_error(cmp_type)) {
            c4m_add_error(ctx->pass_ctx->file_ctx,
                          c4m_err_declared_incompat,
                          ctx->sym->declaration_node,
                          decl_type,
                          ctx->sym->type);
            return;
        }
        if (c4m_type_cmp_exact(decl_type, cmp_type) != c4m_type_match_exact) {
            c4m_add_error(ctx->pass_ctx->file_ctx,
                          c4m_err_too_general,
                          ctx->sym->declaration_node,
                          decl_type,
                          ctx->sym->type);
            return;
        }

        c4m_merge_types(param->type, ctx->sym->type);
    }
    else {
        cmp_type = c4m_merge_types(ctx->sym->type, ctx->formal->type);
        assert(!c4m_tspec_is_error(cmp_type));
    }
}

static void
check_user_decl(fn_check_ctx *ctx)
{
    if (ctx->num_defs == 0) {
        c4m_add_error(ctx->pass_ctx->file_ctx,
                      c4m_err_use_no_def,
                      ctx->sym->declaration_node,
                      ctx->sym->name);
    }

    if (ctx->num_uses == 0) {
        c4m_add_warning(ctx->pass_ctx->file_ctx,
                        c4m_warn_def_without_use,
                        ctx->sym->declaration_node,
                        ctx->sym->name);
    }
}

static void
check_function(pass2_ctx *ctx, c4m_scope_entry_t *fn_sym)
{
    fn_check_ctx check_ctx = {
        .node              = ctx->node,
        .decl              = fn_sym->value,
        .pass_ctx          = ctx,
        .delete_result_var = false,
    };

    check_ctx.si = check_ctx.decl->signature_info;

    static char resname[] = "$result";
    uint64_t    num_items;
    void      **view;

    c4m_scope_t *actuals = check_ctx.si->fn_scope;
    c4m_scope_t *formals = check_ctx.si->formals;

    view = hatrack_dict_values_sort(actuals->symbols, &num_items);

    for (uint64_t i = 0; i < num_items; i++) {
        c4m_scope_entry_t *sym = view[i];

        check_ctx.sym      = sym;
        check_ctx.formal   = hatrack_dict_get(formals->symbols,
                                            sym->name,
                                            NULL);
        check_ctx.num_defs = c4m_xlist_len(sym->sym_defs);
        check_ctx.num_uses = c4m_xlist_len(sym->sym_uses);

        if (!check_ctx.formal) {
            check_user_decl(&check_ctx);
        }
        else {
            if (!strcmp(sym->name->data, resname)) {
                check_return_type(&check_ctx);
            }
            else {
                check_formal_param(&check_ctx);
            }
        }
    }

    if (check_ctx.delete_result_var) {
        c4m_utf8_t *s = c4m_new_utf8(resname);
        hatrack_dict_remove(actuals->symbols, s);
        hatrack_dict_remove(formals->symbols, s);
    }

    // We'd previously set the fn symbol's signature, and we need
    // to update it to something more specific now.
    c4m_xlist_t *param_types = c4m_new(c4m_tspec_xlist(c4m_tspec_typespec()));

    // TODO: we need to handle varargs here and in the
    // check_formal_param bit. Right now this won't work.

    for (int i = 0; i < check_ctx.si->num_params; i++) {
        assert(check_ctx.si->param_info[i].type);
        c4m_xlist_append(param_types, check_ctx.si->param_info[i].type);
    }

    c4m_type_t *ret_type = check_ctx.si->return_info.type;

    check_ctx.si->full_type = c4m_tspec_fn(ret_type, param_types, false);

    c4m_merge_types(fn_sym->type, check_ctx.si->full_type);
}

static void
check_module_toplevel(pass2_ctx *ctx)
{
    ctx->node          = ctx->file_ctx->parse_tree;
    ctx->local_scope   = ctx->file_ctx->module_scope;
    ctx->cfg           = c4m_cfg_enter_block(NULL, ctx->node);
    ctx->file_ctx->cfg = ctx->cfg;
    ctx->func_nodes    = c4m_new(c4m_tspec_xlist(c4m_tspec_ref()));

    use_context_enter(ctx);
    check_pass_toplevel_dispatch(ctx);
    def_use_context_exit(ctx);
    ctx->cfg = c4m_cfg_exit_block(ctx->cfg, ctx->file_ctx->parse_tree);
}

static void
process_function_definitions(pass2_ctx *ctx)
{
    for (int i = 0; i < c4m_xlist_len(ctx->func_nodes); i++) {
        c4m_tree_node_t   *fn_root = c4m_xlist_get(ctx->func_nodes, i, NULL);
        c4m_pnode_t       *pnode   = c4m_tree_get_contents(fn_root);
        c4m_scope_entry_t *sym     = (c4m_scope_entry_t *)pnode->value;
        ctx->fn_decl               = sym->value;
        ctx->local_scope           = ctx->fn_decl->signature_info->fn_scope;
        ctx->cfg                   = c4m_cfg_enter_block(NULL, ctx->node);
        ctx->fn_decl->cfg          = ctx->cfg;

        ctx->fn_exit_node = ctx->cfg->contents.block_entrance.exit_node;

        use_context_enter(ctx);

        ctx->node = fn_root->children[fn_root->num_kids - 1];
        process_children(ctx);
        def_use_context_exit(ctx);
        ctx->cfg = c4m_cfg_exit_block(ctx->cfg, ctx->node);

        check_function(ctx, sym);

        sym->flags = sym->flags | C4M_F_FN_PASS_DONE;
    }
}

static c4m_xlist_t *
module_check_pass(c4m_compile_ctx *cctx, c4m_file_compile_ctx *file_ctx)
{
    // This should be checked before we get here, but belt and suspenders.
    if (c4m_fatal_error_in_module(file_ctx)) {
        return NULL;
    }

    pass2_ctx ctx = {
        .attr_scope     = cctx->final_attrs,
        .global_scope   = cctx->final_globals,
        .spec           = cctx->final_spec,
        .compile        = cctx,
        .file_ctx       = file_ctx,
        .du_stack       = 0,
        .du_stack_ix    = 0,
        .loop_stack     = c4m_new(c4m_tspec_xlist(c4m_tspec_ref())),
        .deferred_calls = c4m_new(c4m_tspec_xlist(c4m_tspec_ref())),
    };

    check_module_toplevel(&ctx);
    process_function_definitions(&ctx);

    return ctx.deferred_calls;
}

typedef struct {
    c4m_file_compile_ctx *file;
    c4m_xlist_t          *deferrals;
} defer_info_t;

static void
process_deferred_calls(c4m_compile_ctx *cctx,
                       defer_info_t    *info,
                       int              num_deferrals)
{
    for (int i = 0; i < num_deferrals; i++) {
        c4m_file_compile_ctx *f       = info->file;
        c4m_xlist_t          *one_set = info->deferrals;
        int                   n       = c4m_xlist_len(one_set);

        for (int i = 0; i < n; i++) {
            call_resolution_info_t *info = c4m_xlist_get(one_set, i, NULL);

            if (!info->deferred) {
                continue;
            }
            if (!info->polymorphic) {
                c4m_type_t *sym_type  = c4m_global_copy(info->resolution->type);
                c4m_type_t *call_type = info->sig;
                c4m_type_t *merged    = c4m_merge_types(sym_type, call_type);

                if (c4m_tspec_is_error(merged)) {
                    c4m_add_error(f,
                                  c4m_err_call_type_err,
                                  info->loc,
                                  call_type,
                                  sym_type);
                }
            }
            else {
                // Nothing for now.  We'll deal with this when we do code
                // generation.
            }
        }
    }
}

void
c4m_check_pass(c4m_compile_ctx *cctx)
{
    int           n            = c4m_xlist_len(cctx->module_ordering);
    int           num_deferred = 0;
    defer_info_t *all_deferred = c4m_gc_array_alloc(defer_info_t, n);
    c4m_xlist_t  *one_deferred;

    for (int i = 0; i < n; i++) {
        c4m_file_compile_ctx *f = c4m_xlist_get(cctx->module_ordering, i, NULL);

        one_deferred = module_check_pass(cctx, f);

        if (one_deferred == NULL) {
            continue;
        }

        if (c4m_xlist_len(one_deferred) != 0) {
            all_deferred[num_deferred].file        = f;
            all_deferred[num_deferred++].deferrals = one_deferred;
        }
        process_deferred_calls(cctx, all_deferred, num_deferred);
    }
}

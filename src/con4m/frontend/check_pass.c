#define C4M_USE_INTERNAL_API
#include "con4m.h"

typedef struct {
    c4m_utf8_t      *name;
    c4m_type_t      *sig;
    c4m_tree_node_t *loc;
    // Return type of container for things like index operations,
    // which will have to be checked later.
    c4m_type_t      *container_ret;
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
    c4m_xlist_t          *func_nodes;
    // Current fn decl object when in a fn. It's NULL in a module context.
    c4m_fn_decl_t        *fn_decl;
    c4m_xlist_t          *current_rhs_uses;
    c4m_xlist_t          *current_section_prefix;
    c4m_xlist_t          *loop_stack;
    c4m_xlist_t          *deferred_calls;
} pass2_ctx;

static inline bool
base_node_tcheck(c4m_pnode_t *pnode, c4m_type_t *type)
{
    return c4m_tspecs_are_compat(pnode->type, type);
}

static inline c4m_type_t *
get_pnode_type(c4m_tree_node_t *node)
{
    c4m_pnode_t *pnode = get_pnode(node);
    return pnode->type;
}

static void
add_def(pass2_ctx *ctx, c4m_scope_entry_t *sym)
{
    ctx->cfg = c4m_cfg_add_def(ctx->cfg, ctx->node, sym, ctx->current_rhs_uses);

    ctx->current_rhs_uses == NULL;
}

static void
add_use(pass2_ctx *ctx, c4m_scope_entry_t *sym)
{
    ctx->cfg = c4m_cfg_add_use(ctx->cfg, ctx->node, sym);
    if (ctx->current_rhs_uses) {
        c4m_xlist_append(ctx->current_rhs_uses, sym);
    }
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
        if (c4m_tspec_is_error(pnode->type)) {
            return; // Already errored for this node.
        }
    }

    if (!c4m_tspec_is_error(sym->type)) {
        if (base_node_tcheck(pnode, c4m_global_copy(sym->type))) {
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

        my_pnode->value = kid_pnode->value;
        my_pnode->type  = kid_pnode->type;
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
sym_lookup(pass2_ctx *ctx, c4m_utf8_t *name, bool add_du)
{
    c4m_scope_entry_t *result;

    result = c4m_symbol_lookup(ctx->local_scope,
                               ctx->file_ctx->module_scope,
                               ctx->global_scope,
                               ctx->attr_scope,
                               name);

    if (!result || !add_du) {
        return NULL;
    }

    if (is_def_context(ctx)) {
        add_def(ctx, result);
    }
    else {
        add_use(ctx, result);
    }

    return result;
}

static c4m_scope_entry_t *
lookup_or_add(pass2_ctx *ctx, c4m_utf8_t *name)
{
    c4m_scope_entry_t *result = sym_lookup(ctx, name, true);
    if (result) {
        return result;
    }
    result = c4m_add_inferred_symbol(ctx->file_ctx,
                                     ctx->local_scope,
                                     name);

    if (is_def_context(ctx)) {
        add_def(ctx, result);
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

    assert(pnode->type == NULL);

    process_child(ctx, 0);
    container_type = get_pnode_type(ctx->node->children[0]);
    use_context_enter(ctx);
    process_child(ctx, 1);
    ix1_type = get_pnode_type(ctx->node->children[1]);

    if (!c4m_tspec_is_int_type(ix1_type)) {
        if (!c4m_tspec_is_tvar(ix1_type)) {
            c4m_type_t *tmp = c4m_tspec_dict(ix1_type, c4m_tspec_typevar());
            container_type  = c4m_merge_types(tmp, container_type);

            if (num_kids == 3) {
                // TODO: error: Slice not allowed on dicts.
                return;
            }
        }
    }

    call_resolution_info_t *info = c4m_gc_alloc(call_resolution_info_t);

    if (num_kids == 3) {
        process_child(ctx, 2);
        ix2_type = get_pnode_type(ctx->node->children[2]);

        if (!c4m_tspec_is_int_type(ix2_type) && !c4m_tspec_is_tvar(ix2_type)) {
            // Error: slice requires int indicies.
        }

        if (c4m_tspec_is_tvar(ix1_type)) {
            c4m_merge_types(ix1_type, c4m_tspec_i32());
        }

        if (c4m_tspec_is_tvar(ix1_type)) {
            c4m_merge_types(ix1_type, c4m_tspec_i32());
        }

        info->name          = c4m_new_utf8("__slice__");
        info->loc           = ctx->node;
        info->container_ret = node_type;
        info->sig           = c4m_tspec_fn_va(info->container_ret,
                                    3,
                                    container_type,
                                    ix1_type,
                                    ix2_type);
    }
    else {
        info->name          = c4m_new_utf8("__index__");
        info->loc           = ctx->node;
        info->container_ret = node_type;
        info->sig           = c4m_tspec_fn_va(info->container_ret,
                                    2,
                                    container_type,
                                    ix1_type);
    }

    c4m_xlist_append(ctx->deferred_calls, info);

    def_use_context_exit(ctx);
}

static void
handle_call(pass2_ctx *ctx)
{
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

    li->loop_ix->flags = C4M_F_ALL_SYM_FLAGS;
    li->loop_ix->type  = c4m_tspec_u32();

    add_def(ctx, li->loop_ix);

    if (li->label != NULL) {
        li->label_ix = c4m_str_concat(li->label, ix_var_name);

        if (c4m_symbol_lookup(ctx->local_scope,
                              NULL,
                              NULL,
                              NULL,
                              li->label_ix)) {
            // TODO: error, dupe loop label.
            return;
        }

        li->named_loop_ix        = c4m_add_inferred_symbol(ctx->file_ctx,
                                                    ctx->local_scope,
                                                    li->label_ix);
        li->named_loop_ix->flags = C4M_F_ALL_SYM_FLAGS;
        li->named_loop_ix->type  = c4m_tspec_u32();

        add_def(ctx, li->named_loop_ix);
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

    li->loop_last        = c4m_add_or_replace_symbol(ctx->file_ctx,
                                              ctx->local_scope,
                                              last_var_name);
    li->loop_last->flags = C4M_F_ALL_SYM_FLAGS;
    li->loop_last->type  = c4m_tspec_u32();

    add_def(ctx, li->loop_last);

    if (li->label != NULL) {
        li->label_last             = c4m_to_utf8(c4m_str_concat(li->label,
                                                    last_var_name));
        li->named_loop_last        = c4m_add_inferred_symbol(ctx->file_ctx,
                                                      ctx->local_scope,
                                                      li->label_last);
        li->named_loop_last->flags = C4M_F_ALL_SYM_FLAGS;
        li->named_loop_last->type  = c4m_tspec_u32();

        add_def(ctx, li->named_loop_last);
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
            printf("TODO: Add an ERROR here for the name collision.\n");
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
    li->lvar_1->flags            = C4M_F_ALL_SYM_FLAGS;
    li->lvar_1->declaration_node = var_node1;

    if (li->shadowed_lvar_1 != NULL) {
        // TODO: Warn about the shadowing.:
    }

    add_def(ctx, li->lvar_1);

    if (var2_name) {
        li->shadowed_lvar_2          = c4m_symbol_lookup(ctx->local_scope,
                                                NULL,
                                                NULL,
                                                NULL,
                                                var2_name);
        li->lvar_2                   = c4m_add_or_replace_symbol(ctx->file_ctx,
                                               ctx->local_scope,
                                               var2_name);
        li->lvar_2->flags            = C4M_F_ALL_SYM_FLAGS;
        li->lvar_2->declaration_node = var_node2;

        if (li->shadowed_lvar_2 != NULL) {
            // TODO: Warn about the shadowing.
        }

        add_def(ctx, li->lvar_2);
    }

    // Okay, now we need to add the branch node to the CFG.

    branch   = c4m_cfg_block_new_branch_node(ctx->cfg, 2, li->label, ctx->node);
    ctx->cfg = c4m_cfg_enter_block(branch, n->children[expr_ix]);

    branch->contents.branches.branch_targets[0] = ctx->cfg;

    // Now, process that body. The body gets its own block in the CFG,
    // which makes this a bit clunkier than it needs to be.
    expr_ix += 2;
    ctx->node = n->children[expr_ix];

    base_check_pass_dispatch(ctx);
    c4m_cfg_exit_block(ctx->cfg, n);
    c4m_cfg_node_t *tmp = c4m_cfg_enter_block(branch, n->children[expr_ix]);

    // This sets up an empty block for the code that runs when the
    // loop condition is false.
    branch->contents.branches.branch_targets[1] = tmp;
    c4m_cfg_exit_block(tmp, n);
    ctx->cfg = entrance->contents.block_entrance.exit_node;

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
    // If there are two variables, we can infer that the container is
    // a dictionary, and compare. Otherwise, if the inferred type
    // isn't a container type, then we give an error.

    if (li->lvar_2 != NULL) {
        c4m_merge_types(c4m_tspec_dict(li->lvar_1->type, li->lvar_2->type),
                        container_pnode->type);
    }
    else {
        switch (c4m_tspec_get_base(container_pnode->type)) {
        case C4M_DT_KIND_list:
            c4m_merge_types(li->lvar_1->type,
                            c4m_tspec_get_param(container_pnode->type, 0));
            break;
        case C4M_DT_KIND_dict:
            if (c4m_tspec_get_num_params(container_pnode->type) == 1) {
                c4m_merge_types(li->lvar_1->type,
                                c4m_tspec_get_param(container_pnode->type, 0));
            }
            else {
                printf("Need an error for dictionary not allowed...\n");
            }
            break;
        case C4M_DT_KIND_type_var:
            printf(
                "For now, need an error saying some specificity "
                "would be required on the container type.\n");
            break;
        default:
            printf("Need an error here that the thing isn't a damn container.");
            break;
        }
    }

    // TODO: capture the data flows, not just the def/use order here.
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
    branch   = c4m_cfg_block_new_branch_node(ctx->cfg, 2, li->label, ctx->node);
    ctx->cfg = c4m_cfg_enter_block(branch, n->children[expr_ix]);

    branch->contents.branches.branch_targets[0] = ctx->cfg;
    ctx->node                                   = n->children[expr_ix];

    base_check_pass_dispatch(ctx);
    c4m_cfg_exit_block(ctx->cfg, n);
    c4m_cfg_node_t *tmp = c4m_cfg_enter_block(branch, n->children[expr_ix]);

    branch->contents.branches.branch_targets[1] = tmp;
    c4m_cfg_exit_block(tmp, n);
    ctx->cfg = entrance->contents.block_entrance.exit_node;

    c4m_xlist_pop(ctx->loop_stack);
    ctx->node = n;
    loop_pop_ix_var(ctx, li);
}

static void
handle_casing_statement(pass2_ctx *ctx)
{
}

static void
handle_assign(pass2_ctx *ctx)
{
}

static void
handle_section_decl(pass2_ctx *ctx)
{
}

static void
handle_identifier(pass2_ctx *ctx)
{
    c4m_pnode_t *pnode = get_pnode(ctx->node);
    c4m_utf8_t  *id    = node_text(ctx->node);

    if (ctx->current_section_prefix != NULL) {
        c4m_xlist_append(ctx->current_section_prefix, id);
    }
    else {
        pnode->value = (void *)lookup_or_add(ctx, id);
    }
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
        pnode->type = ((c4m_partial_lit_t *)pnode->value)->type;
    }
}

static void
handle_member(pass2_ctx *ctx)
{
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
    pn->type = type_check_nodes(tn->children[0], tn->children[1]);
}

static void
handle_binary_op(pass2_ctx *ctx)
{
}

static void
handle_unary_op(pass2_ctx *ctx)
{
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

    case c4m_nt_for:
        handle_for(ctx);
        break;

    case c4m_nt_while:
        handle_while(ctx);
        break;

    case c4m_nt_typeof:
    case c4m_nt_switch:
        handle_casing_statement(ctx);
        break;

    case c4m_nt_assign:
    case c4m_nt_binary_assign_op:
        handle_assign(ctx);
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

    default:
        process_children(ctx);
        break;
    }

    if (pnode->type == NULL) {
        pnode->type = c4m_tspec_typevar();
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

            add_def(ctx, sym);
            process_node(ctx, tnode);

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

static void
check_pass_fn_dispatch(pass2_ctx *ctx)
{
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
    // c4m_cfg_exit_block(ctx->cfg, ctx->file_ctx->parse_tree);
}

static void
process_function_definitions(pass2_ctx *ctx)
{
    for (int i = 0; i < c4m_xlist_len(ctx->func_nodes); i++) {
        ctx->node                = c4m_xlist_get(ctx->func_nodes, 1, NULL);
        c4m_pnode_t       *pnode = c4m_tree_get_contents(ctx->node);
        c4m_scope_entry_t *sym   = (c4m_scope_entry_t *)pnode->value;
        ctx->local_scope         = pnode->static_scope;
        ctx->cfg                 = c4m_cfg_enter_block(NULL, ctx->node);
        ctx->fn_decl             = sym->value;
        ctx->fn_decl->cfg        = ctx->cfg;

        use_context_enter(ctx);
        check_pass_fn_dispatch(ctx);
        def_use_context_exit(ctx);
        c4m_cfg_exit_block(ctx->cfg, ctx->node);
    }
}

static void
module_check_pass(c4m_compile_ctx *cctx, c4m_file_compile_ctx *file_ctx)
{
    // This should be checked before we get here, but belt and suspenders.
    if (c4m_fatal_error_in_module(file_ctx)) {
        return;
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

    c4m_print_parse_node(file_ctx->parse_tree);

    check_module_toplevel(&ctx);
    process_function_definitions(&ctx);

    return;
}

void
c4m_check_pass(c4m_compile_ctx *cctx)
{
    for (int i = 0; i < c4m_xlist_len(cctx->module_ordering); i++) {
        module_check_pass(cctx, c4m_xlist_get(cctx->module_ordering, i, NULL));
    }
}

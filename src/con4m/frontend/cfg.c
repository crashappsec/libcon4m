#define C4M_USE_INTERNAL_API
#include "con4m.h"

typedef struct {
    c4m_dict_t            *base_du_info;
    c4m_cfg_branch_info_t *branches;
} c4m_branch_ctx;

typedef struct {
    c4m_file_compile_ctx *file_ctx;
    c4m_dict_t           *du_info;
    c4m_xlist_t          *sometimes_info;
} cfg_ctx;

static c4m_scope_entry_t *
follow_sym_links(c4m_scope_entry_t *sym)
{
    while (sym->linked_symbol != NULL) {
        sym = sym->linked_symbol;
    }

    return sym;
}

static bool
cfg_propogate_def(cfg_ctx           *ctx,
                  c4m_scope_entry_t *sym,
                  c4m_cfg_node_t    *n,
                  c4m_xlist_t       *deps)
{
    c4m_dict_t *du_info;

    if (n == NULL) {
        du_info = ctx->du_info;
    }
    else {
        du_info = n->liveness_info;
    }
    // TODO, make use of the data flow information. For now, we're just going
    // to drop it on the floor. Note that this
    sym = follow_sym_links(sym);

    c4m_cfg_status_t *new = c4m_gc_alloc(c4m_cfg_status_t);
    c4m_cfg_status_t *old = hatrack_dict_get(du_info, sym, NULL);

    if (old) {
        new->last_use = old->last_use;
    }

    new->last_def = n;

    hatrack_dict_put(du_info, sym, new);

    return old != NULL;
}

static bool
cfg_propogate_use(cfg_ctx           *ctx,
                  c4m_scope_entry_t *sym,
                  c4m_cfg_node_t    *n)
{
    c4m_dict_t *du_info;

    if (n == NULL) {
        du_info = ctx->du_info;
    }
    else {
        du_info = n->liveness_info;
    }

    sym = follow_sym_links(sym);

    c4m_cfg_status_t *new = c4m_gc_alloc(c4m_cfg_status_t);
    c4m_cfg_status_t *old = hatrack_dict_get(du_info, sym, NULL);

    if (old) {
        new->last_def = old->last_def;
    }
    else {
        n->use_without_def = 1;
    }

    new->last_use = n;

    hatrack_dict_put(du_info, sym, new);

    return old != NULL;
}

static void
cfg_copy_du_info(cfg_ctx        *ctx,
                 c4m_cfg_node_t *node,
                 c4m_dict_t    **new_dict,
                 c4m_xlist_t   **new_sometimes)
{
    c4m_dict_t *copy = c4m_new(c4m_tspec_dict(c4m_tspec_ref(),
                                              c4m_tspec_ref()));
    uint64_t    n;

    hatrack_dict_item_t *view = hatrack_dict_items_sort(node->liveness_info,
                                                        &n);

    for (uint64_t i = 0; i < n; i++) {
        hatrack_dict_put(copy, view[i].key, view[i].value);
    }

    *new_dict = copy;

    c4m_xlist_t *old = node->sometimes_live;

    if (old != NULL) {
        *new_sometimes = c4m_new(c4m_tspec_xlist(c4m_tspec_ref()));
        int l          = c4m_xlist_len(old);
        for (int i = 0; i < l; i++) {
            c4m_xlist_append(*new_sometimes, c4m_xlist_get(old, i, NULL));
        }
    }
}

static c4m_utf8_t *result_text = NULL;

static void
check_for_fn_exit_errors(c4m_file_compile_ctx *file, c4m_fn_decl_t *fn_decl)
{
    if (result_text == NULL) {
        result_text = c4m_new_utf8("$result");
        c4m_gc_register_root(&result_text, 1);
    }

    if (fn_decl->signature_info->return_info.type->typeid == C4M_T_VOID) {
        return;
    }

    c4m_scope_t       *fn_scope  = fn_decl->signature_info->fn_scope;
    c4m_scope_entry_t *ressym    = c4m_symbol_lookup(fn_scope,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  result_text);
    c4m_cfg_node_t    *node      = fn_decl->cfg;
    c4m_cfg_node_t    *exit_node = node->contents.block_entrance.exit_node;
    c4m_cfg_status_t  *status    = hatrack_dict_get(exit_node->liveness_info,
                                                ressym,
                                                NULL);

    // the result symbol is in the ending live set, so we're done.

    if (status != NULL) {
        return;
    }

    // If nothing is returned at all ever, we already handled that.
    // Only look for when we didn't see it in all paths.
    if (c4m_xlist_len(ressym->sym_defs) > 0) {
        c4m_add_error(file,
                      c4m_cfg_return_coverage,
                      fn_decl->cfg->reference_location);
    }
}

static void
check_for_module_exit_errors(cfg_ctx *ctx, c4m_cfg_node_t *node)
{
    // TODO: info msg when variables are defined in the module that are:
    // 1. Global; or
    // 2. Used in a function
    // But are not defined by the end of the top-level, taking into
    // account functions the toplevel calls w/in the module.
}

static void
check_block_for_errors(cfg_ctx *ctx, c4m_cfg_node_t *node)
{
    //  Search the graph again for use/before def errors, and give
    //  appropriate error messages, based on my 'sometimes_live'
    //  field.

    if (node == NULL) {
        return;
    }

    node->reached = 1;

    switch (node->kind) {
    case c4m_cfg_block_entrance:
        check_block_for_errors(ctx, node->contents.block_entrance.next_node);
        if (!node->contents.block_entrance.exit_node->reached) {
            check_block_for_errors(ctx,
                                   node->contents.block_entrance.exit_node);
        }
        return;
    case c4m_cfg_block_exit:
        check_block_for_errors(ctx, node->contents.block_exit.next_node);
        return;
    case c4m_cfg_node_branch:
        for (int i = 0; i < node->contents.branches.num_branches; i++) {
            c4m_cfg_node_t *n = node->contents.branches.branch_targets[i];
            check_block_for_errors(ctx, n);
        }
        return;
    case c4m_cfg_def:
        check_block_for_errors(ctx, node->contents.flow.next_node);
        return;
    case c4m_cfg_call:
    case c4m_cfg_use:

        if (node->use_without_def) {
            c4m_scope_entry_t  *sym;
            bool                sometimes = false;
            c4m_scope_entry_t  *check;
            c4m_compile_error_t err;

            sym = node->contents.flow.dst_symbol;

            if (!(sym->flags & C4M_F_USE_ERROR)) {
                int n = c4m_xlist_len(node->sometimes_live);
                for (int i = 0; i < n; i++) {
                    check = c4m_xlist_get(node->sometimes_live, i, NULL);
                    if (check == sym) {
                        sometimes = true;
                        break;
                    }
                }

                if (sometimes) {
                    err = c4m_cfg_use_possible_def;
                }
                else {
                    err = c4m_cfg_use_no_def;
                }
                c4m_add_error(ctx->file_ctx,
                              err,
                              node->reference_location,
                              sym->name);
            }
        }

        check_block_for_errors(ctx, node->contents.flow.next_node);
        return;
    case c4m_cfg_jump:
        return;
    }
}

typedef union {
    struct {
        uint16_t count;
        uint16_t uses;
        uint16_t defs;
    } counters;
    void *ptr;
} cfg_merge_ct_t;

// Aux entries come to us through a continue, break or return that
// is NESTED INSIDE OF US. Any data flows on those branches
// didn't propogate down to any previous join branch. So the
// entry def/use info doesn't need to be modified, but the exit
// info might need to be.
//
// What this means really, is:
//
// 1) If we're at a block entrance, we will stick anything not in our
//    exit d/u set to the 'sometimes' list.
//
// 2) For block exits, we'll have to do a proper merge; symbols only
//    should end up in the d/u list if they exist in both entries.

static void
cfg_merge_aux_entries_to_top(cfg_ctx *ctx, c4m_cfg_node_t *node)
{
    c4m_xlist_t       *inbounds = node->contents.block_entrance.inbound_links;
    c4m_cfg_node_t    *exit     = node->contents.block_entrance.exit_node;
    c4m_dict_t        *exit_du  = exit->liveness_info;
    c4m_scope_entry_t *sym;

    if (inbounds == NULL) {
        return;
    }
    int num_inbounds = c4m_xlist_len(inbounds);

    if (num_inbounds == 0) {
        return;
    }

    c4m_set_t *sometimes = c4m_new(c4m_tspec_set(c4m_tspec_ref()));

    if (exit->sometimes_live != NULL) {
        int n = c4m_xlist_len(exit->sometimes_live);

        // Any existing sometimes items are always outbound sometimes
        // items.
        for (int i = 0; i < n; i++) {
            sym = c4m_xlist_get(exit->sometimes_live, i, NULL);

            if (hatrack_dict_get(exit_du, sym, NULL) == NULL) {
                c4m_set_add(sometimes, sym);
            }
        }
    }

    for (int i = 0; i < num_inbounds; i++) {
        uint64_t        nitems;
        c4m_cfg_node_t *one = c4m_xlist_get(inbounds, i, NULL);

        if (one->liveness_info == NULL) {
            continue;
        }
        void **dusyms = hatrack_dict_keys_sort(one->liveness_info,
                                               &nitems);

        for (unsigned int j = 0; j < nitems; j++) {
            sym = dusyms[j];

            // Dead branch.
            if (exit_du == NULL) {
                continue;
            }

            if (hatrack_dict_get(exit_du, sym, NULL) == NULL) {
                c4m_set_add(sometimes, sym);
            }
        }

        if (one->sometimes_live != NULL) {
            int n = c4m_xlist_len(one->sometimes_live);

            for (int j = 0; j < n; j++) {
                sym = c4m_xlist_get(one->sometimes_live, i, NULL);

                // If it's in the exit set, it's not a 'sometimes'
                // for the block.
                if (hatrack_dict_get(exit_du, sym, NULL) == NULL) {
                    c4m_set_add(sometimes, sym);
                }
            }
        }
    }

    exit->sometimes_live = c4m_set_to_xlist(sometimes);
}

static void
process_branch_exit(cfg_ctx *ctx, c4m_cfg_node_t *node)
{
    // Merge and push forward info on partial crapola.
    c4m_dict_t            *counters  = c4m_new(c4m_tspec_dict(c4m_tspec_ref(),
                                                  c4m_tspec_ref()));
    c4m_set_t             *sometimes = c4m_new(c4m_tspec_set(c4m_tspec_ref()));
    c4m_cfg_branch_info_t *bi        = &node->contents.branches;
    c4m_dict_t            *merged    = c4m_new(c4m_tspec_dict(c4m_tspec_ref(),
                                                c4m_tspec_ref()));
    c4m_cfg_node_t        *exit_node;
    c4m_scope_entry_t     *sym;
    hatrack_dict_item_t   *view;
    c4m_cfg_status_t      *status;
    c4m_cfg_status_t      *old_status;
    cfg_merge_ct_t         count_info;
    uint64_t               len;

    for (int i = 0; i < bi->num_branches; i++) {
        exit_node = bi->branch_targets[i]->contents.block_entrance.exit_node;

        c4m_dict_t  *duinfo = exit_node->liveness_info;
        c4m_xlist_t *stinfo = exit_node->sometimes_live;

        // TODO: fix this.
        if (duinfo == NULL) {
            continue;
        }

        view = hatrack_dict_items_sort(duinfo, &len);

        for (unsigned int j = 0; j < len; j++) {
            sym            = view[j].key;
            status         = view[j].value;
            old_status     = hatrack_dict_get(node->liveness_info, sym, NULL);
            count_info.ptr = hatrack_dict_get(counters, sym, NULL);

            count_info.counters.count++;
            if (old_status == NULL) {
                if (status->last_use != NULL) {
                    count_info.counters.uses++;
                }
                if (status->last_def != NULL) {
                    count_info.counters.defs++;
                }
            }
            else {
                if (status->last_use != old_status->last_use) {
                    count_info.counters.uses++;
                }
                if (status->last_def != old_status->last_def) {
                    count_info.counters.defs++;
                }
            }

            hatrack_dict_put(counters, sym, count_info.ptr);
        }

        // If it's not always live in a subblock, it's not always live
        // in the full block.
        if (stinfo != NULL) {
            len = c4m_xlist_len(stinfo);

            for (unsigned int j = 0; j < len; j++) {
                sym = c4m_xlist_get(stinfo, j, NULL);
                c4m_set_add(sometimes, sym);
            }
        }
    }

    view = hatrack_dict_items_sort(counters, &len);

    for (unsigned int i = 0; i < len; i++) {
        sym            = view[i].key;
        count_info.ptr = view[i].value;

        // Symbol didn't show up in every branch, so it goes on the
        // 'sometimes' list.
        if (count_info.counters.count < bi->num_branches) {
            c4m_set_add(sometimes, sym);
            continue;
        }

        status     = c4m_gc_alloc(c4m_cfg_status_t);
        old_status = hatrack_dict_get(node->liveness_info, sym, NULL);
        if (old_status != NULL) {
            status->last_def = old_status->last_def;
            status->last_use = old_status->last_use;
        }
        if (count_info.counters.uses == bi->num_branches) {
            status->last_use = node;
        }
        if (count_info.counters.defs == bi->num_branches) {
            status->last_def = node;
        }

        hatrack_dict_put(merged, sym, status);
    }

    // Okay, we've done all the merging, now we have to propogate the
    // results to the exit node for the whole branching structure.
    node->liveness_info  = merged;
    node->sometimes_live = c4m_set_to_xlist(sometimes);
}

static void
set_starting_du_info(cfg_ctx *ctx, c4m_cfg_node_t *n, c4m_cfg_node_t *parent)
{
    if (!parent) {
        if (n->liveness_info != NULL) {
            return;
        }
        n->liveness_info          = ctx->du_info;
        n->starting_liveness_info = ctx->du_info;
        n->sometimes_live         = ctx->sometimes_info;
        n->starting_sometimes     = ctx->sometimes_info;
        return;
    }
    else {
        cfg_copy_du_info(ctx,
                         parent,
                         &n->starting_liveness_info,
                         &n->starting_sometimes);
        cfg_copy_du_info(ctx,
                         parent,
                         &n->liveness_info,
                         &n->sometimes_live);
    }
}

static c4m_cfg_node_t *
cfg_process_node(cfg_ctx *ctx, c4m_cfg_node_t *node, c4m_cfg_node_t *parent)
{
    c4m_cfg_node_t             *next;
    c4m_cfg_block_enter_info_t *enter_info;
    c4m_branch_ctx              branch_info;

    if (node == NULL) {
        return NULL;
    }

    node->reached = 1;

    set_starting_du_info(ctx, node, parent);
    switch (node->kind) {
    case c4m_cfg_block_entrance:
        enter_info = &node->contents.block_entrance;

        // Anything defined coming into the block will be defined
        // coming out, so we can copy those into the exit node as
        // 'sure things'. Similarly, anything that happens before the
        // first branch in this block is a sure thing, so we are going
        // to copy the state now, and use the same reference in the
        // entrance and the exit node, so that any d/u info before the
        // branch automatically propogates.
        //
        // Anything that is changes in a branch will, if the branch
        // returns to an entrance or exit node, be pushed into the
        // `to_merge` field. When we're done with a block, we do the
        // merging, which accomplishes a few things:

        // 1. It determines what gets set in EVERY branch, and is
        //    guaranteed to be defined underneath.
        // 2. It allows us to determine what symbols MIGHT be undefined
        //    below this block, depending on the branch, so that we can
        //    give a clear enough error message (as opposed to insisting
        //    on a definitive use-before-def).
        // 3. It allows us to do the same WITHIN our block, when we
        //    can re-enter from the top via continues.
        //
        // Because of #3, we don't want to emit errors for use-before-def
        // until AFTER processing the block. We just mark nodes that
        // have errors, and make a second pass through the block once
        // we can give the right error message.

        c4m_cfg_node_t *ret = cfg_process_node(ctx,
                                               enter_info->next_node,
                                               node);

        if (ret) {
            cfg_copy_du_info(ctx,
                             ret,
                             &node->liveness_info,
                             &node->sometimes_live);
        }
        if (!enter_info->exit_node->reached) {
            ret = cfg_process_node(ctx, enter_info->exit_node, node);
            cfg_copy_du_info(ctx,
                             ret,
                             &node->liveness_info,
                             &node->sometimes_live);
        }
        else {
            // Clear the flag for the error collection pass.
            enter_info->exit_node->reached = 0;
        }

        cfg_merge_aux_entries_to_top(ctx, node);

        return ret;

    case c4m_cfg_block_exit:

        next = node->contents.block_exit.next_node;

        if (!next) {
            return node;
        }

        return cfg_process_node(ctx, next, node);

    case c4m_cfg_node_branch:

        branch_info.base_du_info = node->liveness_info;
        branch_info.branches     = &node->contents.branches;

        for (int i = 0; i < branch_info.branches->num_branches; i++) {
            set_starting_du_info(ctx,
                                 branch_info.branches->branch_targets[i],
                                 node);
            c4m_cfg_node_t *one = cfg_process_node(
                ctx,
                branch_info.branches->branch_targets[i],
                NULL);

            if (one && one->kind == c4m_cfg_block_exit) {
                c4m_cfg_node_t *exit = branch_info.branches->exit_node;
                c4m_xlist_append(exit->contents.block_exit.inbound_links, one);
            }
        }

        process_branch_exit(ctx, node);

        return cfg_process_node(ctx, branch_info.branches->exit_node, node);

    case c4m_cfg_use:
        cfg_copy_du_info(ctx,
                         node->parent,
                         &node->liveness_info,
                         &node->sometimes_live);

        cfg_propogate_use(ctx, node->contents.flow.dst_symbol, node);
        cfg_process_node(ctx, node->contents.flow.next_node, node);
        return node;

    case c4m_cfg_def:
        cfg_copy_du_info(ctx,
                         node->parent,
                         &node->liveness_info,
                         &node->sometimes_live);

        cfg_propogate_def(ctx,
                          node->contents.flow.dst_symbol,
                          node,
                          node->contents.flow.deps);
        cfg_process_node(ctx, node->contents.flow.next_node, node);
        return node;

    case c4m_cfg_call:
        cfg_copy_du_info(ctx,
                         node->parent,
                         &node->liveness_info,
                         &node->sometimes_live);

        for (int i = 0; i < c4m_xlist_len(node->contents.flow.deps); i++) {
            c4m_scope_entry_t *sym = c4m_xlist_get(node->contents.flow.deps,
                                                   i,
                                                   NULL);
            cfg_propogate_use(ctx, sym, node);
        }
        cfg_process_node(ctx, node->contents.flow.next_node, node);
        return node;
    case c4m_cfg_jump:
        cfg_copy_du_info(ctx,
                         node->parent,
                         &node->liveness_info,
                         &node->sometimes_live);
        return NULL;
    }
    c4m_unreachable();
}

// The input will be the module, plus any d/u info that we inherit, which
// includes attributes that are set during the initial import of any
// previous modules to have been analyzed.
//
// We must first analyze the module entry, then any defined functions,
// and then we return def/use info for the lop-level module eval, to pass
// to the next one.
//
// Inbound calls here must provided a mutable du_info dict.
//
//
void
c4m_cfg_analyze(c4m_file_compile_ctx *file_ctx, c4m_dict_t *du_info)
{
    if (du_info == NULL) {
        du_info = c4m_new(c4m_tspec_dict(c4m_tspec_ref(), c4m_tspec_ref()));
    }

    cfg_ctx ctx = {
        .file_ctx       = file_ctx,
        .du_info        = du_info,
        .sometimes_info = NULL,
    };

    uint64_t nparams;
    void   **view = hatrack_dict_values_sort(file_ctx->parameters, &nparams);

    for (uint64_t i = 0; i < nparams; i++) {
        c4m_module_param_info_t *param = view[i];
        c4m_scope_entry_t       *sym   = param->linked_symbol;

        cfg_propogate_def(&ctx, sym, NULL, NULL);
    }

    file_ctx->cfg->liveness_info = du_info;
    cfg_process_node(&ctx, file_ctx->cfg, NULL);
    check_block_for_errors(&ctx, file_ctx->cfg);
    check_for_module_exit_errors(&ctx, file_ctx->cfg);

    int n = c4m_xlist_len(file_ctx->fn_def_syms);

    c4m_cfg_node_t *modexit = file_ctx->cfg->contents.block_entrance.exit_node;
    c4m_dict_t     *moddefs = modexit->liveness_info;
    c4m_xlist_t    *stdefs  = modexit->sometimes_live;

    for (int i = 0; i < n; i++) {
        ctx.du_info             = moddefs;
        ctx.sometimes_info      = stdefs;
        c4m_scope_entry_t *sym  = c4m_xlist_get(file_ctx->fn_def_syms, i, NULL);
        c4m_fn_decl_t     *decl = sym->value;

        cfg_process_node(&ctx, decl->cfg, NULL);
        check_block_for_errors(&ctx, decl->cfg);
        check_for_fn_exit_errors(file_ctx, decl);
    }
}

#define C4M_USE_INTERNAL_API
#include "con4m.h"

typedef struct {
    c4m_scope_t          *attr_scope;
    c4m_scope_t          *global_scope;
    c4m_spec_t           *spec;
    c4m_compile_ctx      *compile;
    c4m_file_compile_ctx *file_ctx;
    __uint128_t           du_stack;
    int                   du_stack_ix;
    c4m_xlist_t          *errors;
    // The above get initialized only once when we start processing a module.
    // Everything below this comment gets updated for each function entry too.
    c4m_scope_t          *local_scope;
    c4m_tree_node_t      *node;
    c4m_cfg_node_t       *cfg; // Current control-flow-graph node.
    c4m_xlist_t          *func_nodes;
    // Current fn decl object when in a fn. It's NULL in a module context.
    c4m_fn_decl_t        *fn_decl;
} pass2_ctx;

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
}

static inline bool
is_def_context(pass2_ctx *ctx)
{
    return (bool)ctx->du_stack & 0x1;
}

static void
handle_index(pass2_ctx *ctx)
{
}

static void
handle_call(pass2_ctx *ctx)
{
}

static void
handle_label(pass2_ctx *ctx)
{
}

static void
handle_break(pass2_ctx *ctx)
{
}

static void
handle_continue(pass2_ctx *ctx)
{
}

static void
handle_for(pass2_ctx *ctx)
{
    process_children(ctx);
}

static void
handle_while(pass2_ctx *ctx)
{
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
handle_enum_decl(pass2_ctx *ctx)
{
}

static void
handle_config_spec(pass2_ctx *ctx)
{
}

static void
handle_section_spec(pass2_ctx *ctx)
{
}

static void
handle_param_block(pass2_ctx *ctx)
{
}

static void
handle_extern_block(pass2_ctx *ctx)
{
}

static void
handle_use_stmt(pass2_ctx *ctx)
{
}

static void
handle_var_decl(pass2_ctx *ctx)
{
}

static void
handle_section_decl(pass2_ctx *ctx)
{
}

static void
handle_identifier(pass2_ctx *ctx)
{
}

static void
handle_literal(pass2_ctx *ctx)
{
}

static void
handle_member(pass2_ctx *ctx)
{
}

static void
pass_dispatch(pass2_ctx *ctx)
{
    c4m_pnode_t *pnode = c4m_tree_get_contents(ctx->node);

    switch (pnode->kind) {
    case c4m_nt_global_enum:
    case c4m_nt_enum:
        handle_enum_decl(ctx);
        break;

    case c4m_nt_func_def:
        return;

    case c4m_nt_variable_decls:
        handle_var_decl(ctx);
        break;

    case c4m_nt_config_spec:
        handle_config_spec(ctx);
        break;

    case c4m_nt_section_spec:
        handle_section_spec(ctx);
        break;

    case c4m_nt_section:
        handle_section_decl(ctx);
        break;

    case c4m_nt_param_block:
        handle_param_block(ctx);
        break;

    case c4m_nt_extern_block:
        handle_extern_block(ctx);
        break;

    case c4m_nt_use:
        handle_use_stmt(ctx);
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
        handle_literal(ctx);
        break;

    case c4m_nt_index:
        handle_index(ctx);
        return;

    case c4m_nt_call:
        handle_call(ctx);
        break;

    case c4m_nt_label:
        handle_label(ctx);
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
        return;

    default:
        process_children(ctx);
        break;
    }
}

static void
check_pass_toplevel_dispatch(pass2_ctx *ctx)
{
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
    c4m_cfg_exit_block(ctx->cfg, ctx->file_ctx->parse_tree);
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
        .attr_scope   = cctx->final_attrs,
        .global_scope = cctx->final_globals,
        .spec         = cctx->final_spec,
        .compile      = cctx,
        .file_ctx     = file_ctx,
        .du_stack     = 0,
        .du_stack_ix  = 0,
        .errors       = file_ctx->errors,
    };

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

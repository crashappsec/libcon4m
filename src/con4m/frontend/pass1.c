// First pass of the raw parse tree.
// Not everything we need would be resolvable in one pass, especially
// symbols in other modules.

typedef struct {
    c4m_tree_node_t      *cur_tree_node;
    c4m_pnode_t          *cur;
    c4m_file_compile_ctx *file_ctx;
} pass_ctx;

static inline void
set_current_node(pass_ctx *ctx, c4m_tree_node_t *n)
{
    ctx->cur_tree_node = n;
    ctx->cur           = c4m_tree_get_contents(n);
}

static inline bool
node_down(pass_ctx *ctx, int i)
{
    c4m_tree_node_t *n = ctx->cur_tree_node;

    if (i >= n->num_kids) {
        return false;
    }

    assert(n->children[i]->parent == n);

    set_current_node(ctx, n->children[i]);

    return true;
}

static inline void *
node_info(pass_ctx *ctx)
{
    return ctx->cur->extra_info;
}

static inline void
process_children(pass_ctx *ctx)
{
    c4m_tree_node_t *n = ctx->cur_tree_node;

    for (int i = 0; i < n->num_kids; i++) {
        node_down(ctx, i);
    }
}

static inline c4m_node_kind_t
cur_node_type(pass_ctx *ctx)
{
    return ctx->cur->kind;
}

static inline c4m_pnode_t *
cur_node(pass_ctx *ctx)
{
    return ctx->cur;
}

static inline static inline void
node_up(c4m_file_compile_ctx *ctx)
{
    ctx->cur = ctx->cur->parent;
}

void
pass_dispatch(pass_ctx *ctx)
{
    switch (cur_node_type(ctx)) {
    case c4m_nt_enum:
        handle_enum_decl(ctx);
        break;
    case c4m_nt_func_def:
        handle_func_decl(ctx);
        break;
    case c4m_nt_var_decls:
    case c4m_nt_global_decls:
    case c4m_nt_const_var_decls:
    case c4m_nt_const_global_decls:
    case c4m_nt_const_var_decls:
    default:
        process_children(ctx);
        break;
    }
}

void
c4m_pass_1(c4m_file_compile_ctx *file_ctx)
{
    pass_ctx ctx = {
        .file_ctx = file_ctx,
    };

    set_current_node(&ctx, file_ctx->parse_tree);
    pass_dispatch(&ctx);
}

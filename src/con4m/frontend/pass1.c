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

static inline void
node_up(c4m_file_compile_ctx *ctx)
{
    ctx->cur = ctx->cur->parent;
}

static void
handle_enum_decl(pass_ctx *ctx)
{
}

static void
handle_func_decl(pass_ctx *ctx)
{
}

static void
handle_var_decl(pass_ctx *ctx)
{
}

static void
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
        handle_var_decl(ctx);
        break;
    default:
        process_children(ctx);
        break;
    }
}

// More consise aliases internally only.
#define tfind(x, y, ...)  _c4m_tpat_find(x, y, KFUNC(__VA_ARGS__))
#define tmatch(x, y, ...) _c4m_tpat_match(x, y, KFUNC(__VA_ARGS__))
#define topt(x, y, ...) \
    _c4m_tpat_opt_match(x, y, KFUNC(__VA_ARGS__))
#define tcount(a, b, c, d, ...) \
    _c4m_tpat_n_m_match(a, b, c, d, KFUNC(__VA_ARGS__))

// From a given node,
static c4m_tpat_node_t *first_pattern = NULL;

static void
setup_pass1_patterns()
{
    if (first_pattern != NULL) {
        return;
    }
}

void
c4m_pass_1(c4m_file_compile_ctx *file_ctx)
{
    setup_pass1_patterns();

    pass_ctx ctx = {
        .file_ctx = file_ctx,
    };

    set_current_node(&ctx, file_ctx->parse_tree);
    pass_dispatch(&ctx);
}

#pragma once
#include "con4m.h"

extern bool        c4m_parse(c4m_file_compile_ctx *);
extern bool        c4m_parse_type(c4m_file_compile_ctx *);
extern c4m_grid_t *c4m_format_parse_tree(c4m_file_compile_ctx *);
extern void        c4m_print_parse_node(c4m_tree_node_t *);
extern c4m_utf8_t *c4m_node_type_name(c4m_node_kind_t);

#ifdef C4M_USE_INTERNAL_API

static inline c4m_utf8_t *
identifier_text(c4m_token_t *tok)
{
    if (tok->literal_value == NULL) {
        tok->literal_value = c4m_token_raw_content(tok);
    }

    return (c4m_utf8_t *)tok->literal_value;
}

static inline c4m_utf8_t *
node_text(c4m_tree_node_t *n)
{
    c4m_pnode_t *p = c4m_tree_get_contents(n);
    return c4m_token_raw_content(p->token);
}

static inline int32_t
c4m_node_get_line_number(c4m_tree_node_t *n)
{
    if (n == NULL) {
        return 0;
    }

    c4m_pnode_t *p = c4m_tree_get_contents(n);

    return p->token->line_no;
}

static inline c4m_utf8_t *
c4m_node_get_loc_str(c4m_tree_node_t *n)
{
    c4m_pnode_t *p = c4m_tree_get_contents(n);

    return c4m_cstr_format("{}:{}:{}",
                           p->token->module->path,
                           c4m_box_i64(p->token->line_no),
                           c4m_box_i64(p->token->line_offset + 1));
}

static inline c4m_utf8_t *
node_list_join(c4m_xlist_t *nodes, c4m_str_t *joiner, bool trailing)
{
    int64_t      n      = c4m_xlist_len(nodes);
    c4m_xlist_t *strarr = c4m_new(c4m_tspec_xlist(c4m_tspec_utf8()));

    for (int64_t i = 0; i < n; i++) {
        c4m_tree_node_t *one = c4m_xlist_get(nodes, i, NULL);
        c4m_xlist_append(strarr, node_text(one));
    }

    return c4m_to_utf8(c4m_str_join(strarr,
                                    joiner,
                                    c4m_kw("add_trailing", c4m_ka(trailing))));
}

static inline int
node_num_kids(c4m_tree_node_t *t)
{
    return (int)t->num_kids;
}

static inline c4m_obj_t
node_simp_literal(c4m_tree_node_t *n)
{
    c4m_pnode_t *p   = c4m_tree_get_contents(n);
    c4m_token_t *tok = p->token;

    return tok->literal_value;
}

// More consise aliases internally only.
#define tfind(x, y, ...) _c4m_tpat_find((void *)(int64_t)x, \
                                        y,                  \
                                        KFUNC(__VA_ARGS__))
#define tfind_content(x, y) c4m_tpat_content_find((void *)(int64_t)x, y)
#define tmatch(x, y, ...)   _c4m_tpat_match((void *)(int64_t)x, \
                                          y,                    \
                                          KFUNC(__VA_ARGS__))
#define tcontent(x, y) c4m_tpat_content_match((void *)(int64_t)x, y)
#define toptional(x, y, ...) \
    _c4m_tpat_opt_match((void *)(int64_t)x, y, KFUNC(__VA_ARGS__))
#define tcount(a, b, c, d, ...) \
    _c4m_tpat_n_m_match((void *)(int64_t)a, b, c, d, KFUNC(__VA_ARGS__))
#define tcount_content(a, b, c, d, ...) \
    c4m_tpat_n_m_content_match((void *)(int64_t)a, b, c, d)

#define get_pnode(x) ((x) ? c4m_tree_get_contents(x) : NULL)

// We use the null value (error) in patterns to match any type node.
#define nt_any    (c4m_nt_error)
#define max_nodes 0x7fff

typedef struct pass1_ctx {
    c4m_tree_node_t      *cur_tnode;
    c4m_pnode_t          *cur;
    c4m_spec_t           *spec;
    c4m_file_compile_ctx *file_ctx;
    c4m_scope_t          *static_scope;
} pass1_ctx;

static inline c4m_tree_node_t *
get_match(pass1_ctx *ctx, c4m_tpat_node_t *pattern)
{
    return get_match_on_node(ctx->cur_tnode, pattern);
}

static inline c4m_xlist_t *
apply_pattern(pass1_ctx *ctx, c4m_tpat_node_t *pattern)
{
    return apply_pattern_on_node(ctx->cur_tnode, pattern);
}

static inline void
set_current_node(pass1_ctx *ctx, c4m_tree_node_t *n)
{
    ctx->cur_tnode = n;
    ctx->cur       = c4m_tree_get_contents(n);
}

static inline bool
node_down(pass1_ctx *ctx, int i)
{
    c4m_tree_node_t *n = ctx->cur_tnode;

    if (i >= n->num_kids) {
        return false;
    }

    assert(n->children[i]->parent == n);
    set_current_node(ctx, n->children[i]);

    return true;
}

static inline void
node_up(pass1_ctx *ctx)
{
    set_current_node(ctx, ctx->cur_tnode->parent);
}

static inline c4m_node_kind_t
cur_node_type(pass1_ctx *ctx)
{
    return ctx->cur->kind;
}

static inline c4m_tree_node_t *
cur_node(pass1_ctx *ctx)
{
    return ctx->cur_tnode;
}

#endif

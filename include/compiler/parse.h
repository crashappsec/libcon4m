#pragma once
#include "con4m.h"

extern bool        c4m_parse(c4m_file_compile_ctx *);
extern bool        c4m_parse_type(c4m_file_compile_ctx *);
extern c4m_grid_t *c4m_format_parse_tree(c4m_file_compile_ctx *);
extern void        c4m_print_parse_node(c4m_tree_node_t *);
extern c4m_utf8_t *c4m_node_type_name(c4m_node_kind_t);

#ifdef C4M_USE_INTERNAL_API

static inline c4m_utf8_t *
c4m_identifier_text(c4m_token_t *tok)
{
    if (tok->literal_value == NULL) {
        tok->literal_value = c4m_token_raw_content(tok);
    }

    return (c4m_utf8_t *)tok->literal_value;
}

static inline c4m_utf8_t *
c4m_node_text(c4m_tree_node_t *n)
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
c4m_node_list_join(c4m_list_t *nodes, c4m_str_t *joiner, bool trailing)
{
    int64_t      n      = c4m_list_len(nodes);
    c4m_list_t *strarr = c4m_new(c4m_type_list(c4m_type_utf8()));

    for (int64_t i = 0; i < n; i++) {
        c4m_tree_node_t *one = c4m_list_get(nodes, i, NULL);
        c4m_list_append(strarr, c4m_node_text(one));
    }

    return c4m_to_utf8(c4m_str_join(strarr,
                                    joiner,
                                    c4m_kw("add_trailing", c4m_ka(trailing))));
}

static inline int
c4m_node_num_kids(c4m_tree_node_t *t)
{
    return (int)t->num_kids;
}

static inline c4m_obj_t
c4m_node_simp_literal(c4m_tree_node_t *n)
{
    c4m_pnode_t *p   = c4m_tree_get_contents(n);
    c4m_token_t *tok = p->token;

    return tok->literal_value;
}


typedef struct c4m_pass1_ctx {
    c4m_tree_node_t      *cur_tnode;
    c4m_pnode_t          *cur;
    c4m_spec_t           *spec;
    c4m_file_compile_ctx *file_ctx;
    c4m_scope_t          *static_scope;
    bool                  in_func;
    c4m_list_t          *extern_decls;
} c4m_pass1_ctx;

static inline c4m_tree_node_t *
c4m_get_match(c4m_pass1_ctx *ctx, c4m_tpat_node_t *pattern)
{
    return c4m_get_match_on_node(ctx->cur_tnode, pattern);
}

static inline c4m_list_t *
c4m_apply_pattern(c4m_pass1_ctx *ctx, c4m_tpat_node_t *pattern)
{
    return c4m_apply_pattern_on_node(ctx->cur_tnode, pattern);
}

static inline void
c4m_set_current_node(c4m_pass1_ctx *ctx, c4m_tree_node_t *n)
{
    ctx->cur_tnode = n;
    ctx->cur       = c4m_tree_get_contents(n);
}

static inline bool
c4m_node_down(c4m_pass1_ctx *ctx, int i)
{
    c4m_tree_node_t *n = ctx->cur_tnode;

    if (i >= n->num_kids) {
        return false;
    }

    assert(n->children[i]->parent == n);
    c4m_set_current_node(ctx, n->children[i]);

    return true;
}

static inline void
c4m_node_up(c4m_pass1_ctx *ctx)
{
    c4m_set_current_node(ctx, ctx->cur_tnode->parent);
}

static inline c4m_node_kind_t
c4m_cur_node_type(c4m_pass1_ctx *ctx)
{
    return ctx->cur->kind;
}

static inline c4m_tree_node_t *
c4m_cur_node(c4m_pass1_ctx *ctx)
{
    return ctx->cur_tnode;
}

#endif

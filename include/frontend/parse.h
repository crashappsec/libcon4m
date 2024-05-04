#pragma once
#include "con4m.h"

extern bool        c4m_parse(c4m_file_compile_ctx *);
extern c4m_grid_t *c4m_format_parse_tree(c4m_file_compile_ctx *);
extern void        c4m_print_parse_node(c4m_tree_node_t *);
extern void        c4m_pass_1(c4m_file_compile_ctx *);

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
node_literal(c4m_tree_node_t *n)
{
    c4m_pnode_t *p   = c4m_tree_get_contents(n);
    c4m_token_t *tok = p->token;

    return tok->literal_value;
}

#endif

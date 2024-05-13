#pragma once
#include "con4m.h"

#ifdef C4M_USE_INTERNAL_API

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

#undef DEBUG_PATTERNS
#ifdef DEBUG_PATTERNS
static inline void
print_type(c4m_obj_t o)
{
    if (o == NULL) {
        printf("<null>\n");
        return;
    }
    c4m_print(c4m_get_my_type(o));
}

static inline c4m_utf8_t *
content_formatter(void *contents)
{
    c4m_node_kind_t kind = (c4m_node_kind_t)(uint64_t)contents;

    if (kind == nt_any) {
        return c4m_rich_lit("[h2]nt_any[/]");
    }

    return c4m_cstr_format("[h2]{}[/]", c4m_node_type_name(kind));
}

static inline void
show_pattern(c4m_tpat_node_t *pat)
{
    c4m_tree_node_t *t = c4m_pat_repr(pat, content_formatter);
    c4m_grid_t      *g = c4m_grid_tree(t);

    c4m_print(g);
}

#endif

extern bool        tcmp(int64_t, c4m_tree_node_t *);
extern void        setup_treematch_patterns();
extern c4m_type_t *c4m_node_to_type(c4m_file_compile_ctx *,
                                    c4m_tree_node_t *,
                                    c4m_dict_t *);
extern c4m_obj_t   node_literal(c4m_file_compile_ctx *,
                                c4m_tree_node_t *,
                                c4m_dict_t *);
extern c4m_obj_t
node_to_callback(c4m_file_compile_ctx *ctx, c4m_tree_node_t *n);

static inline bool
node_has_type(c4m_tree_node_t *node, c4m_node_kind_t expect)
{
    c4m_pnode_t *pnode = get_pnode(node);
    return expect == pnode->kind;
}

static inline c4m_utf8_t *
get_litmod(c4m_pnode_t *p)
{
    if (!p->extra_info) {
        return NULL;
    }

    return c4m_to_utf8(p->extra_info);
}

// Return the first capture if there's a match, and NULL if not.
extern c4m_tree_node_t *get_match_on_node(c4m_tree_node_t *, c4m_tpat_node_t *);

// Return every capture on match.
extern c4m_xlist_t *apply_pattern_on_node(c4m_tree_node_t *, c4m_tpat_node_t *);

extern c4m_tpat_node_t *c4m_first_kid_id;
extern c4m_tpat_node_t *c4m_enum_items;
extern c4m_tpat_node_t *c4m_member_prefix;
extern c4m_tpat_node_t *c4m_member_last;
extern c4m_tpat_node_t *c4m_use_uri;
extern c4m_tpat_node_t *c4m_extern_params;
extern c4m_tpat_node_t *c4m_extern_return;
extern c4m_tpat_node_t *c4m_return_extract;
extern c4m_tpat_node_t *c4m_find_pure;
extern c4m_tpat_node_t *c4m_find_holds;
extern c4m_tpat_node_t *c4m_find_allocs;
extern c4m_tpat_node_t *c4m_find_extern_local;
extern c4m_tpat_node_t *c4m_param_extraction;
extern c4m_tpat_node_t *c4m_qualifier_extract;
extern c4m_tpat_node_t *c4m_sym_decls;
extern c4m_tpat_node_t *c4m_sym_names;
extern c4m_tpat_node_t *c4m_sym_type;
extern c4m_tpat_node_t *c4m_sym_init;

#endif

#pragma once
#include "con4m.h"

#ifdef C4M_USE_INTERNAL_API

#ifdef C4M_DEBUG_PATTERNS

static inline void
c4m_print_type(c4m_obj_t o)
{
    if (o == NULL) {
        printf("<null>\n");
        return;
    }
    c4m_print(c4m_get_my_type(o));
}

extern c4m_utf8_t *c4m_node_type_name(c4m_node_kind_t);

static inline c4m_utf8_t *
c4m_content_formatter(void *contents)
{
    c4m_node_kind_t kind = (c4m_node_kind_t)(uint64_t)contents;

    if (kind == c4m_nt_any) {
        return c4m_rich_lit("[h2]c4m_nt_any[/]");
    }

    return c4m_cstr_format("[h2]{}[/]", c4m_node_type_name(kind));
}

static inline void
_show_pattern(c4m_tpat_node_t *pat)
{
    c4m_tree_node_t *t = c4m_pat_repr(pat, c4m_content_formatter);
    c4m_grid_t      *g = c4m_grid_tree(t);

    c4m_print(g);
}

#define show_pattern(x)                                              \
    printf("Showing pattern: %s (%s:%d)\n", #x, __FILE__, __LINE__); \
    _show_pattern(x)

#endif

#define c4m_get_pnode(x) ((x) ? c4m_tree_get_contents(x) : NULL)

extern bool        c4m_tcmp(int64_t, c4m_tree_node_t *);
extern void        c4m_setup_treematch_patterns();
extern c4m_type_t *c4m_node_to_type(c4m_file_compile_ctx *,
                                    c4m_tree_node_t *,
                                    c4m_dict_t *);
extern c4m_obj_t
c4m_node_to_callback(c4m_file_compile_ctx *ctx, c4m_tree_node_t *n);

static inline bool
c4m_node_has_type(c4m_tree_node_t *node, c4m_node_kind_t expect)
{
    c4m_pnode_t *pnode = (c4m_pnode_t *)c4m_get_pnode(node);
    return expect == pnode->kind;
}

static inline c4m_utf8_t *
c4m_get_litmod(c4m_pnode_t *p)
{
    if (!p->extra_info) {
        return NULL;
    }

    return c4m_to_utf8(p->extra_info);
}

// Return the first capture if there's a match, and NULL if not.
extern c4m_tree_node_t *c4m_get_match_on_node(c4m_tree_node_t *,
                                              c4m_tpat_node_t *);

// Return every capture on match.
extern c4m_list_t *c4m_apply_pattern_on_node(c4m_tree_node_t *,
                                             c4m_tpat_node_t *);

extern c4m_tpat_node_t *c4m_first_kid_id;
extern c4m_tpat_node_t *c4m_2nd_kid_id;
extern c4m_tpat_node_t *c4m_enum_items;
extern c4m_tpat_node_t *c4m_member_prefix;
extern c4m_tpat_node_t *c4m_member_last;
extern c4m_tpat_node_t *c4m_func_mods;
extern c4m_tpat_node_t *c4m_use_uri;
extern c4m_tpat_node_t *c4m_extern_params;
extern c4m_tpat_node_t *c4m_extern_return;
extern c4m_tpat_node_t *c4m_return_extract;
extern c4m_tpat_node_t *c4m_find_pure;
extern c4m_tpat_node_t *c4m_find_holds;
extern c4m_tpat_node_t *c4m_find_allocs;
extern c4m_tpat_node_t *c4m_find_extern_local;
extern c4m_tpat_node_t *c4m_find_extern_box;
extern c4m_tpat_node_t *c4m_param_extraction;
extern c4m_tpat_node_t *c4m_qualifier_extract;
extern c4m_tpat_node_t *c4m_sym_decls;
extern c4m_tpat_node_t *c4m_sym_names;
extern c4m_tpat_node_t *c4m_sym_type;
extern c4m_tpat_node_t *c4m_sym_init;
extern c4m_tpat_node_t *c4m_loop_vars;
extern c4m_tpat_node_t *c4m_case_branches;
extern c4m_tpat_node_t *c4m_case_else;
extern c4m_tpat_node_t *c4m_elif_branches;
extern c4m_tpat_node_t *c4m_else_condition;
extern c4m_tpat_node_t *c4m_case_cond;
extern c4m_tpat_node_t *c4m_case_cond_typeof;
extern c4m_tpat_node_t *c4m_opt_label;
extern c4m_tpat_node_t *c4m_id_node;
extern c4m_tpat_node_t *c4m_tuple_assign;
#endif

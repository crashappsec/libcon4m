#define C4M_USE_INTERNAL_API
#include "con4m.h"

bool
tcmp(int64_t kind_as_64, c4m_tree_node_t *node)
{
    c4m_node_kind_t kind  = (c4m_node_kind_t)(unsigned int)kind_as_64;
    c4m_pnode_t    *pnode = get_pnode(node);

    if (kind == nt_any) {
        return true;
    }

    bool result = kind == pnode->kind;

    return result;
}

c4m_tpat_node_t *c4m_first_kid_id = NULL;
c4m_tpat_node_t *c4m_2nd_kid_id;
c4m_tpat_node_t *c4m_enum_items;
c4m_tpat_node_t *c4m_member_prefix;
c4m_tpat_node_t *c4m_member_last;
c4m_tpat_node_t *c4m_func_mods;
c4m_tpat_node_t *c4m_use_uri;
c4m_tpat_node_t *c4m_extern_params;
c4m_tpat_node_t *c4m_extern_return;
c4m_tpat_node_t *c4m_return_extract;
c4m_tpat_node_t *c4m_find_pure;
c4m_tpat_node_t *c4m_find_holds;
c4m_tpat_node_t *c4m_find_allocs;
c4m_tpat_node_t *c4m_find_extern_local;
c4m_tpat_node_t *c4m_param_extraction;
c4m_tpat_node_t *c4m_qualifier_extract;
c4m_tpat_node_t *c4m_sym_decls;
c4m_tpat_node_t *c4m_sym_names;
c4m_tpat_node_t *c4m_sym_type;
c4m_tpat_node_t *c4m_sym_init;
c4m_tpat_node_t *c4m_loop_vars;
c4m_tpat_node_t *c4m_case_branches;
c4m_tpat_node_t *c4m_case_else;
c4m_tpat_node_t *c4m_elif_branches;
c4m_tpat_node_t *c4m_else_condition;
c4m_tpat_node_t *c4m_case_cond;
c4m_tpat_node_t *c4m_case_cond_typeof;
c4m_tpat_node_t *c4m_opt_label;
c4m_tpat_node_t *c4m_id_node;

void
setup_treematch_patterns()
{
    if (c4m_first_kid_id != NULL) {
        return;
    }
    // Returns first child if it's an identifier, null otherwise.
    c4m_first_kid_id  = tmatch(nt_any,
                              0,
                              tmatch(c4m_nt_identifier, 1),
                              tcount_content(nt_any, 0, max_nodes, 0));
    c4m_2nd_kid_id    = tmatch(nt_any,
                            0,
                            tcontent(nt_any, 0),
                            tmatch(c4m_nt_identifier, 1),
                            tcount_content(nt_any, 0, max_nodes, 0));
    // Skips the identifier if there, and returns all the enum items,
    // regardless of the subtree shape.
    c4m_enum_items    = tmatch(nt_any,
                            0,
                            toptional(c4m_nt_identifier, 0),
                            tcount_content(c4m_nt_enum_item,
                                           0,
                                           max_nodes,
                                           1));
    c4m_member_last   = tfind(c4m_nt_member,
                            0,
                            tcount(c4m_nt_identifier, 0, max_nodes, 0),
                            tmatch(c4m_nt_identifier, 1));
    c4m_member_prefix = tfind(c4m_nt_member,
                              0,
                              tcount(c4m_nt_identifier, 0, max_nodes, 1),
                              tmatch(c4m_nt_identifier, 0));
    c4m_func_mods     = tfind(c4m_nt_func_mods,
                          0,
                          tcount(c4m_nt_func_mod, 0, max_nodes, 1));
    c4m_extern_params = tfind(c4m_nt_extern_sig,
                              0,
                              tcount_content(c4m_nt_extern_param, 0, max_nodes, 1),
                              tcount_content(c4m_nt_lit_tspec_return_type,
                                             0,
                                             1,
                                             0));
    c4m_extern_return = tfind(
        c4m_nt_extern_sig,
        0,
        tcount_content(c4m_nt_extern_param, 0, max_nodes, 0),
        tcount_content(c4m_nt_lit_tspec_return_type,
                       0,
                       1,
                       1));
    c4m_return_extract   = tfind(c4m_nt_lit_tspec_return_type,
                               0,
                               tmatch(nt_any, 1));
    c4m_use_uri          = tfind(c4m_nt_simple_lit, 1);
    c4m_param_extraction = tfind(
        c4m_nt_formals,
        0,
        tcount_content(c4m_nt_sym_decl, 0, max_nodes, 1));

    c4m_find_pure         = tfind_content(c4m_nt_extern_pure, 1);
    c4m_find_holds        = tfind_content(c4m_nt_extern_holds, 1);
    c4m_find_allocs       = tfind_content(c4m_nt_extern_allocs, 1);
    c4m_find_extern_local = tfind_content(c4m_nt_extern_local, 1);
    c4m_qualifier_extract = tfind(c4m_nt_decl_qualifiers,
                                  0,
                                  tcount(c4m_nt_identifier, 0, max_nodes, 1));
    c4m_sym_decls         = tmatch(c4m_nt_variable_decls,
                           0,
                           tcount_content(c4m_nt_decl_qualifiers, 1, 1, 0),
                           tcount_content(c4m_nt_sym_decl, 1, max_nodes, 1));
    c4m_sym_names         = tfind(c4m_nt_sym_decl,
                          0,
                          tcount_content(c4m_nt_identifier, 1, max_nodes, 1),
                          tcount_content(c4m_nt_lit_tspec, 0, 1, 0),
                          tcount_content(c4m_nt_assign, 0, 1, 0));
    c4m_sym_type          = tfind(c4m_nt_sym_decl,
                         0,
                         tcount_content(c4m_nt_identifier, 1, max_nodes, 0),
                         tcount_content(c4m_nt_lit_tspec, 0, 1, 1),
                         tcount_content(c4m_nt_assign, 0, 1, 0));
    c4m_sym_init          = tfind(c4m_nt_sym_decl,
                         0,
                         tcount_content(c4m_nt_identifier, 1, max_nodes, 0),
                         tcount_content(c4m_nt_lit_tspec, 0, 1, 0),
                         tcount_content(c4m_nt_assign, 0, 1, 1));
    c4m_loop_vars         = tfind(c4m_nt_variable_decls,
                          0,
                          tcount_content(c4m_nt_identifier, 1, 2, 1));
    c4m_case_branches     = tmatch(nt_any,
                               0,
                               tcount_content(nt_any, 0, 2, 0),
                               tcount_content(c4m_nt_case, 1, max_nodes, 1),
                               tcount_content(c4m_nt_else, 0, 1, 0));
    c4m_case_else         = tmatch(nt_any,
                           0,
                           tcount_content(nt_any, 0, 2, 0),
                           tcontent(nt_any, 0),
                           tcount_content(c4m_nt_case, 1, max_nodes, 0),
                           tcount_content(c4m_nt_else, 0, 1, 1));
    c4m_elif_branches     = tmatch(nt_any,
                               0,
                               tcontent(c4m_nt_cmp, 0),
                               tcontent(c4m_nt_body, 0),
                               tcount_content(c4m_nt_elif, 0, max_nodes, 1),
                               tcount_content(c4m_nt_else, 0, 1, 0));
    c4m_else_condition    = tfind_content(c4m_nt_else, 1);
    c4m_case_cond         = tmatch(nt_any,
                           0,
                           toptional(c4m_nt_label, 0),
                           tcontent(c4m_nt_expression, 1),
                           tcount_content(c4m_nt_case, 1, max_nodes, 0),
                           tcount_content(c4m_nt_else, 0, 1, 0));
    c4m_case_cond_typeof  = tmatch(nt_any,
                                  0,
                                  toptional(c4m_nt_label, 0),
                                  tcontent(c4m_nt_member, 1),
                                  tcount_content(c4m_nt_case, 1, max_nodes, 0),
                                  tcount_content(c4m_nt_else, 0, 1, 0));
    c4m_opt_label         = tfind(c4m_nt_label, 1);
    c4m_id_node           = tfind(c4m_nt_identifier, 1);

    c4m_gc_register_root(&c4m_first_kid_id, 1);
    c4m_gc_register_root(&c4m_2nd_kid_id, 1);
    c4m_gc_register_root(&c4m_enum_items, 1);
    c4m_gc_register_root(&c4m_member_prefix, 1);
    c4m_gc_register_root(&c4m_member_last, 1);
    c4m_gc_register_root(&c4m_func_mods, 1);
    c4m_gc_register_root(&c4m_use_uri, 1);
    c4m_gc_register_root(&c4m_extern_params, 1);
    c4m_gc_register_root(&c4m_extern_return, 1);
    c4m_gc_register_root(&c4m_return_extract, 1);
    c4m_gc_register_root(&c4m_find_pure, 1);
    c4m_gc_register_root(&c4m_find_holds, 1);
    c4m_gc_register_root(&c4m_find_allocs, 1);
    c4m_gc_register_root(&c4m_find_extern_local, 1);
    c4m_gc_register_root(&c4m_param_extraction, 1);
    c4m_gc_register_root(&c4m_qualifier_extract, 1);
    c4m_gc_register_root(&c4m_sym_decls, 1);
    c4m_gc_register_root(&c4m_sym_names, 1);
    c4m_gc_register_root(&c4m_sym_type, 1);
    c4m_gc_register_root(&c4m_sym_init, 1);
    c4m_gc_register_root(&c4m_loop_vars, 1);
    c4m_gc_register_root(&c4m_case_branches, 1);
    c4m_gc_register_root(&c4m_case_else, 1);
    c4m_gc_register_root(&c4m_elif_branches, 1);
    c4m_gc_register_root(&c4m_else_condition, 1);
    c4m_gc_register_root(&c4m_case_cond, 1);
    c4m_gc_register_root(&c4m_case_cond_typeof, 1);
    c4m_gc_register_root(&c4m_opt_label, 1);
    c4m_gc_register_root(&c4m_id_node, 1);
}

c4m_obj_t
node_to_callback(c4m_file_compile_ctx *ctx, c4m_tree_node_t *n)
{
    if (!node_has_type(n, c4m_nt_lit_callback)) {
        return NULL;
    }

    c4m_utf8_t *name = node_text(c4m_tree_get_child(n, 0));
    c4m_type_t *type = c4m_node_to_type(ctx, c4m_tree_get_child(n, 1), NULL);

    return c4m_new(c4m_tspec_callback(),
                   c4m_kw("symbol_name",
                          c4m_ka(name),
                          "type",
                          c4m_ka(type)));
}

#define ERROR_ON_BAD_LITMOD(ctx, base_type, node, litmod, st) \
    if (base_type == C4M_T_ERROR) {                           \
        c4m_add_error(ctx,                                    \
                      c4m_err_parse_no_lit_mod_match,         \
                      node,                                   \
                      litmod,                                 \
                      c4m_new_utf8(st));                      \
        return NULL;                                          \
    }

c4m_obj_t
node_literal(c4m_file_compile_ctx *ctx,
             c4m_tree_node_t      *node,
             c4m_dict_t           *type_ctx)
{
    c4m_pnode_t       *pnode = get_pnode(node);
    c4m_obj_t          one;
    c4m_partial_lit_t *partial;
    c4m_utf8_t        *litmod;
    int                n;
    c4m_builtin_t      base_type;

    switch (pnode->kind) {
    case c4m_nt_expression:
        return node_literal(ctx, c4m_tree_get_child(node, 0), type_ctx);

    case c4m_nt_simple_lit:
        return node_simp_literal(node);

    case c4m_nt_lit_list:
        // TODO: error if the litmod type is bad.
        litmod = get_litmod(pnode);

        if (litmod == NULL) {
            base_type = C4M_T_LIST;
        }
        else {
            base_type = base_type_from_litmod(ST_List, litmod);
        }

        ERROR_ON_BAD_LITMOD(ctx, base_type, node, litmod, "list");

        n       = c4m_tree_get_number_children(node);
        partial = c4m_new(c4m_tspec_partial_lit(), n, base_type, node);

        c4m_xlist_append(partial->type->details->items, c4m_tspec_typevar());

        for (int i = 0; i < n; i++) {
            one               = node_literal(ctx,
                               c4m_tree_get_child(node, i),
                               type_ctx);
            partial->items[i] = one;
        }

        return partial;

    case c4m_nt_lit_dict:
        litmod = get_litmod(pnode);
        if (litmod == NULL) {
            base_type = C4M_T_DICT;
        }
        else {
handle_dict_or_litmodded_but_empty:
            base_type = base_type_from_litmod(ST_Dict, litmod);
        }

        ERROR_ON_BAD_LITMOD(ctx, base_type, node, litmod, "dict");

        n       = c4m_tree_get_number_children(node);
        partial = c4m_new(c4m_tspec_partial_lit(), n, base_type, node);

        c4m_xlist_append(partial->type->details->items, c4m_tspec_typevar());
        c4m_xlist_append(partial->type->details->items, c4m_tspec_typevar());

        for (int i = 0; i < n; i++) {
            one               = node_literal(ctx,
                               c4m_tree_get_child(node, i),
                               type_ctx);
            partial->items[i] = one;
        }
        return partial;

    case c4m_nt_lit_set:
        litmod = get_litmod(pnode);

        if (litmod == NULL) {
            base_type = C4M_T_SET;
        }
        else {
            base_type = base_type_from_litmod(ST_Dict, litmod);
        }

        ERROR_ON_BAD_LITMOD(ctx, base_type, node, litmod, "set");

        n       = c4m_tree_get_number_children(node);
        partial = c4m_new(c4m_tspec_partial_lit(), n, base_type, node);

        c4m_xlist_append(partial->type->details->items, c4m_tspec_typevar());

        for (int i = 0; i < n; n++) {
            one               = node_literal(ctx,
                               c4m_tree_get_child(node, i),
                               type_ctx);
            partial->items[i] = one;
        }
        return partial;

    case c4m_nt_lit_tuple:
        litmod = get_litmod(pnode);

        if (litmod == NULL) {
            base_type = C4M_T_TUPLE;
        }
        else {
            base_type = base_type_from_litmod(ST_Tuple, litmod);
        }

        ERROR_ON_BAD_LITMOD(ctx, base_type, node, litmod, "tuple");

        n       = c4m_tree_get_number_children(node);
        partial = c4m_new(c4m_tspec_partial_lit(), n, base_type);

        for (int i = 0; i < n; n++) {
            c4m_xlist_append(partial->type->details->items,
                             c4m_tspec_typevar());
            one               = node_literal(ctx,
                               c4m_tree_get_child(node, i),
                               type_ctx);
            partial->items[i] = one;
        }
        return partial;

    case c4m_nt_lit_unquoted:
        C4M_CRAISE("Currently unsupported.");

    case c4m_nt_lit_empty_dict_or_set:
        litmod = get_litmod(pnode);
        if (litmod && c4m_str_codepoint_len(litmod) != 0) {
            goto handle_dict_or_litmodded_but_empty;
        }
        return c4m_new(c4m_tspec_partial_lit(), 0, C4M_T_VOID);

    case c4m_nt_lit_callback:
        return node_to_callback(ctx, node);

    case c4m_nt_lit_tspec:

        if (type_ctx == NULL) {
            type_ctx = c4m_new(c4m_tspec_dict(c4m_tspec_utf8(),
                                              c4m_tspec_ref()));
        }
        return c4m_node_to_type(ctx, node, type_ctx);

    default:
        // Return the parse node!
        return node;
    }
}

c4m_type_t *
c4m_node_to_type(c4m_file_compile_ctx *ctx,
                 c4m_tree_node_t      *n,
                 c4m_dict_t           *type_ctx)
{
    c4m_pnode_t *pnode = get_pnode(n);
    c4m_utf8_t  *varname;
    c4m_type_t  *t;
    bool         found;
    int          numkids;

    switch (pnode->kind) {
    case c4m_nt_lit_tspec:
        return c4m_node_to_type(ctx, c4m_tree_get_child(n, 0), type_ctx);
    case c4m_nt_lit_tspec_tvar:
        varname = node_text(c4m_tree_get_child(n, 0));
        t       = hatrack_dict_get(type_ctx, varname, &found);
        if (!found) {
            t                = c4m_tspec_typevar();
            t->details->name = varname->data;
            hatrack_dict_put(type_ctx, varname, t);
        }
        return t;
    case c4m_nt_lit_tspec_named_type:
        varname = node_text(n);
        for (int i = 0; i < C4M_NUM_BUILTIN_DTS; i++) {
            if (!strcmp(varname->data, c4m_base_type_info[i].name)) {
                return c4m_bi_types[i];
            }
        }

        c4m_add_error(ctx, c4m_err_unk_primitive_type, n);
        return c4m_tspec_typevar();

    case c4m_nt_lit_tspec_parameterized_type:
        varname = node_text(n);
        // Need to do this more generically, but OK for now.
        if (!strcmp(varname->data, "list")) {
            return c4m_tspec_list(c4m_node_to_type(ctx,
                                                   c4m_tree_get_child(n, 0),
                                                   type_ctx));
        }
        if (!strcmp(varname->data, "queue")) {
            return c4m_tspec_queue(c4m_node_to_type(ctx,
                                                    c4m_tree_get_child(n, 0),
                                                    type_ctx));
        }
        if (!strcmp(varname->data, "ring")) {
            return c4m_tspec_queue(c4m_node_to_type(ctx,
                                                    c4m_tree_get_child(n, 0),
                                                    type_ctx));
        }
        if (!strcmp(varname->data, "logring")) {
            c4m_add_error(ctx, c4m_err_no_logring_yet, n);
            return c4m_tspec_typevar();
        }
        if (!strcmp(varname->data, "xlist")) {
            return c4m_tspec_xlist(c4m_node_to_type(ctx,
                                                    c4m_tree_get_child(n, 0),
                                                    type_ctx));
        }
        if (!strcmp(varname->data, "tree")) {
            return c4m_tspec_tree(c4m_node_to_type(ctx,
                                                   c4m_tree_get_child(n, 0),
                                                   type_ctx));
        }
        if (!strcmp(varname->data, "stack")) {
            return c4m_tspec_stack(c4m_node_to_type(ctx,
                                                    c4m_tree_get_child(n, 0),
                                                    type_ctx));
        }
        if (!strcmp(varname->data, "set")) {
            return c4m_tspec_set(c4m_node_to_type(ctx,
                                                  c4m_tree_get_child(n, 0),
                                                  type_ctx));
        }
        if (!strcmp(varname->data, "dict")) {
            return c4m_tspec_dict(c4m_node_to_type(ctx,
                                                   c4m_tree_get_child(n, 0),
                                                   type_ctx),
                                  c4m_node_to_type(ctx,
                                                   c4m_tree_get_child(n, 1),
                                                   type_ctx));
        }
        if (!strcmp(varname->data, "tuple")) {
            c4m_xlist_t *subitems;

            subitems = c4m_new(c4m_tspec_xlist(c4m_tspec_typespec()));

            for (int i = 0; i < c4m_tree_get_number_children(n); i++) {
                c4m_xlist_append(subitems,
                                 c4m_node_to_type(ctx,
                                                  c4m_tree_get_child(n, i),
                                                  type_ctx));
            }

            return c4m_tspec_tuple_from_xlist(subitems);
        }
        c4m_add_error(ctx, c4m_err_unk_param_type, n);
        return c4m_tspec_typevar();
    case c4m_nt_lit_tspec_func:
        numkids = c4m_tree_get_number_children(n);
        if (numkids == 0) {
            return c4m_tspec_varargs_fn(c4m_tspec_typevar(), 0);
        }

        c4m_xlist_t     *args = c4m_new(c4m_tspec_xlist(c4m_tspec_typespec()));
        c4m_tree_node_t *kid  = c4m_tree_get_child(n, numkids - 1);
        bool             va   = false;

        pnode = get_pnode(kid);

        if (pnode->kind == c4m_nt_lit_tspec_return_type) {
            t = c4m_node_to_type(ctx, c4m_tree_get_child(kid, 0), type_ctx);
            numkids--;
        }
        else {
            t = c4m_tspec_typevar();
        }

        for (int i = 0; i < numkids; i++) {
            kid = c4m_tree_get_child(n, i);

            if (i + 1 == numkids) {
                pnode = get_pnode(kid);

                if (pnode->kind == c4m_nt_lit_tspec_varargs) {
                    va  = true;
                    kid = c4m_tree_get_child(kid, 0);
                }
            }

            c4m_xlist_append(args, c4m_node_to_type(ctx, kid, type_ctx));
        }

        return c4m_tspec_fn(t, args, va);

    default:
        c4m_unreachable();
    }
}

c4m_xlist_t *
apply_pattern_on_node(c4m_tree_node_t *node, c4m_tpat_node_t *pattern)
{
    c4m_xlist_t *cap = NULL;
    bool         ok  = c4m_tree_match(node,
                             pattern,
                             (c4m_cmp_fn)tcmp,
                             &cap);

    if (!ok) {
        return NULL;
    }

    return cap;
}

// Return the first capture if there's a match, and NULL if not.
c4m_tree_node_t *
get_match_on_node(c4m_tree_node_t *node, c4m_tpat_node_t *pattern)
{
    c4m_xlist_t *cap = apply_pattern_on_node(node, pattern);

    if (cap != NULL) {
        return c4m_xlist_get(cap, 0, NULL);
    }

    return NULL;
}

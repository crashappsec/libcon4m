#define C4M_USE_INTERNAL_API
#include "con4m.h"

// First pass of the raw parse tree.

// Not everything we need would be resolvable in one pass, especially
// symbols in other modules.
//
// Since declarations can be in any statement, we do go ahead and
// finish walking down to the expression level, and do initial work on
// literal extraction too.

static void pass_dispatch(pass1_ctx *ctx);

static inline void
process_children(pass1_ctx *ctx)
{
    c4m_tree_node_t *n = ctx->cur_tnode;

    for (int i = 0; i < n->num_kids; i++) {
        node_down(ctx, i);
        pass_dispatch(ctx);
        node_up(ctx);
    }

    if (n->num_kids == 1) {
        c4m_pnode_t *pparent = get_pnode(n);
        c4m_pnode_t *pkid    = get_pnode(n->children[0]);
        pparent->value       = pkid->value;
    }
}

static inline c4m_scope_entry_t *
declare_sym(pass1_ctx       *ctx,
            c4m_scope_t     *scope,
            c4m_utf8_t      *name,
            c4m_tree_node_t *node,
            c4m_symbol_kind  kind,
            bool            *success,
            bool             err_if_present)
{
    c4m_scope_entry_t *result = c4m_declare_symbol(ctx->file_ctx,
                                                   scope,
                                                   name,
                                                   node,
                                                   kind,
                                                   success,
                                                   err_if_present);

    c4m_shadow_check(ctx->file_ctx, result, scope);

    result->flags |= C4M_F_IS_DECLARED;

    return result;
}

static inline void
handle_literal(pass1_ctx *ctx)
{
    c4m_tree_node_t *tnode = cur_node(ctx);
    c4m_pnode_t     *pnode = get_pnode(tnode);

    pnode->value = node_literal(ctx->file_ctx, tnode, NULL);
}

static void
validate_str_enum_vals(pass1_ctx *ctx, c4m_xlist_t *items)
{
    c4m_set_t *set = c4m_new(c4m_tspec_set(c4m_tspec_utf8()));
    int64_t    n   = c4m_xlist_len(items);

    for (int i = 0; i < n; i++) {
        c4m_tree_node_t *tnode = c4m_xlist_get(items, i, NULL);
        c4m_pnode_t     *pnode = get_pnode(tnode);

        if (c4m_tree_get_number_children(tnode) == 0) {
            pnode->value = (void *)c4m_new_utf8("error");
            continue;
        }

        c4m_scope_entry_t *sym = pnode->extra_info;
        c4m_utf8_t        *val = (c4m_utf8_t *)pnode->value;
        sym->value             = val;

        if (!c4m_set_add(set, val)) {
            c4m_add_error(ctx->file_ctx, c4m_err_dupe_enum, tnode);
            return;
        }
    }
}

static c4m_type_t *
validate_int_enum_vals(pass1_ctx *ctx, c4m_xlist_t *items)
{
    c4m_set_t  *set           = c4m_new(c4m_tspec_set(c4m_tspec_u64()));
    int64_t     n             = c4m_xlist_len(items);
    int         bits          = 0;
    bool        neg           = false;
    uint64_t    next_implicit = 0;
    c4m_type_t *result;

    for (int i = 0; i < n; i++) {
        c4m_tree_node_t *tnode = c4m_xlist_get(items, i, NULL);
        if (c4m_tree_get_number_children(tnode) == 0) {
            continue;
        }

        c4m_pnode_t   *pnode  = get_pnode(tnode);
        c4m_obj_t     *ref    = pnode->value;
        c4m_type_t    *ty     = c4m_get_my_type(ref);
        c4m_dt_info_t *dtinfo = c4m_tspec_get_data_type_info(ty);
        int            sz;
        uint64_t       val;

        switch (dtinfo->alloc_len) {
        case 1:
            val = (uint64_t) * (uint8_t *)ref;
            break;
        case 2:
            val = (uint64_t) * (uint16_t *)ref;
            break;
        case 4:
            val = (uint64_t) * (uint32_t *)ref;
            break;
        case 8:
            val = *(uint64_t *)ref;
            break;
        default:
            C4M_CRAISE("Invalid int size for enum item");
        }

        sz = 64 - __builtin_clzll(val);

        switch (sz) {
        case 64:
            if (dtinfo->typeid == C4M_T_INT) {
                neg = true;
            }
            break;

        case 32:
            if (dtinfo->typeid == C4M_T_I32) {
                neg = true;
            }
            break;
        case 8:
            if (dtinfo->typeid == C4M_T_I8) {
                neg = true;
            }
            break;

        default:
            break;
        }

        if (sz > bits) {
            bits = sz;
        }

        if (!c4m_set_add(set, (void *)val)) {
            c4m_add_error(ctx->file_ctx, c4m_err_dupe_enum, tnode);
        }
    }

    if (bits > 32) {
        bits = 64;
    }
    else {
        if (bits <= 8) {
            bits = 8;
        }
        else {
            bits = 32;
        }
    }

    switch (bits) {
    case 8:
        result = neg ? c4m_tspec_i8() : c4m_tspec_u8();
        break;
    case 32:
        result = neg ? c4m_tspec_i32() : c4m_tspec_u32();
        break;
    default:
        result = neg ? c4m_tspec_i64() : c4m_tspec_u64();
        break;
    }

    for (int i = 0; i < n; i++) {
        c4m_tree_node_t *tnode = c4m_xlist_get(items, i, NULL);
        c4m_pnode_t     *pnode = get_pnode(tnode);

        if (c4m_tree_get_number_children(tnode) != 0) {
            // pnode->value           = c4m_coerce_object(pnode->value, result);
            c4m_scope_entry_t *sym = pnode->extra_info;
            sym->value             = pnode->value;
            continue;
        }

        while (c4m_set_contains(set, (void *)next_implicit)) {
            next_implicit++;
        }

        pnode->value           = c4m_coerce_object(c4m_box_u64(next_implicit++),
                                         result);
        c4m_scope_entry_t *sym = pnode->extra_info;
        sym->value             = pnode->value;
    }

    return result;
}

// For now, enum types are either going to be integer types, or they're
// going to be string types.
//
// Once we add UDTs, it will be possible to make them propert UDTs,
// so that we can do proper value checking.

static void
handle_enum_decl(pass1_ctx *ctx)
{
    c4m_tree_node_t   *item;
    c4m_tree_node_t   *tnode  = get_match(ctx, c4m_first_kid_id);
    c4m_pnode_t       *id     = get_pnode(tnode);
    c4m_scope_entry_t *idsym  = NULL;
    c4m_xlist_t       *items  = apply_pattern(ctx, c4m_enum_items);
    int                n      = c4m_xlist_len(items);
    bool               is_str = false;
    c4m_scope_t       *scope;
    c4m_utf8_t        *varname;
    c4m_type_t        *inferred_type;

    if (cur_node_type(ctx) == c4m_nt_global_enum) {
        scope = ctx->file_ctx->global_scope;
    }
    else {
        scope = ctx->file_ctx->module_scope;
    }

    inferred_type = c4m_tspec_typevar();

    if (id != NULL) {
        idsym = declare_sym(ctx,
                            scope,
                            identifier_text(id->token),
                            tnode,
                            sk_enum_type,
                            NULL,
                            true);

        idsym->type = inferred_type;
    }

    for (int i = 0; i < n; i++) {
        c4m_pnode_t *pnode;

        item    = c4m_xlist_get(items, i, NULL);
        varname = node_text(item);
        pnode   = get_pnode(item);

        if (node_num_kids(item) != 0) {
            pnode->value = node_simp_literal(c4m_tree_get_child(item, 0));

            if (!c4m_obj_is_int_type(pnode->value)) {
                if (!c4m_obj_type_check(pnode->value, c4m_tspec_utf8())) {
                    c4m_add_error(ctx->file_ctx,
                                  c4m_err_invalid_enum_lit_type,
                                  item);
                    return;
                }
                if (i == 0) {
                    is_str = true;
                }
                else {
                    if (!is_str) {
                        c4m_add_error(ctx->file_ctx,
                                      c4m_err_enum_str_int_mix,
                                      item);
                        return;
                    }
                }
            }
            else {
                if (is_str) {
                    c4m_add_error(ctx->file_ctx,
                                  c4m_err_enum_str_int_mix,
                                  item);
                    return;
                }
            }
        }
        else {
            if (is_str) {
                c4m_add_error(ctx->file_ctx,
                              c4m_err_omit_string_enum_value,
                              item);
                return;
            }
        }

        c4m_scope_entry_t *item_sym = declare_sym(ctx,
                                                  scope,
                                                  varname,
                                                  item,
                                                  sk_enum_val,
                                                  NULL,
                                                  true);

        item_sym->flags |= C4M_F_DECLARED_CONST;

        if (idsym) {
            item_sym->type = idsym->type;
        }
        else {
            item_sym->type = inferred_type;
        }
        pnode->extra_info = item_sym;
    }

    if (is_str) {
        validate_str_enum_vals(ctx, items);
        c4m_merge_types(inferred_type, c4m_tspec_utf8());
    }
    else {
        /*c4m_type_t *ty = */ validate_int_enum_vals(ctx, items);
        // TODO: fix this to cooerce properly.
        c4m_merge_types(inferred_type, c4m_tspec_i64());
    }

#ifdef C4M_PASS1_UNIT_TESTS
    if (id == NULL) {
        printf("Anonymous enum (%lld kids).\n", c4m_xlist_len(items));
    }
    else {
        printf("Enum name: %s (%lld kids)\n",
               identifier_text(id->token)->data,
               c4m_xlist_len(items));
    }

    for (int i = 0; i < n; i++) {
        item               = c4m_xlist_get(items, i, NULL);
        varname            = node_text(item);
        c4m_pnode_t *pnode = get_pnode(item);
        printf("About to print an enum value:\n");
        c4m_print(c4m_cstr_format("enum name: {} value: {}",
                                  varname,
                                  pnode->value));
    }
#endif
}

static void
handle_var_decl(pass1_ctx *ctx)
{
    c4m_tree_node_t *n         = cur_node(ctx);
    c4m_xlist_t     *quals     = apply_pattern_on_node(n,
                                               c4m_qualifier_extract);
    bool             is_const  = false;
    bool             is_let    = false;
    bool             is_global = false;
    c4m_scope_t     *scope;

    for (int i = 0; i < c4m_xlist_len(quals); i++) {
        c4m_utf8_t *qual = node_text(c4m_xlist_get(quals, i, NULL));
        switch (qual->data[0]) {
        case 'g':
            is_global = true;
            continue;
        case 'c':
            is_const = true;
            continue;
        case 'l':
            is_let = true;
        default:
            continue;
        }
    }

    scope = ctx->static_scope;

    if (is_global) {
        while (scope->parent != NULL) {
            scope = scope->parent;
        }
    }

    c4m_xlist_t *syms = apply_pattern_on_node(n, c4m_sym_decls);

    for (int i = 0; i < c4m_xlist_len(syms); i++) {
        c4m_tree_node_t *one_set   = c4m_xlist_get(syms, i, NULL);
        c4m_xlist_t     *var_names = apply_pattern_on_node(one_set,
                                                       c4m_sym_names);
        c4m_tree_node_t *type_node = get_match_on_node(one_set, c4m_sym_type);
        c4m_tree_node_t *init      = get_match_on_node(one_set, c4m_sym_init);
        c4m_type_t      *type      = NULL;

        if (type_node != NULL) {
            type = c4m_node_to_type(ctx->file_ctx, type_node, NULL);
        }

        for (int j = 0; j < c4m_xlist_len(var_names); j++) {
            c4m_tree_node_t   *name_node = c4m_xlist_get(var_names, j, NULL);
            c4m_utf8_t        *name      = node_text(name_node);
            c4m_scope_entry_t *sym       = declare_sym(ctx,
                                                 scope,
                                                 name,
                                                 name_node,
                                                 sk_variable,
                                                 NULL,
                                                 true);

            if (sym != NULL) {
                c4m_pnode_t *pn = get_pnode(name_node);
                pn->value       = (void *)sym;

                if (is_const) {
                    sym->flags |= C4M_F_DECLARED_CONST;
                }
                if (is_let) {
                    sym->flags |= C4M_F_DECLARED_LET;
                }

                if (init != NULL && j + 1 == c4m_xlist_len(var_names)) {
                    set_current_node(ctx, init);
                    process_children(ctx);
                    set_current_node(ctx, n);

                    sym->flags |= C4M_F_HAS_INITIALIZER;

                    c4m_pnode_t *initpn   = get_pnode(init);
                    c4m_type_t  *inf_type = c4m_get_my_type(initpn->value);

                    if (!c4m_is_partial_type(inf_type)) {
                        sym->value = initpn->value;
                        sym->type  = inf_type;
                    }

                    else {
                        sym->value = init;
                    }

                    if (type) {
                        sym->flags |= C4M_F_TYPE_IS_DECLARED;
                        if (!c4m_tspecs_are_compat(inf_type, type)) {
                            c4m_add_error(ctx->file_ctx,
                                          c4m_err_inconsistent_type,
                                          name_node,
                                          inf_type,
                                          type,
                                          c4m_node_get_loc_str(type_node));
                        }
                    }
                }
                else {
                    if (type) {
                        sym->type = type;
                        sym->flags |= C4M_F_TYPE_IS_DECLARED;
                    }
                }
            }
        }
    }
}

static void
handle_param_block(pass1_ctx *ctx)
{
    // Reminder to self: make sure to check for not const in the decl.
    // That really needs to happen at the end of the pass through :)
    c4m_module_param_info_t *prop      = c4m_gc_alloc(c4m_module_param_info_t);
    c4m_tree_node_t         *root      = cur_node(ctx);
    c4m_pnode_t             *pnode     = get_pnode(root);
    c4m_tree_node_t         *name_node = c4m_tree_get_child(root, 0);
    c4m_utf8_t              *sym_name  = node_text(name_node);
    c4m_utf8_t              *dot       = c4m_new_utf8(".");
    int                      nkids     = c4m_tree_get_number_children(root);
    c4m_scope_entry_t       *sym;
    bool                     attr;

    if (pnode->short_doc) {
        prop->short_doc = c4m_token_raw_content(pnode->short_doc);

        if (pnode->long_doc) {
            prop->long_doc = c4m_token_raw_content(pnode->long_doc);
        }
    }

    if (pnode->kind == c4m_nt_member) {
        attr            = true;
        int num_members = c4m_tree_get_number_children(name_node);

        for (int i = 1; i < num_members; i++) {
            sym_name = c4m_str_concat(sym_name, dot);
            sym_name = c4m_str_concat(sym_name,
                                      node_text(
                                          c4m_tree_get_child(name_node, i)));
        }
    }
    else {
        attr = false;
    }

    for (int i = 1; i < nkids; i++) {
        c4m_tree_node_t *prop_node = c4m_tree_get_child(root, i);
        c4m_utf8_t      *prop_name = node_text(prop_node);
        c4m_obj_t        lit       = c4m_tree_get_child(prop_node, 0);

        switch (prop_name->data[0]) {
        case 'v':
            prop->validator = node_literal(ctx->file_ctx, lit, NULL);
            break;
        case 'c':
            prop->callback = node_literal(ctx->file_ctx, lit, NULL);
            break;
        case 'd':
            prop->default_value = node_literal(ctx->file_ctx, lit, NULL);
            break;
        default:
            unreachable();
        }
    }

    if (!hatrack_dict_add(ctx->file_ctx->parameters, sym_name, prop)) {
        c4m_add_error(ctx->file_ctx, c4m_err_dupe_param, name_node);
        return;
    }

    if (attr) {
        sym = c4m_lookup_symbol(ctx->file_ctx->attribute_scope, sym_name);
        if (sym) {
            if (c4m_sym_is_declared_const(sym)) {
                c4m_add_error(ctx->file_ctx, c4m_err_const_param, name_node);
            }
        }
        else {
            sym = declare_sym(ctx,
                              ctx->file_ctx->attribute_scope,
                              sym_name,
                              name_node,
                              sk_attr,
                              NULL,
                              false);
        }
    }
    else {
        sym = c4m_lookup_symbol(ctx->file_ctx->module_scope, sym_name);
        if (sym) {
            if (c4m_sym_is_declared_const(sym)) {
                c4m_add_error(ctx->file_ctx, c4m_err_const_param, name_node);
            }
        }
        else {
            sym = declare_sym(ctx,
                              ctx->file_ctx->module_scope,
                              sym_name,
                              name_node,
                              sk_variable,
                              NULL,
                              false);
        }
    }
}

static void
one_section_prop(pass1_ctx          *ctx,
                 c4m_spec_section_t *section,
                 c4m_tree_node_t    *n)
{
    bool       *value;
    c4m_obj_t   callback;
    c4m_utf8_t *prop = node_text(n);

    switch (prop->data[0]) {
    case 'u': // user_def_ok
        value = node_simp_literal(c4m_tree_get_child(n, 0));

        if (!value || !c4m_obj_type_check((c4m_obj_t)value, c4m_tspec_bool())) {
            c4m_add_error(ctx->file_ctx,
                          c4m_err_spec_bool_required,
                          c4m_tree_get_child(n, 0));
        }
        else {
            if (*value) {
                section->user_def_ok = 1;
            }
        }
        break;
    case 'h': // hidden
        value = node_simp_literal(c4m_tree_get_child(n, 0));
        if (!value || !c4m_obj_type_check((c4m_obj_t)value, c4m_tspec_bool())) {
            c4m_add_error(ctx->file_ctx,
                          c4m_err_spec_bool_required,
                          c4m_tree_get_child(n, 0));
        }
        else {
            if (*value) {
                section->hidden = 1;
            }
        }
        break;
    case 'v': // validator
        callback = node_to_callback(ctx->file_ctx, c4m_tree_get_child(n, 0));

        if (!callback) {
            c4m_add_error(ctx->file_ctx,
                          c4m_err_spec_callback_required,
                          c4m_tree_get_child(n, 0));
        }
        else {
            section->validator = callback;
        }
        break;
    case 'r': // require
        for (int i = 0; i < c4m_tree_get_number_children(n); i++) {
            c4m_utf8_t *name = node_text(c4m_tree_get_child(n, i));
            if (!c4m_set_add(section->required_sections, name)) {
                c4m_add_warning(ctx->file_ctx,
                                c4m_warn_dupe_require,
                                c4m_tree_get_child(n, i));
            }
            if (c4m_set_contains(section->allowed_sections, name)) {
                c4m_add_warning(ctx->file_ctx,
                                c4m_warn_require_allow,
                                c4m_tree_get_child(n, i));
            }
        }
        break;
    default: // allow
        for (int i = 0; i < c4m_tree_get_number_children(n); i++) {
            c4m_utf8_t *name = node_text(c4m_tree_get_child(n, i));
            if (!c4m_set_add(section->allowed_sections, name)) {
                c4m_add_warning(ctx->file_ctx,
                                c4m_warn_dupe_allow,
                                c4m_tree_get_child(n, i));
            }
            if (c4m_set_contains(section->required_sections, name)) {
                c4m_add_warning(ctx->file_ctx,
                                c4m_warn_require_allow,
                                c4m_tree_get_child(n, i));
            }
        }
    }
}

static void
one_field(pass1_ctx          *ctx,
          c4m_spec_section_t *section,
          c4m_tree_node_t    *tnode)
{
    c4m_spec_field_t *f        = c4m_gc_alloc(c4m_spec_field_t);
    c4m_utf8_t       *name     = node_text(c4m_tree_get_child(tnode, 0));
    c4m_pnode_t      *pnode    = get_pnode(tnode);
    int               num_kids = c4m_tree_get_number_children(tnode);
    bool             *value;
    c4m_obj_t         callback;

    f->exclusions       = c4m_new(c4m_tspec_set(c4m_tspec_utf8()));
    f->name             = name;
    f->declaration_node = tnode;

    if (pnode->short_doc) {
        section->short_doc = c4m_token_raw_content(pnode->short_doc);

        if (pnode->long_doc) {
            section->long_doc = c4m_token_raw_content(pnode->long_doc);
        }
    }

    for (int i = 1; i < num_kids; i++) {
        c4m_tree_node_t *kid  = c4m_tree_get_child(tnode, i);
        c4m_utf8_t      *prop = node_text(kid);
        switch (prop->data[0]) {
        case 'c': // choice:
                  // For now, we just stash the raw nodes, and
                  // evaluate it later.
            f->stashed_options = node_literal(ctx->file_ctx,
                                              c4m_tree_get_child(kid, 0),
                                              NULL);
            f->validate_choice = 1;
            break;

        case 'd': // default:
                  // Same.
            f->default_value    = node_literal(ctx->file_ctx,
                                            c4m_tree_get_child(kid, 0),
                                            NULL);
            f->default_provided = 1;
            break;

        case 'h': // hidden
            value = node_simp_literal(c4m_tree_get_child(kid, 0));
            if (!value || !c4m_obj_type_check((c4m_obj_t)value, c4m_tspec_bool())) {
                c4m_add_error(ctx->file_ctx,
                              c4m_err_spec_bool_required,
                              c4m_tree_get_child(kid, 0));
            }
            else {
                if (*value) {
                    f->hidden = 1;
                }
            }
            break;

        case 'l': // lock
            value = node_simp_literal(c4m_tree_get_child(kid, 0));
            if (!value || !c4m_obj_type_check((c4m_obj_t)value, c4m_tspec_bool())) {
                c4m_add_error(ctx->file_ctx,
                              c4m_err_spec_bool_required,
                              c4m_tree_get_child(kid, 0));
            }
            else {
                if (*value) {
                    f->lock_on_write = 1;
                }
            }
            break;

        case 'e': // exclusions
            for (int i = 0; i < c4m_tree_get_number_children(kid); i++) {
                c4m_utf8_t *name = node_text(c4m_tree_get_child(kid, i));

                if (!c4m_set_add(f->exclusions, name)) {
                    c4m_add_warning(ctx->file_ctx,
                                    c4m_warn_dupe_exclusion,
                                    c4m_tree_get_child(kid, i));
                }
            }
            break;

        case 't': // type
            if (node_has_type(c4m_tree_get_child(kid, 0), c4m_nt_identifier)) {
                f->tinfo.type_pointer = node_text(c4m_tree_get_child(kid, 0));
            }
            else {
                f->tinfo.type = c4m_node_to_type(ctx->file_ctx,
                                                 c4m_tree_get_child(kid, 0),
                                                 NULL);
            }
            break;

        case 'v': // validator
            callback = node_to_callback(ctx->file_ctx,
                                        c4m_tree_get_child(kid, 0));

            if (!callback) {
                c4m_add_error(ctx->file_ctx,
                              c4m_err_spec_callback_required,
                              c4m_tree_get_child(kid, 0));
            }
            else {
                f->validator = callback;
            }
            break;
        default:
            if (!strcmp(prop->data, "range")) {
                f->stashed_options = c4m_tree_get_child(kid, 0);
            }
            else {
                // required.
                value = node_simp_literal(c4m_tree_get_child(kid, 0));
                if (!value || !c4m_obj_type_check((c4m_obj_t)value, c4m_tspec_bool())) {
                    c4m_add_error(ctx->file_ctx,
                                  c4m_err_spec_bool_required,
                                  c4m_tree_get_child(kid, 0));
                }
                else {
                    if (*value) {
                        f->required = 1;
                    }
                }
                break;
            }
        }
    }

    if (!hatrack_dict_add(section->fields, name, f)) {
        c4m_add_error(ctx->file_ctx, c4m_err_dupe_spec_field, tnode);
    }
}

static void
handle_section_spec(pass1_ctx *ctx)
{
    c4m_spec_t         *spec     = ctx->spec;
    c4m_spec_section_t *section  = c4m_gc_alloc(c4m_spec_section_t);
    c4m_tree_node_t    *tnode    = cur_node(ctx);
    c4m_pnode_t        *pnode    = get_pnode(tnode);
    int                 ix       = 2;
    int                 num_kids = c4m_tree_get_number_children(tnode);

    section->fields            = c4m_new(c4m_tspec_dict(c4m_tspec_utf8(),
                                             c4m_tspec_ref()));
    section->allowed_sections  = c4m_new(c4m_tspec_set(c4m_tspec_utf8()));
    section->required_sections = c4m_new(c4m_tspec_set(c4m_tspec_utf8()));
    section->declaration_node  = tnode;

    if (pnode->short_doc) {
        section->short_doc = c4m_token_raw_content(pnode->short_doc);

        if (pnode->long_doc) {
            section->long_doc = c4m_token_raw_content(pnode->long_doc);
        }
    }

    c4m_utf8_t *kind = node_text(c4m_tree_get_child(tnode, 0));
    switch (kind->data[0]) {
    case 's': // singleton
        section->singleton = 1;
        // fallthrough
    case 'n': // named
        section->name = node_text(c4m_tree_get_child(tnode, 1));
        break;
    default: // root
        ix = 1;
        break;
    }

    for (; ix < num_kids; ix++) {
        c4m_tree_node_t *tkid = c4m_tree_get_child(tnode, ix);
        c4m_pnode_t     *pkid = get_pnode(tkid);

        if (pkid->kind == c4m_nt_section_prop) {
            one_section_prop(ctx, section, tkid);
        }
        else {
            one_field(ctx, section, tkid);
        }
    }

    if (section->name == NULL) {
        if (spec->root_section) {
            c4m_add_error(ctx->file_ctx,
                          c4m_err_dupe_root_section,
                          tnode);
        }
        else {
            spec->root_section = section;
        }
    }
    else {
        if (!hatrack_dict_add(spec->section_specs, section->name, section)) {
            c4m_add_error(ctx->file_ctx, c4m_err_dupe_section, tnode);
        }
    }
}

static void
handle_config_spec(pass1_ctx *ctx)
{
    c4m_tree_node_t *tnode = cur_node(ctx);
    c4m_pnode_t     *pnode = get_pnode(tnode);

    if (ctx->file_ctx->local_confspecs == NULL) {
        ctx->file_ctx->local_confspecs = c4m_new_spec();
    }
    else {
        c4m_add_error(ctx->file_ctx, c4m_err_dupe_section, tnode);
        return;
    }

    ctx->spec                   = ctx->file_ctx->local_confspecs;
    ctx->spec->declaration_node = tnode;

    if (pnode->short_doc) {
        ctx->spec->short_doc = c4m_token_raw_content(pnode->short_doc);

        if (pnode->long_doc) {
            ctx->spec->long_doc = c4m_token_raw_content(pnode->long_doc);
        }
    }

    process_children(ctx);
}

static c4m_sig_info_t *
new_sig_info(int num_params)
{
    c4m_sig_info_t *result = c4m_gc_alloc(c4m_sig_info_t);
    result->num_params     = num_params;

    if (result->num_params > 0) {
        result->param_info = c4m_gc_array_alloc(c4m_fn_param_info_t,
                                                num_params);
    }

    return result;
}

static c4m_sig_info_t *
extract_fn_sig_info(pass1_ctx       *ctx,
                    c4m_tree_node_t *tree)
{
    c4m_xlist_t    *decls     = apply_pattern_on_node(tree,
                                               c4m_param_extraction);
    int             ndecls    = c4m_xlist_len(decls);
    int             nparams   = 0;
    int             cur_param = 0;
    c4m_xlist_t    *ptypes    = c4m_new(c4m_tspec_xlist(c4m_tspec_typespec()));
    c4m_sig_info_t *info;

    // Allocate space for parameters by counting how many variable
    // names we find.

    for (int i = 0; i < ndecls; i++) {
        c4m_tree_node_t *node  = c4m_xlist_get(decls, i, NULL);
        int              kidct = c4m_tree_get_number_children(node);

        if (kidct > 1) {
            c4m_tree_node_t *kid   = c4m_tree_get_child(node, kidct - 1);
            c4m_pnode_t     *pnode = get_pnode(kid);

            // Skip type specs.
            if (pnode->kind != c4m_nt_identifier) {
                kidct--;
            }
        }
        nparams += kidct;
    }

    info           = new_sig_info(nparams);
    info->fn_scope = c4m_new_scope(ctx->file_ctx->module_scope,
                                   C4M_SCOPE_LOCAL);
    info->formals  = c4m_new_scope(ctx->file_ctx->module_scope,
                                  C4M_SCOPE_FORMALS);

    // Now, we loop through the parameter trees again. In function
    // declarations, named variables with omitted types are given a
    // type variable as a return type. Similarly, omitted return
    // values get a type variable.

    for (int i = 0; i < ndecls; i++) {
        c4m_tree_node_t *node     = c4m_xlist_get(decls, i, NULL);
        int              kidct    = c4m_tree_get_number_children(node);
        c4m_type_t      *type     = NULL;
        bool             got_type = false;

        if (kidct > 1) {
            c4m_tree_node_t *kid   = c4m_tree_get_child(node, kidct - 1);
            c4m_pnode_t     *pnode = get_pnode(kid);

            if (pnode->kind != c4m_nt_identifier) {
                type = c4m_node_to_type(ctx->file_ctx, kid, NULL);
                kidct--;
                got_type = true;
            }
        }

        // All but the last one in a subtree get type variables.
        for (int j = 0; j < kidct - 1; j++) {
            c4m_fn_param_info_t *pi  = &info->param_info[cur_param++];
            c4m_tree_node_t     *kid = c4m_tree_get_child(node, j);
            pi->name                 = node_text(kid);
            pi->type                 = c4m_tspec_typevar();

            declare_sym(ctx,
                        info->formals,
                        pi->name,
                        kid,
                        sk_formal,
                        NULL,
                        true);

            // Redeclare in the 'actual' scope. If there's a declared
            // type this won't get it; it will be fully inferred, and
            // then we'll compare against the declared type at the
            // end.
            declare_sym(ctx,
                        info->fn_scope,
                        pi->name,
                        kid,
                        sk_variable,
                        NULL,
                        true);

            c4m_xlist_append(ptypes, pi->type);
        }

        // last item.
        if (!type) {
            type = c4m_tspec_typevar();
        }

        c4m_fn_param_info_t *pi  = &info->param_info[cur_param++];
        c4m_tree_node_t     *kid = c4m_tree_get_child(node, kidct - 1);
        pi->name                 = node_text(kid);
        pi->type                 = type;

        c4m_scope_entry_t *formal = declare_sym(ctx,
                                                info->formals,
                                                pi->name,
                                                kid,
                                                sk_formal,
                                                NULL,
                                                true);

        formal->type = type;
        if (got_type) {
            formal->flags |= C4M_F_TYPE_IS_DECLARED;
        }

        declare_sym(ctx,
                    info->fn_scope,
                    pi->name,
                    kid,
                    sk_variable,
                    NULL,
                    true);

        c4m_xlist_append(ptypes, pi->type ? pi->type : c4m_tspec_typevar());
    }

    c4m_tree_node_t *retnode = get_match_on_node(tree, c4m_return_extract);

    c4m_scope_entry_t *formal = declare_sym(ctx,
                                            info->formals,
                                            c4m_new_utf8("$result"),
                                            retnode,
                                            sk_formal,
                                            NULL,
                                            true);
    if (retnode) {
        info->return_info.type = c4m_node_to_type(ctx->file_ctx,
                                                  retnode,
                                                  NULL);
        formal->type           = info->return_info.type;
        formal->flags |= C4M_F_TYPE_IS_DECLARED;
    }
    else {
        formal->type = c4m_tspec_typevar();
    }

    declare_sym(ctx,
                info->fn_scope,
                c4m_new_utf8("$result"),
                ctx->cur_tnode,
                sk_variable,
                NULL,
                true);

    // Now fill out the 'local_type' field of the ffi decl.
    // TODO: support varargs.
    //
    // Note that this will get replaced once we do some checking.

    c4m_type_t *ret_for_sig = info->return_info.type;

    if (!ret_for_sig) {
        ret_for_sig = c4m_tspec_typevar();
    }

    info->full_type = c4m_tspec_fn(ret_for_sig, ptypes, false);
    return info;
}

static void
handle_func_decl(pass1_ctx *ctx)
{
    // YOU ARE HERE- HANDLE `ONCE` modifier.
    c4m_tree_node_t   *tnode = cur_node(ctx);
    c4m_fn_decl_t     *decl  = c4m_gc_alloc(c4m_fn_decl_t);
    c4m_utf8_t        *name  = node_text(get_match(ctx, c4m_2nd_kid_id));
    c4m_xlist_t       *mods  = apply_pattern(ctx, c4m_func_mods);
    int                nmods = c4m_xlist_len(mods);
    c4m_scope_entry_t *sym;

    decl->signature_info = extract_fn_sig_info(ctx, cur_node(ctx));

    for (int i = 0; i < nmods; i++) {
        c4m_tree_node_t *mod_node = c4m_xlist_get(mods, i, NULL);
        switch (node_text(mod_node)->data[0]) {
        case 'p':
            decl->private = 1;
            break;
        default:
            decl->once = 1;
            break;
        }
    }

    sym = declare_sym(ctx,
                      ctx->file_ctx->module_scope,
                      name,
                      tnode,
                      sk_func,
                      NULL,
                      true);

    if (sym) {
        sym->type  = decl->signature_info->full_type;
        sym->value = (void *)decl;
    }

    ctx->static_scope = decl->signature_info->fn_scope;

    c4m_pnode_t *pnode = get_pnode(tnode);
    pnode->value       = (c4m_obj_t)sym;

    node_down(ctx, tnode->num_kids - 1);
    process_children(ctx);
    node_up(ctx);
}

static void
handle_extern_block(pass1_ctx *ctx)
{
    c4m_ffi_decl_t  *info          = c4m_gc_alloc(c4m_ffi_info_t);
    c4m_utf8_t      *external_name = node_text(get_match(ctx, c4m_first_kid_id));
    c4m_xlist_t     *ext_params    = apply_pattern(ctx, c4m_extern_params);
    c4m_tree_node_t *ext_ret       = get_match(ctx, c4m_extern_return);
    c4m_pnode_t     *pnode         = get_pnode(cur_node(ctx));
    c4m_tree_node_t *ext_pure      = get_match(ctx, c4m_find_pure);
    c4m_tree_node_t *ext_holds     = get_match(ctx, c4m_find_holds);
    c4m_tree_node_t *ext_allocs    = get_match(ctx, c4m_find_allocs);
    c4m_tree_node_t *ext_lsig      = get_match(ctx, c4m_find_extern_local);

    if (pnode->short_doc) {
        info->short_doc = c4m_token_raw_content(pnode->short_doc);

        if (pnode->long_doc) {
            info->long_doc = c4m_token_raw_content(pnode->long_doc);
        }
    }

    if (ext_params != NULL) {
        int64_t n             = c4m_xlist_len(ext_params);
        info->num_params      = n;
        info->external_name   = external_name;
        info->external_params = c4m_gc_array_alloc(uint8_t, n);

        for (int64_t i = 0; i < n; i++) {
            c4m_tree_node_t *tnode = c4m_xlist_get(ext_params, i, NULL);
            c4m_pnode_t     *pnode = c4m_tree_get_contents(tnode);
            uint64_t         val   = (uint64_t)pnode->extra_info;

            info->external_params[i] = (uint8_t)val;
        }
    }

    if (ext_ret) {
        c4m_pnode_t *pnode = get_pnode(ext_ret);
        uint64_t     val   = (uint64_t)pnode->extra_info;

        info->external_return_type = (uint8_t)val;
    }

    info->local_params = extract_fn_sig_info(ctx, ext_lsig);

    if (ext_pure) {
        bool *pure_ptr = node_simp_literal(c4m_tree_get_child(ext_pure, 0));

        if (pure_ptr && *pure_ptr) {
            info->local_params->pure = 1;
        }
    }

    info->local_name = node_text(get_match_on_node(ext_lsig, c4m_first_kid_id));

    if (ext_holds) {
        if (info->local_params == NULL) {
            c4m_add_error(ctx->file_ctx, c4m_err_no_params_to_hold, ext_holds);
            return;
        }

        uint64_t        bitfield  = 0;
        c4m_sig_info_t *si        = info->local_params;
        int             num_holds = c4m_tree_get_number_children(ext_holds);

        for (int i = 0; i < num_holds; i++) {
            c4m_tree_node_t *kid = c4m_tree_get_child(ext_holds, i);
            c4m_utf8_t      *txt = node_text(kid);

            for (int j = 0; j < si->num_params; j++) {
                c4m_fn_param_info_t *param = &si->param_info[j];
                if (strcmp(txt->data, param->name->data)) {
                    continue;
                }
                param->ffi_holds = 1;
                uint64_t flag    = (uint64_t)(1 << j);
                if (bitfield & flag) {
                    c4m_add_warning(ctx->file_ctx, c4m_warn_dupe_hold, kid);
                }
                bitfield |= flag;
                goto next_i;
            }
            c4m_add_error(ctx->file_ctx, c4m_err_bad_hold_name, kid);
            break;
next_i:
    /* nothing. */;
        }
    }

    if (ext_allocs) {
        uint64_t        bitfield   = 0;
        bool            got_ret    = false;
        c4m_sig_info_t *si         = info->local_params;
        int             num_allocs = c4m_tree_get_number_children(ext_allocs);

        for (int i = 0; i < num_allocs; i++) {
            c4m_tree_node_t *kid = c4m_tree_get_child(ext_allocs, i);
            c4m_utf8_t      *txt = node_text(kid);

            if (!strcmp(txt->data, "return")) {
                if (got_ret) {
                    c4m_add_warning(ctx->file_ctx, c4m_warn_dupe_alloc, kid);
                    continue;
                }
                si->return_info.ffi_allocs = 1;
                continue;
            }

            for (int j = 0; j < si->num_params; j++) {
                c4m_fn_param_info_t *param = &si->param_info[j];
                if (strcmp(txt->data, param->name->data)) {
                    continue;
                }
                param->ffi_allocs = 1;
                uint64_t flag     = (uint64_t)(1 << j);
                if (bitfield & flag) {
                    c4m_add_warning(ctx->file_ctx, c4m_warn_dupe_alloc, kid);
                }
                bitfield |= flag;
                goto next_alloc;
            }
            c4m_add_error(ctx->file_ctx, c4m_err_bad_alloc_name, kid);
            break;
next_alloc:
    /* nothing. */;
        }
    }

    c4m_scope_entry_t *sym = declare_sym(ctx,
                                         ctx->file_ctx->module_scope,
                                         info->local_name,
                                         get_match(ctx, c4m_first_kid_id),
                                         sk_extern_func,
                                         NULL,
                                         true);

    if (sym) {
        sym->type  = info->local_params->full_type;
        sym->value = (void *)info;
    }
}

static void
handle_use_stmt(pass1_ctx *ctx)
{
    c4m_tree_node_t   *uri    = get_match(ctx, c4m_use_uri);
    c4m_tree_node_t   *member = get_match(ctx, c4m_member_last);
    c4m_xlist_t       *prefix = apply_pattern(ctx, c4m_member_prefix);
    c4m_module_info_t *mi     = c4m_gc_alloc(c4m_module_info_t);
    bool               status = false;

    mi->specified_module = node_text(member);

    if (c4m_xlist_len(prefix) != 0) {
        mi->specified_package = node_list_join(prefix,
                                               c4m_utf32_repeat('.', 1),
                                               false);
    }

    if (uri) {
        mi->specified_uri = node_simp_literal(uri);
    }

    c4m_utf8_t *full;

    if (mi->specified_package != NULL) {
        full = c4m_cstr_format("{}.{}",
                               mi->specified_package,
                               mi->specified_module);
    }
    else {
        full = mi->specified_module;
    }

    c4m_scope_entry_t *sym = declare_sym(ctx,
                                         ctx->file_ctx->imports,
                                         full,
                                         cur_node(ctx),
                                         sk_module,
                                         &status,
                                         false);

    if (!status) {
        c4m_add_info(ctx->file_ctx,
                     c4m_info_dupe_import,
                     cur_node(ctx));
    }
    else {
        sym->value = mi;
    }
}

static void
pass_dispatch(pass1_ctx *ctx)
{
    c4m_scope_t *saved_scope;
    c4m_pnode_t *pnode  = get_pnode(cur_node(ctx));
    pnode->static_scope = ctx->static_scope;

    switch (cur_node_type(ctx)) {
    case c4m_nt_global_enum:
    case c4m_nt_enum:
        handle_enum_decl(ctx);
        break;

    case c4m_nt_func_def:
        saved_scope         = ctx->static_scope;
        pnode->static_scope = ctx->static_scope;
        handle_func_decl(ctx);
        ctx->static_scope = saved_scope;
        break;

    case c4m_nt_variable_decls:
        handle_var_decl(ctx);
        break;

    case c4m_nt_config_spec:
        handle_config_spec(ctx);
        break;

    case c4m_nt_section_spec:
        handle_section_spec(ctx);
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

    default:
        process_children(ctx);
        break;
    }
}

static void
find_dependencies(c4m_compile_ctx *cctx, c4m_file_compile_ctx *file_ctx)
{
    c4m_scope_t          *imports = file_ctx->imports;
    uint64_t              len     = 0;
    hatrack_dict_value_t *values  = hatrack_dict_values(imports->symbols,
                                                       &len);

    for (uint64_t i = 0; i < len; i++) {
        c4m_scope_entry_t    *sym = values[i];
        c4m_module_info_t    *mi  = sym->value;
        c4m_tree_node_t      *n   = sym->declaration_node;
        c4m_pnode_t          *pn  = get_pnode(n);
        c4m_file_compile_ctx *mc  = c4m_init_from_use(cctx,
                                                     mi->specified_module,
                                                     mi->specified_package,
                                                     mi->specified_uri);

        if (c4m_set_contains(cctx->processed, mc)) {
            continue;
        }

        pn->value = (c4m_obj_t)mc;
    }
}

void
c4m_file_decl_pass(c4m_compile_ctx *cctx, c4m_file_compile_ctx *file_ctx)
{
    if (c4m_fatal_error_in_module(file_ctx)) {
        return;
    }

    setup_treematch_patterns();

    pass1_ctx ctx = {
        .file_ctx = file_ctx,
    };

    set_current_node(&ctx, file_ctx->parse_tree);

    file_ctx->global_scope    = c4m_new_scope(NULL, C4M_SCOPE_GLOBAL);
    file_ctx->module_scope    = c4m_new_scope(file_ctx->global_scope,
                                           C4M_SCOPE_MODULE);
    file_ctx->attribute_scope = c4m_new_scope(NULL, C4M_SCOPE_ATTRIBUTES);
    file_ctx->imports         = c4m_new_scope(NULL, C4M_SCOPE_IMPORTS);
    file_ctx->parameters      = c4m_new(c4m_tspec_dict(c4m_tspec_utf8(),
                                                  c4m_tspec_ref()));

    ctx.cur->static_scope = file_ctx->module_scope;
    ctx.static_scope      = file_ctx->module_scope;

    c4m_pnode_t *pnode = get_pnode(file_ctx->parse_tree);

    if (pnode->short_doc) {
        file_ctx->short_doc = c4m_token_raw_content(pnode->short_doc);

        if (pnode->long_doc) {
            file_ctx->long_doc = c4m_token_raw_content(pnode->long_doc);
        }
    }

    pass_dispatch(&ctx);
    find_dependencies(cctx, file_ctx);
    if (file_ctx->fatal_errors) {
        cctx->fatality = true;
    }

    return;
}

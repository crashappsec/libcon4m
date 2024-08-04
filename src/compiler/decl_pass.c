#define C4M_USE_INTERNAL_API
#include "con4m.h"

// First pass of the raw parse tree.

// Not everything we need would be resolvable in one pass, especially
// symbols in other modules.
//
// Since declarations can be in any statement, we do go ahead and
// finish walking down to the expression level, and do initial work on
// literal extraction too.

typedef struct c4m_pass1_ctx {
    c4m_tree_node_t        *cur_tnode;
    c4m_pnode_t            *cur;
    c4m_spec_t             *spec;
    c4m_compile_ctx        *cctx;
    c4m_module_compile_ctx *module_ctx;
    c4m_scope_t            *static_scope;
    c4m_list_t             *extern_decls;
    bool                    in_func;
} c4m_pass1_ctx;

static inline c4m_tree_node_t *
c4m_get_match(c4m_pass1_ctx *ctx, c4m_tpat_node_t *pattern)
{
    return c4m_get_match_on_node(ctx->cur_tnode, pattern);
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

    if (n->children[i]->parent != n) {
        c4m_print_parse_node(n->children[i]);
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

static void pass_dispatch(c4m_pass1_ctx *ctx);

void
c4m_module_param_gc_bits(uint64_t *bitmap, c4m_module_param_info_t *pi)
{
    c4m_mark_raw_to_addr(bitmap, pi, &pi->default_value);
}

c4m_module_param_info_t *
c4m_new_module_param(void)
{
    return c4m_gc_alloc_mapped(c4m_module_param_info_t,
                               c4m_module_param_gc_bits);
}

void
c4m_sig_info_gc_bits(uint64_t *bitmap, c4m_sig_info_t *si)
{
    c4m_mark_raw_to_addr(bitmap, si, &si->return_info.type);
}

c4m_sig_info_t *
c4m_new_sig_info(void)
{
    return c4m_gc_alloc_mapped(c4m_sig_info_t, c4m_sig_info_gc_bits);
}

void
c4m_fn_decl_gc_bits(uint64_t *bitmap, c4m_fn_decl_t *decl)
{
    c4m_mark_raw_to_addr(bitmap, decl, &decl->cfg);
}

c4m_fn_decl_t *
c4m_new_fn_decl(void)
{
    return c4m_gc_alloc_mapped(c4m_fn_decl_t, c4m_fn_decl_gc_bits);
}

c4m_ffi_decl_t *
c4m_new_ffi_decl(void)
{
    return c4m_gc_alloc_mapped(c4m_ffi_decl_t, C4M_GC_SCAN_ALL);
}

void
c4m_module_info_gc_bits(uint64_t *bitmap, c4m_module_info_t *info)
{
    c4m_mark_raw_to_addr(bitmap, info, &info->specified_uri);
}

c4m_module_info_t *
c4m_new_module_info()
{
    return c4m_gc_alloc_mapped(c4m_module_info_t, c4m_module_info_gc_bits);
}

static inline void
process_children(c4m_pass1_ctx *ctx)
{
    c4m_tree_node_t *n = ctx->cur_tnode;

    for (int i = 0; i < n->num_kids; i++) {
        c4m_node_down(ctx, i);
        pass_dispatch(ctx);
        c4m_node_up(ctx);
    }

    if (n->num_kids == 1) {
        c4m_pnode_t *pparent = c4m_get_pnode(n);
        c4m_pnode_t *pkid    = c4m_get_pnode(n->children[0]);
        pparent->value       = pkid->value;
    }
}

static bool
obj_type_check(c4m_pass1_ctx *ctx, const c4m_obj_t *obj, c4m_type_t *t2)
{
    int  warn;
    bool res = c4m_obj_type_check(obj, t2, &warn);

    return res;
}

static inline c4m_symbol_t *
declare_sym(c4m_pass1_ctx   *ctx,
            c4m_scope_t     *scope,
            c4m_utf8_t      *name,
            c4m_tree_node_t *node,
            c4m_symbol_kind  kind,
            bool            *success,
            bool             err_if_present)
{
    c4m_symbol_t *result = c4m_declare_symbol(ctx->module_ctx,
                                              scope,
                                              name,
                                              node,
                                              kind,
                                              success,
                                              err_if_present);

    c4m_shadow_check(ctx->module_ctx, result, scope);

    result->flags |= C4M_F_IS_DECLARED;

    return result;
}

static void
validate_str_enum_vals(c4m_pass1_ctx *ctx, c4m_list_t *items)
{
    c4m_set_t *set = c4m_new(c4m_type_set(c4m_type_utf8()));
    int64_t    n   = c4m_list_len(items);

    for (int i = 0; i < n; i++) {
        c4m_tree_node_t *tnode = c4m_list_get(items, i, NULL);
        c4m_pnode_t     *pnode = c4m_get_pnode(tnode);

        if (c4m_tree_get_number_children(tnode) == 0) {
            pnode->value = (void *)c4m_new_utf8("error");
            continue;
        }

        c4m_symbol_t *sym = pnode->extra_info;
        c4m_utf8_t   *val = (c4m_utf8_t *)pnode->value;
        sym->value        = val;

        if (!c4m_set_add(set, val)) {
            c4m_add_error(ctx->module_ctx, c4m_err_dupe_enum, tnode);
            return;
        }
    }
}

static c4m_type_t *
validate_int_enum_vals(c4m_pass1_ctx *ctx, c4m_list_t *items)
{
    c4m_set_t  *set           = c4m_new(c4m_type_set(c4m_type_u64()));
    int64_t     n             = c4m_list_len(items);
    int         bits          = 0;
    bool        neg           = false;
    uint64_t    next_implicit = 0;
    c4m_type_t *result;

    // First, extract numbers from set values.
    for (int i = 0; i < n; i++) {
        c4m_tree_node_t *tnode = c4m_list_get(items, i, NULL);
        if (c4m_tree_get_number_children(tnode) == 0) {
            continue;
        }

        c4m_pnode_t   *pnode  = c4m_get_pnode(tnode);
        c4m_obj_t     *ref    = pnode->value;
        c4m_type_t    *ty     = c4m_get_my_type(ref);
        c4m_dt_info_t *dtinfo = c4m_type_get_data_type_info(ty);
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
            c4m_add_error(ctx->module_ctx, c4m_err_dupe_enum, tnode);
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
        result = neg ? c4m_type_i8() : c4m_type_u8();
        break;
    case 32:
        result = neg ? c4m_type_i32() : c4m_type_u32();
        break;
    default:
        result = neg ? c4m_type_i64() : c4m_type_u64();
        break;
    }

    for (int i = 0; i < n; i++) {
        c4m_tree_node_t *tnode = c4m_list_get(items, i, NULL);
        c4m_pnode_t     *pnode = c4m_get_pnode(tnode);

        if (c4m_tree_get_number_children(tnode) != 0) {
            pnode->value      = c4m_coerce_object(pnode->value, result);
            c4m_symbol_t *sym = pnode->extra_info;
            sym->value        = pnode->value;
            continue;
        }

        while (c4m_set_contains(set, (void *)next_implicit)) {
            next_implicit++;
        }

        pnode->value      = c4m_coerce_object(c4m_box_u64(next_implicit++),
                                         result);
        c4m_symbol_t *sym = pnode->extra_info;
        sym->value        = pnode->value;
    }

    return result;
}

// For now, enum types are either going to be integer types, or they're
// going to be string types.
//
// Once we add UDTs, it will be possible to make them propert UDTs,
// so that we can do proper value checking.

static c4m_list_t *
extract_enum_items(c4m_pass1_ctx *ctx)
{
    c4m_list_t      *result = c4m_list(c4m_type_ref());
    c4m_tree_node_t *node   = ctx->cur_tnode;
    int              len    = node->num_kids;

    for (int i = 0; i < len; i++) {
        c4m_pnode_t *kid = c4m_get_pnode(node->children[i]);

        if (kid->kind == c4m_nt_enum_item) {
            c4m_list_append(result, node->children[i]);
        }
    }

    return result;
}

static void
handle_enum_decl(c4m_pass1_ctx *ctx)
{
    c4m_tree_node_t *item;
    c4m_tree_node_t *tnode  = c4m_get_match(ctx, c4m_first_kid_id);
    c4m_pnode_t     *id     = c4m_get_pnode(tnode);
    c4m_symbol_t    *idsym  = NULL;
    c4m_list_t      *items  = extract_enum_items(ctx);
    int              n      = c4m_list_len(items);
    bool             is_str = false;
    c4m_scope_t     *scope;
    c4m_utf8_t      *varname;
    c4m_type_t      *inferred_type;

    if (c4m_cur_node_type(ctx) == c4m_nt_global_enum) {
        scope = ctx->module_ctx->global_scope;
    }
    else {
        scope = ctx->module_ctx->module_scope;
    }

    inferred_type = c4m_new_typevar();

    if (id != NULL) {
        idsym = declare_sym(ctx,
                            scope,
                            c4m_identifier_text(id->token),
                            tnode,
                            C4M_SK_ENUM_TYPE,
                            NULL,
                            true);

        idsym->type = inferred_type;
    }

    for (int i = 0; i < n; i++) {
        c4m_pnode_t *pnode;

        item    = c4m_list_get(items, i, NULL);
        varname = c4m_node_text(item);
        pnode   = c4m_get_pnode(item);

        if (c4m_node_num_kids(item) != 0) {
            pnode->value = c4m_node_simp_literal(c4m_tree_get_child(item, 0));

            if (!c4m_obj_is_int_type(pnode->value)) {
                if (!obj_type_check(ctx, pnode->value, c4m_type_utf8())) {
                    c4m_add_error(ctx->module_ctx,
                                  c4m_err_invalid_enum_lit_type,
                                  item);
                    return;
                }
                if (i == 0) {
                    is_str = true;
                }
                else {
                    if (!is_str) {
                        c4m_add_error(ctx->module_ctx,
                                      c4m_err_enum_str_int_mix,
                                      item);
                        return;
                    }
                }
            }
            else {
                if (is_str) {
                    c4m_add_error(ctx->module_ctx,
                                  c4m_err_enum_str_int_mix,
                                  item);
                    return;
                }
            }
        }
        else {
            if (is_str) {
                c4m_add_error(ctx->module_ctx,
                              c4m_err_omit_string_enum_value,
                              item);
                return;
            }
        }

        c4m_symbol_t *item_sym = declare_sym(ctx,
                                             scope,
                                             varname,
                                             item,
                                             C4M_SK_ENUM_VAL,
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
        c4m_merge_types(inferred_type, c4m_type_utf8(), NULL);
    }
    else {
        c4m_type_t *ty = validate_int_enum_vals(ctx, items);
        int         warn;

        if (c4m_type_is_error(c4m_merge_types(inferred_type, ty, &warn))) {
            c4m_add_error(ctx->module_ctx,
                          c4m_err_inconsistent_type,
                          c4m_cur_node(ctx),
                          inferred_type,
                          ty);
        }
    }
}

static void
handle_var_decl(c4m_pass1_ctx *ctx)
{
    c4m_tree_node_t *n         = c4m_cur_node(ctx);
    c4m_list_t      *quals     = c4m_apply_pattern_on_node(n,
                                                  c4m_qualifier_extract);
    bool             is_const  = false;
    bool             is_let    = false;
    bool             is_global = false;
    c4m_scope_t     *scope;

    for (int i = 0; i < c4m_list_len(quals); i++) {
        c4m_utf8_t *qual = c4m_node_text(c4m_list_get(quals, i, NULL));
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

    c4m_list_t *syms = c4m_apply_pattern_on_node(n, c4m_sym_decls);

    for (int i = 0; i < c4m_list_len(syms); i++) {
        c4m_tree_node_t *one_set   = c4m_list_get(syms, i, NULL);
        c4m_list_t      *var_names = c4m_apply_pattern_on_node(one_set,
                                                          c4m_sym_names);
        c4m_tree_node_t *type_node = c4m_get_match_on_node(one_set, c4m_sym_type);
        c4m_tree_node_t *init      = c4m_get_match_on_node(one_set, c4m_sym_init);
        c4m_type_t      *type      = NULL;

        if (type_node != NULL) {
            type = c4m_node_to_type(ctx->module_ctx, type_node, NULL);
        }
        else {
            type = c4m_new_typevar();
        }

        for (int j = 0; j < c4m_list_len(var_names); j++) {
            c4m_tree_node_t *name_node = c4m_list_get(var_names, j, NULL);
            c4m_utf8_t      *name      = c4m_node_text(name_node);
            c4m_symbol_t    *sym       = declare_sym(ctx,
                                            scope,
                                            name,
                                            name_node,
                                            C4M_SK_VARIABLE,
                                            NULL,
                                            true);
            sym->type                  = type;

            if (sym != NULL) {
                if (ctx->in_func && !is_global) {
                    sym->flags |= C4M_F_STACK_STORAGE | C4M_F_FUNCTION_SCOPE;
                }

                c4m_pnode_t *pn = c4m_get_pnode(name_node);
                pn->value       = (void *)sym;

                if (is_const) {
                    sym->flags |= C4M_F_DECLARED_CONST;
                }
                if (is_let) {
                    sym->flags |= C4M_F_DECLARED_LET;
                }

                if (init != NULL && j + 1 == c4m_list_len(var_names)) {
                    c4m_set_current_node(ctx, init);
                    process_children(ctx);
                    c4m_set_current_node(ctx, n);

                    sym->flags |= C4M_F_HAS_INITIALIZER;
                    sym->value_node = init;

                    c4m_pnode_t *initpn = c4m_get_pnode(init);

                    if (initpn->value == NULL) {
                        return;
                    }

                    c4m_type_t *inf_type = c4m_get_my_type(initpn->value);

                    sym->value = initpn->value;

                    sym->flags |= C4M_F_TYPE_IS_DECLARED;
                    int warning = 0;

                    if (!c4m_types_are_compat(inf_type, type, &warning)) {
                        c4m_add_error(ctx->module_ctx,
                                      c4m_err_inconsistent_type,
                                      name_node,
                                      inf_type,
                                      type,
                                      c4m_node_get_loc_str(type_node));
                    }
                }
                else {
                    if (type) {
                        sym->flags |= C4M_F_TYPE_IS_DECLARED;
                    }
                }
            }
        }
    }
}

static void
handle_param_block(c4m_pass1_ctx *ctx)
{
    // Reminder to self: make sure to check for not const in the decl.
    // That really needs to happen at the end of the pass through :)
    c4m_module_param_info_t *prop      = c4m_new_module_param();
    c4m_tree_node_t         *root      = c4m_cur_node(ctx);
    c4m_pnode_t             *pnode     = c4m_get_pnode(root);
    c4m_tree_node_t         *name_node = c4m_tree_get_child(root, 0);
    c4m_utf8_t              *sym_name  = c4m_node_text(name_node);
    c4m_utf8_t              *dot       = c4m_new_utf8(".");
    int                      nkids     = c4m_tree_get_number_children(root);
    c4m_symbol_t            *sym;
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
                                      c4m_node_text(
                                          c4m_tree_get_child(name_node, i)));
        }
    }
    else {
        attr = false;
    }

    for (int i = 1; i < nkids; i++) {
        c4m_tree_node_t *prop_node = c4m_tree_get_child(root, i);
        c4m_utf8_t      *prop_name = c4m_node_text(prop_node);
        c4m_obj_t        lit       = c4m_tree_get_child(prop_node, 0);

        switch (prop_name->data[0]) {
        case 'v':
            prop->validator = c4m_node_to_callback(ctx->module_ctx, lit);
            if (!prop->validator) {
                c4m_add_error(ctx->module_ctx,
                              c4m_err_spec_callback_required,
                              prop_node);
            }
            else {
                c4m_list_append(ctx->module_ctx->callback_literals,
                                prop->validator);
            }

            break;
        case 'c':
            prop->callback = c4m_node_to_callback(ctx->module_ctx, lit);
            if (!prop->callback) {
                c4m_add_error(ctx->module_ctx,
                              c4m_err_spec_callback_required,
                              prop_node);
            }
            else {
                c4m_list_append(ctx->module_ctx->callback_literals,
                                prop->callback);
            }
            break;
        case 'd':
            prop->default_value = lit;
            break;
        default:
            c4m_unreachable();
        }
    }

    if (!hatrack_dict_add(ctx->module_ctx->parameters, sym_name, prop)) {
        c4m_add_error(ctx->module_ctx, c4m_err_dupe_param, name_node);
        return;
    }

    if (attr) {
        sym = c4m_lookup_symbol(ctx->module_ctx->attribute_scope, sym_name);
        if (sym) {
            if (c4m_sym_is_declared_const(sym)) {
                c4m_add_error(ctx->module_ctx, c4m_err_const_param, name_node);
            }
        }
        else {
            sym = declare_sym(ctx,
                              ctx->module_ctx->attribute_scope,
                              sym_name,
                              name_node,
                              C4M_SK_ATTR,
                              NULL,
                              false);
        }
    }
    else {
        sym = c4m_lookup_symbol(ctx->module_ctx->module_scope, sym_name);
        if (sym) {
            if (c4m_sym_is_declared_const(sym)) {
                c4m_add_error(ctx->module_ctx, c4m_err_const_param, name_node);
            }
        }
        else {
            sym = declare_sym(ctx,
                              ctx->module_ctx->module_scope,
                              sym_name,
                              name_node,
                              C4M_SK_VARIABLE,
                              NULL,
                              false);
        }
    }
}

static void
one_section_prop(c4m_pass1_ctx      *ctx,
                 c4m_spec_section_t *section,
                 c4m_tree_node_t    *n)
{
    bool       *value;
    c4m_obj_t   callback;
    c4m_utf8_t *prop = c4m_node_text(n);

    switch (prop->data[0]) {
    case 'u': // user_def_ok
        value = c4m_node_simp_literal(c4m_tree_get_child(n, 0));

        if (!value || !obj_type_check(ctx, (c4m_obj_t)value, c4m_type_bool())) {
            c4m_add_error(ctx->module_ctx,
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
        value = c4m_node_simp_literal(c4m_tree_get_child(n, 0));
        if (!value || !obj_type_check(ctx, (c4m_obj_t)value, c4m_type_bool())) {
            c4m_add_error(ctx->module_ctx,
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
        callback = c4m_node_to_callback(ctx->module_ctx,
                                        c4m_tree_get_child(n, 0));

        if (!callback) {
            c4m_add_error(ctx->module_ctx,
                          c4m_err_spec_callback_required,
                          c4m_tree_get_child(n, 0));
        }
        else {
            section->validator = callback;
            c4m_list_append(ctx->module_ctx->callback_literals, callback);
        }
        break;
    case 'r': // require
        for (int i = 0; i < c4m_tree_get_number_children(n); i++) {
            c4m_utf8_t *name = c4m_node_text(c4m_tree_get_child(n, i));
            if (!c4m_set_add(section->required_sections, name)) {
                c4m_add_warning(ctx->module_ctx,
                                c4m_warn_dupe_require,
                                c4m_tree_get_child(n, i));
            }
            if (c4m_set_contains(section->allowed_sections, name)) {
                c4m_add_warning(ctx->module_ctx,
                                c4m_warn_require_allow,
                                c4m_tree_get_child(n, i));
            }
        }
        break;
    default: // allow
        for (int i = 0; i < c4m_tree_get_number_children(n); i++) {
            c4m_utf8_t *name = c4m_node_text(c4m_tree_get_child(n, i));
            if (!c4m_set_add(section->allowed_sections, name)) {
                c4m_add_warning(ctx->module_ctx,
                                c4m_warn_dupe_allow,
                                c4m_tree_get_child(n, i));
            }
            if (c4m_set_contains(section->required_sections, name)) {
                c4m_add_warning(ctx->module_ctx,
                                c4m_warn_require_allow,
                                c4m_tree_get_child(n, i));
            }
        }
    }
}

static void
one_field(c4m_pass1_ctx      *ctx,
          c4m_spec_section_t *section,
          c4m_tree_node_t    *tnode)
{
    c4m_spec_field_t *f        = c4m_new_spec_field();
    c4m_utf8_t       *name     = c4m_node_text(c4m_tree_get_child(tnode, 0));
    c4m_pnode_t      *pnode    = c4m_get_pnode(tnode);
    int               num_kids = c4m_tree_get_number_children(tnode);
    bool             *value;
    c4m_obj_t         callback;

    f->exclusions       = c4m_new(c4m_type_set(c4m_type_utf8()));
    f->name             = name;
    f->declaration_node = tnode;
    f->location_string  = c4m_format_module_location(ctx->module_ctx,
                                                    pnode->token);
    pnode->extra_info   = f;

    if (pnode->short_doc) {
        section->short_doc = c4m_token_raw_content(pnode->short_doc);

        if (pnode->long_doc) {
            section->long_doc = c4m_token_raw_content(pnode->long_doc);
        }
    }

    for (int i = 1; i < num_kids; i++) {
        c4m_tree_node_t *kid  = c4m_tree_get_child(tnode, i);
        c4m_utf8_t      *prop = c4m_node_text(kid);
        switch (prop->data[0]) {
        case 'c': // choice:
                  // For now, we just stash the raw nodes, and
                  // evaluate it later.
            f->stashed_options = c4m_tree_get_child(kid, 0);
            f->validate_choice = true;
            break;

        case 'd': // default:
                  // Same.
            f->default_value    = c4m_tree_get_child(kid, 0);
            f->default_provided = true;
            break;

        case 'h': // hidden
            value = c4m_node_simp_literal(c4m_tree_get_child(kid, 0));
            // clang-format off
            if (!value ||
		!obj_type_check(ctx, (c4m_obj_t)value, c4m_type_bool())) {
                c4m_add_error(ctx->module_ctx,
                              c4m_err_spec_bool_required,
                              c4m_tree_get_child(kid, 0));
                // clang-format on
            }
            else {
                if (*value) {
                    f->hidden = true;
                }
            }
            break;

        case 'l': // lock
            value = c4m_node_simp_literal(c4m_tree_get_child(kid, 0));
            // clang-format off
            if (!value ||
		!obj_type_check(ctx, (c4m_obj_t)value, c4m_type_bool())) {
                // clang-format on
                c4m_add_error(ctx->module_ctx,
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
                c4m_utf8_t *name = c4m_node_text(c4m_tree_get_child(kid, i));

                if (!c4m_set_add(f->exclusions, name)) {
                    c4m_add_warning(ctx->module_ctx,
                                    c4m_warn_dupe_exclusion,
                                    c4m_tree_get_child(kid, i));
                }
            }
            break;

        case 't': // type
            if (c4m_node_has_type(c4m_tree_get_child(kid, 0), c4m_nt_identifier)) {
                f->tinfo.type_pointer = c4m_node_text(c4m_tree_get_child(kid, 0));
            }
            else {
                f->tinfo.type = c4m_node_to_type(ctx->module_ctx,
                                                 c4m_tree_get_child(kid, 0),
                                                 NULL);
            }
            break;

        case 'v': // validator
            callback = c4m_node_to_callback(ctx->module_ctx,
                                            c4m_tree_get_child(kid, 0));

            if (!callback) {
                c4m_add_error(ctx->module_ctx,
                              c4m_err_spec_callback_required,
                              c4m_tree_get_child(kid, 0));
            }
            else {
                f->validator = callback;
                c4m_list_append(ctx->module_ctx->callback_literals, callback);
            }
            break;
        default:
            if (!strcmp(prop->data, "range")) {
                f->stashed_options = c4m_tree_get_child(kid, 0);
                f->validate_range  = true;
            }
            else {
                // required.
                value = c4m_node_simp_literal(c4m_tree_get_child(kid, 0));
                // clang-format off
                if (!value ||
		    !obj_type_check(ctx, (c4m_obj_t)value, c4m_type_bool())) {
                    c4m_add_error(ctx->module_ctx,
                                  c4m_err_spec_bool_required,
                                  c4m_tree_get_child(kid, 0));
                    // clang-format on
                }
                else {
                    if (*value) {
                        f->required = true;
                    }
                }
                break;
            }
        }
    }

    if (!hatrack_dict_add(section->fields, name, f)) {
        c4m_add_error(ctx->module_ctx, c4m_err_dupe_spec_field, tnode);
    }
}

static void
handle_section_spec(c4m_pass1_ctx *ctx)
{
    c4m_spec_t         *spec     = ctx->spec;
    c4m_spec_section_t *section  = c4m_new_spec_section();
    c4m_tree_node_t    *tnode    = c4m_cur_node(ctx);
    c4m_pnode_t        *pnode    = c4m_get_pnode(tnode);
    int                 ix       = 2;
    int                 num_kids = c4m_tree_get_number_children(tnode);

    section->declaration_node = tnode;
    section->location_string  = c4m_format_module_location(ctx->module_ctx,
                                                          pnode->token);

    if (pnode->short_doc) {
        section->short_doc = c4m_token_raw_content(pnode->short_doc);

        if (pnode->long_doc) {
            section->long_doc = c4m_token_raw_content(pnode->long_doc);
        }
    }

    c4m_utf8_t *kind = c4m_node_text(c4m_tree_get_child(tnode, 0));
    switch (kind->data[0]) {
    case 's': // singleton
        section->singleton = 1;
        // fallthrough
    case 'n': // named
        section->name = c4m_node_text(c4m_tree_get_child(tnode, 1));
        break;
    default: // root
        ix = 1;
        break;
    }

    for (; ix < num_kids; ix++) {
        c4m_tree_node_t *tkid = c4m_tree_get_child(tnode, ix);
        c4m_pnode_t     *pkid = c4m_get_pnode(tkid);

        if (pkid->kind == c4m_nt_section_prop) {
            one_section_prop(ctx, section, tkid);
        }
        else {
            one_field(ctx, section, tkid);
        }
    }

    if (section->name == NULL) {
        if (spec->in_use) {
            c4m_add_error(ctx->module_ctx,
                          c4m_err_dupe_root_section,
                          tnode);
        }
        else {
            spec->root_section = section;
        }
    }
    else {
        if (!hatrack_dict_add(spec->section_specs, section->name, section)) {
            c4m_add_error(ctx->module_ctx, c4m_err_dupe_section, tnode);
        }
    }
}

static void
handle_config_spec(c4m_pass1_ctx *ctx)
{
    c4m_tree_node_t *tnode = c4m_cur_node(ctx);
    c4m_pnode_t     *pnode = c4m_get_pnode(tnode);

    if (ctx->module_ctx->local_confspecs == NULL) {
        ctx->module_ctx->local_confspecs = c4m_new_spec();
    }
    else {
        c4m_add_error(ctx->module_ctx, c4m_err_dupe_section, tnode);
        return;
    }

    ctx->spec                   = ctx->module_ctx->local_confspecs;
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
    c4m_sig_info_t *result = c4m_new_sig_info();
    result->num_params     = num_params;

    if (result->num_params > 0) {
        result->param_info = c4m_gc_array_alloc(c4m_fn_param_info_t,
                                                num_params);
    }

    return result;
}

static c4m_sig_info_t *
extract_fn_sig_info(c4m_pass1_ctx   *ctx,
                    c4m_tree_node_t *tree)
{
    c4m_list_t     *decls     = c4m_apply_pattern_on_node(tree,
                                                  c4m_param_extraction);
    c4m_dict_t     *type_ctx  = c4m_dict(c4m_type_utf8(), c4m_type_ref());
    int             ndecls    = c4m_list_len(decls);
    int             nparams   = 0;
    int             cur_param = 0;
    c4m_list_t     *ptypes    = c4m_new(c4m_type_list(c4m_type_typespec()));
    c4m_sig_info_t *info;

    // Allocate space for parameters by counting how many variable
    // names we find.

    for (int i = 0; i < ndecls; i++) {
        c4m_tree_node_t *node  = c4m_list_get(decls, i, NULL);
        int              kidct = c4m_tree_get_number_children(node);

        if (kidct > 1) {
            c4m_tree_node_t *kid   = c4m_tree_get_child(node, kidct - 1);
            c4m_pnode_t     *pnode = c4m_get_pnode(kid);

            // Skip type specs.
            if (pnode->kind != c4m_nt_identifier) {
                kidct--;
            }
        }
        nparams += kidct;
    }

    info           = new_sig_info(nparams);
    info->fn_scope = c4m_new_scope(ctx->module_ctx->module_scope,
                                   C4M_SCOPE_FUNC);
    info->formals  = c4m_new_scope(ctx->module_ctx->module_scope,
                                  C4M_SCOPE_FORMALS);

    // Now, we loop through the parameter trees again. In function
    // declarations, named variables with omitted types are given a
    // type variable as a return type. Similarly, omitted return
    // values get a type variable.

    for (int i = 0; i < ndecls; i++) {
        c4m_tree_node_t *node     = c4m_list_get(decls, i, NULL);
        int              kidct    = c4m_tree_get_number_children(node);
        c4m_type_t      *type     = NULL;
        bool             got_type = false;

        if (kidct > 1) {
            c4m_tree_node_t *kid   = c4m_tree_get_child(node, kidct - 1);
            c4m_pnode_t     *pnode = c4m_get_pnode(kid);

            if (pnode->kind != c4m_nt_identifier) {
                type = c4m_node_to_type(ctx->module_ctx, kid, type_ctx);
                kidct--;
                got_type = true;
            }
        }

        // All but the last one in a subtree get type variables.
        for (int j = 0; j < kidct - 1; j++) {
            c4m_fn_param_info_t *pi  = &info->param_info[cur_param++];
            c4m_tree_node_t     *kid = c4m_tree_get_child(node, j);

            pi->name = c4m_node_text(kid);
            pi->type = c4m_new_typevar();

            declare_sym(ctx,
                        info->formals,
                        pi->name,
                        kid,
                        C4M_SK_FORMAL,
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
                        C4M_SK_VARIABLE,
                        NULL,
                        true);

            c4m_list_append(ptypes, pi->type);
        }

        // last item.
        if (!type) {
            type = c4m_new_typevar();
        }

        c4m_fn_param_info_t *pi  = &info->param_info[cur_param++];
        c4m_tree_node_t     *kid = c4m_tree_get_child(node, kidct - 1);
        pi->name                 = c4m_node_text(kid);
        pi->type                 = type;

        c4m_symbol_t *formal = declare_sym(ctx,
                                           info->formals,
                                           pi->name,
                                           kid,
                                           C4M_SK_FORMAL,
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
                    C4M_SK_VARIABLE,
                    NULL,
                    true);

        c4m_list_append(ptypes, pi->type ? pi->type : c4m_new_typevar());
    }

    c4m_tree_node_t *retnode = c4m_get_match_on_node(tree, c4m_return_extract);

    c4m_symbol_t *formal = declare_sym(ctx,
                                       info->formals,
                                       c4m_new_utf8("$result"),
                                       retnode,
                                       C4M_SK_FORMAL,
                                       NULL,
                                       true);
    if (retnode) {
        info->return_info.type = c4m_node_to_type(ctx->module_ctx,
                                                  retnode,
                                                  type_ctx);
        formal->type           = info->return_info.type;
        formal->flags |= C4M_F_TYPE_IS_DECLARED | C4M_F_REGISTER_STORAGE;
    }
    else {
        formal->type = c4m_new_typevar();
    }

    c4m_symbol_t *actual = declare_sym(ctx,
                                       info->fn_scope,
                                       c4m_new_utf8("$result"),
                                       ctx->cur_tnode,
                                       C4M_SK_VARIABLE,
                                       NULL,
                                       true);

    actual->flags = formal->flags;

    // Now fill out the 'local_type' field of the ffi decl.
    // TODO: support varargs.
    //
    // Note that this will get replaced once we do some checking.

    c4m_type_t *ret_for_sig = info->return_info.type;

    if (!ret_for_sig) {
        ret_for_sig = c4m_new_typevar();
    }

    info->full_type = c4m_type_fn(ret_for_sig, ptypes, false);
    return info;
}

static c4m_list_t *
c4m_get_func_mods(c4m_tree_node_t *tnode)
{
    c4m_list_t *result = c4m_list(c4m_type_tree(c4m_type_parse_node()));

    for (int i = 0; i < tnode->num_kids; i++) {
        c4m_list_append(result, tnode->children[i]);
    }

    return result;
}

static void
handle_func_decl(c4m_pass1_ctx *ctx)
{
    c4m_tree_node_t *tnode = c4m_cur_node(ctx);
    c4m_fn_decl_t   *decl  = c4m_new_fn_decl();
    c4m_utf8_t      *name  = c4m_node_text(tnode->children[1]);
    c4m_list_t      *mods  = c4m_get_func_mods(tnode->children[0]);
    int              nmods = c4m_list_len(mods);
    c4m_symbol_t    *sym;

    decl->signature_info = extract_fn_sig_info(ctx, c4m_cur_node(ctx));

    for (int i = 0; i < nmods; i++) {
        c4m_tree_node_t *mod_node = c4m_list_get(mods, i, NULL);
        switch (c4m_node_text(mod_node)->data[0]) {
        case 'p':
            decl->private = 1;
            break;
        default:
            decl->once = 1;
            break;
        }
    }

    sym = declare_sym(ctx,
                      ctx->module_ctx->module_scope,
                      name,
                      tnode,
                      C4M_SK_FUNC,
                      NULL,
                      true);

    if (sym) {
        sym->type  = decl->signature_info->full_type;
        sym->value = (void *)decl;
    }

    ctx->static_scope = decl->signature_info->fn_scope;

    c4m_pnode_t *pnode = c4m_get_pnode(tnode);
    pnode->value       = (c4m_obj_t)sym;

    ctx->in_func = true;
    c4m_node_down(ctx, tnode->num_kids - 1);
    process_children(ctx);
    c4m_node_up(ctx);
    ctx->in_func = false;
}

static void
handle_extern_block(c4m_pass1_ctx *ctx)
{
    c4m_ffi_decl_t  *info          = c4m_new_ffi_decl();
    c4m_utf8_t      *external_name = c4m_node_text(c4m_get_match(ctx,
                                                            c4m_first_kid_id));
    c4m_tree_node_t *ext_ret       = c4m_get_match(ctx, c4m_extern_return);
    c4m_tree_node_t *cur           = c4m_cur_node(ctx);
    c4m_tree_node_t *ext_pure      = c4m_get_match(ctx, c4m_find_pure);
    c4m_tree_node_t *ext_holds     = c4m_get_match(ctx, c4m_find_holds);
    c4m_tree_node_t *ext_allocs    = c4m_get_match(ctx, c4m_find_allocs);
    c4m_tree_node_t *csig          = c4m_cur_node(ctx)->children[1];
    c4m_tree_node_t *ext_lsig      = c4m_get_match(ctx, c4m_find_extern_local);
    c4m_tree_node_t *ext_box       = c4m_get_match(ctx, c4m_find_extern_box);
    c4m_pnode_t     *pnode         = c4m_get_pnode(cur);

    if (pnode->short_doc) {
        info->short_doc = c4m_token_raw_content(pnode->short_doc);

        if (pnode->long_doc) {
            info->long_doc = c4m_token_raw_content(pnode->long_doc);
        }
    }

    // Since we don't have a tree search primitive to collect
    // all nodes of a type yet, we do this.
    for (int i = 2; i < cur->num_kids; i++) {
        c4m_pnode_t *kid = c4m_get_pnode(cur->children[i]);

        if (kid->kind == c4m_nt_extern_dll) {
            if (info->dll_list == NULL) {
                info->dll_list = c4m_list(c4m_type_utf8());
            }
            c4m_utf8_t *s = c4m_node_text(cur->children[i]->children[0]);
            c4m_list_append(info->dll_list, s);
        }
    }

    int64_t n            = csig->num_kids - 1;
    info->num_ext_params = n;
    info->external_name  = external_name;

    if (n) {
        info->external_params = c4m_gc_array_alloc(uint8_t, n);

        for (int64_t i = 0; i < n; i++) {
            c4m_tree_node_t *tnode = csig->children[i];
            c4m_pnode_t     *pnode = c4m_tree_get_contents(tnode);
            uint64_t         val   = (uint64_t)pnode->extra_info;

            info->external_params[i] = (uint8_t)val;
        }
    }

    if (ext_ret) {
        c4m_pnode_t *pnode = c4m_get_pnode(ext_ret);
        uint64_t     val   = (uint64_t)pnode->extra_info;

        info->external_return_type = (uint8_t)val;
    }

    info->local_params = extract_fn_sig_info(ctx, ext_lsig);

    if (c4m_type_is_void(
            c4m_type_get_last_param(info->local_params->full_type))) {
        info->local_params->void_return = 1;
    }

    if (ext_pure) {
        bool *pure_ptr = c4m_node_simp_literal(c4m_tree_get_child(ext_pure, 0));

        if (pure_ptr && *pure_ptr) {
            info->local_params->pure = 1;
        }
    }

    info->skip_boxes = false;

    if (ext_box) {
        bool *box_ptr = c4m_node_simp_literal(c4m_tree_get_child(ext_box, 0));

        if (box_ptr && !*box_ptr) {
            info->skip_boxes = true;
        }
    }

    info->local_name = c4m_node_text(c4m_get_match_on_node(ext_lsig,
                                                           c4m_first_kid_id));

    if (ext_holds) {
        if (info->local_params == NULL) {
            c4m_add_error(ctx->module_ctx, c4m_err_no_params_to_hold, ext_holds);
            return;
        }

        uint64_t        bitfield  = 0;
        c4m_sig_info_t *si        = info->local_params;
        int             num_holds = c4m_tree_get_number_children(ext_holds);

        for (int i = 0; i < num_holds; i++) {
            c4m_tree_node_t *kid = c4m_tree_get_child(ext_holds, i);
            c4m_utf8_t      *txt = c4m_node_text(kid);

            for (int j = 0; j < si->num_params; j++) {
                c4m_fn_param_info_t *param = &si->param_info[j];
                if (strcmp(txt->data, param->name->data)) {
                    continue;
                }
                param->ffi_holds = 1;
                if (j < 64) {
                    uint64_t flag = (uint64_t)(1 << j);
                    if (bitfield & flag) {
                        c4m_add_warning(ctx->module_ctx, c4m_warn_dupe_hold, kid);
                    }
                    bitfield |= flag;
                }
                goto next_i;
            }
            c4m_add_error(ctx->module_ctx, c4m_err_bad_hold_name, kid);
            break;
next_i:
    /* nothing. */;
        }
        info->cif.hold_info = bitfield;
    }

    if (ext_allocs) {
        uint64_t        bitfield   = 0;
        bool            got_ret    = false;
        c4m_sig_info_t *si         = info->local_params;
        int             num_allocs = c4m_tree_get_number_children(ext_allocs);

        for (int i = 0; i < num_allocs; i++) {
            c4m_tree_node_t *kid = c4m_tree_get_child(ext_allocs, i);
            c4m_utf8_t      *txt = c4m_node_text(kid);

            if (!strcmp(txt->data, "return")) {
                if (got_ret) {
                    c4m_add_warning(ctx->module_ctx, c4m_warn_dupe_alloc, kid);
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
                if (j < 63) {
                    uint64_t flag = (uint64_t)(1 << j);
                    if (bitfield & flag) {
                        c4m_add_warning(ctx->module_ctx,
                                        c4m_warn_dupe_alloc,
                                        kid);
                    }
                    bitfield |= flag;
                }
                goto next_alloc;
            }
            c4m_add_error(ctx->module_ctx, c4m_err_bad_alloc_name, kid);
            break;
next_alloc:
    /* nothing. */;
        }
        info->cif.alloc_info = bitfield;
    }

    c4m_symbol_t *sym = declare_sym(ctx,
                                    ctx->module_ctx->module_scope,
                                    info->local_name,
                                    c4m_get_match(ctx, c4m_first_kid_id),
                                    C4M_SK_EXTERN_FUNC,
                                    NULL,
                                    true);

    if (sym) {
        sym->type  = info->local_params->full_type;
        sym->value = (void *)info;
    }

    c4m_list_append(ctx->module_ctx->extern_decls, sym);
}

static c4m_list_t *
get_member_prefix(c4m_tree_node_t *n)
{
    c4m_list_t *result = c4m_list(c4m_type_tree(c4m_type_parse_node()));

    for (int i = 0; i < n->num_kids - 1; i++) {
        c4m_list_append(result, n->children[i]);
    }

    return result;
}

static void
handle_use_stmt(c4m_pass1_ctx *ctx)
{
    c4m_tree_node_t        *unode   = c4m_get_match(ctx, c4m_use_uri);
    c4m_tree_node_t        *modnode = c4m_get_match(ctx, c4m_member_last);
    c4m_list_t             *prefix  = get_member_prefix(ctx->cur_tnode->children[0]);
    bool                    status  = false;
    c4m_utf8_t             *modname = c4m_node_text(modnode);
    c4m_utf8_t             *package = NULL;
    c4m_utf8_t             *uri     = NULL;
    c4m_pnode_t            *pnode   = c4m_get_pnode(ctx->cur_tnode);
    c4m_module_compile_ctx *mi;

    if (c4m_list_len(prefix) != 0) {
        package = c4m_node_list_join(prefix, c4m_utf32_repeat('.', 1), false);
    }

    if (unode) {
        uri = c4m_node_simp_literal(unode);
    }

    mi = c4m_find_module(ctx->cctx,
                         uri,
                         modname,
                         package,
                         ctx->module_ctx->package,
                         ctx->module_ctx->path,
                         NULL);

    pnode->value = (void *)mi;

    if (!mi) {
        if (package != NULL) {
            modname = c4m_cstr_format("{}.{}", package, modname);
        }

        c4m_add_error(ctx->module_ctx,
                      c4m_err_search_path,
                      ctx->cur_tnode,
                      modname);
        return;
    }

    c4m_add_module_to_worklist(ctx->cctx, mi);

    c4m_symbol_t *sym = declare_sym(ctx,
                                    ctx->module_ctx->imports,
                                    c4m_module_fully_qualified(mi),
                                    c4m_cur_node(ctx),
                                    C4M_SK_MODULE,
                                    &status,
                                    false);

    if (!status) {
        c4m_add_info(ctx->module_ctx,
                     c4m_info_dupe_import,
                     c4m_cur_node(ctx));
    }
    else {
        sym->value = mi;
    }
}

static void
look_for_dead_code(c4m_pass1_ctx *ctx)
{
    c4m_tree_node_t *cur    = c4m_cur_node(ctx);
    c4m_tree_node_t *parent = cur->parent;

    if (parent->num_kids > 1) {
        if (parent->children[parent->num_kids - 1] != cur) {
            c4m_add_warning(ctx->module_ctx, c4m_warn_dead_code, cur);
        }
    }
}

static void
pass_dispatch(c4m_pass1_ctx *ctx)
{
    c4m_scope_t *saved_scope;
    c4m_pnode_t *pnode  = c4m_get_pnode(c4m_cur_node(ctx));
    pnode->static_scope = ctx->static_scope;

    switch (c4m_cur_node_type(ctx)) {
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

    case c4m_nt_break:
    case c4m_nt_continue:
    case c4m_nt_return:
        look_for_dead_code(ctx);
        process_children(ctx);
        break;
    default:
        process_children(ctx);
        break;
    }
}

static void
find_dependencies(c4m_compile_ctx *cctx, c4m_module_compile_ctx *module_ctx)
{
    c4m_scope_t          *imports = module_ctx->imports;
    uint64_t              len     = 0;
    hatrack_dict_value_t *values  = hatrack_dict_values(imports->symbols,
                                                       &len);

    for (uint64_t i = 0; i < len; i++) {
        c4m_symbol_t           *sym = values[i];
        c4m_module_compile_ctx *mi  = sym->value;
        c4m_tree_node_t        *n   = sym->declaration_node;
        c4m_pnode_t            *pn  = c4m_get_pnode(n);

        pn->value = (c4m_obj_t)mi;

        if (c4m_set_contains(cctx->processed, mi)) {
            continue;
        }
    }
}

void
c4m_module_decl_pass(c4m_compile_ctx *cctx, c4m_module_compile_ctx *module_ctx)
{
    if (c4m_fatal_error_in_module(module_ctx)) {
        return;
    }

    if (module_ctx->status >= c4m_compile_status_code_loaded) {
        return;
    }

    if (module_ctx->status != c4m_compile_status_code_parsed) {
        C4M_CRAISE("Cannot extract declarations for code that is not parsed.");
    }

    c4m_setup_treematch_patterns();

    c4m_pass1_ctx ctx = {
        .module_ctx = module_ctx,
        .cctx       = cctx,
    };

    c4m_set_current_node(&ctx, module_ctx->parse_tree);

    module_ctx->global_scope      = c4m_new_scope(NULL, C4M_SCOPE_GLOBAL);
    module_ctx->module_scope      = c4m_new_scope(module_ctx->global_scope,
                                             C4M_SCOPE_MODULE);
    module_ctx->attribute_scope   = c4m_new_scope(NULL, C4M_SCOPE_ATTRIBUTES);
    module_ctx->imports           = c4m_new_scope(NULL, C4M_SCOPE_IMPORTS);
    module_ctx->parameters        = c4m_new(c4m_type_dict(c4m_type_utf8(),
                                                   c4m_type_ref()));
    module_ctx->fn_def_syms       = c4m_new(c4m_type_list(c4m_type_ref()));
    module_ctx->callback_literals = c4m_new(c4m_type_list(c4m_type_ref()));
    module_ctx->extern_decls      = c4m_new(c4m_type_list(c4m_type_ref()));

    ctx.cur->static_scope = module_ctx->module_scope;
    ctx.static_scope      = module_ctx->module_scope;

    c4m_pnode_t *pnode = c4m_get_pnode(module_ctx->parse_tree);

    if (pnode->short_doc) {
        module_ctx->short_doc = c4m_token_raw_content(pnode->short_doc);

        if (pnode->long_doc) {
            module_ctx->long_doc = c4m_token_raw_content(pnode->long_doc);
        }
    }

    pass_dispatch(&ctx);
    find_dependencies(cctx, module_ctx);
    if (module_ctx->fatal_errors) {
        cctx->fatality = true;
    }

    c4m_module_set_status(module_ctx, c4m_compile_status_code_loaded);

    return;
}

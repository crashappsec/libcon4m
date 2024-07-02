#define C4M_USE_INTERNAL_API
#include "con4m.h"

// clang-format off
char *c4m_symbol_kind_names[C4M_SK_NUM_SYM_KINDS] = {
    [C4M_SK_MODULE]      = "module",
    [C4M_SK_FUNC]        = "function",
    [C4M_SK_EXTERN_FUNC] = "extern function",
    [C4M_SK_ENUM_TYPE]   = "enum type",
    [C4M_SK_ENUM_VAL]    = "enum value",
    [C4M_SK_ATTR]        = "attribute",
    [C4M_SK_VARIABLE]    = "variable",
    [C4M_SK_FORMAL]      = "function parameter",
};
// clang-format on

c4m_scope_t *
c4m_new_scope(c4m_scope_t *parent, c4m_scope_kind kind)
{
    c4m_scope_t *result = c4m_gc_alloc(c4m_scope_t);
    result->parent      = parent;
    result->symbols     = c4m_new(c4m_type_dict(c4m_type_utf8(),
                                            c4m_type_ref()));
    result->kind        = kind;

    return result;
}

c4m_utf8_t *
c4m_sym_get_best_ref_loc(c4m_symbol_t *sym)
{
    c4m_tree_node_t *node = sym->declaration_node;

    if (node == NULL && sym->sym_defs != NULL) {
        node = c4m_list_get(sym->sym_defs, 0, NULL);
    }

    if (node == NULL && sym->sym_uses != NULL) {
        node = c4m_list_get(sym->sym_defs, 0, NULL);
    }

    if (node) {
        return c4m_node_get_loc_str(node);
    }

    c4m_unreachable();
}

void
c4m_shadow_check(c4m_file_compile_ctx *ctx,
                 c4m_symbol_t    *sym,
                 c4m_scope_t          *cur_scope)
{
    c4m_scope_t *module_scope = ctx->module_scope;
    c4m_scope_t *global_scope = ctx->global_scope;
    c4m_scope_t *attr_scope   = ctx->attribute_scope;

    c4m_symbol_t *in_module = NULL;
    c4m_symbol_t *in_global = NULL;
    c4m_symbol_t *in_attr   = NULL;

    if (module_scope && module_scope != cur_scope) {
        in_module = hatrack_dict_get(module_scope->symbols, sym->name, NULL);
    }
    if (global_scope && global_scope != cur_scope) {
        in_global = hatrack_dict_get(global_scope->symbols, sym->name, NULL);
    }
    if (attr_scope && attr_scope != cur_scope) {
        in_attr = hatrack_dict_get(attr_scope->symbols, sym->name, NULL);
    }

    if (in_module) {
        if (cur_scope == global_scope) {
            c4m_add_error(ctx,
                          c4m_err_decl_mask,
                          sym->declaration_node,
                          c4m_new_utf8("global"),
                          c4m_new_utf8("module"),
                          c4m_node_get_loc_str(in_module->declaration_node));
            return;
        }

        c4m_add_error(ctx,
                      c4m_err_attr_mask,
                      sym->declaration_node,
                      c4m_node_get_loc_str(in_module->declaration_node));
        return;
    }

    if (in_global) {
        if (cur_scope == module_scope) {
            c4m_add_error(ctx,
                          c4m_err_decl_mask,
                          sym->declaration_node,
                          c4m_new_utf8("module"),
                          c4m_new_utf8("global"),
                          c4m_node_get_loc_str(in_global->declaration_node));
            return;
        }
        c4m_add_error(ctx,
                      c4m_err_attr_mask,
                      sym->declaration_node,
                      c4m_node_get_loc_str(in_global->declaration_node));
        return;
    }

    if (in_attr) {
        if (cur_scope == module_scope) {
            c4m_add_warning(ctx, c4m_warn_attr_mask, sym->declaration_node);
        }
        else {
            c4m_add_error(ctx, c4m_err_attr_mask, sym->declaration_node);
        }
    }
}
c4m_symbol_t *
c4m_declare_symbol(c4m_file_compile_ctx *ctx,
                   c4m_scope_t          *scope,
                   c4m_utf8_t           *name,
                   c4m_tree_node_t      *node,
                   c4m_symbol_kind       kind,
                   bool                 *success,
                   bool                  err_if_present)
{
    c4m_symbol_t *entry = c4m_gc_alloc(c4m_symbol_t);
    entry->path              = c4m_to_utf8(ctx->path);
    entry->name              = name;
    entry->declaration_node  = node;
    entry->type              = c4m_new_typevar();
    entry->kind              = kind;
    entry->my_scope          = scope;
    entry->sym_defs          = c4m_new(c4m_type_list(c4m_type_ref()));

    if (hatrack_dict_add(scope->symbols, name, entry)) {
        if (success != NULL) {
            *success = true;
        }

        switch (kind) {
        case C4M_SK_ATTR:
        case C4M_SK_VARIABLE:
            break;
        default:
            c4m_list_append(entry->sym_defs, node);
        }

        return entry;
    }

    c4m_symbol_t *old = hatrack_dict_get(scope->symbols, name, NULL);

    if (success != NULL) {
        *success = false;
    }

    if (!err_if_present) {
        return old;
    }

    c4m_add_error(ctx,
                  c4m_err_invalid_redeclaration,
                  node,
                  name,
                  c4m_sym_kind_name(old),
                  c4m_node_get_loc_str(old->declaration_node));

    return old;
}

c4m_symbol_t *
c4m_add_inferred_symbol(c4m_file_compile_ctx *ctx,
                        c4m_scope_t          *scope,
                        c4m_utf8_t           *name)
{
    c4m_symbol_t *entry = c4m_gc_alloc(c4m_symbol_t);
    entry->name              = name;
    entry->type              = c4m_new_typevar();
    entry->kind              = C4M_SK_VARIABLE;
    entry->my_scope          = scope;

    if (scope->kind & (C4M_SCOPE_FUNC | C4M_SCOPE_FORMALS)) {
        entry->flags |= C4M_F_FUNCTION_SCOPE;
    }

    if (!hatrack_dict_add(scope->symbols, name, entry)) {
        C4M_CRAISE(
            "c4m_add_inferred_symbol must only be called if the symbol "
            "is not already in the symbol table.");
    }

    return entry;
}

c4m_symbol_t *
c4m_add_or_replace_symbol(c4m_file_compile_ctx *ctx,
                          c4m_scope_t          *scope,
                          c4m_utf8_t           *name)
{
    c4m_symbol_t *entry = c4m_gc_alloc(c4m_symbol_t);
    entry->name              = name;
    entry->type              = c4m_new_typevar();
    entry->kind              = C4M_SK_VARIABLE;
    entry->my_scope          = scope;

    hatrack_dict_put(scope->symbols, name, entry);

    return entry;
}

// There's also a "c4m_symbol_lookup" below. We should resolve
// these names by deleting this and rewriting any call sites (TODO).
c4m_symbol_t *
c4m_lookup_symbol(c4m_scope_t *scope, c4m_utf8_t *name)
{
    return hatrack_dict_get(scope->symbols, name, NULL);
}

// Used for declaration comparisons. use_declared_type is passed when
// the ORIGINAL symbol had a declared type. If it doesn't, then we are
// comparing against a third place, which had an explicit decl, which
// will have the type stored in inferred_type.

static void
type_cmp_exact_match(c4m_file_compile_ctx *new_ctx,
                     c4m_symbol_t    *new_sym,
                     c4m_symbol_t    *old_sym)
{
    c4m_type_t *t1 = new_sym->type;
    c4m_type_t *t2 = old_sym->type;

    switch (c4m_type_cmp_exact(t1, t2)) {
    case c4m_type_match_exact:
        // Link any type variables together now.
        old_sym->type = c4m_merge_types(t1, t2, NULL);
        return;
    case c4m_type_match_left_more_specific:
        // Even if the authoritative symbol did not have a declared
        // type, it got the field set whenever some other version of
        // the symbol explicitly declared. So it's the previous
        // location to complain about.
        c4m_add_error(new_ctx,
                      c4m_err_redecl_neq_generics,
                      new_sym->declaration_node,
                      new_sym->name,
                      c4m_new_utf8("a less generic / more concrete"),
                      c4m_value_obj_repr(t2),
                      c4m_value_obj_repr(t1),
                      c4m_node_get_loc_str(old_sym->declaration_node));
        return;
    case c4m_type_match_right_more_specific:
        c4m_add_error(new_ctx,
                      c4m_err_redecl_neq_generics,
                      new_sym->declaration_node,
                      new_sym->name,
                      c4m_new_utf8("a more generic / less concrete"),
                      c4m_value_obj_repr(t2),
                      c4m_value_obj_repr(t1),
                      c4m_node_get_loc_str(old_sym->declaration_node));
        return;
    case c4m_type_match_both_have_more_generic_bits:
        c4m_add_error(new_ctx,
                      c4m_err_redecl_neq_generics,
                      new_sym->declaration_node,
                      new_sym->name,
                      c4m_new_utf8("a type with different generic parts"),
                      c4m_value_obj_repr(t2),
                      c4m_value_obj_repr(t1),
                      c4m_node_get_loc_str(old_sym->declaration_node));
        return;
    case c4m_type_cant_match:
        c4m_add_error(new_ctx,
                      c4m_err_redecl_neq_generics,
                      new_sym->declaration_node,
                      new_sym->name,
                      c4m_new_utf8("a completely incompatible"),
                      c4m_value_obj_repr(t2),
                      c4m_value_obj_repr(t1),
                      c4m_node_get_loc_str(old_sym->declaration_node));
        return;
    }
}

// This is meant for statically merging global symbols that exist in
// multiple modules, etc.
//
// This happens BEFORE any type inferencing happens, so we only need
// to compare when exact types are provided.

bool
c4m_merge_symbols(c4m_file_compile_ctx *ctx1,
                  c4m_symbol_t    *sym1,
                  c4m_symbol_t    *sym2) // The older symbol.
{
    if (sym1->kind != sym2->kind) {
        c4m_add_error(ctx1,
                      c4m_err_redecl_kind,
                      sym1->declaration_node,
                      sym1->name,
                      c4m_sym_kind_name(sym2),
                      c4m_sym_kind_name(sym1),
                      c4m_node_get_loc_str(sym2->declaration_node));
        return false;
    }

    switch (sym1->kind) {
    case C4M_SK_FUNC:
    case C4M_SK_EXTERN_FUNC:
    case C4M_SK_ENUM_TYPE:
    case C4M_SK_ENUM_VAL:
        c4m_add_error(ctx1,
                      c4m_err_no_redecl,
                      sym1->declaration_node,
                      sym1->name,
                      c4m_sym_kind_name(sym1),
                      c4m_node_get_loc_str(sym2->declaration_node));
        return false;

    case C4M_SK_VARIABLE:
        if (!c4m_type_is_error(sym1->type)) {
            type_cmp_exact_match(ctx1, sym1, sym2);
            if (!sym2->type_declaration_node) {
                if (c4m_type_is_declared(sym2)) {
                    sym2->type_declaration_node = sym2->declaration_node;
                }
                else {
                    if (c4m_type_is_declared(sym1)) {
                        sym2->type_declaration_node = sym1->declaration_node;
                    }
                }
            }
        }
        return true;
    default:
        // For instance, we never call this on scopes
        // that hold C4M_SK_MODULE symbols or C4M_SK_FORMAL symbols.
        c4m_unreachable();
    }
}

#define one_scope_lookup(scopevar, name)                          \
    if (scopevar != NULL) {                                       \
        result = hatrack_dict_get(scopevar->symbols, name, NULL); \
        if (result) {                                             \
            return result;                                        \
        }                                                         \
    }

c4m_symbol_t *
c4m_symbol_lookup(c4m_scope_t *local_scope,
                  c4m_scope_t *module_scope,
                  c4m_scope_t *global_scope,
                  c4m_scope_t *attr_scope,
                  c4m_utf8_t  *name)
{
    c4m_symbol_t *result = NULL;

    one_scope_lookup(local_scope, name);
    one_scope_lookup(module_scope, name);
    one_scope_lookup(global_scope, name);
    one_scope_lookup(attr_scope, name);

    return NULL;
}

c4m_grid_t *
c4m_format_scope(c4m_scope_t *scope)
{
    uint64_t              len;
    hatrack_dict_value_t *values;
    c4m_grid_t           *grid       = c4m_new(c4m_type_grid(),
                               c4m_kw("start_cols",
                                      c4m_ka(6),
                                      "header_rows",
                                      c4m_ka(1),
                                      "stripe",
                                      c4m_ka(true)));
    c4m_list_t          *row        = c4m_new_table_row();
    c4m_utf8_t           *decl_const = c4m_new_utf8("declared");
    c4m_utf8_t           *inf_const  = c4m_new_utf8("inferred");
    c4m_dict_t           *memos      = c4m_new(c4m_type_dict(c4m_type_ref(),
                                              c4m_type_utf8()));
    int64_t               nexttid    = 0;

    values = hatrack_dict_values_sort(scope->symbols,
                                      &len);

    if (len == 0) {
        grid = c4m_new(c4m_type_grid(), c4m_kw("start_cols", c4m_ka(1)));
        c4m_list_append(row, c4m_new_utf8("Scope is empty"));
        c4m_grid_add_row(grid, row);
        c4m_set_column_style(grid, 0, "full_snap");
        return grid;
    }

    c4m_list_append(row, c4m_new_utf8("Name"));
    c4m_list_append(row, c4m_new_utf8("Kind"));
    c4m_list_append(row, c4m_new_utf8("Type"));
    c4m_list_append(row, c4m_new_utf8("Offset"));
    c4m_list_append(row, c4m_new_utf8("Defs"));
    c4m_list_append(row, c4m_new_utf8("Uses"));
    c4m_grid_add_row(grid, row);

    for (uint64_t i = 0; i < len; i++) {
        c4m_utf8_t        *kind;
        c4m_symbol_t *entry = values[i];

        kind = inf_const;

        if (c4m_type_is_declared(entry) || entry->kind == C4M_SK_EXTERN_FUNC) {
            kind = decl_const;
        }
        row = c4m_new_table_row();

        c4m_list_append(row, entry->name);

        if (entry->kind == C4M_SK_VARIABLE) {
            if (entry->flags & C4M_F_DECLARED_CONST) {
                c4m_list_append(row, c4m_new_utf8("const"));
            }
            else {
                if (entry->flags & C4M_F_USER_IMMUTIBLE) {
                    c4m_list_append(row, c4m_new_utf8("loop var"));
                }
                else {
                    c4m_list_append(row,
                                     c4m_new_utf8(
                                         c4m_symbol_kind_names[entry->kind]));
                }
            }
        }
        else {
            c4m_list_append(row,
                             c4m_new_utf8(c4m_symbol_kind_names[entry->kind]));
        }

        c4m_type_t *symtype = c4m_get_sym_type(entry);
        symtype             = c4m_type_resolve(symtype);

        if (c4m_type_is_box(symtype)) {
            symtype = c4m_type_unbox(symtype);
        }

        c4m_list_append(row,
                         c4m_cstr_format("[em]{} [/][i]({})",
                                         c4m_internal_type_repr(symtype,
                                                                memos,
                                                                &nexttid),
                                         kind));

#if 0
        if (c4m_sym_is_declared_const(entry)) {
            if (entry->value == NULL) {
                c4m_list_append(row, c4m_cstr_format("[red]not set[/]"));
            }
            else {
                c4m_list_append(row, c4m_value_obj_repr(entry->value));
            }
        }
        else {
            c4m_list_append(row, c4m_cstr_format("[gray]n/a[/]"));
        }
#else
        c4m_list_append(row,
                         c4m_cstr_format("{:x}",
                                         c4m_box_u64(entry->static_offset)));
#endif

        int         n = c4m_list_len(entry->sym_defs);
        c4m_utf8_t *def_text;

        if (n == 0) {
            def_text = c4m_cstr_format("[gray]none[/]");
        }
        else {
            c4m_list_t *defs = c4m_new(c4m_type_list(c4m_type_utf8()));
            for (int i = 0; i < n; i++) {
                c4m_tree_node_t *t = c4m_list_get(entry->sym_defs, i, NULL);
                if (t == NULL) {
                    c4m_list_append(defs, c4m_new_utf8("??"));
                    continue;
                }
                c4m_list_append(defs, c4m_node_get_loc_str(t));
            }
            def_text = c4m_str_join(defs, c4m_new_utf8(", "));
        }
        c4m_list_append(row, def_text);

        n = c4m_list_len(entry->sym_uses);
        c4m_utf8_t *use_text;

        if (n == 0) {
            use_text = c4m_cstr_format("[gray]none[/]");
        }
        else {
            c4m_list_t *uses = c4m_new(c4m_type_list(c4m_type_utf8()));
            for (int i = 0; i < n; i++) {
                c4m_tree_node_t *t = c4m_list_get(entry->sym_uses, i, NULL);
                if (t == NULL) {
                    c4m_list_append(uses, c4m_new_utf8("??"));
                    continue;
                }
                c4m_list_append(uses, c4m_node_get_loc_str(t));
            }
            use_text = c4m_str_join(uses, c4m_new_utf8(", "));
        }
        c4m_list_append(row, use_text);
        c4m_grid_add_row(grid, row);
    }

    c4m_set_column_style(grid, 0, "snap");
    c4m_set_column_style(grid, 1, "snap");
    c4m_set_column_style(grid, 2, "snap");
    c4m_set_column_style(grid, 3, "snap");

    return grid;
}

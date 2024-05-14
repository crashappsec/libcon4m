#define C4M_USE_INTERNAL_API
#include "con4m.h"

// clang-format off
static char *symbol_kind_names[sk_num_sym_kinds] = {
    [sk_module]      = "module",
    [sk_func]        = "function",
    [sk_extern_func] = "extern function",
    [sk_enum_type]   = "enum type",
    [sk_enum_val]    = "enum value",
    [sk_attr]        = "attribute",
    [sk_variable]    = "variable",
    [sk_formal]      = "function parameter",
};
// clang-format on

static inline c4m_utf8_t *
sym_kind_name(c4m_scope_entry_t *sym)
{
    return c4m_new_utf8(symbol_kind_names[sym->kind]);
}

static inline int32_t *
sym_line_no(const c4m_scope_entry_t *sym)
{
    c4m_pnode_t *pnode = c4m_tree_get_contents(sym->declaration_node);
    c4m_token_t *tok   = pnode->token;

    return c4m_box_i32(tok->line_no);
}

static inline int32_t *
sym_line_offset(const c4m_scope_entry_t *sym)
{
    c4m_pnode_t *pnode = c4m_tree_get_contents(sym->declaration_node);
    c4m_token_t *tok   = pnode->token;

    return c4m_box_i32(tok->line_offset + 1);
}

c4m_scope_t *
c4m_new_scope(c4m_scope_t *parent, c4m_scope_kind kind)
{
    c4m_scope_t *result = c4m_gc_alloc(c4m_scope_t);
    result->parent      = parent;
    result->symbols     = c4m_new(c4m_tspec_dict(c4m_tspec_utf8(),
                                             c4m_tspec_ref()));
    result->kind        = kind;
    return result;
}

c4m_scope_entry_t *
c4m_declare_symbol(c4m_file_compile_ctx *ctx,
                   c4m_scope_t          *scope,
                   c4m_utf8_t           *name,
                   c4m_tree_node_t      *node,
                   c4m_symbol_kind       kind,
                   bool                 *success,
                   bool                  err_if_present)
{
    c4m_scope_entry_t *entry = c4m_gc_alloc(c4m_scope_entry_t);
    entry->path              = c4m_to_utf8(ctx->path);
    entry->name              = name;
    entry->declaration_node  = node;
    entry->declared_type     = c4m_tspec_error();
    entry->inferred_type     = c4m_tspec_typevar();
    entry->kind              = kind;
    entry->my_scope          = scope;

    if (hatrack_dict_add(scope->symbols, name, entry)) {
        if (success != NULL) {
            *success = true;
        }

        return entry;
    }

    c4m_scope_entry_t *old = hatrack_dict_get(scope->symbols, name, NULL);

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
                  sym_kind_name(old),
                  old->path,
                  sym_line_no(old),
                  sym_line_offset(old));

    return old;
}

c4m_scope_entry_t *
c4m_add_if_missing(c4m_file_compile_ctx *ctx,
                   c4m_utf8_t           *name,
                   c4m_tree_node_t      *node,
                   c4m_symbol_kind       kind,
                   bool                  err_on_conflict)
{
    return NULL;
}

c4m_scope_entry_t *
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
                     c4m_scope_entry_t    *new_sym,
                     c4m_scope_entry_t    *old_sym,
                     bool                  use_declared_type)
{
    c4m_type_t *t1 = new_sym->declared_type;
    c4m_type_t *t2;

    if (use_declared_type == true) {
        t2 = old_sym->declared_type;
    }
    else {
        t2 = old_sym->inferred_type;
    }

    switch (c4m_type_cmp_exact(t1, t2)) {
    case c4m_type_match_exact:
        // Link any type variables together now.
        old_sym->inferred_type = c4m_merge_types(t1, t2);
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
                  c4m_scope_entry_t    *sym1,
                  c4m_scope_entry_t    *sym2)
{
    if (sym1->kind != sym2->kind) {
        c4m_add_error(ctx1,
                      c4m_err_redecl_kind,
                      sym1->declaration_node,
                      sym1->name,
                      sym_kind_name(sym2),
                      sym_kind_name(sym1),
                      c4m_node_get_loc_str(sym2->declaration_node));
        return false;
    }

    switch (sym1->kind) {
    case sk_func:
    case sk_extern_func:
    case sk_enum_type:
    case sk_enum_val:
        c4m_add_error(ctx1,
                      c4m_err_no_redecl,
                      sym1->declaration_node,
                      sym1->name,
                      sym_kind_name(sym1),
                      c4m_node_get_loc_str(sym2->declaration_node));
        return false;

    case sk_variable:
        if (sym1->declared_type != c4m_tspec_error()) {
            if (sym2->declared_type != c4m_tspec_error()) {
                type_cmp_exact_match(ctx1, sym1, sym2, true);
            }
            else {
                if (sym2->declaration_node != NULL) {
                    type_cmp_exact_match(ctx1, sym1, sym2, false);
                    // Track the most recent declaration in case
                    // other declarations fail.
                    sym2->declaration_node = sym1->declaration_node;
                }
                else {
                    // Same; sym2 was chosen as authoritative, but
                    // it didn't have a type. So if there's ever an error
                    // complaining about a type relative to the declared
                    // type, since there isn't a base declaration, we
                    // just give the most recent one, and keep that
                    // in declaration_node.
                    sym2->inferred_type    = sym1->declared_type;
                    sym2->declaration_node = sym1->declaration_node;
                }
            }
        }

        sym1->authoritative_symbol = sym2;

        return true;
    default:
        // For instance, we never call this on scopes
        // that hold sk_module symbols or sk_formal symbols.
        unreachable();
    }
}

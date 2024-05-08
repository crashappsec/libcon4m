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

    if (hatrack_dict_add(scope->symbols, name, entry)) {
        if (success != NULL) {
            *success = true;
        }

        return entry;
    }

    c4m_scope_entry_t *old   = hatrack_dict_get(scope->symbols, name, NULL);
    c4m_pnode_t       *pnode = c4m_tree_get_contents(old->declaration_node);
    c4m_token_t       *tok   = pnode->token;

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
                  c4m_new_utf8(symbol_kind_names[old->kind]),
                  old->path,
                  c4m_box_i32(tok->line_no),
                  c4m_box_i32(tok->line_offset + 1));

    return old;
}

c4m_scope_entry_t *
c4m_lookup_symbol(c4m_scope_t *scope, c4m_utf8_t *name)
{
    return hatrack_dict_get(scope->symbols, name, NULL);
}

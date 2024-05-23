#pragma once

#include "con4m.h"

#ifdef C4M_USE_INTERNAL_API
extern c4m_scope_t       *c4m_new_scope(c4m_scope_t *, c4m_scope_kind);
extern c4m_scope_entry_t *c4m_declare_symbol(c4m_file_compile_ctx *,
                                             c4m_scope_t *,
                                             c4m_utf8_t *,
                                             c4m_tree_node_t *,
                                             c4m_symbol_kind,
                                             bool *,
                                             bool);
extern c4m_scope_entry_t *c4m_lookup_symbol(c4m_scope_t *, c4m_utf8_t *);
extern bool               c4m_merge_symbols(c4m_file_compile_ctx *,
                                            c4m_scope_entry_t *,
                                            c4m_scope_entry_t *);
extern c4m_grid_t        *c4m_format_scope(c4m_scope_t *);
extern void               c4m_shadow_check(c4m_file_compile_ctx *,
                                           c4m_scope_entry_t *,
                                           c4m_scope_t *);
extern c4m_scope_entry_t *c4m_symbol_lookup(c4m_scope_t *,
                                            c4m_scope_t *,
                                            c4m_scope_t *,
                                            c4m_scope_t *,
                                            c4m_utf8_t *);
extern c4m_scope_entry_t *c4m_add_inferred_symbol(c4m_file_compile_ctx *,
                                                  c4m_scope_t *,
                                                  c4m_utf8_t *);
extern c4m_scope_entry_t *c4m_add_or_replace_symbol(c4m_file_compile_ctx *,
                                                    c4m_scope_t *,
                                                    c4m_utf8_t *);
extern c4m_utf8_t        *c4m_sym_get_best_ref_loc(c4m_scope_entry_t *);

static inline bool
c4m_sym_is_declared_const(c4m_scope_entry_t *sym)
{
    return (sym->flags & C4M_F_DECLARED_CONST) != 0;
}

static inline c4m_type_t *
c4m_get_sym_type(c4m_scope_entry_t *sym)
{
    return sym->type;
}

static inline bool
c4m_type_is_declared(c4m_scope_entry_t *sym)
{
    return (bool)(sym->flags & C4M_F_TYPE_IS_DECLARED);
}

extern char *c4m_symbol_kind_names[sk_num_sym_kinds];

static inline c4m_utf8_t *
c4m_sym_kind_name(c4m_scope_entry_t *sym)
{
    return c4m_new_utf8(c4m_symbol_kind_names[sym->kind]);
}

#endif

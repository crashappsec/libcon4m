#pragma once

#include "con4m.h"

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

static inline bool
c4m_sym_is_const(c4m_scope_entry_t *sym)
{
    return (sym->flags & C4M_F_IS_CONST) != 0;
}

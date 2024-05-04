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

#pragma once
#include "con4m.h"

extern c4m_cfg_node_t *c4m_cfg_enter_block(c4m_cfg_node_t *,
                                           c4m_tree_node_t *);
extern c4m_cfg_node_t *c4m_cfg_exit_block(c4m_cfg_node_t *,
                                          c4m_cfg_node_t *,
                                          c4m_tree_node_t *);
extern c4m_cfg_node_t *c4m_cfg_block_new_branch_node(c4m_cfg_node_t *,
                                                     int,
                                                     c4m_utf8_t *,
                                                     c4m_tree_node_t *);
extern c4m_cfg_node_t *c4m_cfg_add_return(c4m_cfg_node_t *,
                                          c4m_tree_node_t *,
                                          c4m_cfg_node_t *);
extern c4m_cfg_node_t *c4m_cfg_add_continue(c4m_cfg_node_t *,
                                            c4m_tree_node_t *,
                                            c4m_utf8_t *);
extern c4m_cfg_node_t *c4m_cfg_add_break(c4m_cfg_node_t *,
                                         c4m_tree_node_t *,
                                         c4m_utf8_t *);
extern c4m_cfg_node_t *c4m_cfg_add_def(c4m_cfg_node_t *,
                                       c4m_tree_node_t *,
                                       c4m_symbol_t *,
                                       c4m_list_t *);
extern c4m_cfg_node_t *c4m_cfg_add_call(c4m_cfg_node_t *,
                                        c4m_tree_node_t *,
                                        c4m_symbol_t *,
                                        c4m_list_t *);
extern c4m_cfg_node_t *c4m_cfg_add_use(c4m_cfg_node_t *,
                                       c4m_tree_node_t *,
                                       c4m_symbol_t *);
extern c4m_grid_t     *c4m_cfg_repr(c4m_cfg_node_t *);
extern void            c4m_cfg_analyze(c4m_module_compile_ctx *, c4m_dict_t *);

static inline c4m_cfg_node_t *
c4m_cfg_exit_node(c4m_cfg_node_t *block_entry)
{
    return block_entry->contents.block_entrance.exit_node;
}

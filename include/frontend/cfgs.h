#pragma once
#include "con4m.h"

extern c4m_cfg_node_t *c4m_cfg_enter_block(c4m_cfg_node_t *,
                                           c4m_tree_node_t *);
extern c4m_cfg_node_t *c4m_cfg_exit_block(c4m_cfg_node_t *,
                                          c4m_tree_node_t *);
extern c4m_cfg_node_t *c4m_cfg_block_new_branch_node(c4m_cfg_node_t *,
                                                     int,
                                                     c4m_utf8_t *,
                                                     c4m_tree_node_t *);
extern c4m_cfg_node_t *c4m_cfg_add_return(c4m_cfg_node_t *,
                                          c4m_tree_node_t *);
extern c4m_cfg_node_t *c4m_cfg_add_continue(c4m_cfg_node_t *,
                                            c4m_tree_node_t *,
                                            c4m_utf8_t *);
extern c4m_cfg_node_t *c4m_cfg_add_break(c4m_cfg_node_t *,
                                         c4m_tree_node_t *,
                                         c4m_utf8_t *);
extern c4m_cfg_node_t *c4m_cfg_add_def(c4m_cfg_node_t *,
                                       c4m_tree_node_t *,
                                       c4m_scope_entry_t *,
                                       c4m_xlist_t *);
extern c4m_cfg_node_t *c4m_cfg_add_call(c4m_cfg_node_t *,
                                        c4m_tree_node_t *,
                                        c4m_scope_entry_t *,
                                        c4m_xlist_t *);
extern c4m_cfg_node_t *c4m_cfg_add_use(c4m_cfg_node_t *,
                                       c4m_tree_node_t *,
                                       c4m_scope_entry_t *);

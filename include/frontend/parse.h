#pragma once
#include "con4m.h"

extern bool        c4m_parse(c4m_file_compile_ctx *);
extern c4m_grid_t *c4m_format_parse_tree(c4m_file_compile_ctx *);
extern void        c4m_print_parse_node(c4m_tree_node_t *);

#pragma once
#include "con4m.h"

typedef struct c4m_cfg_node_t c4m_cfg_node_t;

typedef enum {
    c4m_cfg_block_entrance,
    c4m_cfg_block_exit,
    c4m_cfg_node_branch,
    c4m_cfg_use,  // Use outside of any assignment or call.
    c4m_cfg_def,  // Def on the left, the the list of uses on the RHS
    c4m_cfg_call, // A set of uses passed to a call.
    c4m_cfg_jump
} c4m_cfg_node_type;

typedef struct {
    c4m_cfg_node_t *next_node;
    c4m_cfg_node_t *exit_node;
    c4m_xlist_t    *inbound_links;
} c4m_cfg_block_enter_info_t;

typedef struct {
    c4m_cfg_node_t *next_node;
    c4m_xlist_t    *inbound_links;
} c4m_cfg_block_exit_info_t;

typedef struct {
    c4m_cfg_node_t *dead_code;
    c4m_cfg_node_t *target;
} c4m_cfg_jump_info_t;

typedef struct {
    int32_t          num_branches;
    int32_t          next_to_process;
    c4m_cfg_node_t **branch_targets;
    c4m_utf8_t      *label; // For loops
} c4m_cfg_branch_info_t;

typedef struct {
    c4m_cfg_node_t    *next_node;
    c4m_scope_entry_t *dst_symbol;
    c4m_xlist_t       *deps; // all symbols influencing the def
} c4m_cfg_flow_info_t;

struct c4m_cfg_node_t {
    c4m_tree_node_t *reference_location;
    c4m_cfg_node_t  *parent;

    union {
        c4m_cfg_block_enter_info_t block_entrance;
        c4m_cfg_block_exit_info_t  block_exit;
        c4m_cfg_branch_info_t      branches;
        c4m_cfg_flow_info_t        flow; // Used for any def / use activity.
        c4m_cfg_jump_info_t        jump;
    } contents;

    c4m_cfg_node_type kind;
};

#pragma once
#include "con4m.h"

// This data structure is the first bytes of the extra_info field for anything
// that might be a jump target, including loops, conditionals, case statements,
// etc.
//
// Some of these things can be labeled: loops, typeof() and switch().
// And, we can `break` out of switch / typeof cases. But we can't use
// 'continue' without a loop, which is why we need to track whether
// this data structure is associated with a loop... `continue`
// statements always look for an enclosing loop.

typedef struct {
    c4m_utf8_t *label;
    c4m_list_t *awaiting_patches;
    int         entry_ip;
    int         exit_ip;
    bool        non_loop;
} c4m_control_info_t;

typedef struct {
    c4m_control_info_t branch_info;
    // switch() and typeof() can be labeled, but do not have automatic
    // variables, so they don't ever get renamed. That's why `label`
    // lives inside of branch_info, but the rest of this is in the
    // loop info.
    c4m_utf8_t        *label_ix;
    c4m_utf8_t        *label_last;
    c4m_tree_node_t   *prelude;
    c4m_tree_node_t   *test;
    c4m_tree_node_t   *body;
    c4m_symbol_t      *shadowed_ix;
    c4m_symbol_t      *loop_ix;
    c4m_symbol_t      *named_loop_ix;
    c4m_symbol_t      *shadowed_last;
    c4m_symbol_t      *loop_last;
    c4m_symbol_t      *named_loop_last;
    c4m_symbol_t      *lvar_1;
    c4m_symbol_t      *lvar_2;
    c4m_symbol_t      *shadowed_lvar_1;
    c4m_symbol_t      *shadowed_lvar_2;
    bool               ranged;
    unsigned int       gen_ix       : 1;
    unsigned int       gen_named_ix : 1;
} c4m_loop_info_t;

typedef struct c4m_jump_info_t {
    c4m_control_info_t *linked_control_structure;
    c4m_zinstruction_t *to_patch;
    bool                top;
} c4m_jump_info_t;

#ifdef C4M_USE_INTERNAL_API
typedef struct {
    c4m_utf8_t      *name;
    c4m_type_t      *sig;
    c4m_tree_node_t *loc;
    c4m_symbol_t    *resolution;
    unsigned int     polymorphic : 1;
    unsigned int     deferred    : 1;
} c4m_call_resolution_info_t;
#endif

#pragma once
#include "con4m.h"

typedef struct {
    c4m_type_t          *full_type;
    c4m_scope_t         *fn_scope;
    c4m_scope_t         *formals;
    c4m_fn_param_info_t *param_info;
    c4m_fn_param_info_t  return_info;
    int                  num_params;
    unsigned int         pure        : 1;
    unsigned int         void_return : 1;
} c4m_sig_info_t;

typedef struct {
    c4m_utf8_t            *short_doc;
    c4m_utf8_t            *long_doc;
    c4m_sig_info_t        *signature_info;
    struct c4m_cfg_node_t *cfg;
    int32_t                frame_size;
    // sc = 'short circuit'
    // If we are a 'once' function, this is the offset into static data,
    // where we will place:
    //
    // - A boolean.
    // - A pthread_mutex_t
    // - A void *
    //
    // The idea is, if the boolean is true, we only ever read and
    // return the cached (memoized) result, stored in the void *. If
    // it's false, we grab the lock, check the boolean a second time,
    // run the function, set the memo and the boolean, and then
    // unlock.
    int32_t                sc_lock_offset;
    int32_t                sc_bool_offset;
    int32_t                sc_memo_offset;

    unsigned int private : 1;
    unsigned int once    : 1;
} c4m_fn_decl_t;

typedef struct {
    c4m_utf8_t     *short_doc;
    c4m_utf8_t     *long_doc;
    c4m_utf8_t     *local_name;
    c4m_sig_info_t *local_params;
    int             num_params;
    c4m_utf8_t     *external_name;
    uint8_t        *external_params;
    uint8_t         external_return_type;
    int             holds;
    int             allocs;
} c4m_ffi_decl_t;

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
    int          entry_ip;
    int          exit_ip;
    c4m_utf8_t  *label;
    bool         non_loop;
    c4m_xlist_t *awaiting_patches;
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
    c4m_scope_entry_t *shadowed_ix;
    c4m_scope_entry_t *loop_ix;
    c4m_scope_entry_t *named_loop_ix;
    c4m_scope_entry_t *shadowed_last;
    c4m_scope_entry_t *loop_last;
    c4m_scope_entry_t *named_loop_last;
    c4m_scope_entry_t *lvar_1;
    c4m_scope_entry_t *lvar_2;
    c4m_scope_entry_t *shadowed_lvar_1;
    c4m_scope_entry_t *shadowed_lvar_2;
    bool               ranged;
    unsigned int       gen_ix       : 1;
    unsigned int       gen_named_ix : 1;
} c4m_loop_info_t;

typedef struct c4m_jump_info_t {
    c4m_control_info_t *linked_control_structure;
    c4m_zinstruction_t *to_patch;
    bool                top;
} c4m_jump_info_t;

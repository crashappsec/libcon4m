#pragma once
#include "con4m.h"

typedef struct {
    c4m_utf8_t  *name;
    c4m_type_t  *type;
    unsigned int ffi_holds  : 1;
    unsigned int ffi_allocs : 1;
} c4m_fn_param_info_t;

typedef struct c4m_sig_info_t {
    c4m_type_t          *full_type;
    c4m_fn_param_info_t *param_info;
    c4m_fn_param_info_t  return_info;
    int                  num_params;
    unsigned int         pure        : 1;
    unsigned int         void_return : 1;
    c4m_scope_t         *fn_scope;
    c4m_scope_t         *formals;
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
    // run thecm function, set the memo and the boolean, and then
    // unlock.
    int32_t                sc_lock_offset;
    int32_t                sc_bool_offset;
    int32_t                sc_memo_offset;
    int32_t                local_id;
    int32_t                offset;
    int32_t                module_id;

    unsigned int private : 1;
    unsigned int once    : 1;
} c4m_fn_decl_t;

typedef struct c4m_funcinfo_t {
    union {
        c4m_ffi_decl_t *ffi_interface;
        c4m_fn_decl_t  *local_interface;
    } implementation;

    unsigned int ffi : 1;
    unsigned int va  : 1;
} c4m_funcinfo_t;

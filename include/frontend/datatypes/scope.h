#pragma once
#include "con4m.h"

typedef enum : int8_t {
    sk_module,
    sk_package,
    sk_func,
    sk_extern_func,
    sk_enum_type,
    sk_enum_val,
    sk_attr,
    sk_variable,
    sk_formal,
    // Will be adding more for sure.
    sk_num_sym_kinds
} c4m_symbol_kind;

enum {
    C4M_F_HAS_DEFAULT_VALUE = 1,
    C4M_F_IS_CONST          = 2,
};

typedef enum c4m_scope_kind {
    C4M_SCOPE_GLOBAL,
    C4M_SCOPE_MODULE,
    C4M_SCOPE_LOCAL,
    C4M_SCOPE_ATTRIBUTES,
    C4M_SCOPE_IMPORTS,
} c4m_scope_kind;

// For module entries, the c4m_module_info_t data structure
// will be in the `value` field of the scope entry.
typedef struct {
    c4m_utf8_t *specified_module;
    c4m_utf8_t *specified_package;
    c4m_utf8_t *specified_uri;
} c4m_module_info_t;

// For extern entries, the data structure will be in the `value`
// field.

typedef struct {
    c4m_utf8_t  *name;
    c4m_type_t  *type;
    unsigned int ffi_holds  : 1;
    unsigned int ffi_allocs : 1;
} c4m_param_info_t;

typedef struct {
    // The `value` field gets the proper value for vars and enums, but
    // for other types, it gets a pointer to one of the specific data
    // structures in this file.

    c4m_obj_t        value;
    c4m_utf8_t      *path;
    c4m_utf8_t      *name;
    c4m_tree_node_t *declaration_node;
    c4m_xlist_t     *use_locations;
    c4m_xlist_t     *lhs_locations;
    uint32_t         offset;
    uint32_t         size;
    uint8_t          flags;
    c4m_symbol_kind  kind;
    c4m_type_t      *declared_type;
    c4m_type_t      *inferred_type;
} c4m_scope_entry_t;

typedef struct c4m_scope_t {
    struct c4m_scope_t *parent;
    c4m_dict_t         *symbols;
    enum c4m_scope_kind kind;
} c4m_scope_t;

typedef struct {
    c4m_type_t       *full_type;
    c4m_scope_t      *fn_scope;
    c4m_param_info_t *param_info;
    c4m_param_info_t  return_info;
    int               num_params;
    unsigned int      pure : 1;
} c4m_sig_info_t;

typedef struct {
    c4m_utf8_t     *short_doc;
    c4m_utf8_t     *long_doc;
    c4m_sig_info_t *signature_info;
    unsigned int private : 1;
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

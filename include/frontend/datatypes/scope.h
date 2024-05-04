#pragma once
#include "con4m.h"

typedef enum : int8_t {
    sk_module,
    sk_package,
    sk_func,
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
    C4M_SCOPE_IMPORTS
} c4m_scope_kind;

// Note that for module entries, the c4m_module_info_t data structure
// will be in the `value` field of the scope entry.
typedef struct {
    c4m_utf8_t *specified_module;
    c4m_utf8_t *specified_package;
    c4m_utf8_t *specified_uri;
} c4m_module_info_t;

typedef struct {
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
    c4m_type_t      *value;
} c4m_scope_entry_t;

typedef struct c4m_scope_t {
    struct c4m_scope_t *parent;
    c4m_dict_t         *symbols;
    enum c4m_scope_kind kind;
} c4m_scope_t;

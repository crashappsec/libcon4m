#pragma once
#include "con4m.h"

typedef enum : int8_t {
    c4m_sk_module,
    c4m_sk_func,
    c4m_sk_enum_val,
    c4m_sk_attr,
    c4m_sk_local,
    c4m_sk_global,
    c4m_sk_module_param,
    c4m_sk_formal,
    c4m_sk_scoped_type_variable,
    c4m_sk_user_defined_type,
} c4m_symbol_kind_t;

enum {
    C4M_F_HAS_DEFAULT_VALUE = 1,
    C4M_F_IS_CONST          = 2,
};

typedef struct {
} c4m_scopeinfo_module_t;

typedef struct {
    c4m_type_t *declared_type;
    c4m_type_t *inferred_type;
} c4m_scopeinfo_func_t;

typedef struct {
    c4m_type_t *declared_type;
    c4m_type_t *inferred_type;
    void       *value;
} c4m_scopeinfo_param_t;

typedef struct {
    c4m_type_t *declared_type;
    c4m_type_t *inferred_type;
    size_t      required_storage;
    void       *value;
} c4m_scopeinfo_variable_t;

typedef struct {
    c4m_type_t *declared_type;
    c4m_type_t *inferred_type;
    void       *value;
} c4m_enum_val_t;

typedef struct {
    c4m_utf8_t       *name;
    c4m_pnode_t      *declaration_node;
    void             *info; // pointer to one of the above structs.
    c4m_symbol_kind_t kind;
    uint8_t           flags;
} c4m_scope_entry;

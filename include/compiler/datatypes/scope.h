#pragma once
#include "con4m.h"

typedef enum : int8_t {
    sk_module,
    sk_func,
    sk_extern_func,
    sk_enum_type,
    sk_enum_val,
    sk_attr,
    sk_variable,
    sk_formal,
    sk_num_sym_kinds
} c4m_symbol_kind;

enum {
    C4M_F_HAS_INITIALIZER  = 0x0001,
    C4M_F_DECLARED_CONST   = 0x0002,
    C4M_F_DECLARED_LET     = 0x0004,
    C4M_F_IS_DECLARED      = 0x0008,
    C4M_F_TYPE_IS_DECLARED = 0x0010,
    // Internally, when something has the 'const' flag above set, it
    // only implies the object is user immutable. This flag
    // differentiates between true CONSTs that we could put in
    // read-only storage, and what is for iteration variables on
    // loops, etc.
    C4M_F_USER_IMMUTIBLE   = 0x0020,
    C4M_F_FN_PASS_DONE     = 0x0040,
    C4M_F_USE_ERROR        = 0x0080,
    C4M_F_STATIC_STORAGE   = 0x0100,
    C4M_F_STACK_STORAGE    = 0x0200,
    C4M_F_REGISTER_STORAGE = 0x0400,
    C4M_F_FUNCTION_SCOPE   = 0x0800,
};

typedef enum c4m_scope_kind : int8_t {
    C4M_SCOPE_GLOBAL     = 1,
    C4M_SCOPE_MODULE     = 2,
    C4M_SCOPE_LOCAL      = 4,
    C4M_SCOPE_FUNC       = 8,
    C4M_SCOPE_FORMALS    = 16,
    C4M_SCOPE_ATTRIBUTES = 32,
    C4M_SCOPE_IMPORTS    = 64,
} c4m_scope_kind;

// For module entries, the c4m_module_info_t data structure
// will be in the `value` field of the scope entry.
typedef struct {
    c4m_utf8_t *specified_module;
    c4m_utf8_t *specified_package;
    c4m_utf8_t *specified_uri;
} c4m_module_info_t;

typedef struct c4m_scope_entry_t {
    // The `value` field gets the proper value for vars and enums, but
    // for other types, it gets a pointer to one of the specific data
    // structures in this file.
    c4m_tree_node_t          *type_declaration_node;
    void                     *other_info;
    c4m_xlist_t              *sym_defs;
    c4m_xlist_t              *sym_uses;
    struct c4m_scope_entry_t *linked_symbol;
    c4m_utf8_t               *name;
    c4m_tree_node_t          *declaration_node;
    c4m_tree_node_t          *value_node;
    c4m_obj_t                 value;
    c4m_utf8_t               *path;
    c4m_symbol_kind           kind;
    c4m_type_t               *type;
    struct c4m_scope_t       *my_scope;

    // For constant value types, this is an absolute byte offset
    // from the start of the `const_data` buffer.
    //
    // For constant reference types, this field represents a byte
    // offset into the const_instantiations buffer. The object
    // marshaling sticks this value after the object when it
    // successfully marshals it, giving it the offset to put the
    // unmarshaled reference.
    //
    // For all other values, this represents a byte offset from
    // whatever reference point we have; for globals and module
    // variables, it's the start of the runtime mutable static space,
    // and for functions, it'll be from the frame pointer, if it's in
    // the function scope.
    uint32_t static_offset;

    // For things that are statically allocated, this indicates which
    // module's arena we're using, using an index that is equal to
    // the module's index into the `module_ordering` field in the
    // compilation context.
    uint32_t local_module_id;
    void    *cfg_kill_node;
    uint32_t flags;
} c4m_scope_entry_t;

typedef struct {
    c4m_utf8_t        *short_doc;
    c4m_utf8_t        *long_doc;
    c4m_obj_t          callback;
    c4m_obj_t          validator;
    c4m_obj_t          default_value;
    c4m_scope_entry_t *linked_symbol;
    unsigned int       param_index;
    unsigned int       have_default : 1;
} c4m_module_param_info_t;

typedef struct c4m_scope_t {
    struct c4m_scope_t *parent;
    c4m_dict_t         *symbols;
    enum c4m_scope_kind kind;
} c4m_scope_t;

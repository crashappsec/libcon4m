#pragma once
#include "con4m.h"

typedef enum {
    c4m_nt_error,
    c4m_nt_module,
    c4m_nt_body,
    c4m_nt_assign,
    c4m_nt_attr_set_lock,
    c4m_nt_cast,
    c4m_nt_section,
    c4m_nt_if,
    c4m_nt_elif,
    c4m_nt_else,
    c4m_nt_typeof,
    c4m_nt_switch,
    c4m_nt_for,
    c4m_nt_while,
    c4m_nt_break,
    c4m_nt_continue,
    c4m_nt_return,
    c4m_nt_simple_lit,
    c4m_nt_lit_list,
    c4m_nt_lit_dict,
    c4m_nt_lit_set,
    c4m_nt_lit_empty_dict_or_set,
    c4m_nt_lit_tuple,
    c4m_nt_lit_unquoted,
    c4m_nt_lit_callback,
    c4m_nt_lit_tspec,
    c4m_nt_lit_tspec_tvar,
    c4m_nt_lit_tspec_named_type,
    c4m_nt_lit_tspec_parameterized_type,
    c4m_nt_lit_tspec_func,
    c4m_nt_lit_tspec_varargs,
    c4m_nt_lit_tspec_return_type,
    c4m_nt_or,
    c4m_nt_and,
    c4m_nt_cmp,
    c4m_nt_binary_op,
    c4m_nt_binary_assign_op,
    c4m_nt_unary_op,
    c4m_nt_enum,
    c4m_nt_global_enum,
    c4m_nt_enum_item,
    c4m_nt_identifier,
    c4m_nt_func_def,
    c4m_nt_formals,
    c4m_nt_varargs_param,
    c4m_nt_member,
    c4m_nt_index,
    c4m_nt_call,
    c4m_nt_paren_expr,
    c4m_nt_var_decls,
    c4m_nt_global_decls,
    c4m_nt_const_var_decls,
    c4m_nt_const_global_decls,
    c4m_nt_const_decls, // global / var not specified.
    c4m_nt_sym_decl,
    c4m_nt_use,
    c4m_nt_param_block,
    c4m_nt_param_prop,
    c4m_nt_extern_block,
    c4m_nt_extern_sig,
    c4m_nt_extern_param,
    c4m_nt_extern_local,
    c4m_nt_extern_dll,
    c4m_nt_extern_pure,
    c4m_nt_extern_holds,
    c4m_nt_extern_allocs,
    c4m_nt_extern_return,
    c4m_nt_extern_expression,
    c4m_nt_label,
    c4m_nt_case,
    c4m_nt_range,
    c4m_nt_assert,
    c4m_nt_config_spec,
    c4m_nt_section_spec,
    c4m_nt_section_prop,
    c4m_nt_field_spec,
    c4m_nt_field_prop,
    c4m_nt_short_doc_string,
    c4m_nt_long_doc_string,
    c4m_nt_expression,
} c4m_node_kind_t;

typedef struct {
    c4m_token_t *comment_tok;
    int          sibling_id;
} c4m_comment_node_t;

typedef struct {
    // Parse children are stored beside us because we're using the c4m_tree.
    c4m_node_kind_t     kind;
    // Every node gets a token to mark its location, even if the same
    // token appears in separate nodes (it will never have semantic
    // meaning in more than one).
    c4m_token_t        *token;
    c4m_token_t        *short_doc;
    c4m_token_t        *long_doc;
    c4m_xlist_t        *comments;
    int                 total_kids;
    int                 sibling_id;
    void               *extra_info;
    c4m_obj_t          *value;
    struct c4m_scope_t *static_scope;
    c4m_type_t         *type;
} c4m_pnode_t;

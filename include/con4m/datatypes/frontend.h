#pragma once
#include "con4m.h"

typedef enum {
    c4m_tt_error, // 0
    c4m_tt_space,
    c4m_tt_semi,
    c4m_tt_newline,
    c4m_tt_line_comment,
    c4m_tt_long_comment,
    c4m_tt_lock_attr,
    c4m_tt_plus,
    c4m_tt_minus,
    c4m_tt_mul,
    c4m_tt_div, // 10
    c4m_tt_mod,
    c4m_tt_lte,
    c4m_tt_lt,
    c4m_tt_gte,
    c4m_tt_gt,
    c4m_tt_neq,
    c4m_tt_not,
    c4m_tt_colon,
    c4m_tt_assign,
    c4m_tt_cmp, // 20
    c4m_tt_comma,
    c4m_tt_period,
    c4m_tt_lbrace,
    c4m_tt_rbrace,
    c4m_tt_lbracket,
    c4m_tt_rbracket,
    c4m_tt_lparen,
    c4m_tt_rparen,
    c4m_tt_and,
    c4m_tt_or, // 30
    c4m_tt_int_lit,
    c4m_tt_hex_lit,
    c4m_tt_float_lit,
    c4m_tt_string_lit,
    c4m_tt_char_lit,
    c4m_tt_unquoted_lit,
    c4m_tt_true,
    c4m_tt_false,
    c4m_tt_nil,
    c4m_tt_if, // 40
    c4m_tt_elif,
    c4m_tt_else,
    c4m_tt_for,
    c4m_tt_from,
    c4m_tt_to,
    c4m_tt_break,
    c4m_tt_continue,
    c4m_tt_return,
    c4m_tt_enum,
    c4m_tt_identifier, // 50
    c4m_tt_func,
    c4m_tt_var,
    c4m_tt_global,
    c4m_tt_const,
    c4m_tt_private,
    c4m_tt_backtick,
    c4m_tt_arrow,
    c4m_tt_object,
    c4m_tt_while,
    c4m_tt_in, // 60
    c4m_tt_bit_and,
    c4m_tt_bit_or,
    c4m_tt_bit_xor,
    c4m_tt_shl,
    c4m_tt_shr,
    c4m_tt_typeof,
    c4m_tt_switch,
    c4m_tt_case,
    c4m_tt_plus_eq,
    c4m_tt_minus_eq, // 70
    c4m_tt_mul_eq,
    c4m_tt_div_eq,
    c4m_tt_mod_eq,
    c4m_tt_bit_and_eq,
    c4m_tt_bit_or_eq,
    c4m_tt_bit_xor_eq,
    c4m_tt_shl_eq,
    c4m_tt_shr_eq,
    c4m_tt_eof // 79
} c4m_token_kind_t;

typedef enum {
    c4m_err_open_file,
    c4m_err_lex_stray_cr,
    c4m_err_lex_eof_in_comment,
    c4m_err_lex_invalid_char,
    c4m_err_lex_eof_in_str_lit,
    c4m_err_lex_nl_in_str_lit,
    c4m_err_lex_eof_in_char_lit,
    c4m_err_lex_nl_in_char_lit,
    c4m_err_lex_extra_in_char_lit,
    c4m_err_lex_esc_in_esc,
    c4m_err_lex_invalid_float_lit,
    c4m_err_lex_float_oflow,
    c4m_err_lex_float_uflow,
    c4m_err_lex_int_oflow,
    c4m_err_parse_continue_outside_loop,
    c4m_err_parse_break_outside_loop,
    c4m_err_parse_return_outside_func,
    c4m_err_parse_expected_stmt_end,
    c4m_err_parse_unexpected_after_expr,
    c4m_err_parse_expected_brace,
    c4m_err_parse_expected_range_tok,
    c4m_err_parse_eof,
    c4m_err_parse_bad_use_uri,
    c4m_err_parse_id_expected,
    c4m_err_parse_id_member_part,
    c4m_err_parse_not_docable_block,
    c4m_err_parse_for_syntax,
    c4m_err_parse_missing_type_rbrak,
    c4m_err_parse_bad_tspec,
    c4m_err_parse_vararg_wasnt_last_thing,
    c4m_err_parse_fn_param_syntax,
    c4m_err_parse_enums_are_toplevel,
    c4m_err_parse_funcs_are_toplevel,
    c4m_err_parse_parameter_is_toplevel,
    c4m_err_parse_extern_is_toplevel,
    c4m_err_parse_confspec_is_toplevel,
    c4m_err_parse_bad_confspec_sec_type,
    c4m_err_parse_invalid_token_in_sec,
    c4m_err_parse_expected_token,
    c4m_err_parse_invalid_sec_part,
    c4m_err_parse_invalid_field_part,
    c4m_err_parse_no_empty_tuples,
    c4m_err_parse_lit_or_id,
    c4m_err_parse_1_item_tuple,
    c4m_err_parse_decl_kw_x2,
    c4m_err_parse_decl_2_scopes,
    c4m_err_parse_case_else_or_end,
    c4m_err_parse_case_body_start,
    c4m_err_parse_empty_enum,
    c4m_err_parse_enum_item,
    c4m_err_parse_need_simple_lit,
    c4m_err_parse_need_str_lit,
    c4m_err_parse_need_bool_lit,
    c4m_err_parse_formal_expect_id,
    c4m_err_parse_bad_extern_field,
    c4m_err_parse_extern_sig_needed,
    c4m_err_parse_extern_bad_hold_param,
    c4m_err_parse_extern_bad_alloc_param,
    c4m_err_parse_extern_bad_prop,
    c4m_err_parse_enum_value_type,
    c4m_err_parse_csig_id,
    c4m_err_parse_bad_ctype_id,
    c4m_err_parse_mod_param_no_const,
    c4m_err_parse_bad_param_start,
    c4m_err_parse_param_def_and_callback,
    c4m_err_parse_param_dupe_prop,
    c4m_err_parse_param_invalid_prop,
    c4m_err_parse_bad_expression_start,
    c4m_err_parse_missing_expression,
    c4m_err_last,
} c4m_compile_error_t;

typedef struct {
    c4m_codepoint_t *start_ptr;
    c4m_codepoint_t *end_ptr;
    c4m_utf32_t     *literal_modifier;
    void            *literal_value; // Once parsed.
    c4m_token_kind_t kind;
    int              token_id;
    int              line_no;
    int              line_offset;
    // Original index relative to all added tokens under a parse node.
    // We do this because we don't keep the comments in the main tree,
    // we stash them with the node payload.
    uint16_t         child_ix;
    uint8_t          adjustment; // For keeping track of quoting.
} c4m_token_t;

typedef enum {
    c4m_err_severity_error,
    c4m_err_severity_warning,
    c4m_err_severity_info,
} c4m_err_severity_t;

typedef struct {
    c4m_compile_error_t code;
    // These may turn into a tagged union or transparent pointer with
    // a phase indicator, depending on whether we think we need
    // additional context.
    //
    // the `msg_parameters` field we allocate when we have data we
    // want to put into the error message via substitution. We use $1
    // .. $n, and the formatter will assume the right number of array
    // elements are there based on the values it sees.

    c4m_token_t       *current_token;
    c4m_str_t         *long_info;
    int32_t            num_args;
    c4m_err_severity_t severity;
    c4m_str_t         *msg_parameters[];
} c4m_compile_error;

typedef enum {
    c4m_nt_error, // 0
    c4m_nt_module,
    c4m_nt_body,
    c4m_nt_assign,
    c4m_nt_attr_set_lock,
    c4m_nt_cast,
    c4m_nt_section,
    c4m_nt_if,
    c4m_nt_elif,
    c4m_nt_else,
    c4m_nt_typeof, // 10
    c4m_nt_switch,
    c4m_nt_for,
    c4m_nt_while,
    c4m_nt_break,
    c4m_nt_continue,
    c4m_nt_return,
    c4m_nt_simple_lit,
    c4m_nt_lit_list,
    c4m_nt_lit_dict,
    c4m_nt_lit_set, // 20
    c4m_nt_lit_empty_dict_or_set,
    c4m_nt_lit_tuple,
    c4m_nt_lit_unquoted,
    c4m_nt_lit_callback,
    c4m_nt_lit_tspec,
    c4m_nt_lit_tspec_tvar,
    c4m_nt_lit_tspec_named_type,
    c4m_nt_lit_tspec_parameterized_type,
    c4m_nt_lit_tspec_func,
    c4m_nt_lit_tspec_varargs, // 30
    c4m_nt_lit_tspec_return_type,
    c4m_nt_or,
    c4m_nt_and,
    c4m_nt_cmp,
    c4m_nt_binary_op,
    c4m_nt_binary_assign_op,
    c4m_nt_unary_op,
    c4m_nt_enum,
    c4m_nt_enum_item,
    c4m_nt_identifier, // 40
    c4m_nt_func_def,
    c4m_nt_formals,
    c4m_nt_varargs_param,
    c4m_nt_member,
    c4m_nt_index,
    c4m_nt_call,
    c4m_nt_paren_expr,
    c4m_nt_var_decls,
    c4m_nt_global_decls,
    c4m_nt_const_var_decls, // 50
    c4m_nt_const_global_decls,
    c4m_nt_const_decls,     // global / var not specified.
    c4m_nt_sym_decl,
    c4m_nt_use,
    c4m_nt_param_block,
    c4m_nt_param_prop,
    c4m_nt_extern_block,
    c4m_nt_extern_sig,
    c4m_nt_extern_param,
    c4m_nt_extern_local, // 60
    c4m_nt_extern_dll,
    c4m_nt_extern_pure,
    c4m_nt_extern_holds,
    c4m_nt_extern_allocs,
    c4m_nt_extern_return,
    c4m_nt_extern_expression,
    c4m_nt_label,
    c4m_nt_case,
    c4m_nt_range,
    c4m_nt_assert, // 70
    c4m_nt_config_spec,
    c4m_nt_section_spec,
    c4m_nt_section_prop,
    c4m_nt_field_spec,
    c4m_nt_field_prop,
    c4m_nt_short_doc_string,
    c4m_nt_long_doc_string,
    c4m_nt_expression, // 78
} c4m_node_kind_t;

typedef struct {
    c4m_token_t *comment_tok;
    int          sibling_id;
} c4m_comment_node_t;

typedef struct {
    // Parse children are stored beside us because we're using the c4m_tree.
    c4m_node_kind_t kind;
    // Every node gets a token to mark its location, even if the same
    // token appears in separate nodes (it will never have semantic
    // meaning in more than one).
    c4m_token_t    *token;
    c4m_token_t    *short_doc;
    c4m_token_t    *long_doc;
    c4m_xlist_t    *comments;
    int             total_kids;
    int             sibling_id;
    void           *extra_info;
} c4m_pnode_t;

typedef struct {
    // The module_id is calculated by combining the package name and the
    // module name, then hashing it with SHA256. We use Unix style paths
    // but this is not necessarily derived from the URI path.
    //
    // Note that packages (and our combining of it and the module) use
    // dotted syntax like with most PLs. When we combine for the hash,
    // we add a dot in there.
    //
    // c4m_new_compile_ctx will add __default__ as the package if none
    // is provided. The URI fields are optional (via API you can just
    // pass raw source as long as you give at least a module name).

    __int128_t       module_id;
    c4m_str_t       *scheme;    // http, https or file; if NULL, then file.
    c4m_str_t       *authority; // http/s only.
    c4m_str_t       *path;      // Path component in the URI.
    c4m_str_t       *package;   // Package name.
    c4m_str_t       *module;    // Module name.
    c4m_utf32_t     *raw;       // raw contents read when we do the lex pass.
    c4m_xlist_t     *tokens;    // an xlist of x4m_token_t objects;
    c4m_tree_node_t *parse_tree;
    c4m_xlist_t     *errors;    // an xlist of c4m_compile_errors
} c4m_file_compile_ctx;

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

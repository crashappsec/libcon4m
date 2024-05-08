#pragma once
#include "con4m.h"

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
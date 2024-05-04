#include "con4m.h"

typedef struct {
    c4m_compile_error_t errorid;
    char               *name;
    char               *message;
    bool                takes_args;
} error_info_t;

static error_info_t error_info[] = {
    [c4m_err_open_file] = {
        c4m_err_open_file,
        "open_file",
        "Could not open the file: [em]{}[/]",
        true,
    },
    [c4m_err_lex_stray_cr] = {
        c4m_err_lex_stray_cr,
        "stray_cr",
        "Found carriage return ('\r') without a paired newline ('\n')",
        false,
    },
    [c4m_err_lex_eof_in_comment] = {
        c4m_err_lex_eof_in_comment,
        "eof_in_comment",
        "Found end of file when reading a long (/* */ style) comment.",
        false,
    },
    [c4m_err_lex_invalid_char] = {
        c4m_err_lex_invalid_char,
        "invalid_char",
        "Invalid character outside of strings or char constants",
        false,
    },
    [c4m_err_lex_eof_in_str_lit] = {
        c4m_err_lex_eof_in_str_lit,
        "eof_in_str_lit",
        "Found end of file inside a triple-quote string",
        false,
    },
    [c4m_err_lex_nl_in_str_lit] = {
        c4m_err_lex_nl_in_str_lit,
        "nl_in_str_lit",
        "Missing closing quote (\")",
        false,
    },
    [c4m_err_lex_eof_in_char_lit] = {
        c4m_err_lex_eof_in_char_lit,
        "eof_in_char_lit",
        "Unterminated character literal",
        false,
    },
    [c4m_err_lex_nl_in_char_lit] = {
        c4m_err_lex_nl_in_char_lit,
        "nl_in_char_lit",
        "Unterminated character literal",
        false,
    },
    [c4m_err_lex_extra_in_char_lit] = {
        c4m_err_lex_extra_in_char_lit,
        "extra_in_char_lit",
        "Extra character(s) in character literal",
        false,
    },
    [c4m_err_lex_esc_in_esc] = {
        c4m_err_lex_esc_in_esc,
        "esc_in_esc",
        "Escaped backslashes not allowed when specifying a character with "
        "[i]\\x[/], [i]\\X[/], [i]\\u[/], or [i]\\U[/]",
        false,
    },
    [c4m_err_lex_invalid_float_lit] = {
        c4m_err_lex_invalid_float_lit,
        "invalid_float_lit",
        "Invalid float literal",
        false,
    },
    [c4m_err_lex_float_oflow] = {
        c4m_err_lex_float_oflow,
        "float_oflow",
        "Float value overflows the maximum representable value",
        false,
    },
    [c4m_err_lex_float_uflow] = {
        c4m_err_lex_float_uflow,
        "float_uflow",
        "Float underflow error",
        false,
    },
    [c4m_err_lex_int_oflow] = {
        c4m_err_lex_int_oflow,
        "int_oflow",
        "Integer literal is too large for any data type",
        false,
    },
    [c4m_err_parse_continue_outside_loop] = {
        c4m_err_parse_continue_outside_loop,
        "continue_outside_loop",
        "[em]continue[/] not allowed outside of loop bodies",
        false,
    },
    [c4m_err_parse_break_outside_loop] = {
        c4m_err_parse_break_outside_loop,
        "break_outside_loop",
        "[em]break[/] not allowed outside of loop bodies",
        false,
    },
    [c4m_err_parse_return_outside_func] = {
        c4m_err_parse_return_outside_func,
        "return_outside_func",
        "[em]return[/] not allowed outside of function bodies",
        false,
    },
    [c4m_err_parse_expected_stmt_end] = {
        c4m_err_parse_expected_stmt_end,
        "expected_stmt_end",
        "Expected the end of a statement here",
        false,
    },
    [c4m_err_parse_unexpected_after_expr] = {
        c4m_err_parse_unexpected_after_expr,
        "unexpected_after_expr",
        "Unexpected content after an expression",
        false,
    },
    [c4m_err_parse_expected_brace] = {
        c4m_err_parse_expected_brace,
        "expected_brace",
        "Expected a brace",
        false,
    },
    [c4m_err_parse_expected_range_tok] = {
        c4m_err_parse_expected_range_tok,
        "expected_range_tok",
        "Expected [em]to[/] or [em]:[/] here",
        false,
    },
    [c4m_err_parse_eof] = {
        c4m_err_parse_eof,
        "eof",
        "Unexpected end of file due to unclosed block",
        false,
    },
    [c4m_err_parse_bad_use_uri] = {
        c4m_err_parse_bad_use_uri,
        "bad_use_uri",
        "The URI after [em]use ... from[/] must be a quoted string literal",
        false,
    },
    [c4m_err_parse_id_expected] = {
        c4m_err_parse_id_expected,
        "id_expected",
        "Expected a variable name or other identifier here",
        false,
    },
    [c4m_err_parse_id_member_part] = {
        c4m_err_parse_id_member_part,
        "id_member_part",
        "Expected a (possibly dotted) name here",
        false,
    },
    [c4m_err_parse_not_docable_block] = {
        c4m_err_parse_not_docable_block,
        "not_docable_block",
        "Found documentation in a block that does not use documentation",
        false,
    },
    [c4m_err_parse_for_syntax] = {
        c4m_err_parse_for_syntax,
        "for_syntax",
        "Invalid syntax in [em]for[/] statement",
        false,
    },
    [c4m_err_parse_missing_type_rbrak] = {
        c4m_err_parse_missing_type_rbrak,
        "missing_type_rbrak",
        "Missing right bracket ([em]][/]) in type specifier",
        false,
    },
    [c4m_err_parse_bad_tspec] = {
        c4m_err_parse_bad_tspec,
        "bad_tspec",
        "Invalid symbol found in type specifier",
        false,
    },
    [c4m_err_parse_vararg_wasnt_last_thing] = {
        c4m_err_parse_vararg_wasnt_last_thing,
        "vararg_wasnt_last_thing",
        "Variable argument specifier ([em]*[/] can only appear in final "
        "function parameter",
        false,
    },
    [c4m_err_parse_fn_param_syntax] = {
        c4m_err_parse_fn_param_syntax,
        "fn_param_syntax",
        "Invalid syntax for a [em]parameter[/] block",
        false,
    },
    [c4m_err_parse_enums_are_toplevel] = {
        c4m_err_parse_enums_are_toplevel,
        "enums_are_toplevel",
        "Enumerations are only allowed at the top-level of a file",
        false,
    },
    [c4m_err_parse_funcs_are_toplevel] = {
        c4m_err_parse_funcs_are_toplevel,
        "funcs_are_toplevel",
        "Functions are only allowed at the top-level of a file",
        false,
    },
    [c4m_err_parse_parameter_is_toplevel] = {
        c4m_err_parse_parameter_is_toplevel,
        "parameter_is_toplevel",
        "Parameter blocks are only allowed at the top-level of a file",
        false,
    },
    [c4m_err_parse_extern_is_toplevel] = {
        c4m_err_parse_extern_is_toplevel,
        "extern_is_toplevel",
        "Extern blocks are only allowed at the top-level of a file",
        false,
    },
    [c4m_err_parse_confspec_is_toplevel] = {
        c4m_err_parse_confspec_is_toplevel,
        "Confspec blocks are only allowed at the top-level of a file",
        "",
        false,
    },
    [c4m_err_parse_bad_confspec_sec_type] = {
        c4m_err_parse_bad_confspec_sec_type,
        "bad_confspec_sec_type",
        "Expected a [i]confspec[/] type ([i]named, singleton or root[/]), "
        "but got [em]{}[/]",
        true,
    },
    [c4m_err_parse_invalid_token_in_sec] = {
        c4m_err_parse_invalid_token_in_sec,
        "invalid_token_in_sec",
        "Invalid symbol in section block",
        false,
    },
    [c4m_err_parse_expected_token] = {
        c4m_err_parse_expected_token,
        "expected_token",
        "Expected a [em]{}[/] token here",
        true,
    },
    [c4m_err_parse_invalid_sec_part] = {
        c4m_err_parse_invalid_sec_part,
        "invalid_sec_part",
        "Invalid in a section property",
        true,
    },
    [c4m_err_parse_invalid_field_part] = {
        c4m_err_parse_invalid_field_part,
        "invalid_field_part",
        "Invalid in a field property",
        false,
    },
    [c4m_err_parse_no_empty_tuples] = {
        c4m_err_parse_no_empty_tuples,
        "no_empty_tuples",
        "Empty tuples are not allowed",
        false,
    },
    [c4m_err_parse_lit_or_id] = {
        c4m_err_parse_lit_or_id,
        "lit_or_id",
        "Expected either a literal or an identifier",
        false,
    },
    [c4m_err_parse_1_item_tuple] = {
        c4m_err_parse_1_item_tuple,
        "1_item_tuple",
        "Tuples with only one item are not allowed",
        false,
    },
    [c4m_err_parse_decl_kw_x2] = {
        c4m_err_parse_decl_kw_x2,
        "decl_kw_x2",
        "Duplicate declaration keyword",
        false,
    },
    [c4m_err_parse_decl_2_scopes] = {
        c4m_err_parse_decl_2_scopes,
        "decl_2_scopes",
        "Invalid declaration; cannot be part of two different scopes",
        false,
    },
    [c4m_err_parse_case_else_or_end] = {
        c4m_err_parse_case_else_or_end,
        "case_else_or_end",
        "Expected either a new case, an [em]else[/] block, or an ending brace",
        false,
    },
    [c4m_err_parse_case_body_start] = {
        c4m_err_parse_case_body_start,
        "case_body_start",
        "Invalid start for a case body",
        false,
    },
    [c4m_err_parse_empty_enum] = {
        c4m_err_parse_empty_enum,
        "empty_enum",
        "Enumeration cannot be empty",
        false,
    },
    [c4m_err_parse_enum_item] = {
        c4m_err_parse_enum_item,
        "enum_item",
        "Invalid enumeration item",
        false,
    },
    [c4m_err_parse_need_simple_lit] = {
        c4m_err_parse_need_simple_lit,
        "need_simple_lit",
        "Expected a basic literal value",
        false,
    },
    [c4m_err_parse_need_str_lit] = {
        c4m_err_parse_need_str_lit,
        "need_str_lit",
        "Expected a string literal",
        false,
    },
    [c4m_err_parse_need_bool_lit] = {
        c4m_err_parse_need_bool_lit,
        "need_bool_lit",
        "Expected a boolean literal",
        false,
    },
    [c4m_err_parse_formal_expect_id] = {
        c4m_err_parse_formal_expect_id,
        "formal_expect_id",
        "Expect an ID for a formal parameter",
        false,
    },
    [c4m_err_parse_bad_extern_field] = {
        c4m_err_parse_bad_extern_field,
        "bad_extern_field",
        "Bad [em]extern[/] field",
        false,
    },
    [c4m_err_parse_extern_sig_needed] = {
        c4m_err_parse_extern_sig_needed,
        "extern_sig_needed",
        "Signature needed for [em]extern[/] block",
        false,
    },
    [c4m_err_parse_extern_bad_hold_param] = {
        c4m_err_parse_extern_bad_hold_param,
        "extern_bad_hold_param",
        "Invalid value for parameter's [em]hold[/] property",
        false,
    },
    [c4m_err_parse_extern_bad_alloc_param] = {
        c4m_err_parse_extern_bad_alloc_param,
        "extern_bad_alloc_param",
        "Invalid value for parameter's [em]alloc[/] property",
        false,
    },
    [c4m_err_parse_extern_bad_prop] = {
        c4m_err_parse_extern_bad_prop,
        "extern_bad_prop",
        "Invalid property",
        false,
    },
    [c4m_err_parse_enum_value_type] = {
        c4m_err_parse_enum_value_type,
        "enum_value_type",
        "Enum values must only be integers or strings",
        false,
    },
    [c4m_err_parse_csig_id] = {
        c4m_err_parse_csig_id,
        "csig_id",
        "Expected identifier here for extern signature",
        false,
    },
    [c4m_err_parse_bad_ctype_id] = {
        c4m_err_parse_bad_ctype_id,
        "bad_ctype_id",
        "Invalid [em]ctype[/] identifier",
        false,
    },
    [c4m_err_parse_mod_param_no_const] = {
        c4m_err_parse_mod_param_no_const,
        "mod_param_no_const",
        "[em]const[/] variables may not be used in parameters",
        false,
    },
    [c4m_err_parse_bad_param_start] = {
        c4m_err_parse_bad_param_start,
        "bad_param_start",
        "Invalid start to a parameter block; expected a variable or attribute",
        false,
    },
    [c4m_err_parse_param_def_and_callback] = {
        c4m_err_parse_param_def_and_callback,
        "param_def_and_callback",
        "Parameters cannot contain both [em]default[/] and [em]callback[/]"
        "properties",
        false,
    },
    [c4m_err_parse_param_dupe_prop] = {
        c4m_err_parse_param_dupe_prop,
        "param_dupe_prop",
        "Duplicate parameter property",
        false,
    },
    [c4m_err_parse_param_invalid_prop] = {
        c4m_err_parse_param_invalid_prop,
        "param_invalid_prop",
        "Invalid name for a parameter property",
        false,
    },
    [c4m_err_parse_bad_expression_start] = {
        c4m_err_parse_bad_expression_start,
        "bad_expression_start",
        "Invalid start to an expression",
        false,
    },
    [c4m_err_parse_missing_expression] = {
        c4m_err_parse_missing_expression,
        "missing_expression",
        "Expecting an expression here",
        false,
    },
    [c4m_err_parse_no_lit_mod_match] = {
        c4m_err_parse_no_lit_mod_match,
        "no_lit_mod_match",
        "Could not find the named literal modifier, so cannot instantiate.",
        false,
    },
    [c4m_err_parse_invalid_lit_char] = {
        c4m_err_parse_invalid_lit_char,
        "invalid_lit_char",
        "Found a character in this literal that is invalid.",
        false,
    },
    [c4m_err_parse_lit_overflow] = {
        c4m_err_parse_lit_overflow,
        "lit_overflow",
        "Literal value is too large for the data type.",
        false,
    },
    [c4m_err_parse_lit_underflow] = {
        c4m_err_parse_lit_underflow,
        "lit_underflow",
        "Value is too small to be represented in this data type.",
        false,
    },
    [c4m_err_parse_lit_odd_hex] = {
        c4m_err_parse_lit_odd_hex,
        "lit_odd_hex",
        "Hex literals need an even number of digits (one digit is 1/2 a byte).",
        false,
    },
    [c4m_err_parse_lit_invalid_neg] = {
        c4m_err_parse_lit_invalid_neg,
        "lit_invalid_neg",
        "Declared type may not have a negative value.",
        false,
    },
    [c4m_err_invalid_redeclaration] = {
        c4m_err_invalid_redeclaration,
        "invalid_redeclaration",
        "Re-declaration of {} is not allowed here; previous declaration of "
        "{} was here: {}:{}:{}",
        true,
    },
    [c4m_err_omit_string_enum_value] = {
        c4m_err_omit_string_enum_value,
        "omit_string_enum_value",
        "Cannot omit values for enumerations with string values.",
        false,
    },
    [c4m_err_invalid_enum_lit_type] = {
        c4m_err_invalid_enum_lit_type,
        "invalid_enum_lit_type",
        "Enumerations must contain either integer values or string values."
        " No other values are permitted.",
        false,
    },
    [c4m_err_enum_str_int_mix] = {
        c4m_err_enum_str_int_mix,
        "enum_str_int_mix",
        "Cannot mix string and integer values in one enumeration.",
        false,
    },
    [c4m_err_dupe_enum] = {
        c4m_err_dupe_enum,
        "dupe_enum",
        "Duplicate value in the same [em]enum[/] is not allowed.",
        false,
    },
    [c4m_info_dupe_import] = {
        c4m_info_dupe_import,
        "dupe_import",
        "Multiple calls to [em]use[/] with the exact same package. "
        "Each statement runs the module top-level code each time "
        "execution reaches the [em]use[/] statement.",
        false,
    },
    [c4m_err_last] = {
        c4m_err_last,
        "last",
        "If you see this error, the compiler writer messed up bad",
        false,
    },
};

c4m_utf8_t *
c4m_format_error_message(c4m_compile_error *one_err, bool add_code_name)
{
    // This formats JUST the error message. We look it up in the error
    // info table, and apply styling and arguments, if appropriate.

    error_info_t *info = (error_info_t *)&error_info[one_err->code];
    c4m_utf8_t   *result;

    if (info->takes_args) {
        result = c4m_cstr_array_format(info->message,
                                       one_err->num_args,
                                       one_err->msg_parameters);
    }
    else {
        result = c4m_rich_lit(info->message);
    }

    if (add_code_name) {
        c4m_utf8_t *code = c4m_cstr_format(" ([em]{}[/])",
                                           c4m_new_utf8(info->name));
        result           = c4m_str_concat(result, code);
    }

    return result;
}

static c4m_utf8_t *error_constant = NULL;
static c4m_utf8_t *warn_constant  = NULL;
static c4m_utf8_t *info_constant  = NULL;

static inline c4m_utf8_t *
format_severity(c4m_compile_error *err)
{
    switch (err->severity) {
    case c4m_err_severity_warning:
        return warn_constant;
    case c4m_err_severity_info:
        return info_constant;
    default:
        return error_constant;
    }
}

static inline c4m_utf8_t *
format_location(c4m_file_compile_ctx *ctx, c4m_compile_error *err)
{
    c4m_token_t *tok = err->current_token;

    if (!tok) {
        return c4m_cstr_format("[b]{}[/]", ctx->path);
    }
    return c4m_cstr_format("[b]{}:{:n}:{:n}:[/]",
                           ctx->path,
                           c4m_box_i32(tok->line_no),
                           c4m_box_i32(tok->line_offset));
}

c4m_grid_t *
c4m_format_errors(c4m_file_compile_ctx *ctx)
{
    if (ctx->errors == NULL) {
        return NULL;
    }

    if (error_constant == NULL) {
        error_constant = c4m_rich_lit("[red]error:[/]");
        warn_constant  = c4m_rich_lit("[yellow]warning:[/]");
        info_constant  = c4m_rich_lit("[atomic lime]info:[/]");
    }

    int64_t n = c4m_xlist_len(ctx->errors);

    if (n == 0) {
        return NULL;
    }

    c4m_grid_t *table = c4m_new(c4m_tspec_grid(),
                                c4m_kw("container_tag",
                                       c4m_ka("error_grid"),
                                       "td_tag",
                                       c4m_ka("tcol"),
                                       "start_rows",
                                       c4m_ka(n),
                                       "start_cols",
                                       c4m_ka(3),
                                       "header_rows",
                                       c4m_ka(0)));

    for (int i = 0; i < n; i++) {
        c4m_compile_error *err = c4m_xlist_get(ctx->errors, i, NULL);
        c4m_xlist_t       *row = c4m_new(c4m_tspec_xlist(c4m_tspec_utf8()));

        c4m_xlist_append(row, format_severity(err));
        c4m_xlist_append(row, format_location(ctx, err));
        c4m_xlist_append(row, c4m_format_error_message(err, true));

        c4m_grid_add_row(table, row);
    }

    c4m_set_column_style(table, 0, "full_snap");
    c4m_set_column_style(table, 1, "full_snap");

    return table;
}

c4m_compile_error *
c4m_base_add_error(c4m_xlist_t        *err_list,
                   c4m_compile_error_t code,
                   c4m_token_t        *tok,
                   c4m_err_severity_t  severity,
                   va_list             args)
{
    va_list arg_counter;
    int     num_args = 0;

    va_copy(arg_counter, args);
    while (va_arg(arg_counter, void *) != NULL) {
        num_args++;
    }
    va_end(arg_counter);

    c4m_compile_error *err = c4m_gc_flex_alloc(c4m_compile_error,
                                               c4m_str_t *,
                                               num_args,
                                               GC_SCAN_ALL);

    err->code          = code;
    err->current_token = tok;
    err->severity      = severity;

    if (num_args) {
        for (int i = 0; i < num_args; i++) {
            err->msg_parameters[i] = va_arg(args, c4m_str_t *);
        }
        err->num_args = num_args;
    }

    c4m_xlist_append(err_list, err);

    return err;
}

c4m_compile_error *
_c4m_error_from_token(c4m_file_compile_ctx *ctx,
                      c4m_compile_error_t   code,
                      c4m_token_t          *tok,
                      ...)
{
    c4m_compile_error *result;

    va_list args;
    va_start(args, tok);
    result = c4m_base_add_error(ctx->errors,
                                code,
                                tok,
                                c4m_err_severity_error,
                                args);
    va_end(args);

    return result;
}

#define c4m_base_err_decl(func_name, severity_value)            \
    c4m_compile_error *                                         \
    func_name(c4m_file_compile_ctx *ctx,                        \
              c4m_compile_error_t   code,                       \
              c4m_tree_node_t      *node,                       \
              ...)                                              \
    {                                                           \
        c4m_compile_error *result;                              \
        c4m_pnode_t       *pnode = c4m_tree_get_contents(node); \
                                                                \
        va_list args;                                           \
        va_start(args, node);                                   \
        result = c4m_base_add_error(ctx->errors,                \
                                    code,                       \
                                    pnode->token,               \
                                    severity_value,             \
                                    args);                      \
        va_end(args);                                           \
                                                                \
        return result;                                          \
    }

c4m_base_err_decl(_c4m_add_error, c4m_err_severity_error);
c4m_base_err_decl(_c4m_add_warning, c4m_err_severity_warning);
c4m_base_err_decl(_c4m_add_info, c4m_err_severity_info);

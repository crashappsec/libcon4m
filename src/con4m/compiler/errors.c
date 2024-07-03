#include "con4m.h"

typedef struct {
    c4m_compile_error_t errorid;
    alignas(8) char *name;
    char *message;
    bool  takes_args;
} error_info_t;

static error_info_t error_info[] = {
    [c4m_err_open_file] = {
        c4m_err_open_file,
        "open_file",
        "Could not open the file [i]{}[/]. Reason: [em]{}[/]",
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
        "Invalid declaration; cannot be both global and local ([em]var[/])",
        false,
    },
    [c4m_err_parse_decl_const_not_const] = {
        c4m_err_parse_decl_const_not_const,
        "const_not_const",
        "Cannot declare variables to be both "
        "[em]once[/] (each instantiation can be set dynamically once) and "
        "[em]const[/] (must have a value that can be fully computed before running).",
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
    [c4m_err_parse_extern_dup] = {
        c4m_err_parse_extern_dup,
        "extern_dup",
        "The field [em]{}[/] cannot appear twice in one [i]extern[/] block.",
        true,
    },
    [c4m_err_parse_extern_need_local] = {
        c4m_err_parse_extern_need_local,
        "extern_need_local",
        "Extern blocks currently define foreign functions only, and require a "
        "[em]local[/] property to define the con4m signature.",
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
        "Could not find a handler for the literal modifier [em]{}[/] "
        "for literals using [i]{}[/] syntax.",
        true,
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
    [c4m_err_parse_for_assign_vars] = {
        c4m_err_parse_for_assign_vars,
        "for_assign_vars",
        "Too many assignment variables in [em]for[/] loop. Must have one in "
        "most cases, and two when iterating over a dictionary type.",
        false,
    },
    [c4m_err_parse_lit_bad_flags] = {
        c4m_err_parse_lit_bad_flags,
        "lit_bad_flags",
        "Flag literals must currently start with [em]0x[/] and contain only "
        "hex characters after that.",
        false,
    },
    [c4m_err_invalid_redeclaration] = {
        c4m_err_invalid_redeclaration,
        "invalid_redeclaration",
        "Re-declaration of [em]{}[/] is not allowed here; "
        "previous declaration of "
        "{} was here: [i]{}[/]",
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
    [c4m_err_unk_primitive_type] = {
        c4m_err_unk_primitive_type,
        "unk_primitive_type",
        "Type name is not a known primitive type.",
        false,
    },
    [c4m_err_unk_param_type] = {
        c4m_err_unk_param_type,
        "unk_param_type",
        "Type name is not a known parameterized type.",
        false,
    },
    [c4m_err_no_logring_yet] = {
        c4m_err_no_logring_yet,
        "no_logring_yet",
        "Log rings are not yet implemented.",
        false,
    },
    [c4m_err_no_params_to_hold] = {
        c4m_err_no_params_to_hold,
        "no_params_to_hold",
        "Hold values can't be specified for an imported function without "
        "any parameters.",
        false,
    },
    [c4m_warn_dupe_hold] = {
        c4m_warn_dupe_hold,
        "dupe_hold",
        "The [em]hold[/] property is already specified for this parameter. ",
        false,
    },
    [c4m_warn_dupe_alloc] = {
        c4m_warn_dupe_alloc,
        "dupe_alloc",
        "The [em]alloc[/] property is already specified for this parameter.",
        false,
    },
    [c4m_err_bad_hold_name] = {
        c4m_err_bad_hold_name,
        "bad_hold_name",
        "Parameter name specified for the [em]hold[/] property here was not "
        "listed as a local parameter name.",
        false,
    },
    [c4m_err_bad_alloc_name] = {
        c4m_err_bad_alloc_name,
        "bad_alloc_name",
        "Parameter name specified for the [em]alloc[/] property here was not "
        "listed as a local parameter name.",
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
    [c4m_warn_dupe_require] = {
        c4m_warn_dupe_require,
        "dupe_require",
        "Duplicate entry in spec for [em]required[/] subsections.",
        false,
    },
    [c4m_warn_dupe_allow] = {
        c4m_warn_dupe_allow,
        "dupe_allow",
        "Duplicate entry in spec for [em]allowed[/] subsections.",
        false,
    },
    [c4m_warn_require_allow] = {
        c4m_warn_require_allow,
        "require_allow",
        "It's redundant to put a subsection on both the [em]required[/] "
        "and [em]allowed[/] lists.",
        false,
    },
    [c4m_err_spec_bool_required] = {
        c4m_err_spec_bool_required,
        "spec_bool_required",
        "Specification field requires a boolean value.",
        false,
    },
    [c4m_err_spec_callback_required] = {
        c4m_err_spec_callback_required,
        "spec_callback_required",
        "Specification field requires a callback literal.",
        false,
    },
    [c4m_warn_dupe_exclusion] = {
        c4m_warn_dupe_exclusion,
        "dupe_exclusion",
        "Redundant entry in field spec for [em]exclusion[/] (same field"
        "is excluded multiple times)",
        false,
    },
    [c4m_err_dupe_spec_field] = {
        c4m_err_dupe_spec_field,
        "dupe_spec_field",
        "Field inside section specification has already been specified.",
        false,
    },
    [c4m_err_dupe_root_section] = {
        c4m_err_dupe_root_section,
        "dupe_root_section",
        "Configuration section root section additions currently "
        "may only appear once in a module.",
        false,
    },
    [c4m_err_dupe_section] = {
        c4m_err_dupe_section,
        "dupe_section",
        "Multiple specifications for the same section are not allowed.",
        false,
    },
    [c4m_err_dupe_confspec] = {
        c4m_err_dupe_confspec,
        "dupe_confspec",
        "Modules may only have a single [em]confspec[/] section.",
        false,
    },
    [c4m_err_dupe_param] = {
        c4m_err_dupe_param,
        "dupe_param",
        "Multiple parameter specifications for the same parameter are not allowed in one module.",
        false,
    },
    [c4m_err_const_param] = {
        c4m_err_const_param,
        "const_param",
        "Module parameters may not be [em]const[/] variables.",
        false,
    },
    [c4m_err_malformed_url] = {
        c4m_err_malformed_url,
        "malformed_url",
        "URL for module path is invalid.",
        false,
    },
    [c4m_warn_no_tls] = {
        c4m_warn_no_tls,
        "no_tls",
        "URL for module path is insecure.",
        false,
    },
    [c4m_err_search_path] = {
        c4m_err_search_path,
        "search_path",
        "Could not find module in the search path.",
        true,
    },
    [c4m_err_no_http] = {
        c4m_err_no_http,
        "no_http",
        "HTTP and HTTPS support is not yet back in Con4m.",
        false,
    },
    [c4m_info_recursive_use] = {
        c4m_info_recursive_use,
        "recursive_use",
        "This [em]use[/] creates a cyclic import. Items imported from here "
        "will be available, but if there are analysis conflicts in redeclared "
        "symbols, the errors could end up confusing.",
        false,
    },
    [c4m_err_self_recursive_use] = {
        c4m_err_self_recursive_use,
        "self_recursive_use",

        "[em]use[/] statements are not allowed to directly import the current "
        "module.",
        false,
    },
    [c4m_err_redecl_kind] = {
        c4m_err_redecl_kind,
        "redecl_kind",
        "Global symbol [em]{}[/] was previously declared as a [i]{}[/], so "
        "cannot be redeclared as a [i]{}[/]. Previous declaration was: "
        "[strong]{}[/]",
        true,
    },
    [c4m_err_no_redecl] = {
        c4m_err_no_redecl,
        "no_redecl",
        "Redeclaration of [i]{}[/] [em]{}[/] is not allowed. Previous "
        "definition's location was: [strong]{}[/]",
        true,
    },
    [c4m_err_redecl_neq_generics] = {
        c4m_err_redecl_neq_generics,
        "redecl_neq_generics",
        "Redeclaration of [em]{}[/] uses {} type when "
        "re-declared in a different module (redeclarations that name a "
        "type, must name the exact same time as in other modules). "
        "Previous type was: [em]{}[/] vs. current type: [em]{}[/]."
        "Previous definition's location was: [strong]{}[/]",
        true,
    },
    [c4m_err_spec_redef_section] = {
        c4m_err_spec_redef_section,
        "redef_section",
        "Redefinition of [i]confspec[/] sections is not allowed. You can "
        "add data to the [i]root[/] section, but no named sections may "
        "appear twice in any program. Previous declaration of "
        "section [em]{}[/] was: [strong]{}[/]",
        true,
    },
    [c4m_err_spec_redef_field] = {
        c4m_err_spec_redef_field,
        "redef_field",
        "Redefinition of [i]confspec[/] fields are not allowed. You can "
        "add new fields to the [i]root[/] section, but no new ones. "
        "Previous declaration of field [em]{}[/] was: [strong]{}[/]",
        true,
    },
    [c4m_err_spec_locked] = {
        c4m_err_spec_locked,
        "spec_locked",
        "The configuration file spec has been programatically locked, "
        "and as such, cannot be changed here.",
        false,
    },
    [c4m_err_dupe_validator] = {
        c4m_err_dupe_validator,
        "dupe_validator",
        "The root section already has a validator; currently only a single "
        "validator is supported.",
        false,
    },
    [c4m_err_decl_mismatch] = {
        c4m_err_decl_mismatch,
        "decl_mismatch",
        "Right-hand side of assignment has a type [em]({})[/] that is not "
        "compatable with the declared type of the left-hand side [em]({})[/]. "
        "Previous declaration location is: [i]{}[/]",
        true,
    },
    [c4m_err_inconsistent_type] = {
        c4m_err_inconsistent_type,
        "inconsistent_type",
        "The type here [em]({})[/] is not compatable with the "
        "declared type of this variable [em]({})[/]. "
        "Declaration location is: [i]{}[/]",
        true,
    },
    [c4m_err_inconsistent_infer_type] = {
        c4m_err_inconsistent_infer_type,
        "inconsistent_type",
        "The previous type, [em]{}[/], is not compatable with the "
        "type used in this context ([em]{}[/]). ",
        true,
    },
    [c4m_err_inconsistent_item_type] = {
        c4m_err_inconsistent_item_type,
        "inconsistent_item_type",
        "Item types must be either identical to the first item type in "
        " the {}, or must be automatically convertable to that type. "
        "First item type was [em]{}[/], but this one is [em]{}[/]",
        true,
    },
    [c4m_err_decl_mask] = {
        c4m_err_decl_mask,
        "decl_mask",
        "This variable cannot be declared in the [em]{}[/] scope because "
        "there is already a {}-level declaration in the same module at: [i]{}",
        true,
    },
    [c4m_warn_attr_mask] = {
        c4m_warn_attr_mask,
        "attr_mask",
        "This module-level declaration shares a name with an attribute."
        "The module definition will be used throughout this module, "
        "and the attribute will not be available.",
        false,
    },
    [c4m_err_attr_mask] = {
        c4m_err_attr_mask,
        "attr_mask",
        "This global declaration is invalid, because it has the same "
        "name as an existing attribute.",
        false,
    },
    [c4m_err_label_target] = {
        c4m_err_label_target,
        "label_target",
        "The label target [em]{}[/] for this [i]{}[/] statement is not an "
        "enclosing loop.",
        true,
    },
    [c4m_err_fn_not_found] = {
        c4m_err_fn_not_found,
        "fn_not_found",
        "Could not find an implementation for the function [em]{}[/].",
        true,
    },
    [c4m_err_num_params] = {
        c4m_err_num_params,
        "num_params",
        "Wrong number of parameters in call to function [em]{}[/]. "
        "Call site used [i]{}[/] parameters, but the implementation of "
        "that function requires [i]{}[/] parameters.",
        true,
    },
    [c4m_err_calling_non_fn] = {
        c4m_err_calling_non_fn,
        "calling_non_fn",
        "Cannot call [em]{}[/]; it is a [i]{}[/] not a function.",
        true,
    },
    [c4m_err_spec_needs_field] = {
        c4m_err_spec_needs_field,
        "spec_needs_field",
        "Attribute specification says [em]{}[/] must be a [i]{}[/]. It cannot "
        "be used as a field here.",
        true,
    },
    [c4m_err_field_not_spec] = {
        c4m_err_field_not_spec,
        "field_not_spec",
        "Attribute [em]{}[/] does not follow the specticiation. The component "
        "[em]{}[/] is expected to refer to a field, not a "
        "configuration section.",
        true,
    },
    [c4m_err_undefined_section] = {
        c4m_err_undefined_section,
        "undefined_section",
        "The config file spec doesn't support a section named [em]{}[/].",
        true,
    },
    [c4m_err_section_not_allowed] = {
        c4m_err_section_not_allowed,
        "section_not_allowed",
        "The config file spec does not allow [em]{}[/] to be a subsection "
        "here.",
        true,
    },
    [c4m_err_slice_on_dict] = {
        c4m_err_slice_on_dict,
        "slice_on_dict",
        "The slice operator is not supported for data types using dictionary "
        "syntax (e.g., dictionaries or sets).",
        false,
    },
    [c4m_err_bad_slice_ix] = {
        c4m_err_bad_slice_ix,
        "bad_slice_ix",
        "The slice operator only supports integer indices.",
        false,
    },
    [c4m_err_dupe_label] = {
        c4m_err_dupe_label,
        "dupe_label",
        "The loop label [em]{}[/] cannot be used in multiple nested loops. "
        "Note that, in the future, this constraint might apply per-function "
        "context as well.",
        true,
    },
    [c4m_err_iter_name_conflict] = {
        c4m_err_iter_name_conflict,
        "iter_name_conflict",
        "Loop iteration over a dictionary requires two different loop "
        "variable names. Rename one of them.",
        false,
    },
    [c4m_warn_shadowed_var] = {
        c4m_warn_shadowed_var,
        "shadowed_var",
        "Declaration of [em]{}[/] shadows a [i]{}[/] that would otherwise "
        "be in scope here. Shadow value's first location: [i]{}[/]",
        true,
    },
    [c4m_err_dict_one_var_for] = {
        c4m_err_dict_one_var_for,
        "dict_one_var_for",
        "When using [em]for[/] loops to iterate over a dictionary, "
        "you need to define two variables, one to hold the [i]key[/], "
        "the other to hold the associated value.",
        false,
    },
    [c4m_err_future_dynamic_typecheck] = {
        c4m_err_future_dynamic_typecheck,
        "future_dynamic_typecheck",
        "There isn't enough syntactic context at this point for the system "
        "to properly keep track of type info. Usually this happens when "
        "using containers, when we cannot determine, for instance, dict vs. "
        "set vs. list. In the future, this will be fine; in some cases "
        "we will keep more type info, and in others, we will convert to "
        "a runtime check. But for now, please add a type annotation that "
        "at least unambiguously specifies a container type.",
        false,
    },
    [c4m_err_iterate_on_non_container] = {
        c4m_err_iterate_on_non_container,
        "iterate_on_non_container",
        "Cannot iterate over this value, as it is not a container type "
        "(current type is [em]{}[/])",
        true,
    },
    [c4m_err_unary_minus_type] = {
        c4m_err_unary_minus_type,
        "unary_minus_type",
        "Unary minus currently only allowed on signed int types.",
        false,
    },
    [c4m_err_cannot_cmp] = {
        c4m_err_cannot_cmp,
        "cannot_cmp",
        "The two sides of the comparison have incompatible types: "
        "[em]{}[/] vs [em]{}[/]",
        true,
    },
    [c4m_err_range_type] = {
        c4m_err_range_type,
        "range_type",
        "Ranges must consist of two int values.",
        false,
    },
    [c4m_err_switch_case_type] = {
        c4m_err_switch_case_type,
        "switch_case_type",
        "This switch branch has a type ([em]{}[/]) that doesn't match"
        "The type of the object being switched on ([em]{}[/])",
        true,
    },
    [c4m_err_concrete_typeof] = {
        c4m_err_concrete_typeof,
        "concrete_typeof",
        "[em]typeof[/] requires the expression to be of a variable type, "
        "but in this context it is always a [em]{}",
        true,
    },
    [c4m_warn_type_overlap] = {
        c4m_warn_type_overlap,
        "type_overlap",
        "This case in the [em]typeof[/] statement has a type [em]{}[/] "
        "that overlaps with a previous case type: [em]{}[/]",
        true,
    },
    [c4m_err_dead_branch] = {
        c4m_err_dead_branch,
        "dead_branch",
        "This type case ([em]{}[/] is not compatable with the constraints "
        "for the variable you're testing (i.e., this is not a subtype)",
        true,
    },
    [c4m_err_no_ret] = {
        c4m_err_no_ret,
        "no_ret",
        "Function was declared with a return type [em]{}[/], but no "
        "values were returned.",
        true,
    },
    [c4m_err_use_no_def] = {
        c4m_err_use_no_def,
        "use_no_def",
        "Variable is used, but not ever set.",
        false,
    },
    [c4m_err_declared_incompat] = {
        c4m_err_declared_incompat,
        "declared_incompat",
        "The declared type ([em]{}[/]) is not compatable with the type as "
        "used in the function ([em]{}[/])",
        true,
    },
    [c4m_err_too_general] = {
        c4m_err_too_general,
        "too_general",
        "The declared type ([em]{}[/]) is more generic than the implementation "
        "requires ([em]{}[/]). Please use the more specific type (or remove "
        "the declaration)",
        true,
    },
    [c4m_warn_unused_param] = {
        c4m_warn_unused_param,
        "unused_param",
        "The parameter [em]{}[/] is declared, but not used.",
        true,
    },
    [c4m_warn_def_without_use] = {
        c4m_warn_def_without_use,
        "def_without_use",
        "Variable [em]{}[/] is explicitly declared, but not used.",
        true,
    },
    [c4m_err_call_type_err] = {
        c4m_err_call_type_err,
        "call_type_err",
        "Type inferred at call site ([em]{}[/]) is not compatiable with "
        "the implementated type ([em]{}[/]).",
        true,
    },
    [c4m_err_single_def] = {
        c4m_err_single_def,
        "single_def",
        "Variable declared using [em]{}[/] may only be assigned in a single "
        "location. The first declaration is: [i]{}[/]",
        true,
    },
    [c4m_warn_unused_decl] = {
        c4m_warn_unused_decl,
        "unused_decl",
        "Declared variable [em]{}[/] is unused.",
        true,
    },
    [c4m_err_global_remote_def] = {
        c4m_err_global_remote_def,
        "global_remote_def",
        "Global variable [em]{}[/] can only be set in the module that first "
        "declares it. The symbol's origin is: [i]{}",
        true,
    },
    [c4m_err_global_remote_unused] = {
        c4m_err_global_remote_unused,
        "global_remote_unused",
        "Global variable [em]{}[/] is imported via the [i]global[/] keyword, "
        "but is not used here.",
        true,
    },
    [c4m_info_unused_global_decl] = {
        c4m_info_unused_global_decl,
        "unused_global_decl",
        "Global variable is unused.",
        true,
    },
    [c4m_global_def_without_use] = {
        c4m_global_def_without_use,
        "def_without_use",
        "This module is the first to declare the global variable[em]{}[/], "
        "but it is not explicitly initialized. It will be set to a default "
        "value at the beginning of the program.",
        true,
    },
    [c4m_warn_dead_code] = {
        c4m_warn_dead_code,
        "dead_code",
        "This code below this line is not reachable.",
        false,
    },
    [c4m_cfg_use_no_def] = {
        c4m_cfg_use_no_def,
        "use_no_def",
        "The variable [em]{}[/] is not defined here, and cannot be used "
        "without a previous assignment.",
        true,
    },
    [c4m_cfg_use_possible_def] = {
        c4m_cfg_use_possible_def,
        "use_possible_def",
        "It is possible for this use of [em]{}[/] to occur without a previous "
        "assignment to the variable. While program logic might prevent it, "
        "the analysis doesn't see it, so please set a default value in an "
        "encompassing scope.",
        true,
    },
    [c4m_cfg_return_coverage] = {
        c4m_cfg_return_coverage,
        "return_coverage",
        "This function has control paths where no return value is set.",
        false,
    },
    [c4m_err_const_not_provided] = {
        c4m_err_const_not_provided,
        "const_not_provided",
        "[em]{}[/] is declared [i]const[/], but no value was provided.",
        true,
    },
    [c4m_err_augmented_assign_to_slice] = {
        c4m_err_augmented_assign_to_slice,
        "augmented_assign_to_slice",
        "Only regular assignment to a slice is currently allowed.",
        false,
    },
    [c4m_warn_cant_export] = {
        c4m_warn_cant_export,
        "cant_export",
        "Cannot export this function, because there is already a "
        "global with the same name.",
        false,
    },
    [c4m_err_assigned_void] = {
        c4m_err_assigned_void,
        "assigned_void",
        "Cannot assign the results of a function that returns [em]void[/] "
        "(void explicitly is reserved to indicate the function does not use "
        "return values.",
        false,
    },
    [c4m_err_callback_no_match] = {
        c4m_err_callback_no_match,
        "callback_no_match",
        "Callback does not have a matching declaration. It requires either a "
        "con4m function, or an [em]extern[/] declaration.",
        false,
    },
    [c4m_err_callback_bad_target] = {
        c4m_err_callback_bad_target,
        "callback_bad_target",
        "Callback matches a symbol that is defined, but not as a callable "
        "function. First definition is here: {}",
        true,
    },
    [c4m_err_callback_type_mismatch] = {
        c4m_err_callback_type_mismatch,
        "callback_type_mismatch",
        "Declared callback type is not compatable with the implementation "
        "callback type ([em]{}[/] vs declared type [em]{}[/]). "
        " Declaration is here: {}",
        true,
    },
    [c4m_err_tup_ix] = {
        c4m_err_tup_ix,
        "up_ix",
        "Tuple indices currently must be a literal int value.",
        false,
    },
    [c4m_err_tup_ix_bounds] = {
        c4m_err_tup_ix_bounds,
        "tup_ix_bounds",
        "Tuple index is out of bounds for this tuple of type [em]{}[/].",
        true,
    },
    [c4m_warn_may_wrap] = {
        c4m_warn_may_wrap,
        "may_wrap",
        "Automatic integer conversion from unsigned to signed of the same size "
        "can turn large positive values negative.",
        false,
    },
    [c4m_internal_type_error] = {
        c4m_internal_type_error,
        "internal_type_error",
        "Could not type check due to an internal error.",
        false,
    },
    [c4m_err_concrete_index] = {
        c4m_err_concrete_index,
        "concrete_index",
        "Currently, con4m does not generate polymorphic code for indexing "
        "operations; it must be able to figure out the type of the index "
        "statically.",
        false,
    },
    [c4m_err_non_dict_index_type] = {
        c4m_err_non_dict_index_type,
        "non_dict_index_type",
        "Only dictionaries can currently use the index operator with non-"
        "integer types.",
        false,
    },
    [c4m_err_last] = {
        c4m_err_last,
        "last",
        "If you see this error, the compiler writer messed up bad",
        false,
    },
#ifdef C4M_DEV
    [c4m_err_void_print] = {
        c4m_err_void_print,
        "void_print",
        "Development [em]print[/] statement must not take a void value.",
        false,
    },
#endif
};

c4m_utf8_t *
c4m_err_code_to_str(c4m_compile_error_t code)
{
    error_info_t *info = (error_info_t *)&error_info[code];
    return c4m_new_utf8(info->name);
}

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
        if (!ctx->path) {
            ctx->path = c4m_cstr_format("{}.{}",
                                        ctx->package,
                                        ctx->module);
        }
        return c4m_cstr_format("[b]{}[/]", ctx->path);
    }
    return c4m_cstr_format("[b]{}:{:n}:{:n}:[/]",
                           ctx->path,
                           c4m_box_i64(tok->line_no),
                           c4m_box_i64(tok->line_offset + 1));
}

static void
c4m_format_module_errors(c4m_file_compile_ctx *ctx, c4m_grid_t *table)
{
    if (error_constant == NULL) {
        error_constant = c4m_rich_lit("[red]error:[/]");
        warn_constant  = c4m_rich_lit("[yellow]warning:[/]");
        info_constant  = c4m_rich_lit("[atomic lime]info:[/]");
        c4m_gc_register_root(&error_constant, 1);
        c4m_gc_register_root(&warn_constant, 1);
        c4m_gc_register_root(&info_constant, 1);
    }

    int64_t n = c4m_list_len(ctx->errors);

    if (n == 0) {
        return;
    }

    for (int i = 0; i < n; i++) {
        c4m_compile_error *err = c4m_list_get(ctx->errors, i, NULL);
        c4m_list_t        *row = c4m_new(c4m_type_list(c4m_type_utf8()));

        c4m_list_append(row, format_severity(err));
        c4m_list_append(row, format_location(ctx, err));
        c4m_list_append(row, c4m_format_error_message(err, true));

        c4m_grid_add_row(table, row);
    }
}

c4m_grid_t *
c4m_format_errors(c4m_compile_ctx *cctx)
{
    c4m_grid_t *table = c4m_new(c4m_type_grid(),
                                c4m_kw("container_tag",
                                       c4m_ka("error_grid"),
                                       "td_tag",
                                       c4m_ka("tcol"),
                                       "start_cols",
                                       c4m_ka(3),
                                       "header_rows",
                                       c4m_ka(0)));

    int      n           = 0;
    uint64_t num_modules = 0;

    hatrack_dict_item_t *view = hatrack_dict_items_sort(cctx->module_cache,
                                                        &num_modules);

    for (unsigned int i = 0; i < num_modules; i++) {
        c4m_file_compile_ctx *ctx = view[i].value;
        if (ctx->errors != NULL) {
            n += c4m_list_len(ctx->errors);
            c4m_format_module_errors(ctx, table);
        }
    }

    if (!n) {
        return NULL;
    }

    c4m_set_column_style(table, 0, "full_snap");
    c4m_set_column_style(table, 1, "full_snap");

    return table;
}

c4m_list_t *
c4m_compile_extract_all_error_codes(c4m_compile_ctx *cctx)
{
    c4m_list_t          *result      = c4m_list(c4m_type_ref());
    uint64_t             num_modules = 0;
    hatrack_dict_item_t *view;

    view = hatrack_dict_items_sort(cctx->module_cache, &num_modules);

    for (unsigned int i = 0; i < num_modules; i++) {
        c4m_file_compile_ctx *ctx = view[i].value;

        if (ctx->errors != NULL) {
            int n = c4m_list_len(ctx->errors);
            for (int j = 0; j < n; j++) {
                c4m_compile_error *err = c4m_list_get(ctx->errors, j, NULL);

                c4m_list_append(result, (void *)(uint64_t)err->code);
            }
        }
    }

    return result;
}

static void
c4m_err_set_gc_bits(uint64_t *bitfield, int length)
{
    *bitfield = 0x0b;
    for (int i = 4; i < length; i++) {
        c4m_set_bit(bitfield, i);
    }
}

c4m_compile_error *
c4m_new_error(int nargs)
{
    return c4m_gc_flex_alloc(c4m_compile_error,
                             void *,
                             nargs,
                             c4m_err_set_gc_bits);
}

c4m_compile_error *
c4m_base_add_error(c4m_list_t         *err_list,
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

    c4m_compile_error *err = c4m_new_error(num_args);

    err->code          = code;
    err->current_token = tok;
    err->severity      = severity;

    if (num_args) {
        for (int i = 0; i < num_args; i++) {
            err->msg_parameters[i] = va_arg(args, c4m_str_t *);
        }
        err->num_args = num_args;
    }

    c4m_list_append(err_list, err);

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

    ctx->fatal_errors = 1;

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
        if (severity_value == c4m_err_severity_error) {         \
            ctx->fatal_errors = 1;                              \
        }                                                       \
        return result;                                          \
    }
c4m_base_err_decl(_c4m_add_error, c4m_err_severity_error);
c4m_base_err_decl(_c4m_add_warning, c4m_err_severity_warning);
c4m_base_err_decl(_c4m_add_info, c4m_err_severity_info);

void
_c4m_file_load_error(c4m_file_compile_ctx *ctx, c4m_compile_error_t code, ...)
{
    va_list args;

    va_start(args, code);
    c4m_base_add_error(ctx->errors, code, NULL, c4m_err_severity_error, args);
    ctx->fatal_errors = 1;
    va_end(args);
}

void
_c4m_file_load_warn(c4m_file_compile_ctx *ctx, c4m_compile_error_t code, ...)
{
    va_list args;

    va_start(args, code);
    c4m_base_add_error(ctx->errors, code, NULL, c4m_err_severity_warning, args);
    va_end(args);
}

#pragma once
#include "con4m.h"

typedef enum {
    c4m_tt_space,
    c4m_tt_semi,
    c4m_tt_newline,
    c4m_tt_line_comment,
    c4m_tt_lock_attr,
    c4m_tt_plus,
    c4m_tt_minus,
    c4m_tt_mul,
    c4m_tt_long_comment,
    c4m_tt_div,
    c4m_tt_mod,
    c4m_tt_lte,
    c4m_tt_lt,
    c4m_tt_gte,
    c4m_tt_gt,
    c4m_tt_neq,
    c4m_tt_not,
    c4m_tt_colon,
    c4m_tt_assign,
    c4m_tt_cmp,
    c4m_tt_comma,
    c4m_tt_period,
    c4m_tt_lbrace,
    c4m_tt_rbrace,
    c4m_tt_lbracket,
    c4m_tt_rbracket,
    c4m_tt_lparen,
    c4m_tt_rparen,
    c4m_tt_and,
    c4m_tt_or,
    c4m_tt_int_lit,
    c4m_tt_hex_lit,
    c4m_tt_float_lit,
    c4m_tt_string_lit,
    c4m_tt_char_lit,
    c4m_tt_true,
    c4m_tt_false,
    c4m_tt_nil,
    c4m_tt_if,
    c4m_tt_elif,
    c4m_tt_else,
    c4m_tt_for,
    c4m_tt_from,
    c4m_tt_to,
    c4m_tt_break,
    c4m_tt_continue,
    c4m_tt_return,
    c4m_tt_enum,
    c4m_tt_identifier,
    c4m_tt_func,
    c4m_tt_var,
    c4m_tt_global,
    c4m_tt_const,
    c4m_tt_unquoted_lit,
    c4m_tt_backtick,
    c4m_tt_arrow,
    c4m_tt_object,
    c4m_tt_while,
    c4m_tt_in,
    c4m_tt_bit_and,
    c4m_tt_bit_or,
    c4m_tt_bit_xor,
    c4m_tt_shl,
    c4m_tt_shr,
    c4m_tt_typeof,
    c4m_tt_switch,
    c4m_tt_case,
    c4m_tt_plus_eq,
    c4m_tt_minus_eq,
    c4m_tt_mul_eq,
    c4m_tt_div_eq,
    c4m_tt_mod_eq,
    c4m_tt_bit_and_eq,
    c4m_tt_bit_or_eq,
    c4m_tt_bit_xor_eq,
    c4m_tt_shl_eq,
    c4m_tt_shr_eq,
    c4m_tt_sof,
    c4m_tt_eof,
    c4m_tt_lex_error
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
    uint8_t          adjustment; // For keeping track of quoting.
} c4m_token_t;

typedef struct {
    c4m_compile_error_t code;
    // These will probably turn into a tagged union or transparent
    // pointer with a phase indicator, so we can design the aux data
    // appropriate per-phase.
    c4m_token_t        *current_token;
    c4m_str_t          *exception_message;
} c4m_compile_error;

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

    __int128_t   module_id;
    c4m_str_t   *scheme;    // http, https or file; if NULL, then file.
    c4m_str_t   *authority; // http/s only.
    c4m_str_t   *path;      // Path component in the URI.
    c4m_str_t   *package;   // Package name.
    c4m_str_t   *module;    // Module name.
    c4m_utf32_t *raw;       // raw contents read when we do the lex pass.
    c4m_xlist_t *tokens;    // an xlist of x4m_token_t objects;
    c4m_xlist_t *errors;    // an xlist of c4m_compile_errors
} c4m_file_compile_ctx;

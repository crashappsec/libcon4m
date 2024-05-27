#pragma once
#include "con4m.h"

typedef enum {
    c4m_compile_status_struct_allocated,
    c4m_compile_status_code_loaded,
    c4m_compile_status_tree_typed,
    c4m_compile_status_applied_folding,
    c4m_compile_status_generated_code
} c4m_file_compile_status;

typedef struct c4m_file_compile_ctx {
    // The module_id is calculated by combining the package name and
    // the module name, then hashing it with SHA256. This is not
    // necessarily derived from the URI path.
    //
    // Note that packages (and our combining of it and the module) use
    // dotted syntax like with most PLs. When we combine for the hash,
    // we add a dot in there.
    //
    // c4m_new_compile_ctx will add __default__ as the package if none
    // is provided. The URI fields are optional (via API you can just
    // pass raw source as long as you give at least a module name).

    uint64_t                module_id;
    c4m_str_t              *authority;       // http/s only.
    c4m_str_t              *path;            // Fully qualified path
    c4m_str_t              *provided_path;   // Provided in use statement.
    c4m_str_t              *package;         // Package name.
    c4m_str_t              *module;          // Module name.
    c4m_utf32_t            *raw;             // raw contents read during lex pass.
    c4m_xlist_t            *tokens;          // an xlist of x4m_token_t objects;
    c4m_tree_node_t        *parse_tree;
    c4m_xlist_t            *errors;          // an xlist of c4m_compile_errors
    c4m_scope_t            *global_scope;    // Symbols used w/ global scope
    c4m_scope_t            *module_scope;    // Symbols used w/ module scope
    c4m_scope_t            *attribute_scope; // Declared or used attrs
    c4m_scope_t            *imports;
    c4m_dict_t             *parameters;
    c4m_spec_t             *local_confspecs;
    c4m_cfg_node_t         *cfg; // CFG for the module top-level.
    c4m_utf8_t             *short_doc;
    c4m_utf8_t             *long_doc;
    unsigned int            fatal_errors : 1;
    unsigned int            file         : 1;
    unsigned int            secure       : 1;
    c4m_file_compile_status status;
} c4m_file_compile_ctx;

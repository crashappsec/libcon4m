#pragma once
#include "con4m.h"

typedef enum {
    c4m_compile_status_struct_allocated,
    c4m_compile_status_tokenized,
    c4m_compile_status_code_parsed,
    c4m_compile_status_code_loaded,     // parsed w/ declarations processed.
    c4m_compile_status_scopes_merged,   // Merged info global scope,
    c4m_compile_status_tree_typed,      // full symbols and parsing.
    c4m_compile_status_applied_folding, // Skippable and not done yet.
    c4m_compile_status_generated_code
} c4m_file_compile_status;

typedef struct c4m_file_compile_ctx {
#ifdef C4M_DEV
    // Cache all the print nodes to type check before running.
    c4m_list_t *print_nodes;
#endif

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

    c4m_str_t              *module;          // Module name.
    c4m_str_t              *authority;       // http/s only.
    c4m_str_t              *path;            // Fully qualified path
    c4m_str_t              *provided_path;   // Provided in use statement.
    c4m_str_t              *package;         // Package name.
    c4m_utf32_t            *raw;             // raw contents before lex pass.
    c4m_list_t             *tokens;          // an xlist of x4m_token_t objects;
    c4m_tree_node_t        *parse_tree;
    c4m_list_t             *errors;          // an xlist of c4m_compile_errors
    c4m_scope_t            *global_scope;    // Symbols used w/ global scope
    c4m_scope_t            *module_scope;    // Symbols used w/ module scope
    c4m_scope_t            *attribute_scope; // Declared or used attrs
    c4m_scope_t            *imports;
    c4m_dict_t             *parameters;
    c4m_spec_t             *local_confspecs;
    c4m_cfg_node_t         *cfg; // CFG for the module top-level.
    c4m_utf8_t             *short_doc;
    c4m_utf8_t             *long_doc;
    c4m_list_t             *fn_def_syms; // Cache of fns defined.
    c4m_zmodule_info_t     *module_object;
    c4m_list_t             *call_patch_locs;
    c4m_list_t             *callback_literals;
    c4m_list_t             *extern_decls;
    uint64_t                module_id; // Module hash.
    int32_t                 static_size;
    uint32_t                num_params;
    uint32_t                local_module_id; // Index in object file.
    unsigned int            fatal_errors : 1;
    unsigned int            file         : 1;
    unsigned int            secure       : 1;
    c4m_file_compile_status status;

} c4m_file_compile_ctx;

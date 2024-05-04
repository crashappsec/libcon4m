#pragma once
#include "con4m.h"

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
    c4m_str_t       *scheme;          // http, https or file; NULL == file.
    c4m_str_t       *authority;       // http/s only.
    c4m_str_t       *path;            // Path component in the URI.
    c4m_str_t       *package;         // Package name.
    c4m_str_t       *module;          // Module name.
    c4m_utf32_t     *raw;             // raw contents read during lex pass.
    c4m_xlist_t     *tokens;          // an xlist of x4m_token_t objects;
    c4m_tree_node_t *parse_tree;
    c4m_xlist_t     *errors;          // an xlist of c4m_compile_errors
    c4m_scope_t     *global_scope;    // Symbols used w/ global scope
    c4m_scope_t     *module_scope;    // Symbols used w/ module scope
    c4m_scope_t     *attribute_scope; // Declared or used attrs
    c4m_scope_t     *imports;
} c4m_file_compile_ctx;

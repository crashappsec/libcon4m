#pragma once
#include "con4m.h"

typedef struct c4m_module_t {
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

    struct c4m_ct_module_info_t *ct;           // Compile-time only info.
    c4m_str_t                   *name;         // Module name.
    c4m_str_t                   *path;         // Fully qualified path
    c4m_str_t                   *package;      // Package name.
    c4m_str_t                   *full_uri;     // Abs path / URL if found.
    c4m_utf32_t                 *source;       // raw contents before lex pass.
    c4m_scope_t                 *module_scope; // Symbols used w/ module scope
    c4m_list_t                  *instructions;
    c4m_dict_t                  *parameters;
    c4m_utf8_t                  *short_doc;
    c4m_utf8_t                  *long_doc;
    uint64_t                     modref;    // A unique ref for  module w/o src
    int32_t                      static_size;
    uint32_t                     module_id; // Index in object file.
} c4m_module_t;

extern void _c4m_set_package_search_path(c4m_utf8_t *, ...);

#pragma once
#include "con4m.h"

typedef struct {
    c4m_file_compile_ctx *entry_point;
    c4m_dict_t           *module_cache;
    c4m_xlist_t          *module_ordering;
    c4m_scope_t          *final_attrs;
    c4m_scope_t          *final_globals;
    c4m_spec_t           *final_spec;
    c4m_set_t            *backlog;   // Modules we need to process.
    c4m_set_t            *processed; // Modules we've finished with.
    bool                  fatality;
} c4m_compile_ctx;

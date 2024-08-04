#pragma once
#include "con4m.h"

// This is a kind of INTERFACE to static objects.  At compile time, we
// essentially assign out indexes into this array; everything added to
// it gets 64 bits reserved.
//
// We give strings their own space so that, during incremental
// compilation, we can de-dupe any string objects.
//
// At the end of compilation, we fill this out and marshal it.

typedef struct {
    c4m_static_memory      *memory_layout;
    c4m_dict_t             *str_consts;
    c4m_dict_t             *obj_consts;
    c4m_dict_t             *value_consts;
    c4m_scope_t            *final_attrs;
    c4m_scope_t            *final_globals;
    c4m_spec_t             *final_spec;
    c4m_module_compile_ctx *entry_point;
    c4m_module_compile_ctx *sys_package;
    c4m_dict_t             *module_cache;
    c4m_list_t             *module_ordering;
    c4m_set_t              *backlog;   // Modules we need to process.
    c4m_set_t              *processed; // Modules we've finished with.
    // Object location, instead of the place to unmarshal it from.
    // Since we have to unmarshal into writable space, we keep this
    // data seprately from const data.
    c4m_dict_t             *instance_map;
    c4m_dict_t             *str_map;
    int64_t                 const_memoid; // Must start at 1.

    // New static memory implementation.  The object / value bits in
    // the c4m_static_memory struct will be NULL until we actually go
    // to save out the object file. Until then, we keep a dictionary
    // of memos that map the memory address of objects to save to
    // their index in the appropriate list.

    bool fatality;
} c4m_compile_ctx;

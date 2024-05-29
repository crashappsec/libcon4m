#pragma once
#include "con4m.h"

typedef struct {
    c4m_scope_t *final_attrs;
    c4m_scope_t *final_globals;

    c4m_spec_t           *final_spec;
    c4m_file_compile_ctx *entry_point;
    c4m_dict_t           *module_cache;
    c4m_xlist_t          *module_ordering;
    c4m_set_t            *backlog;   // Modules we need to process.
    c4m_set_t            *processed; // Modules we've finished with.
    bool                  fatality;
    c4m_buf_t            *const_data;
    c4m_buf_t            *const_instantiations;
    c4m_dict_t           *const_memos;
    c4m_dict_t           *const_instance_map;
    c4m_stream_t         *const_stream;
    // Tbject location, instead of the place to unmarshal it from.
    // Since we have to unmarshal into writable space, we keep this
    // data seprately from const data.
    //
    // These instantiations will get put in their own separate heap
    // and will me mprotect()'d.
    c4m_dict_t           *instance_map;
    c4m_dict_t           *str_map;
    int64_t               const_memoid;
    // index for which the next marshaled (non-value) const will go.
    int32_t               const_instantiation_id;
    // offset index for the next statically allocated object we add.
    int32_t               static_offset;
} c4m_compile_ctx;

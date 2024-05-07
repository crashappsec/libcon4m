#pragma once
#include "con4m.h"

typedef struct {
    union {
        c4m_type_t *type;
        c4m_utf8_t *type_pointer;
    } tinfo;

    // The 'stashed options' field is for holding data specific to the
    // builtin validation routine we run. In our first pass, it will
    // start out as either a partially evaluated literal (for choices)
    // or as a raw parse node to evaluate later (for range nodes).
    //
    // We defer compile-time evaluation when extracting symbols until
    // we have everything explicitly declared available to us, so that
    // we can allow for as much constant folding as possible.  This
    // even includes loading exported symbols from dependent modules.

    void            *stashed_options;
    c4m_tree_node_t *declaration_node;
    c4m_utf8_t      *name;
    c4m_utf8_t      *short_doc;
    c4m_utf8_t      *long_doc;
    c4m_utf8_t      *deferred_type_field; // The field that contains our type.
    void            *default_value;
    void            *validator;
    c4m_set_t       *exclusions;
    unsigned int     user_def_ok       : 1;
    unsigned int     hidden            : 1;
    unsigned int     required          : 1;
    unsigned int     lock_on_write     : 1;
    unsigned int     default_provided  : 1;
    unsigned int     validate_range    : 1;
    unsigned int     validate_choice   : 1;
    unsigned int     have_type_pointer : 1;

} c4m_spec_field_t;

typedef struct {
    c4m_tree_node_t *declaration_node;
    c4m_utf8_t      *name;
    c4m_utf8_t      *short_doc;
    c4m_utf8_t      *long_doc;
    c4m_dict_t      *fields;
    c4m_set_t       *allowed_sections;
    c4m_set_t       *required_sections;
    void            *validator;
    unsigned int     singleton   : 1;
    unsigned int     user_def_ok : 1;
    unsigned int     hidden      : 1;
    unsigned int     cycle       : 1;
} c4m_spec_section_t;

typedef struct {
    c4m_tree_node_t    *declaration_node;
    c4m_utf8_t         *short_doc;
    c4m_utf8_t         *long_doc;
    c4m_spec_section_t *root_section;
    c4m_dict_t         *section_specs;
    c4m_xlist_t        *errors; // an xlist of c4m_compile_errors
    unsigned int        used   : 1;
    unsigned int        locked : 1;
} c4m_spec_t;

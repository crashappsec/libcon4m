#pragma once
#include "con4m.h"

// This type represents a partially evaluated literal during execution
// of the front-end. You can tell whether the literal is partial or
// not by checking the type of the object.
//
// All 'simple' literals we can immediately evaluate. But container
// literals might contain expressions inside that aren't constant.
//
// We use this to represent ANY container that is not known to be
// fully static during the front end. Once it's not static, we replace
// it with an actual value of the appropriate type.
//
// The children are all objects. The state of evaluation can be
// determined by the type of those child objects. For instance, ints
// will be replaced with boxed ints (possibly after folding an
// expression).
//
// Children could be other partials too, and they can be parse nodes,
// allowing us to generate code to instantiate the literal from this
// data structure.

typedef struct {
    c4m_type_t      *type;
    unsigned int     num_items         : 30;
    unsigned int     empty_container   : 1;
    unsigned int     empty_dict_or_set : 1;
    // This is a bit field that keeps track of which items are themselves
    // partially evaluated.
    uint64_t        *cached_state;
    c4m_obj_t       *items;
    c4m_tree_node_t *node;
} c4m_partial_lit_t;

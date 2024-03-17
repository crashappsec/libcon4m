#include <con4m.h>

#pragma once

typedef enum  {
    BT_nil,
    BT_primitive,
    BT_list,
    BT_dict,
    BT_tuple,
    BT_func,
    BT_type_var,
    BT_type_spec,
    BT_ref,
    BT_maybe,
    BT_object,
    BT_oneof,
} base_type_t;

typedef uint64_t type_t;

typedef struct type_details_t type_details_t;

typedef struct {
    // The `typeid field` is EITHER the node's type ID, or a forwarding link
    // to another node in the graph. The rest of the data is accessed via
    // indirection, so that we can easily use CAS when this needs to be
    // multi-threaded (not sure that it would actually ever be necessary.)

    type_t          typeid;
    type_details_t *details;
} type_node_t;

struct type_details_t {
    any_str_t     *name;  // Obj type name or type var name
    struct dict_t *props; // Object properties. maps name to type_t.
    xlist_t       *items;
    base_type_t    kind;
    // 'Locked' means this type node cannot forward, even though it
    //  might have type variables. That causes the system to
    //  copy. That is used for function signatures for instance, to
    //  keep calls from restricting generic types.
    //
    // Basically think of locked types as fully resolved in one
    // universe, but being copied over into others.

    int8_t         locked : 1;
    int8_t         varargs: 1;
};

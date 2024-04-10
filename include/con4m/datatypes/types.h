#include "con4m.h"

#pragma once

typedef uint64_t type_t;

typedef struct type_details_t type_details_t;

typedef struct type_spec_t {
    // The `typeid field` is EITHER the node's type ID, or a forwarding link
    // to another node in the graph. The rest of the data is accessed via
    // indirection, so that we can easily use CAS when this needs to be
    // multi-threaded (not sure that it would actually ever be necessary.)
    // But each type spec will always get a unique details node.

    type_t typeid;
    type_details_t *details;
} type_spec_t;

typedef struct type_details_t {
    char          *name; // Obj type name or type var name
    c4m_dt_info_t *base_type;
    xlist_t       *items;
    struct dict_t *props; // Object properties. maps name to type node.
    // 'Locked' means this type node cannot forward, even though it
    //  might have type variables. That causes the system to
    //  copy. That is used for function signatures for instance, to
    //  keep calls from restricting generic types.
    //
    // Basically think of locked types as fully resolved in one
    // universe, but being copied over into others.

    uint8_t flags;
} type_details_t;

#define FN_TY_VARARGS 1
#define FN_TY_LOCK    2

typedef struct {
    struct dict_t   *store;
    _Atomic uint64_t next_tid;
} type_env_t;

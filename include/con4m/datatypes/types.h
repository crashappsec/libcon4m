#include "con4m.h"

#pragma once

typedef uint64_t c4m_type_hash_t;

typedef struct c4m_type_info_t c4m_type_info_t;

typedef struct c4m_type_t {
    // The `typeid field` is EITHER the node's type ID, or a forwarding link
    // to another node in the graph. The rest of the data is accessed via
    // indirection, so that we can easily use CAS when this needs to be
    // multi-threaded (not sure that it would actually ever be necessary.)
    // But each type spec will always get a unique details node.

    c4m_type_hash_t typeid;
    c4m_type_info_t *details;
} c4m_type_t;

typedef struct c4m_type_info_t {
    char          *name; // Obj type name or type var name
    c4m_dt_info_t *base_type;
    c4m_xlist_t   *items;
    c4m_dict_t    *props; // Object properties. maps name to type node.
    // 'Locked' means this type node cannot forward, even though it
    //  might have type variables. That causes the system to
    //  copy. That is used for function signatures for instance, to
    //  keep calls from restricting generic types.
    //
    // Basically think of locked types as fully resolved in one
    // universe, but being copied over into others.

    uint8_t flags;
} c4m_type_info_t;

#define C4M_FN_TY_VARARGS 1
#define C4M_FN_TY_LOCK    2

typedef struct {
    c4m_dict_t      *store;
    _Atomic uint64_t next_tid;
} c4m_type_env_t;

typedef enum {
    c4m_type_match_exact,
    c4m_type_match_left_more_specific,
    c4m_type_match_right_more_specific,
    c4m_type_match_both_have_more_generic_bits,
    c4m_type_cant_match,
} c4m_type_exact_result_t;

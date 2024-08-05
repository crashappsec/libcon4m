#include "con4m.h"

#pragma once

typedef uint64_t c4m_type_hash_t;

typedef struct c4m_type_info_t c4m_type_info_t;

typedef struct {
    // Bitfield of what container types we might be while inferring.
    uint64_t          *container_options;
    // Holds known value info when infering containers.
    struct c4m_type_t *value_type;
    // Object properties. maps prop name to type node.
    c4m_dict_t        *props;
} tv_options_t;

typedef struct c4m_type_t {
    tv_options_t    options; // Type-specific info.
    c4m_utf8_t     *name;
    c4m_list_t     *items;
    c4m_builtin_t   base_index;
    uint64_t        flags;
    c4m_type_hash_t fw;
    c4m_type_hash_t typeid;
} c4m_type_t;

#define C4M_FN_TY_VARARGS 1
// 'Locked' means this type node cannot forward, even though it might
//  have type variables. That causes the system to copy. That is used
//  for function signatures for instance, to keep calls from
//  restricting generic types.
//
// Basically think of locked types as fully resolved in one universe,
// but being copied over into others.
#define C4M_FN_TY_LOCK    2

// When we're type inferencing, we will keep things as type variables
// until the last possible minute, whenever we don't have an absolutely
// concrete type.

// For containers where the exact container type is unknown, this
// indicates whether or not we're confident in how many type
// parameters there will be. This is present mainly to aid in tuple
// merging, and I'm not focused on it right now, so will need to do
// more testing later.
#define C4M_FN_UNKNOWN_TV_LEN 4

// This indicates that we know about a function type, but have not yet
// received any information about their arguments.
//
// This isn't consulted during unification; it's primarily intended for
// call sites to check, so they can fill in param info on callbacks where
// the arguments are elided before we're prepared to do a global lookup.
#define C4M_FN_MISSING_PARAMS 8

typedef enum {
    c4m_type_match_exact,
    c4m_type_match_left_more_specific,
    c4m_type_match_right_more_specific,
    c4m_type_match_both_have_more_generic_bits,
    c4m_type_cant_match,
} c4m_type_exact_result_t;

typedef uint64_t (*c4m_next_typevar_fn)(void);

typedef struct c4m_type_universe_t {
    c4m_dict_t      *dict;
    _Atomic uint64_t next_typeid;
} c4m_type_universe_t;

#ifdef C4M_USE_INTERNAL_API
c4m_str_t *c4m_internal_type_repr(c4m_type_t *, c4m_dict_t *, int64_t *);
#endif

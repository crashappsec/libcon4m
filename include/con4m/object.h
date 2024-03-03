#pragma once

// This header is used for any allocations of non-primitive con4m
// types, in case we need to do dynamic type checking or dynamic
// dispatch.
//
// We keep both of these things instead of redirecting to a base object
// structure to avoid the extra indirection.

typedef struct {
    // Used for type checking, dynamic lookup of vtable info, and for
    // looking up what offsets have pointers, if a data structure body
    // has any pointers beyond a certain point (see below). This must
    // be a *concrete* type that the compiler would produce.
    uint64_t type_id;

    // This is a pointer to a vtable for the associated type.
    // Generally, there will be a minimum size for vtables with
    // built-in methods given hardcoded values.
    //
    // Here, we reserve 64 bits explicitly, just in case we attempt to
    // port to an environment with smaller pointers someday.
    uint64_t vtable;

    // The exposed object data.
    uint64_t data[];
} con4m_obj_t;



typedef struct {

} thread_arena_t;

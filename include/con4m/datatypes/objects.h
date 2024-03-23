#pragma once

#include <con4m.h>

typedef struct con4m_obj_t con4m_obj_t;

typedef enum {
    BT_nil,
    BT_primitive,
    BT_internal,   // Internal primitives.
    BT_type_var,
    BT_list,
    BT_dict,
    BT_tuple,
    BT_func,
    BT_maybe,
    BT_object,
    BT_oneof,
} base_t;

// At least for now, we're going to only us built-in methods of fixed
// size and know parameters in the vtable.
typedef void (*con4m_vtable_entry)(con4m_obj_t *, va_list);
typedef void (*container_init)(con4m_obj_t *, void *, va_list);

typedef struct {
    uint64_t           num_entries;
    con4m_vtable_entry methods[];
} con4m_vtable;

typedef struct {
    alignas(8)
    // The base ID for the data type. For now, this is redundant while we
    // don't have user-defined types, since we're caching data type info
    // in a fixed array. But once we add user-defined types, they'll
    // get their IDs from a hash function, and that won't work for
    // everything.
    const char         *name;
    const uint64_t      typeid;
    const uint64_t      alloc_len;  // How much space to allocate.
    const uint64_t     *ptr_info;  // Shows GC u64 offsets to examine for ptrs.
    const con4m_vtable *vtable;
    const base_t        base;
    const uint32_t      hash_fn;
    const bool          by_value : 1;
} dt_info;

// Below, con4m_obj_t is the *internal* object type.
//
// For most uses, we use `object_t`, which is a promise that there's a
// con4m_obj_t header behind the pointer.  Since generic objects will
// always get passed around by pointer, we skip the '*' whenever using
// object_t.
//
// This header is used for any allocations of non-primitive con4m
// types, in case we need to do dynamic type checking or dynamic
// dispatch.
//
// Note that I expect to steal a bit from the `base_data_type` pointer
// to distinguish whether the object has been freed; while I currently
// do not have

struct con4m_obj_t {
    dt_info            *base_data_type;
    struct type_spec_t *concrete_type;
    __uint128_t         cached_hash;
    // The exposed object data.
    uint64_t data[];
};

// A lot of these are placeholders; most are implemented in the
// current Nim runtime, but some aren't. Particularly, the destructor
// isn't even implemented here yet-- we are NOT yet sweaping discarded
// arenas for freed objects. Once we get to objects with
// non-persistent state, we will implement that, along with some
// ability in the marshal / unmarshal code to control attempting to
// recover such state.

typedef enum {
    CON4M_BI_CONSTRUCTOR = 0,
    CON4M_BI_TO_STR,
    CON4M_BI_DESTRUCTOR,
    CON4M_BI_MARSHAL,
    CON4M_BI_UNMARSHAL,
    CON4M_BI_NUM_FUNCS
} con4m_buitin_type_fn;

typedef enum {
    TO_STR_USE_AS_VALUE,
    TO_STR_USE_QUOTED
} to_str_use_t;

typedef enum : int64_t {
    T_TYPE_ERROR = 0,
    T_VOID,
    T_BOOL,
    T_I8,
    T_BYTE,
    T_I32,
    T_CHAR,
    T_U32,
    T_INT,
    T_UINT,
    T_F32,
    T_F64,
    T_UTF8,
    T_BUFFER,
    T_UTF32,
    T_GRID,
    T_LIST,
    T_TUPLE,
    T_DICT,
    T_SET,
    T_TYPESPEC,
    T_IPV4,
    T_IPV6,
    T_DURATION,
    T_SIZE,
    T_DATETIME,
    T_DATE,
    T_TIME,
    T_URL,
    T_CALLBACK,
    T_QUEUE,
    T_RING,
    T_LOGRING,
    T_STACK,
    T_RENDERABLE,
    T_XLIST, // single-threaded list.
    T_RENDER_STYLE,
    T_SHA,
    T_EXCEPTION,
    T_TYPE_ENV,
    T_TREE,
    T_FUNCDEF,
    T_REF,     // A managed pointer.
    T_GENERIC, // If instantiated, instantiates a 'mixed' object.
    CON4M_NUM_BUILTIN_DTS

} con4m_builtin_t;

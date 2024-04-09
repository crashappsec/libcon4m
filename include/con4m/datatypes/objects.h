#pragma once

#include "con4m.h"

typedef struct c4m_obj_t c4m_obj_t;

typedef enum {
    BT_nil,
    BT_primitive,
    BT_internal, // Internal primitives.
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
typedef void (*c4m_vtable_entry)(c4m_obj_t *, va_list);
typedef void (*container_init)(c4m_obj_t *, void *, va_list);

typedef int64_t  i64_box;
typedef uint64_t u64_box;
typedef int32_t  i32_box;
typedef uint32_t u32_box;
typedef int16_t  i16_box;
typedef uint16_t u16_box;
typedef int8_t   i8_box;
typedef uint8_t  u8_box;
typedef double   double_box;

typedef struct {
    uint64_t         num_entries;
    c4m_vtable_entry methods[];
} c4m_vtable;

typedef struct {
    alignas(8)
        // The base ID for the data type. For now, this is redundant while we
        // don't have user-defined types, since we're caching data type info
        // in a fixed array. But once we add user-defined types, they'll
        // get their IDs from a hash function, and that won't work for
        // everything.
        const char *name;
    const uint64_t typeid;
    const uint64_t    alloc_len; // How much space to allocate.
    const uint64_t   *ptr_info;  // Shows GC u64 offsets to examine for ptrs.
    const c4m_vtable *vtable;
    const base_t      base;
    const uint32_t    hash_fn;
    const bool        by_value : 1;
} dt_info;

// Below, c4m_obj_t is the *internal* object type.
//
// For most uses, we use `object_t`, which is a promise that there's a
// c4m_obj_t header behind the pointer.  Since generic objects will
// always get passed around by pointer, we skip the '*' whenever using
// object_t.
//
// This header is used for any allocations of non-primitive c4m
// types, in case we need to do dynamic type checking or dynamic
// dispatch.
//
// Note that I expect to steal a bit from the `base_data_type` pointer
// to distinguish whether the object has been freed; while I currently
// do not have

struct c4m_obj_t {
    dt_info            *base_data_type;
    struct type_spec_t *concrete_type;
    __uint128_t         cached_hash;
    // The exposed object data.
    uint64_t            data[];
};

// A lot of these are placeholders; most are implemented in the
// current Nim runtime, but some aren't. Particularly, the destructor
// isn't even implemented here yet-- we are NOT yet recording freed
// objects that need finalization. Once we get to objects with
// non-persistent state, we will implement that, along with some
// ability in the marshal / unmarshal code to control attempting to
// recover such state.
//
// Note that in the long term, many of these things wouldn't be baked
// into a static table, but because we are not implementing any of
// them in c4m itself right now, this is just better overall.
typedef enum {
    C4M_BI_CONSTRUCTOR = 0,
    C4M_BI_TO_STR,
    C4M_BI_FINALIZER,
    C4M_BI_MARSHAL,
    C4M_BI_UNMARSHAL,
    C4M_BI_COERCIBLE,    // Pass 2 types, return coerrced type, or type error.
    C4M_BI_COERCE,       // Actually do the coercion.
    C4M_BI_FROM_LITERAL, // Used to parse a literal.
    C4M_BI_COPY,         // If not used, defaults to marshal / unmarshal
    // __ functions. With primitive numeric types the compiler knows
    // to generate the proper underlying code, so don't need them.
    // But any higher-level stuff that want to overload the op, they do.
    //
    // The compiler will use their presence to type check, and will use
    // coercible to see if conversion is possible.
    //
    // The type requirements are annotated to the right.  For
    // functions that can have a different return type, we need to add
    // something for that.
    //
    // If the function isn't provided, we assume it must be the same
    // as the operand.
    C4M_BI_ADD, // `t + `t -> `t
    C4M_BI_SUB, // `t - `t -> `t
    C4M_BI_MUL, // `t * `t -> `v -- requires return type
    C4M_BI_DIV, // `t / `t -> `v -- requires return type
    C4M_BI_MOD, // `t % `t -> `v
    C4M_BI_EQ,
    C4M_BI_LT,
    C4M_BI_GT,
    // Container funcs
    C4M_BI_LEN,       // `t -> int
    C4M_BI_INDEX_GET, // `t[`n] -> `v (such that `t = list[`v] or
                      //  `t = dict[`n, `v]
    C4M_BI_INDEX_SET, // `t[`n] = `v -- requires index type
    C4M_BI_SLICE_GET, // `t[int:int] -> `v
    C4M_BI_SLICE_SET,
    C4M_BI_NUM_FUNCS
} c4m_buitin_type_fn;

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
    T_STREAM,  // streaming IO interface.
    T_KEYWORD, // Keyword arg object for internal use.
    C4M_NUM_BUILTIN_DTS

} c4m_builtin_t;

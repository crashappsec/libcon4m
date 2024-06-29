#pragma once
#include "con4m.h"

typedef struct c4m_base_obj_t c4m_base_obj_t;
typedef void                 *c4m_obj_t;

typedef enum {
    C4M_DT_KIND_nil,
    C4M_DT_KIND_primitive,
    C4M_DT_KIND_internal, // Internal primitives.
    C4M_DT_KIND_type_var,
    C4M_DT_KIND_list,
    C4M_DT_KIND_dict,
    C4M_DT_KIND_tuple,
    C4M_DT_KIND_func,
    C4M_DT_KIND_box,
    C4M_DT_KIND_maybe,
    C4M_DT_KIND_object,
    C4M_DT_KIND_oneof,
} c4m_dt_kind_t;

// At least for now, we're going to only us built-in methods of fixed
// size and know parameters in the vtable.
typedef void (*c4m_vtable_entry)(c4m_obj_t *, va_list);
typedef void (*c4m_container_init)(c4m_obj_t *, void *, va_list);

typedef struct {
    uint64_t         num_entries;
    c4m_vtable_entry methods[];
} c4m_vtable_t;

typedef struct {
    alignas(8)
        // clang-format off
    // The base ID for the data type. For now, this is redundant while we
    // don't have user-defined types, since we're caching data type info
    // in a fixed array. But once we add user-defined types, they'll
    // get their IDs from a hash function, and that won't work for
    // everything.
    const char         *name;
    const uint64_t      typeid;
    const uint64_t     *ptr_info;  // Shows GC u64 offsets to examine for ptrs.
    const c4m_vtable_t *vtable;
    const uint32_t      hash_fn;
    const uint32_t      alloc_len; // How much space to allocate.
    const c4m_dt_kind_t dt_kind;
    const bool          by_value : 1;
    // clang-format on
} c4m_dt_info_t;

// Below, c4m_obj_t is the *internal* object type.
//
// For most uses, we use `c4m_obj_t`, which is a promise that there's a
// c4m_obj_t header behind the pointer.  Since generic objects will
// always get passed around by pointer, we skip the '*' whenever using
// c4m_obj_t.
//
// This header is used for any allocations of non-primitive c4m
// types, in case we need to do dynamic type checking or dynamic
// dispatch.
//
// Note that I expect to steal a bit from the `base_data_type` pointer
// to distinguish whether the object has been freed.

struct c4m_base_obj_t {
    c4m_dt_info_t     *base_data_type;
    struct c4m_type_t *concrete_type;
    __uint128_t        cached_hash;
    // The exposed object data.
    uint64_t           data[];
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
    C4M_BI_FORMAT,
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
    // Returns the item type, given how the type is parameterized.
    C4M_BI_ITEM_TYPE,
    C4M_BI_VIEW, // Return a view on a container.
    C4M_BI_CONTAINER_LIT,
    C4M_BI_REPR,
    C4M_BI_NUM_FUNCS,
} c4m_builtin_type_fn;

typedef enum : uint8_t {
    c4m_ix_item_sz_byte    = 0,
    c4m_ix_item_sz_16_bits = 1,
    c4m_ix_item_sz_32_bits = 2,
    c4m_ix_item_sz_64_bits = 3,
    c4m_ix_item_sz_1_bit   = 0xff,
} c4m_ix_item_sz_t;

typedef enum : int64_t {
    C4M_T_ERROR = 0,
    C4M_T_VOID,
    C4M_T_BOOL,
    C4M_T_I8,
    C4M_T_BYTE,
    C4M_T_I32,
    C4M_T_CHAR,
    C4M_T_U32,
    C4M_T_INT,
    C4M_T_UINT,
    C4M_T_F32,
    C4M_T_F64,
    C4M_T_UTF8,
    C4M_T_BUFFER,
    C4M_T_UTF32,
    C4M_T_GRID,
    C4M_T_XLIST,
    C4M_T_TUPLE,
    C4M_T_DICT,
    C4M_T_SET,
    C4M_T_TYPESPEC,
    C4M_T_IPV4,
    C4M_T_IPV6,
    C4M_T_DURATION,
    C4M_T_SIZE,
    C4M_T_DATETIME,
    C4M_T_DATE,
    C4M_T_TIME,
    C4M_T_URL,
    C4M_T_FLAGS,
    C4M_T_CALLBACK,
    C4M_T_QUEUE,
    C4M_T_RING,
    C4M_T_LOGRING,
    C4M_T_STACK,
    C4M_T_RENDERABLE,
    C4M_T_FLIST, // single-threaded list.
    C4M_T_RENDER_STYLE,
    C4M_T_SHA,
    C4M_T_EXCEPTION,
    C4M_T_TREE,
    C4M_T_FUNCDEF,
    C4M_T_REF,     // A managed pointer.
    C4M_T_GENERIC, // If instantiated, instantiates a 'mixed' object.
    C4M_T_STREAM,  // streaming IO interface.
    C4M_T_KEYWORD, // Keyword arg object for internal use.
    C4M_T_VM,
    C4M_T_PARSE_NODE,
    C4M_T_BIT,
    C4M_T_BOX,
    C4M_NUM_BUILTIN_DTS,
} c4m_builtin_t;

#define C4M_T_LIST C4M_T_XLIST

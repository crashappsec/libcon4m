#pragma once

#include <con4m.h>


// At least for now, we're going to only us built-in methods of fixed
// size and know parameters in the vtable.
typedef void * con4m_vtable_entry;

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
    uint64_t     typeid;
    uint64_t    *ptr_info;   // Shows GC u64 offsets to examine for pointers.
    size_t       alloc_len;  // How much space to allocate.
    con4m_vtable vtable;
} con4m_dt_info;


// This header is used for any allocations of non-primitive con4m
// types, in case we need to do dynamic type checking or dynamic
// dispatch.
//
// Note that I expect to steal a bit from the `base_data_type` pointer
// to distinguish whether the object has been freed; while I currently
// do not have

typedef struct {
    con4m_dt_info *base_data_type;
    uint64_t       concrete_type;

    // The exposed object data.
    uint64_t data[];
} con4m_obj_t;

// A lot of these are placeholders; most are implemented in the
// current Nim runtime, but some aren't. Particularly, the destructor
// isn't even implemented here yet-- we are NOT yet sweaping discarded
// arenas for freed objects. Once we get to objects with
// non-persistent state, we will implement that, along with some
// ability in the marshal / unmarshal code to control attempting to
// recover such state.

typedef enum {
    CON4M_BI_CONSTRUCTOR = 0,
    CON4M_BI_DESTRUCTOR,
    CON4M_BI_MARSHAL,
    CON4M_BI_UNMARSHAL,
    CON4M_BI_NUM_FUNCS
} con4m_buitin_type_fn;

typedef enum {
    CON4M_BI_TYPE_ERROR = 0,
    CON4M_BI_VOID,
    CON4M_BI_BOOL,
    CON4M_BI_I8,
    CON4M_BI_BYTE,
    CON4M_BI_I32,
    CON4M_BI_CHAR,
    CON4M_BI_U32,
    CON4M_BI_INT,
    CON4M_BI_UINT,
    CON4M_BI_F32,
    CON4M_BI_F64,
    CON4M_BI_STR,
    CON4M_BI_BUFFER,
    CON4M_BI_UTF32,
    CON4M_BI_GRID,
    CON4M_BI_LIST,
    CON4M_BI_TUPLE,
    CON4M_BI_DICT,
    CON4M_BI_TYPESPEC,
    CON4M_BI_IPV4,
    CON4M_BI_IPV6,
    CON4M_BI_DURATION,
    CON4M_BI_SIZE,
    CON4M_BI_DATETIME,
    CON4M_BI_DATE,
    CON4M_BI_TIME,
    CON4M_BI_URL,
    CON4M_BI_CALLBACK,
    CON4M_BI_NUM_BUILTIN_DTS
} con4m_builtin_t;

// in object.c
extern const con4m_dt_info builtin_type_info[CON4M_BI_NUM_BUILTIN_DTS];

static inline void *
c4m_new(con4m_builtin_t typeid)
{
    // Right now, this will call

    // This currently looks up in in the above array and uses the func
    // pointer there. Eventually it will instead take an arbitrary
    // type, and look in a dict if it's a user-defined type.

    //  con4m_gc_alloc(builtin_type_info[typeid]);
}

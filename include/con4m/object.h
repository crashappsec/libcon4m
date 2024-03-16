#pragma once

#include <con4m.h>

typedef struct con4m_obj_t con4m_obj_t;

// At least for now, we're going to only us built-in methods of fixed
// size and know parameters in the vtable.
typedef void (*con4m_vtable_entry)(con4m_obj_t *, va_list);

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
    const uint64_t     typeid;
    const uint64_t     alloc_len;  // How much space to allocate.
    const uint64_t     *ptr_info;  // Shows GC u64 offsets to examine for ptrs.
    const con4m_vtable *vtable;
} con4m_dt_info;


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
    con4m_dt_info *base_data_type;
    uint64_t       concrete_type;

    // The exposed object data.
    uint64_t data[];
};

static inline con4m_obj_t *
get_object_header(object_t user_object)
{
    return &((con4m_obj_t *)user_object)[-1];
}

// This produces a proper con4m type that encodes the type info known
// about the entire contents. For instance, for a dictionary, the
// value would encode not just the fact that it's a dictionary, but
// the type of the key and the type of the value as well.
static inline uint64_t
get_concrete_type(object_t user_object)
{
    con4m_obj_t *obj = get_object_header(user_object);
    return obj->concrete_type;
}

static inline con4m_vtable *
get_vtable(object_t user_object)
{
    con4m_obj_t *obj = get_object_header(user_object);
    return (con4m_vtable *)obj->base_data_type->vtable;
}

static inline uint64_t
get_base_type(object_t user_object)
{
    con4m_obj_t *obj = get_object_header(user_object);
    return obj->base_data_type->typeid;
}

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
    T_STR,
    T_BUFFER,
    T_UTF32,
    T_GRID,
    T_LIST,
    T_TUPLE,
    T_DICT,
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
    CON4M_NUM_BUILTIN_DTS
} con4m_builtin_t;

#define T_UTF8 T_STR

// in object.c
extern const con4m_dt_info builtin_type_info[CON4M_NUM_BUILTIN_DTS];

#define con4m_new(tid, ...) _con4m_new(tid, KFUNC(__VA_ARGS__))

extern object_t _con4m_new(con4m_builtin_t typeid,  ...);
extern uint64_t *gc_get_ptr_info(con4m_builtin_t);

typedef str_t *(*repr_fn)(object_t, to_str_use_t);

static inline str_t *
con4m_raw_repr(con4m_builtin_t tinfo, object_t val, to_str_use_t kind)
{
    const con4m_vtable *vtbl = builtin_type_info[tinfo].vtable;
    repr_fn             ptr  = (repr_fn)vtbl->methods[CON4M_BI_TO_STR];

    return (*ptr)(val, kind);
}

static inline str_t *
con4m_value_obj_repr(object_t obj)
{
    repr_fn ptr = (repr_fn)get_vtable(obj)->methods[CON4M_BI_TO_STR];
    return (*ptr)(obj, TO_STR_USE_AS_VALUE);
}

extern const uint64_t str_ptr_info[];
extern const con4m_vtable u8str_vtable;
extern const con4m_vtable u32str_vtable;
extern const con4m_vtable buffer_vtable;
extern const con4m_vtable grid_vtable;
extern const con4m_vtable dimensions_vtable;
extern const con4m_vtable gridprops_vtable;
extern const con4m_vtable renderable_vtable;
extern const con4m_vtable list_vtable;
extern const con4m_vtable queue_vtable;
extern const con4m_vtable ring_vtable;
extern const con4m_vtable logring_vtable;
extern const con4m_vtable stack_vtable;
extern const con4m_vtable dict_vtable;
extern const con4m_vtable xlist_vtable;

extern uint64_t pmap_first_word[2];

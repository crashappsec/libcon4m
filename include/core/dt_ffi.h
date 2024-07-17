#pragma once
#include "con4m.h"

// For the foreign function interface, it's easier to redeclare
// libffi's structures than to deal w/ ensuring we have the right .h
// on each architecture. The only thing we need that might be
// different on different platforms is the ABI; hopefully
// FFI_DEFAUL T_ABI is the same everywhere, but if it isn't, that's
// easier to deal with than the header file situation.
//

typedef enum {
    C4M_FFI_FIRST_ABI   = 0,
    C4M_FFI_NON_GNU_ABI = 1,
    C4M_FFI_GNU_ABI     = 2,
    C4M_FFI_LAST_ABI,
#ifdef __GNUC__
    C4M_FFI_DEFAULT_ABI = C4M_FFI_GNU_ABI
#else
    C4M_FFI_DEFAULT_ABI = C4M_NON_GNU_ABI
#endif
} c4m_ffi_abi;

// This is libffi's `ffi_type`.
typedef struct c4m_ffi_type {
    size_t                size;
    unsigned short        alignment;
    unsigned short        ffitype;
    struct c4m_ffi_type **elements;
} c4m_ffi_type;

// This is libffi's `ffi_cif` type.
typedef struct {
    c4m_ffi_abi    abi;
    unsigned       nargs;
    c4m_ffi_type **arg_types;
    c4m_ffi_type  *rtype;
    unsigned       bytes;
    unsigned       flags;
    // Currently, no platform in libffi takes more than two 'unsigned int's
    // worth of space, so only one of these should be necessary, but
    // adding an extra one just in case; enough platforms take two fields
    // that I can see there eventually being a platform w/ 2 64-bit slots.
    // We alloc ourselves based on this size, so no worries there.
    uint64_t       extra_cif1;
    uint64_t       extra_cif2;
} c4m_ffi_cif;

typedef struct {
    void          *fptr;
    c4m_utf8_t    *local_name;
    c4m_utf8_t    *extern_name;
    uint64_t       str_convert;
    uint64_t       hold_info;
    uint64_t       alloc_info;
    c4m_ffi_type **args;
    c4m_ffi_type  *ret;
    c4m_ffi_cif    cif;
} c4m_zffi_cif;

typedef enum {
    C4M_FFI_OK = 0,
    C4M_FFI_BAD_TYPEDEF,
    C4M_FFI_BAD_ABI,
    C4M_FFI_BAD_ARGTYPE
} c4m_ffi_status;

typedef struct c4m_ffi_decl_t {
    c4m_utf8_t            *short_doc;
    c4m_utf8_t            *long_doc;
    c4m_utf8_t            *local_name;
    struct c4m_sig_info_t *local_params;
    c4m_utf8_t            *external_name;
    c4m_list_t            *dll_list;
    uint8_t               *external_params;
    uint8_t                external_return_type;
    bool                   skip_boxes;
    c4m_zffi_cif           cif;
    int                    num_ext_params;
    int                    global_ffi_call_ix;
} c4m_ffi_decl_t;

extern c4m_ffi_type ffi_type_void;
extern c4m_ffi_type ffi_type_uint8;
extern c4m_ffi_type ffi_type_sint8;
extern c4m_ffi_type ffi_type_uint16;
extern c4m_ffi_type ffi_type_sint16;
extern c4m_ffi_type ffi_type_uint32;
extern c4m_ffi_type ffi_type_sint32;
extern c4m_ffi_type ffi_type_uint64;
extern c4m_ffi_type ffi_type_sint64;
extern c4m_ffi_type ffi_type_float;
extern c4m_ffi_type ffi_type_double;
extern c4m_ffi_type ffi_type_pointer;

#define ffi_type_uchar  ffi_type_uint8
#define ffi_type_schar  ffi_type_sint8
#define ffi_type_ushort ffi_type_uint16
#define ffi_type_sshort ffi_type_sint16
#define ffi_type_ushort ffi_type_uint16
#define ffi_type_sshort ffi_type_sint16
#define ffi_type_uint   ffi_type_uint32
#define ffi_type_sint   ffi_type_sint32
#define ffi_type_ulong  ffi_type_uint64
#define ffi_type_slong  ffi_type_sint64

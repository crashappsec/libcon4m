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
    C4M_FFI_FIRST_ABI = 0,
    C4M_FFI_DEFAULT_ABI,
    C4M_FFI_LAST_ABI,
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
} c4m_ffi_cif;

typedef enum {
    C4M_FFI_OK = 0,
    C4M_FFI_BAD_TYPEDEF,
    C4M_FFI_BAD_ABI,
    C4M_FFI_BAD_ARGTYPE
} c4m_ffi_status;

typedef struct {
    c4m_utf8_t            *short_doc;
    c4m_utf8_t            *long_doc;
    c4m_utf8_t            *local_name;
    struct c4m_sig_info_t *local_params;
    int                    num_params;
    c4m_utf8_t            *external_name;
    uint8_t               *external_params;
    uint8_t                external_return_type;
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
extern c4m_ffi_type ffi_type_uchar;
extern c4m_ffi_type ffi_type_schar;
extern c4m_ffi_type ffi_type_ushort;
extern c4m_ffi_type ffi_type_sshort;
extern c4m_ffi_type ffi_type_uint;
extern c4m_ffi_type ffi_type_ulong;
extern c4m_ffi_type ffi_type_slong;
extern c4m_ffi_type ffi_type_longdouble;
extern c4m_ffi_type ffi_type_pointer;

#pragma once

typedef enum {
    FFI_ABI_0 = 0,
    FFI_ABI_1 = 1,
    FFI_ABI_2 = 2,
} c4m_ffi_abi;
typedef struct _c4m_ffi_type {
    size_t                 size;
    unsigned short         alignment;
    unsigned short         type;
    struct _c4m_ffi_type **elements;
} c4m_ffi_type;

typedef enum {
    FFI_OK = 0,
    FFI_BAD_TYPEDEF,
    FFI_BAD_ABI
} c4m_ffi_status;

typedef struct {
    c4m_ffi_abi    abi;
    unsigned       nargs;
    c4m_ffi_type **arg_types;
    c4m_ffi_type  *rtype;
    unsigned       bytes;
    unsigned       flags;
} c4m_ffi_cif;

#include "con4m.h"

typedef struct {
    c4m_ffi_cif   call_interface;
    c4m_ffi_abi   abi;
    c4m_ffi_type  return_type;
    unsigned int  fixedargs;
    c4m_ffi_type *arg_types;
} c4m_ffi_info_t;

typedef struct {
    void           *fn; // Can point into the VM or to ELF fn
    c4m_ffi_info_t *ffi;
    c4m_type_t     *type;
    char           *name;
    uint8_t         flags;
} c4m_funcinfo_t;

typedef struct {
    c4m_funcinfo_t *info; // Shared when possible.
    bool            bound;
} c4m_callback_t;

#define C4M_CB_FLAG_FFI    1
#define C4M_CB_FLAG_STATIC 2

#pragma once

typedef enum {
    FFI_ABI_0 = 0,
    FFI_ABI_1 = 1,
    FFI_ABI_2 = 2,
} ffi_abi;
typedef struct _ffi_type
{
  size_t size;
  unsigned short alignment;
  unsigned short type;
  struct _ffi_type **elements;
} ffi_type;

typedef enum {
  FFI_OK = 0,
  FFI_BAD_TYPEDEF,
  FFI_BAD_ABI
} ffi_status;

typedef struct {
  ffi_abi abi;
  unsigned nargs;
  ffi_type **arg_types;
  ffi_type *rtype;
  unsigned bytes;
  unsigned flags;
} ffi_cif;


#include <con4m.h>

typedef struct {
    ffi_cif      call_interface;
    ffi_abi      abi;
    ffi_type     return_type;
    unsigned int fixedargs;
    ffi_type    *arg_types;
} ffi_info_t;

typedef struct {
    void        *fn; // Can point into the VM or into directly executable bits.
    ffi_info_t  *ffi;
    type_spec_t *type;
    char        *name;
    uint8_t      flags;
} funcinfo_t;

typedef struct {
    funcinfo_t *info; // Shared when possible.
    bool        bound;
} callback_t;


#define CB_FLAG_FFI    1
#define CB_FLAG_STATIC 2

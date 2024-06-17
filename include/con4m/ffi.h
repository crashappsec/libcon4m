#pragma once
#include "con4m.h"

extern void           c4m_add_static_function(c4m_utf8_t *, void *);
extern void          *c4m_ffi_find_symbol(c4m_utf8_t *, c4m_xlist_t *);
extern int64_t        c4m_lookup_ctype_id(char *);
extern c4m_ffi_type  *c4m_ffi_arg_type_map(uint8_t);
extern c4m_ffi_status ffi_prep_cif(c4m_ffi_cif *,
                                   c4m_ffi_abi,
                                   unsigned int,
                                   c4m_ffi_type *,
                                   c4m_ffi_type **);
extern c4m_ffi_status ffi_prep_cif_var(c4m_ffi_cif *,
                                       c4m_ffi_abi,
                                       unsigned int,
                                       unsigned int,
                                       c4m_ffi_type *,
                                       c4m_ffi_type **);
extern void           ffi_call(c4m_ffi_cif *, void *, void *, void **);

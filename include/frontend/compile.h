#pragma once
#include "con4m.h"

extern void                  _c4m_set_package_search_path(c4m_utf8_t *, ...);
extern c4m_compile_ctx      *c4m_new_compile_context(c4m_str_t *);
extern bool                  c4m_validate_module_info(c4m_file_compile_ctx *);
extern c4m_compile_ctx      *c4m_compile_from_entry_point(c4m_str_t *);
extern void                  c4m_perform_module_loads(c4m_compile_ctx *);
extern c4m_file_compile_ctx *c4m_init_module_from_loc(c4m_compile_ctx *,
                                                      c4m_str_t *);
extern c4m_type_t           *c4m_str_to_type(c4m_utf8_t *);

#define c4m_set_package_search_path(x, ...) \
    _c4m_set_package_search_path(x, KFUNC(__VA_ARGS__))

#ifdef C4M_USE_INTERNAL_API
extern void                  c4m_file_decl_pass(c4m_compile_ctx *,
                                                c4m_file_compile_ctx *);
extern void                  c4m_check_pass(c4m_compile_ctx *);
extern c4m_file_compile_ctx *c4m_init_from_use(c4m_compile_ctx *,
                                               c4m_str_t *,
                                               c4m_str_t *,
                                               c4m_str_t *);

#endif

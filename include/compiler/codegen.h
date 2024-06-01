#pragma once
#include "con4m.h"

#ifdef C4M_USE_INTERNAL_API
extern void     c4m_layout_module_symbols(c4m_compile_ctx *,
                                          c4m_file_compile_ctx *);
extern int64_t  c4m_layout_string_const(c4m_compile_ctx *, c4m_str_t *);
extern uint32_t c4m_layout_const_obj(c4m_compile_ctx *, c4m_obj_t);
#endif

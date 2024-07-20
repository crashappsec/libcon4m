#pragma once
#include "con4m.h"

extern c4m_compile_ctx        *c4m_new_compile_context(c4m_str_t *);
extern c4m_compile_ctx        *c4m_compile_from_entry_point(c4m_str_t *);
extern c4m_module_compile_ctx *c4m_init_module_from_loc(c4m_compile_ctx *,
                                                        c4m_str_t *);
extern c4m_type_t             *c4m_str_to_type(c4m_utf8_t *);
extern c4m_vm_t               *c4m_generate_code(c4m_compile_ctx *);

static inline bool
c4m_got_fatal_compiler_error(c4m_compile_ctx *ctx)
{
    return ctx->fatality;
}

#ifdef C4M_USE_INTERNAL_API
extern void c4m_module_decl_pass(c4m_compile_ctx *,
                               c4m_module_compile_ctx *);
extern void c4m_check_pass(c4m_compile_ctx *);
#endif

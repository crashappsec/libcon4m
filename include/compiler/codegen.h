#pragma once
#include "con4m.h"

#ifdef C4M_USE_INTERNAL_API
extern void        c4m_layout_module_symbols(c4m_compile_ctx *,
                                             c4m_file_compile_ctx *);
extern int64_t     c4m_layout_string_const(c4m_compile_ctx *, c4m_str_t *);
extern uint32_t    _c4m_layout_const_obj(c4m_compile_ctx *, c4m_obj_t, ...);
extern c4m_grid_t *c4m_disasm(c4m_vm_t *, c4m_zmodule_info_t *m);
extern void        setup_obj(c4m_buf_t *, c4m_zobject_file_t *);
extern void        c4m_add_module(c4m_zobject_file_t *, c4m_zmodule_info_t *);
extern c4m_vm_t   *c4m_new_vm(c4m_compile_ctx *cctx);
extern void        c4m_internal_codegen(c4m_compile_ctx *, c4m_vm_t *);
extern c4m_utf8_t *c4m_fmt_instr_name(c4m_zinstruction_t *);

#define c4m_layout_const_obj(c, f, ...) \
    _c4m_layout_const_obj(c, f, KFUNC(__VA_ARGS__))
#endif

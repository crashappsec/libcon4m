#pragma once
#include "con4m.h"

typedef struct {
    c4m_fn_decl_t      *decl;
    c4m_zinstruction_t *i;
} c4m_call_backpatch_info_t;

extern void        c4m_layout_module_symbols(c4m_compile_ctx *,
                                             c4m_module_t *);
extern uint32_t    _c4m_layout_const_obj(c4m_compile_ctx *, c4m_obj_t, ...);
extern c4m_grid_t *c4m_disasm(c4m_vm_t *, c4m_module_t *m);
extern void        c4m_add_module(c4m_zobject_file_t *, c4m_module_t *);
extern c4m_vm_t   *c4m_new_vm(c4m_compile_ctx *cctx);
extern void        c4m_internal_codegen(c4m_compile_ctx *, c4m_vm_t *);
extern c4m_utf8_t *c4m_fmt_instr_name(c4m_zinstruction_t *);

#define c4m_layout_const_obj(c, f, ...) \
    _c4m_layout_const_obj(c, f, C4M_VA(__VA_ARGS__))

// The new API.
extern uint64_t c4m_add_static_object(void *, c4m_compile_ctx *);
extern uint64_t c4m_add_static_string(c4m_str_t *, c4m_compile_ctx *);
extern uint64_t c4m_add_value_const(uint64_t, c4m_compile_ctx *);

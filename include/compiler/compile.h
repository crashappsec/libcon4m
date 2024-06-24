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
extern c4m_vm_t             *c4m_generate_code(c4m_compile_ctx *);

#define c4m_set_package_search_path(x, ...) \
    _c4m_set_package_search_path(x, KFUNC(__VA_ARGS__))

static inline bool
c4m_got_fatal_compiler_error(c4m_compile_ctx *ctx)
{
    return ctx->fatality;
}

#ifdef C4M_USE_INTERNAL_API
extern void                  c4m_file_decl_pass(c4m_compile_ctx *,
                                                c4m_file_compile_ctx *);
extern void                  c4m_check_pass(c4m_compile_ctx *);
extern c4m_file_compile_ctx *c4m_init_from_use(c4m_compile_ctx *,
                                               c4m_str_t *,
                                               c4m_str_t *,
                                               c4m_str_t *);

#define C4M_INDEX_FN  "$index"
#define C4M_SLICE_FN  "$slice"
#define C4M_PLUS_FN   "$plus"
#define C4M_MINUS_FN  "$minus"
#define C4M_MUL_FN    "$mul"
#define C4M_MOD_FN    "$mod"
#define C4M_DIV_FN    "$div"
#define C4M_FDIV_FN   "$fdiv"
#define C4M_SHL_FN    "$shl"
#define C4M_SHR_FN    "$shr"
#define C4M_BAND_FN   "$bit_and"
#define C4M_BOR_FN    "$bit_or"
#define C4M_BXOR_FN   "$bit_xor"
#define C4M_CMP_FN    "$cmp"
#define C4M_SET_INDEX "$set_index"
#define C4M_SET_SLICE "$set_slice"

#endif

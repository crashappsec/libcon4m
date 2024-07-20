#pragma once
#include "con4m.h"

extern void                    _c4m_set_package_search_path(c4m_utf8_t *, ...);
extern bool                    c4m_validate_module_info(c4m_module_compile_ctx *);
extern c4m_module_compile_ctx *c4m_init_module_from_loc(c4m_compile_ctx *,
                                                        c4m_str_t *);
extern c4m_module_compile_ctx *c4m_new_module_compile_ctx();
extern c4m_grid_t             *c4m_get_module_summary_info(c4m_compile_ctx *);
extern bool                    c4m_add_module_to_worklist(c4m_compile_ctx *,
                                                          c4m_module_compile_ctx *);

static inline void
c4m_module_set_status(c4m_module_compile_ctx *ctx, c4m_module_compile_status status)
{
    if (ctx->status < status) {
        ctx->status = status;
    }
}

#define c4m_set_package_search_path(x, ...) \
    _c4m_set_package_search_path(x, C4M_VA(__VA_ARGS__))

extern c4m_module_compile_ctx *
c4m_find_module(c4m_compile_ctx *ctx,
                c4m_str_t       *path,
                c4m_str_t       *module,
                c4m_str_t       *package,
                c4m_str_t       *relative_package,
                c4m_str_t       *relative_path,
                c4m_list_t      *fext);

static inline c4m_utf8_t *
c4m_module_fully_qualified(c4m_module_compile_ctx *f)
{
    if (f->package) {
        return c4m_cstr_format("{}.{}", f->package, f->module);
    }

    return f->module;
}

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

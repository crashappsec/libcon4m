#pragma once
#include "con4m.h"

extern c4m_str_t         *c4m_format_error_message(c4m_compile_error *, bool);
extern c4m_grid_t        *c4m_format_errors(c4m_compile_ctx *);
extern c4m_compile_error *c4m_base_add_error(c4m_xlist_t *,
                                             c4m_compile_error_t,
                                             c4m_token_t *,
                                             c4m_err_severity_t,
                                             va_list);
extern c4m_compile_error *_c4m_add_error(c4m_file_compile_ctx *,
                                         c4m_compile_error_t,
                                         c4m_tree_node_t *,
                                         ...);
extern c4m_compile_error *_c4m_add_warning(c4m_file_compile_ctx *,
                                           c4m_compile_error_t,
                                           c4m_tree_node_t *,
                                           ...);
extern c4m_compile_error *_c4m_add_info(c4m_file_compile_ctx *,
                                        c4m_compile_error_t,
                                        c4m_tree_node_t *,
                                        ...);
extern c4m_compile_error *_c4m_add_spec_error(c4m_spec_t *,
                                              c4m_compile_error_t,
                                              c4m_tree_node_t *,
                                              ...);
extern c4m_compile_error *_c4m_add_spec_warning(c4m_spec_t *,
                                                c4m_compile_error_t,
                                                c4m_tree_node_t *,
                                                ...);
extern c4m_compile_error *_c4m_add_spec_info(c4m_spec_t *,
                                             c4m_compile_error_t,
                                             c4m_tree_node_t *,
                                             ...);
extern c4m_compile_error *_c4m_error_from_token(c4m_file_compile_ctx *,
                                                c4m_compile_error_t,
                                                c4m_token_t *,
                                                ...);

extern void _c4m_file_load_error(c4m_file_compile_ctx *,
                                 c4m_compile_error_t,
                                 ...);

extern void _c4m_file_load_warn(c4m_file_compile_ctx *,
                                c4m_compile_error_t,
                                ...);

#define c4m_add_error(x, y, z, ...) \
    _c4m_add_error(x, y, z, KFUNC(__VA_ARGS__))

#define c4m_add_warning(x, y, z, ...) \
    _c4m_add_warning(x, y, z, KFUNC(__VA_ARGS__))

#define c4m_add_info(x, y, z, ...) \
    _c4m_add_info(x, y, z, KFUNC(__VA_ARGS__))

#define c4m_add_spec_error(x, y, z, ...) \
    _c4m_add_spec_error(x, y, z, KFUNC(__VA_ARGS__))

#define c4m_add_spec_warning(x, y, z, ...) \
    _c4m_add_spec_warning(x, y, z, KFUNC(__VA_ARGS__))

#define c4m_add_spec_info(x, y, z, ...) \
    _c4m_add_spec_info(x, y, z, KFUNC(__VA_ARGS__))

#define c4m_error_from_token(x, y, z, ...) \
    _c4m_error_from_token(x, y, z, KFUNC(__VA_ARGS__))

#define c4m_file_load_error(x, y, ...) \
    _c4m_file_load_error(x, y, KFUNC(__VA_ARGS__))

#define c4m_file_load_warn(x, y, ...) \
    _c4m_file_load_warn(x, y, KFUNC(__VA_ARGS__))

static inline bool
c4m_fatal_error_in_module(c4m_file_compile_ctx *ctx)
{
    return ctx->fatal_errors != 0;
}
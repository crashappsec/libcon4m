#pragma once
#include "con4m.h"

extern c4m_str_t         *c4m_format_error_message(c4m_compile_error *, bool);
extern c4m_grid_t        *c4m_format_errors(c4m_file_compile_ctx *);
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
extern c4m_compile_error *_c4m_error_from_token(c4m_file_compile_ctx *,
                                                c4m_compile_error_t,
                                                c4m_token_t *,
                                                ...);

#define c4m_add_error(x, y, z, ...) \
    _c4m_add_error(x, y, z, KFUNC(__VA_ARGS__))

#define c4m_add_warning(x, y, z, ...) \
    _c4m_add_warning(x, y, z, KFUNC(__VA_ARGS__))

#define c4m_add_info(x, y, z, ...) \
    _c4m_add_info(x, y, z, KFUNC(__VA_ARGS__))

#define c4m_error_from_token(x, y, z, ...) \
    _c4m_error_from_token(x, y, z, KFUNC(__VA_ARGS__))

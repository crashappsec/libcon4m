#pragma once
#include "con4m.h"

c4m_file_compile_ctx *_c4m_new_compile_ctx(c4m_str_t *module_name, ...);
bool                  c4m_validate_module_info(c4m_file_compile_ctx *);
c4m_stream_t         *c4m_load_code(c4m_file_compile_ctx *);

#define c4m_new_compile_ctx(m, ...) \
    _c4m_new_compile_ctx(m, KFUNC(__VA_ARGS__))

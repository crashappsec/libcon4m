#pragma once
#include "con4m.h"

#ifdef C4M_BACKTRACE_SUPPORTED
extern void c4m_backtrace_init(char *);
extern void c4m_print_c_backtrace();
#else
#define c4m_backtrace_init(x)
#define c4m_print_c_backtrace()
#endif

extern c4m_grid_t *c4m_get_c_backtrace();
extern void        c4m_static_c_backtrace();

#pragma once
#include "con4m.h"

#ifdef BACKTRACE_SUPPORTED
void c4m_backtrace_init(char *);
void c4m_print_c_backtrace();
#else
#define c4m_backtrace_init(x)
#define c4m_print_c_backtrace()
#endif

c4m_grid_t *c4m_get_c_backtrace();

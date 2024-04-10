#pragma once
#include "con4m.h"

extern void c4m_mixed_set_value(c4m_mixed_t *, c4m_type_t *, void **);
extern void c4m_unbox_mixed(c4m_mixed_t *, c4m_type_t *, void **);

static inline void *
c4m_double_to_ptr(double d)
{
    union {
        double  d;
        int64_t i;
    } u;

    u.d = d;

    return (void *)(u.i);
}

static inline double
c4m_ptr_to_double(void *p)
{
    union {
        double  d;
        int64_t i;
    } u;

    u.i = (int64_t)p;

    return u.d;
}

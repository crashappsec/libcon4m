#pragma once
#include <con4m.h>

extern void mixed_set_value(mixed_t *, type_spec_t *, void **);
extern void unbox_mixed(mixed_t *, type_spec_t *, void **);

static inline void *
double_to_ptr(double d)
{
    union {
	double  d;
	int64_t i;
    } u;

    u.d = d;

    return (void *)(u.i);
}

static inline double
ptr_to_double(void *p)
{
    union {
	double  d;
	int64_t i;
    } u;

    u.i = (int64_t)p;

    return u.d;
}

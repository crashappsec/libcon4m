#pragma once

#include <con4m.h>


extern void     tuple_set(tuple_t *, int64_t, void *);
extern void    *tuple_get(tuple_t *, int64_t);
extern int64_t  tuple_len(tuple_t *);

#pragma once

#include <con4m.h>

extern buffer_t *buffer_add(buffer_t *, buffer_t *);
extern buffer_t *buffer_join(xlist_t *, buffer_t *);
extern int64_t   buffer_len(buffer_t *);

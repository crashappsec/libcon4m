#pragma once

#include <con4m.h>

extern buffer_t *buffer_add(buffer_t *, buffer_t *);
extern buffer_t *buffer_join(xlist_t *, buffer_t *);
extern int64_t   buffer_len(buffer_t *);
extern void      buffer_resize(buffer_t *, uint64_t);
extern utf8_t   *buffer_to_utf8_string(buffer_t *);

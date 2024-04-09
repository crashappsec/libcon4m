#pragma once

#include "con4m.h"

// TODO, should use a streaming interface for long dumps.

extern char   *chexl(void *ptr, int32_t len, uint64_t start_offset, int32_t width, char *prefix);
extern utf8_t *_hex_dump(void *ptr, uint32_t len, ...);

#define hex_dump(p, l, ...) _hex_dump(p, l, KFUNC(__VA_ARGS__))

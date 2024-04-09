#pragma once

#include "con4m.h"

// TODO, should use a streaming interface for long dumps.

extern char   *c4m_hexl(void *, int32_t, uint64_t, int32_t, char *);
extern utf8_t *_c4m_hex_dump(void *, uint32_t, ...);

#define c4m_hex_dump(p, l, ...) _c4m_hex_dump(p, l, KFUNC(__VA_ARGS__))

extern const uint8_t c4m_hex_map[16];

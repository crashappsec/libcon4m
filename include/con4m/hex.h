#pragma once

#include <con4m/str.h>

// TODO, should use a streaming interface for long dumps.

extern int calculate_size_prefix(uint64_t len, uint64_t start);
extern void add_offset(char **optr, uint64_t start_offset, uint64_t offset_len,
		       uint64_t line, uint64_t cpl);
extern char * chexl(void *ptr, unsigned int len, uint64_t start_offset,
		   unsigned int width, char *prefix);

extern str_t *hex_dump(void *ptr, unsigned int len, uint64_t start_offset,
		       unsigned int width, char *prefix);


// Legacy calls for Chalk compat.
extern char *chex(void *ptr, unsigned int len, uint64_t start_offset,
		  unsigned int width);
extern void print_hex(void *ptr, unsigned int len, char *prefix);

#pragma once

#include <con4m/str.h>
#include <con4m/breaks.h>

typedef enum {
    U8_STATE_START_DEFAULT,
    U8_STATE_START_STYLE,
    U8_STATE_DEFAULT_STYLE, // Stop at a new start ix or at the end.
    U8_STATE_IN_STYLE       // Stop at a new end ix or at the end.
} u8_state_t;

extern void ansi_render_u8(real_str_t *s, FILE *outstream);
extern void ansi_render_u32(real_str_t *s, int32_t start_ix, int32_t end_ix,
		     FILE *outstream);
extern void ansi_render(str_t *s, FILE *out);
extern void ansi_render_to_width(str_t *s, int32_t width, int32_t hang,
				 FILE *out);
extern size_t ansi_render_len(str_t *s);

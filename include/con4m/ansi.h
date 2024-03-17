#pragma once

#include <con4m.h>


extern void utf8_ansi_render(const utf8_t *s, FILE *outstream);
extern void utf32_ansi_render(const utf32_t *s, int32_t start_ix,
			      int32_t end_ix, FILE *outstream);
extern void ansi_render(const any_str_t *s, FILE *out);
extern void ansi_render_to_width(const any_str_t *s, int32_t width,
				 int32_t hang, FILE *out);
extern size_t ansi_render_len(const any_str_t *s);

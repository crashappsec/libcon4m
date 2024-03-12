#pragma once

#include <con4m.h>

// Flags in the style `info` bitfield.
#define BOLD_ON         0x0001000000000000
#define INV_ON          0x0002000000000000
#define ST_ON           0x0004000000000000
#define ITALIC_ON       0x0008000000000000
#define UL_ON           0x0010000000000000
#define FG_COLOR_ON     0x0020000000000000
#define BG_COLOR_ON     0x0040000000000000
#define UL_DOUBLE       0x0080000000000000
#define UPPER_CASE      0x0100000000000000
#define LOWER_CASE      0x0200000000000000
#define TITLE_CASE      0x0300000000000000 // (UPPER | LOWER)
#define STYLE_INVALID   0xffffffffffffffff
#define FG_COLOR_MASK   0xffffffffff000000
#define BG_COLOR_MASK   0xffff000000ffffff
#define FLAG_MASK       0x0000ffffffffffff
#define OFFSET_BG_RED   40
#define OFFSET_BG_GREEN 32
#define OFFSET_BG_BLUE  24
#define OFFSET_FG_RED   16
#define OFFSET_FG_GREEN 8
#define OFFSET_FG_BLUE  0

// The remaining 5 flags will currently be used for fonts. Might
// add another word in for other bits, not sure.

static inline void
copy_styles(style_info_t *dst, style_info_t *src, int start)
{
    if (!src) {
	return;
    }

    for (unsigned int i = 0; i < src->num_entries; i++) {
        dst->styles[i + start] = src->styles[i];
    }
}

static inline size_t
style_size(uint64_t num_entries)
{
    return sizeof(style_info_t) + (sizeof(style_entry_t) * num_entries);
}

static inline size_t
alloc_style_len(real_str_t *s)
{
    return sizeof(style_info_t) +
        s->styling->num_entries * sizeof(style_entry_t);
}

static inline int64_t
style_num_entries(real_str_t *s)
{
    if (s->styling == NULL) {
	return 0;
    }
    return s->styling->num_entries;
}

static inline int64_t
cstr_num_styles(const str_t *s)
{
    return style_num_entries(to_internal(s));
}

static inline void
alloc_styles(real_str_t *s, int n)
{
    if (n <= 0) {
	s->styling              = gc_flex_alloc(style_info_t, style_entry_t, 0,
						GC_SCAN_ALL);
	s->styling->num_entries = 0;
    }
    else {
	s->styling = gc_flex_alloc(style_info_t, style_entry_t, n,
				   GC_SCAN_ALL);

	s->styling->num_entries = n;
    }
}

static inline void
copy_style_info(real_str_t *from_str, real_str_t *to_str)
{
    if (from_str->styling == NULL) {
	return;
    }

    size_t sz = style_size(from_str->styling->num_entries);

    alloc_styles(to_str, (int)(from_str->styling->num_entries));
    memcpy(to_str->styling, from_str->styling, sz);
}

static inline int
copy_and_offset_styles(real_str_t *from_str, real_str_t *to_str,
		       int dst_style_ix, int offset)
{
    if (from_str->styling == NULL || from_str->styling->num_entries == 0) {
	return dst_style_ix;
    }

    for (int i = 0; i < from_str->styling->num_entries; i++) {
	style_entry_t style = from_str->styling->styles[i];
	style.start += offset;
	style.end += offset;
	to_str->styling->styles[dst_style_ix++] = style;
    }

    return dst_style_ix;
}

extern void set_default_style(style_t s);
extern style_t get_default_style();
extern style_t new_style();
extern style_t add_bold(style_t style);
extern style_t remove_bold(style_t style);
extern style_t add_inverse(style_t style);
extern style_t remove_inverse(style_t style);
extern style_t add_strikethrough(style_t style);
extern style_t remove_strikethrough(style_t style);
extern style_t add_italic(style_t style);
extern style_t remove_italic(style_t style);
extern style_t add_underline(style_t style);
extern style_t add_double_underline(style_t style);
extern style_t remove_underline(style_t style);
extern style_t add_bg_color(style_t style, uint8_t red, uint8_t green,
			    uint8_t blue);
extern style_t add_fg_color(style_t style, uint8_t red, uint8_t green,
			    uint8_t blue);
extern style_t apply_bg_color(style_t style, color_t c);
extern style_t apply_fg_color(style_t style, color_t c);
extern style_t add_upper_case(style_t style);
extern style_t add_lower_case(style_t style);
extern style_t add_title_case(style_t style);
extern style_t remove_case(style_t style);
extern style_t remove_bg_color(style_t style);
extern style_t remove_fg_color(style_t style);
extern style_t remove_all_color(style_t style);

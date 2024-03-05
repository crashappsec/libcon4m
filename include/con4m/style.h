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
#define FG_COLOR_MASK   0xffffffffff000000
#define BG_COLOR_MASK   0xffff000000ffffff
#define OFFSET_BG_RED   40
#define OFFSET_BG_GREEN 32
#define OFFSET_BG_BLUE  24
#define OFFSET_FG_RED   16
#define OFFSET_FG_GREEN 8
#define OFFSET_FG_BLUE  0

// The remaining 5 flags will currently be used for fonts. Might
// add another word in for other bits, not sure.

typedef uint64_t style_t;

typedef struct {
    uint32_t start;
    uint32_t end;
    style_t  info;  // 16 bits of flags, 24 bits bg color, 24 bits fg color
} style_entry_t;

typedef struct {
    uint64_t      num_entries;
    style_entry_t styles[];
} style_info_t;

static inline size_t
style_size(uint64_t num_entries)
{
    return sizeof(style_info_t) + (sizeof(style_entry_t) * num_entries);
}

static inline void
copy_styles(style_info_t *dst, style_info_t *src, int start)
{
    for (unsigned int i = 0; i < src->num_entries; i++) {
        dst->styles[i + start] = src->styles[i];
    }
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

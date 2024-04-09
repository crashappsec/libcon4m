#pragma once

#include "con4m.h"

// Flags in the style `info` bitfield.
#define C4M_STY_FG          (0x0002000000000000UL)
#define C4M_STY_BG          (0x0004000000000000UL)
#define C4M_STY_BOLD        (0x0008000000000000UL)
#define C4M_STY_ITALIC      (0x0010000000000000UL)
#define C4M_STY_ST          (0x0020000000000000UL)
#define C4M_STY_UL          (0x0040000000000000UL)
#define C4M_STY_UUL         (0x0080000000000000UL)
#define C4M_STY_REV         (0x0100000000000000UL)
#define C4M_STY_LOWER       (0x0200000000000000UL)
#define C4M_STY_UPPER       (0x0400000000000000UL)
#define C4M_STY_TITLE       (C4M_STY_UPPER | C4M_STY_LOWER)
#define C4M_STY_BAD         (0xffffffffffffffffUL)
#define C4M_STY_CLEAR_FG    (0xffffffffff000000UL)
#define C4M_STY_CLEAR_BG    (0xffff000000ffffffUL)
#define C4M_STY_CLEAR_FLAGS (0x0000ffffffffffffUL)

#define C4M_OFFSET_BG_RED   40
#define C4M_OFFSET_BG_GREEN 32
#define C4M_OFFSET_BG_BLUE  24
#define C4M_OFFSET_FG_RED   16
#define C4M_OFFSET_FG_GREEN 8
#define C4M_OFFSET_FG_BLUE  0

extern style_t c4m_apply_bg_color(style_t style, utf8_t *name);
extern style_t c4m_apply_fg_color(style_t style, utf8_t *name);
extern void    c4m_style_gaps(any_str_t *, style_t);
extern void    c4m_str_layer_style(any_str_t *, style_t, style_t);

static inline void
c4m_style_debug(char *prefix, const any_str_t *p)
{
    if (!p)
        return;

    if (p->styling == NULL) {
        printf("debug (%s): len: %lld styles: nil\n",
               prefix,
               (long long)c4m_str_codepoint_len(p));
        return;
    }
    else {
        printf("debug (%s): len: %lld # styles: %lld\n",
               prefix,
               (long long)c4m_str_codepoint_len(p),
               (long long)p->styling->num_entries);
    }
    for (int i = 0; i < p->styling->num_entries; i++) {
        style_entry_t entry = p->styling->styles[i];
        printf("%d: %llx (%d:%d)\n",
               i + 1,
               (long long)p->styling->styles[i].info,
               entry.start,
               entry.end);
    }
}

// The remaining 5 flags will currently be used for fonts. Might
// add another word in for other bits, not sure.

static inline size_t
c4m_style_size(uint64_t num_entries)
{
    return sizeof(style_info_t) + (sizeof(style_entry_t) * num_entries);
}

static inline size_t
c4m_alloc_style_len(any_str_t *s)
{
    return sizeof(style_info_t) + s->styling->num_entries * sizeof(style_entry_t);
}

static inline int64_t
c4m_style_num_entries(any_str_t *s)
{
    if (s->styling == NULL) {
        return 0;
    }
    return s->styling->num_entries;
}

static inline void
c4m_alloc_styles(any_str_t *s, int n)
{
    if (n <= 0) {
        s->styling              = c4m_gc_flex_alloc(style_info_t,
                                       style_entry_t,
                                       0,
                                       NULL);
        s->styling->num_entries = 0;
    }
    else {
        s->styling              = c4m_gc_flex_alloc(style_info_t,
                                       style_entry_t,
                                       n,
                                       NULL);
        s->styling->num_entries = n;
    }
}

static inline void
c4m_copy_style_info(const any_str_t *from_str, any_str_t *to_str)
{
    if (from_str->styling == NULL) {
        return;
    }
    int n = from_str->styling->num_entries;

    c4m_alloc_styles(to_str, n);

    for (int i = 0; i < n; i++) {
        style_entry_t s            = from_str->styling->styles[i];
        to_str->styling->styles[i] = s;
    }

    to_str->styling->num_entries = from_str->styling->num_entries;
}

static inline void
c4m_str_set_style(any_str_t *s, style_t style)
{
    c4m_alloc_styles(s, 1);
    s->styling->styles[0].start = 0;
    s->styling->styles[0].end   = c4m_str_codepoint_len(s);
    s->styling->styles[0].info  = style;
}

static inline int
c4m_copy_and_offset_styles(any_str_t *from_str,
                           any_str_t *to_str,
                           int        dst_style_ix,
                           int        offset)
{
    if (from_str->styling == NULL || from_str->styling->num_entries == 0) {
        return dst_style_ix;
    }

    for (int i = 0; i < from_str->styling->num_entries; i++) {
        style_entry_t style = from_str->styling->styles[i];

        if (style.end <= style.start) {
            break;
        }

        style.start += offset;
        style.end += offset;
        to_str->styling->styles[dst_style_ix++] = style;

        style = to_str->styling->styles[dst_style_ix - 1];
    }

    return dst_style_ix;
}

static inline void
c4m_str_apply_style(any_str_t *s, style_t style, bool replace)
{
    if (replace) {
        c4m_str_set_style(s, style);
    }
    else {
        c4m_str_layer_style(s, style, 0);
    }
}

static inline style_t
c4m_set_bg_color(style_t style, color_t color)
{
    return (style & C4M_STY_CLEAR_BG) | C4M_STY_BG | ((uint64_t)color) << 24;
}

static inline style_t
c4m_set_fg_color(style_t style, color_t color)
{
    return (style & C4M_STY_CLEAR_FG) | C4M_STY_FG | (uint64_t)color;
}

extern style_t default_style;

static inline void
c4m_set_default_style(style_t s)
{
    default_style = s;
}

static inline style_t
c4m_get_default_style()
{
    return default_style;
}

static inline style_t
c4m_new_style()
{
    return (uint64_t)0;
}

static inline style_t
c4m_add_bold(style_t style)
{
    return style | C4M_STY_BOLD;
}

static inline style_t
c4m_remove_bold(style_t style)
{
    return style & ~C4M_STY_BOLD;
}

static inline style_t
c4m_add_inverse(style_t style)
{
    return style | C4M_STY_REV;
}

static inline style_t
c4m_remove_inverse(style_t style)
{
    return style & ~C4M_STY_REV;
}

static inline style_t
c4m_add_strikethrough(style_t style)
{
    return style | C4M_STY_ST;
}

static inline style_t
c4m_remove_strikethrough(style_t style)
{
    return style & ~C4M_STY_ST;
}

static inline style_t
c4m_add_italic(style_t style)
{
    return style | C4M_STY_ITALIC;
}

static inline style_t
c4m_remove_italic(style_t style)
{
    return style & ~C4M_STY_ITALIC;
}

static inline style_t
c4m_add_underline(style_t style)
{
    return (style | C4M_STY_UL) & ~C4M_STY_UUL;
}

static inline style_t
c4m_add_double_underline(style_t style)
{
    return (style | C4M_STY_UUL) & ~C4M_STY_UL;
}

static inline style_t
c4m_remove_underline(style_t style)
{
    return style & ~(C4M_STY_UL | C4M_STY_UUL);
}

static inline style_t
c4m_add_upper_case(style_t style)
{
    return (style & ~C4M_STY_TITLE) | C4M_STY_UPPER;
}
static inline style_t
c4m_add_lower_case(style_t style)
{
    return (style & ~C4M_STY_TITLE) | C4M_STY_LOWER;
}

static inline style_t
c4m_add_title_case(style_t style)
{
    return style | C4M_STY_TITLE;
}

static inline style_t
c4m_remove_case(style_t style)
{
    return style & ~C4M_STY_TITLE;
}

static inline style_t
c4m_remove_bg_color(style_t style)
{
    return ((uint64_t)style) & (C4M_STY_CLEAR_BG & ~C4M_STY_BG);
}

static inline style_t
c4m_remove_fg_color(style_t style)
{
    return ((uint64_t)style) & (C4M_STY_CLEAR_FG & ~C4M_STY_FG);
}

static inline style_t
c4m_remove_all_color(style_t style)
{
    // This should mainly constant fold down to a single AND.
    return ((uint64_t)style) & (C4M_STY_CLEAR_FG & C4M_STY_CLEAR_BG & ~(C4M_STY_FG | C4M_STY_BG));
}

// After the slice, remove dead styles.
// This isn't being used, but it's a reasonable debugging tool.
static inline void
c4m_clean_styles(any_str_t *s)
{
    if (!s->styling) {
        return;
    }
    int l = s->styling->num_entries;

    if (l < 2) {
        return;
    }

    int move = 0;

    style_entry_t prev = s->styling->styles[0];
    style_entry_t cur;

    for (int i = 1; i < s->styling->num_entries; i++) {
        cur = s->styling->styles[i];
        if (cur.end <= prev.end) {
            move++;
            continue;
        }
        if (prev.end > cur.start) {
            cur.start = prev.end;
        }
        if (cur.start >= cur.end) {
            move++;
            continue;
        }

        s->styling->styles[i - move] = cur;

        prev = cur;
    }
    s->styling->num_entries -= move;
}

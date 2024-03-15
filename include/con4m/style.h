#pragma once

#include <con4m.h>

// Flags in the style `info` bitfield.
#define FG_COLOR_ON     0x0002000000000000
#define BG_COLOR_ON     0x0004000000000000
#define BOLD_ON         0x0008000000000000
#define ITALIC_ON       0x0010000000000000
#define ST_ON           0x0020000000000000
#define UL_ON           0x0040000000000000
#define UL_DOUBLE       0x0080000000000000
#define INV_ON          0x0100000000000000
#define LOWER_CASE      0x0200000000000000
#define UPPER_CASE      0x0400000000000000
#define TITLE_CASE      (UPPER_CASE | LOWER_CASE)
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


static inline void
style_debug(char *prefix, const str_t *s) {
    if (!s) return;

    real_str_t *p = to_internal(s);

    if (p->styling == NULL) {
	printf("debug (%s): len: %lld styles: nil\n", prefix, c4str_len(s));
	return;
    }
    else {
	printf("debug (%s): len: %lld # styles: %lld\n", prefix, c4str_len(s),
	       p->styling->num_entries);
    }
    for (int i = 0 ; i < p->styling->num_entries; i++) {
	style_entry_t entry = p->styling->styles[i];
	printf("%d: %llx (%d:%d)\n", i + 1, p->styling->styles[i].info, entry.start, entry.end);
    }
}

// The remaining 5 flags will currently be used for fonts. Might
// add another word in for other bits, not sure.

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
						NULL);
	s->styling->num_entries = 0;
    }
    else {
	s->styling = gc_flex_alloc(style_info_t, style_entry_t, n, NULL);
	s->styling->num_entries = n;
    }
}

static inline void
copy_style_info(real_str_t *from_str, real_str_t *to_str)
{
    if (from_str->styling == NULL) {
	return;
    }
    int n = from_str->styling->num_entries;

    alloc_styles(to_str, n);

    for (int i = 0; i < n; i++) {
	style_entry_t s = from_str->styling->styles[i];
	to_str->styling->styles[i] = s;
    }

    to_str->styling->num_entries = from_str->styling->num_entries;
}

static inline void
apply_style_to_real_string(real_str_t *s, style_t style)
{

    alloc_styles(s, 1);
    s->styling->styles[0].start = 0;
    s->styling->styles[0].end   = internal_num_cp(s);
    s->styling->styles[0].info  = style;
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

	if (style.end <= style.start) {
	    break;
	}

	style.start += offset;
	style.end   += offset;
	to_str->styling->styles[dst_style_ix++] = style;
	style = to_str->styling->styles[dst_style_ix - 1];
    }

    return dst_style_ix;
}

static inline void
style_gaps(real_str_t *s, style_t gapstyle)
{
    if (!s->styling || !s->styling->num_entries) {
	apply_style_to_real_string(s, gapstyle);
	return;
    }

    int num_gaps = 0;
    int last_end = 0;
    int num_cp   = internal_num_cp(s);

    for (int i = 0; i < s->styling->num_entries; i++) {
	style_entry_t style = s->styling->styles[i];
	if (style.start > last_end) {
	    num_gaps++;
	}
	last_end = style.end;
    }
    if (num_cp > last_end) {
	num_gaps++;
    }

    if (!num_gaps) {
	return;
    }
    style_info_t *old = s->styling;
    int new_ix        = 0;

    alloc_styles(s, old->num_entries + num_gaps);


    last_end = 0;

    for (int i = 0; i < old->num_entries; i++) {
	style_entry_t style = s->styling->styles[i];

	if (style.start > last_end) {
	    style_entry_t filler = {
		.start = last_end,
                .end   = style.start,
	        .info  = gapstyle
	    };

	    s->styling->styles[new_ix++] = filler;
	}
	s->styling->styles[new_ix++] = old->styles[i];
	last_end = old->styles[i].end;
    }
    if (last_end != num_cp) {
	style_entry_t filler = {
	    .start = last_end,
	    .end   = num_cp,
	    .info  = gapstyle
	};

	s->styling->styles[new_ix] = filler;
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
extern style_t apply_bg_color(style_t style, char *name);
extern style_t apply_fg_color(style_t style, char *name);
extern style_t add_upper_case(style_t style);
extern style_t add_lower_case(style_t style);
extern style_t add_title_case(style_t style);
extern style_t remove_case(style_t style);
extern style_t remove_bg_color(style_t style);
extern style_t remove_fg_color(style_t style);
extern style_t remove_all_color(style_t style);

// After the slice, remove dead styles.
// This isn't being used, but it's a reasonable debugging tool.
static inline void
clean_styles(real_str_t *s) {
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

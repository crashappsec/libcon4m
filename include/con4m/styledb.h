#pragma once

#include <con4m.h>

// This is a 'repository' for style info that can be applied to grids
// or strings. When we apply to strings, anything grid-related gets
// ignored.

// This compacts grid style info, but currently that is internal.
// Some of the items apply to text, some apply to renderables.

typedef struct {
    uint64_t     fg_color    : 24;
    uint64_t     bg_color    : 24;
    unsigned int invalid     : 1;
    unsigned int fg_color_on : 1;   // Whether a fg color is set, else inherit.
    unsigned int bg_color_on : 1;   // Whether a fg color is set, else inherit.
    unsigned int bold        : 1;   // Use a 'bold' font if possible.
    unsigned int italic      : 1;
    unsigned int strikethru  : 1;
    unsigned int underline   : 2;   // single or double.
    unsigned int inverse     : 1;
    unsigned int casing      : 2;   // text casing.
    // unsigned int reserved    : 5;   // For expansion of string stuff.
} str_bitfield_t;

typedef union {
    str_bitfield_t bf;
    style_t        style;
} str_style_t;

typedef struct {
    border_theme_t *border_theme;
    str_style_t     base_style;   // Should be able to slam a style_t onto it.
    color_t         pad_color;

    union {
	float      percent;
	uint64_t   units;
	int32_t    range[2];
    } dims;

    int8_t         top_pad;
    int8_t         bottom_pad;
    int8_t         left_pad;
    int8_t         right_pad;
    int8_t         wrap;


    // Eventually we'll add more in like z-ordering and transparency.
    alignment_t    alignment     : 7;
    dimspec_kind_t dim_kind      : 3;
    border_set_t   borders       : 6;
    unsigned int   pad_color_set : 1;
    unsigned int   disable_wrap  : 1;
} render_style_t;

extern void            set_style(char *name, render_style_t *style);
extern render_style_t *lookup_cell_style(char *name);
extern void            install_default_styles();

static inline style_t
lookup_text_style(char *name)
{
    render_style_t *rs = lookup_cell_style(name);
    return rs->base_style.style;
}

static inline void
set_fg_color(render_style_t *style, color_t color)
{
    style->base_style.bf.fg_color    = color;
    style->base_style.bf.fg_color_on = 1;
}

static inline void
set_bg_color(render_style_t *style, color_t color)
{
    style->base_style.bf.bg_color    = color;
    style->base_style.bf.bg_color_on = 1;
}

static inline void
bold_on(render_style_t *style)
{
    style->base_style.bf.bold = 1;
}

static inline void
bold_off(render_style_t *style)
{
    style->base_style.bf.bold = 0;
}

static inline void
italic_on(render_style_t *style)
{
    style->base_style.bf.italic = 1;
}

static inline void
italic_off(render_style_t *style)
{
    style->base_style.bf.italic = 0;
}

static inline void
strikethru_on(render_style_t *style)
{
    style->base_style.bf.strikethru = 1;
}

static inline void
strikethru_off(render_style_t *style)
{
    style->base_style.bf.strikethru = 0;
}

static inline void
underline_on(render_style_t *style)
{
    style->base_style.bf.underline = 1;
}


static inline void
double_underline_on(render_style_t *style)
{
    style->base_style.bf.underline = 2;
}

static inline void
underline_off(render_style_t *style)
{
    style->base_style.bf.underline = 0;
}

static inline void
inverse_on(render_style_t *style)
{
    style->base_style.bf.inverse = 1;
}

static inline void
inverse_off(render_style_t *style)
{
    style->base_style.bf.inverse = 0;
}


static inline void
lowercase_on(render_style_t *style)
{
    style->base_style.bf.casing = 1;
}

static inline void
uppercase_on(render_style_t *style)
{
    style->base_style.bf.casing = 2;
}

static inline void
titlecase_on(render_style_t *style)
{
    style->base_style.bf.casing = 2;
}

static inline void
casing_off(render_style_t *style)
{
    style->base_style.bf.casing = 0;
}

const extern border_theme_t *registered_borders;

static inline _Bool
set_border_theme(render_style_t *style, char *name) {
    border_theme_t *cur = (border_theme_t *)registered_borders;

    while (cur != NULL) {
	if (!strcmp((char *)name, cur->name)) {
	    style->border_theme = cur;
	    return true;
	}
	cur = cur->next_style;
    }
    return false;
}

static inline void
set_flex_size(render_style_t *style, int64_t size)
{
    assert(size >= 0);
    style->dim_kind = DIM_FLEX_UNITS;
    style->dims.units = (uint64_t)size;
}

static inline void
set_absolute_size(render_style_t *style, int64_t size)
{
    assert(size >= 0);
    style->dim_kind = DIM_ABSOLUTE;
    style->dims.units = (uint64_t)size;
}

static inline void
set_size_range(render_style_t *style, int32_t min, int32_t max)
{
    assert(min >= 0 && max >= min);
    style->dim_kind = DIM_ABSOLUTE_RANGE;
    style->dims.range[0] = min;
    style->dims.range[1] = max;
}

static inline void
set_fit_to_text(render_style_t *style)
{
    style->dim_kind = DIM_FIT_TO_TEXT;
}

static inline void
set_auto_size(render_style_t *style)
{
    style->dim_kind = DIM_AUTO;
}

static inline void
set_size_as_percent(render_style_t *style, float pct, bool round)
{
    assert(pct >= 0);
    if (round) {
	style->dim_kind = DIM_PERCENT_ROUND;
    }
    else {
	style->dim_kind = DIM_PERCENT_TRUNCATE;
    }
    style->dims.percent = pct;
}

static inline void
set_top_pad(render_style_t *style, int8_t pad)
{
    assert(pad >= 0);
    style->top_pad = pad;
}

static inline void
set_bottom_pad(render_style_t *style, int8_t pad)
{
    assert(pad >= 0);
    style->bottom_pad = pad;
}

static inline void
set_left_pad(render_style_t *style, int8_t pad)
{
    assert(pad >= 0);
    style->left_pad = pad;
}

static inline void
set_right_pad(render_style_t *style, int8_t pad)
{
    assert(pad >= 0);
    style->right_pad = pad;
}

static inline void
set_wrap_hang(render_style_t *style, int8_t hang)
{
    assert(hang >= 0);
    style->wrap = hang;
    style->disable_wrap = 0;
}

static inline void
disable_line_wrap(render_style_t *style)
{
    style->disable_wrap = 1;
}

static inline void
set_pad_color(render_style_t *style, color_t color)
{
    style->pad_color     = color;
    style->pad_color_set = 1;
}

static inline void
clear_fg_color(render_style_t *style)
{
    style->base_style.bf.fg_color_on = 0;
}

static inline void
clear_bg_color(render_style_t *style)
{
    style->base_style.bf.bg_color_on = 0;
}

static inline void
set_alignment(render_style_t *style, alignment_t alignment)
{
    style->alignment = alignment;
}

static inline void
set_borders(render_style_t *style, border_set_t borders)
{
    style->borders = borders;
}

extern const con4m_vtable render_style_vtable;
extern const uint64_t rs_pmap[2];

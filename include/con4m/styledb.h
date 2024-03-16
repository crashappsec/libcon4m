#pragma once

#include <con4m.h>

typedef enum : int8_t {
    ALIGN_IGNORE = 0,
    ALIGN_LEFT   = 1,
    ALIGN_RIGHT  = 2,
    ALIGN_CENTER = 4,
    ALIGN_TOP    = 8,
    ALIGN_BOTTOM = 16,
    ALIGN_MIDDLE = 32,

    ALIGN_TOP_LEFT      = ALIGN_LEFT   | ALIGN_TOP,
    ALIGN_TOP_RIGHT     = ALIGN_RIGHT  | ALIGN_TOP,
    ALIGN_TOP_CENTER    = ALIGN_CENTER | ALIGN_TOP,

    ALIGN_MID_LEFT      = ALIGN_LEFT   | ALIGN_MIDDLE,
    ALIGN_MID_RIGHT     = ALIGN_RIGHT  | ALIGN_MIDDLE,
    ALIGN_MID_CENTER    = ALIGN_CENTER | ALIGN_MIDDLE,

    ALIGN_BOTTOM_LEFT   = ALIGN_LEFT   | ALIGN_BOTTOM,
    ALIGN_BOTTOM_RIGHT  = ALIGN_RIGHT  | ALIGN_BOTTOM,
    ALIGN_BOTTOM_CENTER = ALIGN_CENTER | ALIGN_BOTTOM,
} alignment_t;

#define HORIZONTAL_MASK (ALIGN_LEFT | ALIGN_CENTER | ALIGN_RIGHT)
#define VERTICAL_MASK (ALIGN_TOP | ALIGN_MIDDLE | ALIGN_BOTTOM )

typedef enum : uint8_t {
    DIM_UNSET,
    DIM_AUTO,
    DIM_PERCENT_TRUNCATE,
    DIM_PERCENT_ROUND,
    DIM_FLEX_UNITS,
    DIM_ABSOLUTE,
    DIM_ABSOLUTE_RANGE,
    DIM_FIT_TO_TEXT
} dimspec_kind_t;

typedef struct border_theme_t {
    char                  *name;
    int32_t                horizontal_rule;
    int32_t                vertical_rule;
    int32_t                upper_left;
    int32_t                upper_right;
    int32_t                lower_left;
    int32_t                lower_right;
    int32_t                cross;
    int32_t                top_t;
    int32_t                bottom_t;
    int32_t                left_t;
    int32_t                right_t;
    struct border_theme_t *next_style;
} border_theme_t;

#define BORDER_TOP          0x01
#define BORDER_BOTTOM       0x02
#define BORDER_LEFT         0x04
#define BORDER_RIGHT        0x08
#define INTERIOR_HORIZONTAL 0x10
#define INTERIOR_VERTICAL   0x20

typedef uint8_t border_set_t;

// This is a 'repository' for style info that can be applied to grids
// or strings. When we apply to strings, anything grid-related gets
// ignored.

// This compacts grid style info, but currently that is internal.
// Some of the items apply to text, some apply to renderables.

typedef struct {
    char           *name;
    border_theme_t *border_theme;
    style_t         base_style;
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
    unsigned int   tpad_set      : 1; // These prevent overrides when
    unsigned int   bpad_set      : 1; // the pad value is 0. If it's not
    unsigned int   lpad_set      : 1; // 0, this gets ignored.
    unsigned int   rpad_set      : 1;
    unsigned int   hang_set      : 1;
} render_style_t;

extern void            set_style(char *name, render_style_t *style);
extern render_style_t *lookup_cell_style(char *name);
extern void            install_default_styles();

static inline render_style_t *
copy_render_style(const render_style_t *style)
{
    render_style_t *result = con4m_new(T_RENDER_STYLE);

    memcpy(result, style, sizeof(render_style_t));

    return result;
}
static inline style_t
lookup_text_style(char *name)
{
    render_style_t *rs = lookup_cell_style(name);
    return rs->base_style;
}

static inline void
set_fg_color(render_style_t *style, color_t color)
{
    style->base_style |= FG_COLOR_ON | color;
}

static inline void
set_bg_color(render_style_t *style, color_t color)
{
    style->base_style |= BG_COLOR_ON | ((uint64_t)color) << 24;
}

static inline void
bold_on(render_style_t *style)
{
    style->base_style |= BOLD_ON;
}

static inline void
bold_off(render_style_t *style)
{
    style->base_style &= ~BOLD_ON;
}

static inline void
italic_on(render_style_t *style)
{
    style->base_style |= ITALIC_ON;
}

static inline void
italic_off(render_style_t *style)
{
    style->base_style &= ~ITALIC_ON;
}

static inline void
strikethru_on(render_style_t *style)
{
    style->base_style |= ST_ON;
}

static inline void
strikethru_off(render_style_t *style)
{
    style->base_style &= ~ST_ON;
}

static inline void
underline_off(render_style_t *style)
{
    style->base_style &= ~(UL_ON|UL_DOUBLE);
}

static inline void
underline_on(render_style_t *style)
{
    underline_off(style);
    style->base_style |= UL_ON;
}


static inline void
double_underline_on(render_style_t *style)
{
    underline_off(style);
    style->base_style |= UL_DOUBLE;
}


static inline void
inverse_on(render_style_t *style)
{
    style->base_style |= INV_ON;
}

static inline void
inverse_off(render_style_t *style)
{
    style->base_style &= ~INV_ON;
}

static inline void
casing_off(render_style_t *style)
{
    style->base_style &= ~TITLE_CASE;
}


static inline void
lowercase_on(render_style_t *style)
{
    casing_off(style);
    style->base_style |= LOWER_CASE;
}

static inline void
uppercase_on(render_style_t *style)
{
    casing_off(style);
    style->base_style |= UPPER_CASE;
}

static inline void
titlecase_on(render_style_t *style)
{
    casing_off(style);
    style->base_style |= TITLE_CASE;
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
set_size_as_percent(render_style_t *style, double pct, bool round)
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
    style->top_pad  = pad;
    style->tpad_set = 1;
}

static inline void
set_bottom_pad(render_style_t *style, int8_t pad)
{
    assert(pad >= 0);
    style->bottom_pad = pad;
    style->bpad_set = 1;
}

static inline void
set_left_pad(render_style_t *style, int8_t pad)
{
    assert(pad >= 0);
    style->left_pad = pad;
    style->lpad_set = 1;
}

static inline void
set_right_pad(render_style_t *style, int8_t pad)
{
    assert(pad >= 0);
    style->right_pad = pad;
    style->rpad_set = 1;
}

static inline void
set_wrap_hang(render_style_t *style, int8_t hang)
{
    assert(hang >= 0);
    style->wrap = hang;
    style->disable_wrap = 0;
    style->hang_set     = 1;
}

static inline void
disable_line_wrap(render_style_t *style)
{
    style->disable_wrap = 1;
    style->hang_set     = 1;
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
    style->base_style &= ~FG_COLOR_ON;
}

static inline void
clear_bg_color(render_style_t *style)
{
    style->base_style &= ~BG_COLOR_ON;
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

static inline bool
is_bg_color_on(render_style_t *style)
{
    return style->base_style & BG_COLOR_ON;
}

static inline bool
is_fg_color_on(render_style_t *style)
{
    return style->base_style & FG_COLOR_ON;
}

static inline color_t
get_fg_color(render_style_t *style)
{
    return (color_t)style->base_style & ~FG_COLOR_MASK;
}

static inline color_t
get_bg_color(render_style_t *style)
{
    return (color_t)((style->base_style & ~BG_COLOR_MASK) >> 24);
}

static inline style_t
get_pad_style(render_style_t *style)
{
    if (style->pad_color_set) {
	return (style->pad_color << 24) | BG_COLOR_ON;
    }
    return style->base_style;
}

static inline style_t
get_string_style(render_style_t *style)
{
    return style->base_style;
}

static inline border_theme_t *
get_border_theme(render_style_t *style)
{
    border_theme_t *result = style->border_theme;

    if (!result) {
	result = (border_theme_t *)registered_borders;
    }
    return result;
}

extern void layer_styles(const render_style_t *, render_style_t *);
extern const con4m_vtable render_style_vtable;
extern const uint64_t rs_pmap[2];

#pragma once

#include "con4m.h"

extern void            c4m_set_style(char *name, render_style_t *style);
extern render_style_t *c4m_lookup_cell_style(char *name);
extern void            c4m_install_default_styles();

static inline render_style_t *
c4m_copy_render_style(const render_style_t *style)
{
    render_style_t *result = c4m_new(c4m_tspec_render_style());

    memcpy(result, style, sizeof(render_style_t));

    return result;
}
static inline style_t
c4m_str_style(render_style_t *style)
{
    return style->base_style;
}

static inline style_t
c4m_lookup_text_style(char *name)
{
    return c4m_str_style(c4m_lookup_cell_style(name));
}

static inline void
c4m_set_render_style_fg_color(render_style_t *style, color_t color)
{
    style->base_style |= C4M_STY_FG | color;
}

static inline void
c4m_set_render_style_bg_color(render_style_t *style, color_t color)
{
    style->base_style |= C4M_STY_BG | ((uint64_t)color) << 24;
}

static inline void
c4m_bold_on(render_style_t *style)
{
    style->base_style |= C4M_STY_BOLD;
}

static inline void
c4m_bold_off(render_style_t *style)
{
    style->base_style &= ~C4M_STY_BOLD;
}

static inline void
c4m_italic_on(render_style_t *style)
{
    style->base_style |= C4M_STY_ITALIC;
}

static inline void
c4m_italic_off(render_style_t *style)
{
    style->base_style &= ~C4M_STY_ITALIC;
}

static inline void
c4m_strikethru_on(render_style_t *style)
{
    style->base_style |= C4M_STY_ST;
}

static inline void
c4m_strikethru_off(render_style_t *style)
{
    style->base_style &= ~C4M_STY_ST;
}

static inline void
c4m_underline_off(render_style_t *style)
{
    style->base_style &= ~(C4M_STY_UL | C4M_STY_UUL);
}

static inline void
c4m_underline_on(render_style_t *style)
{
    c4m_underline_off(style);
    style->base_style |= C4M_STY_UL;
}

static inline void
c4m_double_underline_on(render_style_t *style)
{
    c4m_underline_off(style);
    style->base_style |= C4M_STY_UUL;
}

static inline void
c4m_reverse_on(render_style_t *style)
{
    style->base_style |= C4M_STY_REV;
}

static inline void
c4m_reverse_off(render_style_t *style)
{
    style->base_style &= ~C4M_STY_REV;
}

static inline void
c4m_casing_off(render_style_t *style)
{
    style->base_style &= ~C4M_STY_TITLE;
}

static inline void
c4m_lowercase_on(render_style_t *style)
{
    c4m_casing_off(style);
    style->base_style |= C4M_STY_LOWER;
}

static inline void
c4m_uppercase_on(render_style_t *style)
{
    c4m_casing_off(style);
    style->base_style |= C4M_STY_UPPER;
}

static inline void
c4m_titlecase_on(render_style_t *style)
{
    c4m_casing_off(style);
    style->base_style |= C4M_STY_TITLE;
}

const extern border_theme_t *c4m_registered_borders;

static inline bool
c4m_set_border_theme(render_style_t *style, char *name)
{
    border_theme_t *cur = (border_theme_t *)c4m_registered_borders;

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
c4m_set_flex_size(render_style_t *style, int64_t size)
{
    assert(size >= 0);
    style->dim_kind   = DIM_FLEX_UNITS;
    style->dims.units = (uint64_t)size;
}

static inline void
c4m_set_absolute_size(render_style_t *style, int64_t size)
{
    assert(size >= 0);
    style->dim_kind   = DIM_ABSOLUTE;
    style->dims.units = (uint64_t)size;
}

static inline void
c4m_set_size_range(render_style_t *style, int32_t min, int32_t max)
{
    assert(min >= 0 && max >= min);
    style->dim_kind      = DIM_ABSOLUTE_RANGE;
    style->dims.range[0] = min;
    style->dims.range[1] = max;
}

static inline void
c4m_set_fit_to_text(render_style_t *style)
{
    style->dim_kind = DIM_FIT_TO_TEXT;
}

static inline void
c4m_set_auto_size(render_style_t *style)
{
    style->dim_kind = DIM_AUTO;
}

static inline void
c4m_set_size_as_percent(render_style_t *style, double pct, bool round)
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
c4m_set_top_pad(render_style_t *style, int8_t pad)
{
    assert(pad >= 0);
    style->top_pad  = pad;
    style->tpad_set = 1;
}

static inline void
c4m_set_bottom_pad(render_style_t *style, int8_t pad)
{
    assert(pad >= 0);
    style->bottom_pad = pad;
    style->bpad_set   = 1;
}

static inline void
c4m_set_left_pad(render_style_t *style, int8_t pad)
{
    assert(pad >= 0);
    style->left_pad = pad;
    style->lpad_set = 1;
}

static inline void
c4m_set_right_pad(render_style_t *style, int8_t pad)
{
    assert(pad >= 0);
    style->right_pad = pad;
    style->rpad_set  = 1;
}

static inline void
c4m_set_wrap_hang(render_style_t *style, int8_t hang)
{
    assert(hang >= 0);
    style->wrap         = hang;
    style->disable_wrap = 0;
    style->hang_set     = 1;
}

static inline void
c4m_disable_line_wrap(render_style_t *style)
{
    style->disable_wrap = 1;
    style->hang_set     = 1;
}

static inline void
c4m_set_pad_color(render_style_t *style, color_t color)
{
    style->pad_color     = color;
    style->pad_color_set = 1;
}

static inline void
c4m_clear_fg_color(render_style_t *style)
{
    style->base_style &= ~C4M_STY_FG;
}

static inline void
c4m_clear_bg_color(render_style_t *style)
{
    style->base_style &= ~C4M_STY_BG;
}

static inline void
c4m_set_alignment(render_style_t *style, alignment_t alignment)
{
    style->alignment = alignment;
}

static inline void
c4m_set_borders(render_style_t *style, border_set_t borders)
{
    style->borders = borders;
}

static inline bool
c4m_is_bg_color_on(render_style_t *style)
{
    return style->base_style & C4M_STY_BG;
}

static inline bool
c4m_is_fg_color_on(render_style_t *style)
{
    return style->base_style & C4M_STY_FG;
}

static inline color_t
c4m_get_fg_color(render_style_t *style)
{
    return (color_t)style->base_style & ~C4M_STY_CLEAR_FG;
}

static inline color_t
c4m_get_bg_color(render_style_t *style)
{
    return (color_t)((style->base_style & ~C4M_STY_CLEAR_BG) >> 24);
}

static inline style_t
c4m_get_pad_style(render_style_t *style)
{
    if (style->pad_color_set) {
        return (style->pad_color << 24) | C4M_STY_BG;
    }
    return style->base_style;
}

static inline border_theme_t *
c4m_get_border_theme(render_style_t *style)
{
    border_theme_t *result = style->border_theme;

    if (!result) {
        result = (border_theme_t *)c4m_registered_borders;
    }
    return result;
}

static inline render_style_t *
c4m_new_render_style()
{
    return c4m_new(c4m_tspec_render_style());
}

extern bool c4m_style_exists(char *name);
extern void c4m_layer_styles(const render_style_t *, render_style_t *);

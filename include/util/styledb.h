#pragma once

#include "con4m.h"

extern void                c4m_set_style(c4m_utf8_t *,
                                         c4m_render_style_t *);
extern c4m_render_style_t *c4m_lookup_cell_style(c4m_utf8_t *);
extern void                c4m_install_default_styles();

static inline c4m_render_style_t *
c4m_copy_render_style(const c4m_render_style_t *style)
{
    c4m_render_style_t *result = c4m_new(c4m_type_render_style());

    if (!style) {
        return result;
    }

    memcpy(result, style, sizeof(c4m_render_style_t));

    return result;
}
static inline c4m_style_t
c4m_str_style(c4m_render_style_t *style)
{
    return style->base_style;
}

static inline c4m_style_t
c4m_lookup_text_style(c4m_utf8_t *name)
{
    return c4m_str_style(c4m_lookup_cell_style(name));
}

static inline void
c4m_style_set_fg_color(c4m_render_style_t *style, c4m_color_t color)
{
    style->base_style &= C4M_STY_CLEAR_FG;
    style->base_style |= C4M_STY_FG;
    style->base_style |= color;
}

static inline void
c4m_style_set_bg_color(c4m_render_style_t *style, c4m_color_t color)
{
    style->base_style &= C4M_STY_CLEAR_BG;
    style->base_style |= C4M_STY_BG;
    style->base_style |= (((int64_t)color) << (int64_t)C4M_BG_SHIFT);
}

static inline void
c4m_style_bold_on(c4m_render_style_t *style)
{
    style->base_style |= C4M_STY_BOLD;
}

static inline void
c4m_style_bold_off(c4m_render_style_t *style)
{
    style->base_style &= ~C4M_STY_BOLD;
}

static inline bool
c4m_style_get_bold(c4m_render_style_t *style)
{
    return (bool)(style->base_style & C4M_STY_BOLD);
}

static inline void
c4m_style_italic_on(c4m_render_style_t *style)
{
    style->base_style |= C4M_STY_ITALIC;
}

static inline void
c4m_style_italic_off(c4m_render_style_t *style)
{
    style->base_style &= ~C4M_STY_ITALIC;
}

static inline bool
c4m_style_get_italic(c4m_render_style_t *style)
{
    return (bool)(style->base_style & C4M_STY_ITALIC);
}

static inline void
c4m_style_strikethru_on(c4m_render_style_t *style)
{
    style->base_style |= C4M_STY_ST;
}

static inline void
c4m_style_strikethru_off(c4m_render_style_t *style)
{
    style->base_style &= ~C4M_STY_ST;
}

static inline bool
c4m_style_get_strikethru(c4m_render_style_t *style)
{
    return (bool)(style->base_style & C4M_STY_ST);
}

static inline void
c4m_style_underline_off(c4m_render_style_t *style)
{
    style->base_style &= ~(C4M_STY_UL | C4M_STY_UUL);
}

static inline void
c4m_style_underline_on(c4m_render_style_t *style)
{
    c4m_style_underline_off(style);
    style->base_style |= C4M_STY_UL;
}

static inline bool
c4m_style_get_underline(c4m_render_style_t *style)
{
    return (bool)(style->base_style & C4M_STY_UL);
}

static inline void
c4m_style_double_underline_on(c4m_render_style_t *style)
{
    c4m_style_underline_off(style);
    style->base_style |= C4M_STY_UUL;
}

static inline bool
c4m_style_get_double_underline(c4m_render_style_t *style)
{
    return (bool)(style->base_style & C4M_STY_UUL);
}

static inline void
c4m_style_reverse_on(c4m_render_style_t *style)
{
    style->base_style |= C4M_STY_REV;
}

static inline void
c4m_style_reverse_off(c4m_render_style_t *style)
{
    style->base_style &= ~C4M_STY_REV;
}

static inline bool
c4m_style_get_reverse(c4m_render_style_t *style)
{
    return (bool)(style->base_style & ~C4M_STY_REV);
}

static inline void
c4m_style_casing_off(c4m_render_style_t *style)
{
    style->base_style &= ~C4M_STY_TITLE;
}

static inline void
c4m_style_lowercase_on(c4m_render_style_t *style)
{
    c4m_style_casing_off(style);
    style->base_style |= C4M_STY_LOWER;
}

static inline void
c4m_style_uppercase_on(c4m_render_style_t *style)
{
    c4m_style_casing_off(style);
    style->base_style |= C4M_STY_UPPER;
}

static inline void
c4m_style_titlecase_on(c4m_render_style_t *style)
{
    c4m_style_casing_off(style);
    style->base_style |= C4M_STY_TITLE;
}

static inline bool
c4m_style_get_upper(c4m_render_style_t *style)
{
    return (style->base_style & C4M_STY_TITLE) == C4M_STY_UPPER;
}

static inline bool
c4m_style_get_lower(c4m_render_style_t *style)
{
    return (style->base_style & C4M_STY_TITLE) == C4M_STY_LOWER;
}

static inline bool
c4m_style_get_title(c4m_render_style_t *style)
{
    return (style->base_style & C4M_STY_TITLE) == C4M_STY_TITLE;
}

static inline c4m_dimspec_kind_t
c4m_style_col_kind(c4m_render_style_t *style)
{
    return style->dim_kind;
}

extern const c4m_border_theme_t *c4m_registered_borders;

static inline bool
c4m_style_set_border_theme(c4m_render_style_t *style, char *name)
{
    c4m_border_theme_t *cur = (c4m_border_theme_t *)c4m_registered_borders;

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
c4m_style_set_flex_size(c4m_render_style_t *style, int64_t size)
{
    assert(size >= 0);
    style->dim_kind   = C4M_DIM_FLEX_UNITS;
    style->dims.units = (uint64_t)size;
}

static inline void
c4m_style_set_absolute_size(c4m_render_style_t *style, int64_t size)
{
    assert(size >= 0);
    style->dim_kind   = C4M_DIM_ABSOLUTE;
    style->dims.units = (uint64_t)size;
}

static inline void
c4m_style_set_size_range(c4m_render_style_t *style, int32_t min, int32_t max)
{
    assert(min >= 0 && max >= min);
    style->dim_kind      = C4M_DIM_ABSOLUTE_RANGE;
    style->dims.range[0] = min;
    style->dims.range[1] = max;
}

static inline void
c4m_style_set_fit_to_text(c4m_render_style_t *style)
{
    style->dim_kind = C4M_DIM_FIT_TO_TEXT;
}

static inline void
c4m_style_set_auto_size(c4m_render_style_t *style)
{
    style->dim_kind = C4M_DIM_AUTO;
}

static inline void
c4m_style_set_size_as_percent(c4m_render_style_t *style, double pct, bool round)
{
    assert(pct >= 0);
    if (round) {
        style->dim_kind = C4M_DIM_PERCENT_ROUND;
    }
    else {
        style->dim_kind = C4M_DIM_PERCENT_TRUNCATE;
    }
    style->dims.percent = pct;
}

static inline void
c4m_style_set_top_pad(c4m_render_style_t *style, int8_t pad)
{
    assert(pad >= 0);
    style->top_pad  = pad;
    style->tpad_set = 1;
}

static inline void
c4m_style_set_bottom_pad(c4m_render_style_t *style, int8_t pad)
{
    assert(pad >= 0);
    style->bottom_pad = pad;
    style->bpad_set   = 1;
}

static inline void
c4m_style_set_left_pad(c4m_render_style_t *style, int8_t pad)
{
    assert(pad >= 0);
    style->left_pad = pad;
    style->lpad_set = 1;
}

static inline void
c4m_style_set_right_pad(c4m_render_style_t *style, int8_t pad)
{
    assert(pad >= 0);
    style->right_pad = pad;
    style->rpad_set  = 1;
}

static inline void
c4m_style_set_wrap_hang(c4m_render_style_t *style, int8_t hang)
{
    assert(hang >= 0);
    style->wrap         = hang;
    style->disable_wrap = 0;
    style->hang_set     = 1;
}

static inline void
c4m_style_disable_line_wrap(c4m_render_style_t *style)
{
    style->disable_wrap = 1;
    style->hang_set     = 1;
}

static inline void
c4m_set_pad_color(c4m_render_style_t *style, c4m_color_t color)
{
    style->pad_color     = color;
    style->pad_color_set = 1;
}

static inline void
c4m_style_clear_fg_color(c4m_render_style_t *style)
{
    style->base_style &= ~(C4M_STY_FG | C4M_STY_FG_BITS);
}

static inline void
c4m_style_clear_bg_color(c4m_render_style_t *style)
{
    style->base_style &= ~(C4M_STY_BG | C4M_STY_BG_BITS);
}

static inline void
c4m_style_set_alignment(c4m_render_style_t *style, c4m_alignment_t alignment)
{
    style->alignment = alignment;
}

static inline c4m_alignment_t
c4m_style_get_alignment(c4m_render_style_t *style)
{
    return style->alignment;
}

static inline void
c4m_style_set_borders(c4m_render_style_t *style, c4m_border_set_t borders)
{
    style->borders = borders;
}

static inline bool
c4m_style_is_bg_color_on(c4m_render_style_t *style)
{
    return style->base_style & C4M_STY_BG;
}

static inline bool
c4m_style_is_fg_color_on(c4m_render_style_t *style)
{
    return style->base_style & C4M_STY_FG;
}

static inline bool
c4m_style_is_pad_color_on(c4m_render_style_t *style)
{
    return style->pad_color_set;
}

static inline c4m_color_t
c4m_style_get_fg_color(c4m_render_style_t *style)
{
    return (c4m_color_t)style->base_style & ~C4M_STY_CLEAR_FG;
}

static inline c4m_color_t
c4m_style_get_bg_color(c4m_render_style_t *style)
{
    uint64_t extract = style->base_style & ~C4M_STY_CLEAR_BG;
    return (c4m_color_t)(extract >> C4M_BG_SHIFT);
}

static inline c4m_style_t
c4m_style_get_pad_color(c4m_render_style_t *style)
{
    if (style->pad_color_set) {
        return style->pad_color;
    }
    return c4m_style_get_bg_color(style);
}

static inline c4m_border_theme_t *
c4m_style_get_border_theme(c4m_render_style_t *style)
{
    c4m_border_theme_t *result = style->border_theme;

    if (!result) {
        result = (c4m_border_theme_t *)c4m_registered_borders;
    }
    return result;
}

static inline int64_t
c4m_style_get_top_pad(c4m_render_style_t *style)
{
    return (int64_t)style->top_pad;
}

static inline int64_t
c4m_style_get_bottom_pad(c4m_render_style_t *style)
{
    return (int64_t)style->bottom_pad;
}

static inline int64_t
c4m_style_get_left_pad(c4m_render_style_t *style)
{
    return (int64_t)style->left_pad;
}

static inline int64_t
c4m_style_get_right_pad(c4m_render_style_t *style)
{
    return (int64_t)style->right_pad;
}

static inline int64_t
c4m_style_get_wrap(c4m_render_style_t *style)
{
    if (style->disable_wrap) {
        return -1;
    }

    return (int64_t)style->wrap;
}

static inline bool
c4m_style_hang_set(c4m_render_style_t *style)
{
    return style->hang_set;
}

static inline bool
c4m_style_tpad_set(c4m_render_style_t *style)
{
    return style->tpad_set;
}

static inline bool
c4m_style_bpad_set(c4m_render_style_t *style)
{
    return style->bpad_set;
}

static inline bool
c4m_style_lpad_set(c4m_render_style_t *style)
{
    return style->lpad_set;
}

static inline bool
c4m_style_rpad_set(c4m_render_style_t *style)
{
    return style->rpad_set;
}

static inline c4m_render_style_t *
c4m_new_render_style()
{
    return c4m_new(c4m_type_render_style());
}

extern bool                c4m_style_exists(c4m_utf8_t *name);
extern c4m_render_style_t *c4m_layer_styles(c4m_render_style_t *,
                                            c4m_render_style_t *);
extern void
c4m_override_style(c4m_render_style_t *,
                   c4m_render_style_t *);

extern c4m_grid_t *c4m_style_details(c4m_render_style_t *);

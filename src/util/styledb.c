#define C4M_USE_INTERNAL_API
#include "con4m.h"

static c4m_dict_t *style_dictionary = NULL;

static const c4m_border_theme_t border_ascii = {
    .name            = "ascii",
    .horizontal_rule = '-',
    .vertical_rule   = '|',
    .upper_left      = '/',
    .upper_right     = '\\',
    .lower_left      = '\\',
    .lower_right     = '/',
    .cross           = '+',
    .top_t           = '-',
    .bottom_t        = '-',
    .left_t          = '|',
    .right_t         = '|',
    .next_style      = NULL,
};

static const c4m_border_theme_t border_asterisk = {
    .name            = "asterisk",
    .horizontal_rule = '*',
    .vertical_rule   = '*',
    .upper_left      = '*',
    .upper_right     = '*',
    .lower_left      = '*',
    .lower_right     = '*',
    .cross           = '*',
    .top_t           = '*',
    .bottom_t        = '*',
    .left_t          = '*',
    .right_t         = '*',
    .next_style      = (c4m_border_theme_t *)&border_ascii,
};

static const c4m_border_theme_t border_bold_dash2 = {
    .name            = "bold_dash2",
    .horizontal_rule = 0x2509,
    .vertical_rule   = 0x250b,
    .upper_left      = 0x250f,
    .upper_right     = 0x2513,
    .lower_left      = 0x2517,
    .lower_right     = 0x251b,
    .cross           = 0x254b,
    .top_t           = 0x2533,
    .bottom_t        = 0x253b,
    .left_t          = 0x2523,
    .right_t         = 0x252b,
    .next_style      = (c4m_border_theme_t *)&border_asterisk,
};

static const c4m_border_theme_t border_dash2 = {
    .name            = "dash2",
    .horizontal_rule = 0x2508,
    .vertical_rule   = 0x250a,
    .upper_left      = 0x250c,
    .upper_right     = 0x2510,
    .lower_left      = 0x2514,
    .lower_right     = 0x2518,
    .cross           = 0x253c,
    .top_t           = 0x252c,
    .bottom_t        = 0x2534,
    .left_t          = 0x251c,
    .right_t         = 0x2524,
    .next_style      = (c4m_border_theme_t *)&border_bold_dash2,
};

static const c4m_border_theme_t border_bold_dash = {
    .name            = "bold_dash",
    .horizontal_rule = 0x2505,
    .vertical_rule   = 0x2507,
    .upper_left      = 0x250f,
    .upper_right     = 0x2513,
    .lower_left      = 0x2517,
    .lower_right     = 0x251b,
    .cross           = 0x254b,
    .top_t           = 0x2533,
    .bottom_t        = 0x253b,
    .left_t          = 0x2523,
    .right_t         = 0x252b,
    .next_style      = (c4m_border_theme_t *)&border_dash2,
};

static const c4m_border_theme_t border_dash = {
    .name            = "dash",
    .horizontal_rule = 0x2504,
    .vertical_rule   = 0x2506,
    .upper_left      = 0x250c,
    .upper_right     = 0x2510,
    .lower_left      = 0x2514,
    .lower_right     = 0x2518,
    .cross           = 0x253c,
    .top_t           = 0x252c,
    .bottom_t        = 0x2534,
    .left_t          = 0x251c,
    .right_t         = 0x2524,
    .next_style      = (c4m_border_theme_t *)&border_bold_dash,
};

static const c4m_border_theme_t border_double = {
    .name            = "double",
    .horizontal_rule = 0x2550,
    .vertical_rule   = 0x2551,
    .upper_left      = 0x2554,
    .upper_right     = 0x2557,
    .lower_left      = 0x255a,
    .lower_right     = 0x255d,
    .cross           = 0x256c,
    .top_t           = 0x2566,
    .bottom_t        = 0x2569,
    .left_t          = 0x2560,
    .right_t         = 0x2563,
    .next_style      = (c4m_border_theme_t *)&border_dash,
};

static const c4m_border_theme_t border_bold = {
    .name            = "bold",
    .horizontal_rule = 0x2501,
    .vertical_rule   = 0x2503,
    .upper_left      = 0x250f,
    .upper_right     = 0x2513,
    .lower_left      = 0x2517,
    .lower_right     = 0x251b,
    .cross           = 0x254b,
    .top_t           = 0x2533,
    .bottom_t        = 0x253b,
    .left_t          = 0x2523,
    .right_t         = 0x252b,
    .next_style      = (c4m_border_theme_t *)&border_double,
};

static const c4m_border_theme_t border_plain = {
    .name            = "plain",
    .horizontal_rule = 0x2500,
    .vertical_rule   = 0x2502,
    .upper_left      = 0x250c,
    .upper_right     = 0x2510,
    .lower_left      = 0x2514,
    .lower_right     = 0x2518,
    .cross           = 0x253c,
    .top_t           = 0x252c,
    .bottom_t        = 0x2534,
    .left_t          = 0x251c,
    .right_t         = 0x2524,
    .next_style      = (c4m_border_theme_t *)&border_bold,
};

const c4m_border_theme_t *c4m_registered_borders = (c4m_border_theme_t *)&border_plain;

// Used for border drawing and background (pad color).
static const c4m_render_style_t default_table = {
    .borders        = C4M_BORDER_TOP | C4M_BORDER_BOTTOM | C4M_BORDER_LEFT | C4M_BORDER_RIGHT | C4M_INTERIOR_HORIZONTAL | C4M_INTERIOR_VERTICAL,
    .border_theme   = (c4m_border_theme_t *)&border_bold_dash,
    .dim_kind       = C4M_DIM_AUTO,
    .alignment      = C4M_ALIGN_MID_LEFT,
    .base_style     = 0x2f3f3f00f8f8ffull | C4M_STY_BG | C4M_STY_FG,
    .pad_color      = 0x2f3f3f,
    .weight_borders = 10,
    .weight_fg      = 4,
    .weight_bg      = 4,
    .weight_align   = 5,
    .weight_width   = 10,
};

static const c4m_render_style_t col_borders_table = {
    .borders        = C4M_BORDER_TOP | C4M_BORDER_BOTTOM | C4M_BORDER_LEFT | C4M_BORDER_RIGHT | C4M_INTERIOR_VERTICAL,
    .dim_kind       = C4M_DIM_AUTO,
    .alignment      = C4M_ALIGN_MID_LEFT,
    .base_style     = 0x2f3f3f00f8f8ffull | C4M_STY_BG | C4M_STY_FG,
    .pad_color      = 0x2f3f3f,
    .weight_borders = 10,
    .weight_fg      = 4,
    .weight_bg      = 4,
    .weight_align   = 5,
    .weight_width   = 10,
};

static const c4m_render_style_t default_tr = {
    .alignment = C4M_ALIGN_TOP_LEFT,
};

static c4m_render_style_t default_tr_even = {
    .alignment  = C4M_ALIGN_TOP_LEFT,
    .base_style = 0x1f2f2f00f2f3f4ull | C4M_STY_BG | C4M_STY_FG,
    .pad_color  = 0xa9a9a9,
    .weight_bg  = 15,
    .weight_fg  = 10,
};

static c4m_render_style_t default_tr_odd = {
    .alignment  = C4M_ALIGN_TOP_LEFT,
    .base_style = 0x2f3f3f00f8f8ffull | C4M_STY_BG | C4M_STY_FG,
    .pad_color  = 0x2f3f3f,
    .weight_bg  = 15,
    .weight_fg  = 10,
};

static const c4m_render_style_t default_th = {
    .base_style   = C4M_STY_UPPER | 0xb3ff00 | C4M_STY_BG | C4M_STY_FG | C4M_STY_BOLD,
    .alignment    = C4M_ALIGN_MID_CENTER,
    .weight_bg    = 20,
    .weight_fg    = 20,
    .weight_flags = 10,
};

static const c4m_render_style_t default_td = {
    .left_pad  = 1,
    .right_pad = 1,
};

static const c4m_render_style_t default_tcol = {
    .dim_kind     = C4M_DIM_AUTO,
    .weight_width = 10,
    .left_pad     = 1,
    .right_pad    = 1,
};

static const c4m_render_style_t default_snap_col = {
    .dim_kind     = C4M_DIM_FIT_TO_TEXT,
    .left_pad     = 1,
    .right_pad    = 1,
    .weight_width = 10,
};

static const c4m_render_style_t default_full_snap_col = {
    .dim_kind     = C4M_DIM_FIT_TO_TEXT,
    .left_pad     = 1,
    .right_pad    = 1,
    .weight_width = 10,
};

static const c4m_render_style_t default_flex_col = {
    .dim_kind     = C4M_DIM_FLEX_UNITS,
    .left_pad     = 1,
    .right_pad    = 1,
    .dims.units   = 1,
    .weight_width = 10,
};

static const c4m_render_style_t default_list_grid = {
    .base_style   = 0x2f3f3ff8f8fful | C4M_STY_BG | C4M_STY_FG,
    .bottom_pad   = 1,
    .dim_kind     = C4M_DIM_AUTO,
    .alignment    = C4M_ALIGN_MID_LEFT,
    .weight_bg    = 10,
    .weight_fg    = 20,
    .weight_width = 100,
};

static const c4m_render_style_t default_ordered_list_grid = {
    .base_style   = 0x2f3f3ff8f8fful | C4M_STY_BG | C4M_STY_FG,
    .bottom_pad   = 1,
    .dim_kind     = C4M_DIM_AUTO,
    .alignment    = C4M_ALIGN_MID_LEFT,
    .weight_bg    = 10,
    .weight_fg    = 20,
    .weight_width = 1,
};

static const c4m_render_style_t default_bullet_column = {
    .dim_kind     = C4M_DIM_ABSOLUTE,
    .base_style   = 0x0f3f3ff008f8fful | C4M_STY_BG | C4M_STY_FG,
    .left_pad     = 1,
    .dims.units   = 1,
    .alignment    = C4M_ALIGN_TOP_RIGHT,
    .weight_bg    = 1,
    .weight_fg    = 20,
    .weight_width = 10,
};

static const c4m_render_style_t default_list_text_column = {
    .base_style   = 0x0f3f300ff8f8fful | C4M_STY_BG | C4M_STY_FG,
    .dim_kind     = C4M_DIM_AUTO,
    .left_pad     = 1,
    .right_pad    = 1,
    .alignment    = C4M_ALIGN_TOP_LEFT,
    .weight_bg    = 1,
    .weight_fg    = 20,
    .weight_width = 10,
};

static const c4m_render_style_t default_tree_item = {
    .base_style   = 0x2f3f3f00f8f8fful | C4M_STY_BG | C4M_STY_FG,
    .left_pad     = 1,
    .right_pad    = 1,
    .alignment    = C4M_ALIGN_TOP_LEFT,
    .disable_wrap = true,
    .pad_color    = 0x2f3f3f,
    .weight_bg    = 10,
    .weight_fg    = 7,
};

static const c4m_render_style_t default_h1 = {
    .base_style = C4M_STY_ITALIC | C4M_STY_FG | C4M_STY_BG | C4M_STY_BOLD | 0x343434000ff2f8eULL,
    .top_pad    = 2,
    .alignment  = C4M_ALIGN_BOTTOM_CENTER,
    .weight_fg  = 10,
    .weight_bg  = 5,
};

static const c4m_render_style_t default_h2 = {
    .base_style   = C4M_STY_ITALIC | C4M_STY_FG | C4M_STY_BG | C4M_STY_BOLD | 0x60606000b3ff00ULL,
    .top_pad      = 1,
    .alignment    = C4M_ALIGN_BOTTOM_CENTER,
    .weight_fg    = 10,
    .weight_bg    = 3,
    .weight_align = 10,

};

static const c4m_render_style_t default_h3 = {
    .base_style   = C4M_STY_ITALIC | C4M_STY_FG | C4M_STY_BG | C4M_STY_BOLD | 0x45454500ee82eeULL,
    .top_pad      = 1,
    .alignment    = C4M_ALIGN_BOTTOM_CENTER,
    .weight_fg    = 10,
    .weight_bg    = 3,
    .weight_align = 10,

};

static const c4m_render_style_t default_h4 = {
    .base_style   = C4M_STY_ITALIC | C4M_STY_FG | C4M_STY_BG | C4M_STY_UL | 0xff2f8e00343434ULL,
    .alignment    = C4M_ALIGN_BOTTOM_LEFT,
    .weight_fg    = 10,
    .weight_bg    = 10,
    .weight_align = 10,
};

static const c4m_render_style_t default_h5 = {
    .base_style   = C4M_STY_ITALIC | C4M_STY_FG | C4M_STY_BG | C4M_STY_UL | 0xb3ff0000606060ULL,
    .alignment    = C4M_ALIGN_BOTTOM_LEFT,
    .weight_fg    = 10,
    .weight_bg    = 10,
    .weight_align = 10,
};

static const c4m_render_style_t default_h6 = {
    .base_style   = C4M_STY_ITALIC | C4M_STY_FG | C4M_STY_BG | C4M_STY_UL | 0x00ee82ee00454545ULL,
    .alignment    = C4M_ALIGN_BOTTOM_LEFT,
    .weight_fg    = 10,
    .weight_bg    = 10,
    .weight_align = 10,
};

static const c4m_render_style_t default_flow = {
    .base_style   = 0x2f3f3f00f8f8ffull | C4M_STY_BG | C4M_STY_FG,
    .left_pad     = 1,
    .right_pad    = 1,
    .alignment    = C4M_ALIGN_TOP_LEFT,
    .pad_color    = 0x2f3f3f,
    .weight_bg    = 5,
    .weight_fg    = 5,
    .weight_align = 5,
    .weight_width = 5,
};

static const c4m_render_style_t default_error_grid = {
    .base_style     = 0x2f3f3f00f8f8ffull | C4M_STY_BG | C4M_STY_FG,
    .alignment      = C4M_ALIGN_TOP_LEFT,
    .weight_bg      = 5,
    .weight_fg      = 5,
    .weight_align   = 5,
    .weight_width   = 5,
    .weight_borders = 10,
};

static const c4m_render_style_t default_em = {
    .base_style   = C4M_STY_ITALIC | C4M_STY_FG | 0xff2f8eULL,
    .weight_flags = 10,
    .weight_fg    = 20,
};

static const c4m_render_style_t default_callout_cell = {
    .top_pad      = 1,
    .bottom_pad   = 1,
    .left_pad     = 0,
    .right_pad    = 0,
    .alignment    = C4M_ALIGN_BOTTOM_CENTER,
    .dim_kind     = C4M_DIM_PERCENT_TRUNCATE,
    .dims.percent = 90,
    .weight_width = 20,
};

static const c4m_render_style_t default_callout = {
    .base_style     = C4M_STY_ITALIC | C4M_STY_FG | C4M_STY_BG | C4M_STY_BOLD | 0xff2f8e00b3ff00UL,
    .top_pad        = 2,
    .bottom_pad     = 2,
    .alignment      = C4M_ALIGN_BOTTOM_CENTER,
    .dim_kind       = C4M_DIM_FIT_TO_TEXT,
    .weight_borders = 10,
    .weight_fg      = 4,
    .weight_bg      = 4,
    .weight_align   = 5,
    .weight_width   = 10,
};

void
c4m_rs_gc_bits(uint64_t *bitfield, c4m_render_style_t *style)
{
    c4m_set_bit(bitfield, 0);
    c4m_set_bit(bitfield, 1);
}

static inline void
init_style_db()
{
    if (style_dictionary == NULL) {
        c4m_gc_register_root(&style_dictionary, 1);
        style_dictionary = c4m_dict(c4m_type_utf8(), c4m_type_ref());
    }
}

void
c4m_set_style(c4m_utf8_t *name, c4m_render_style_t *style)
{
    init_style_db();
    hatrack_dict_put(style_dictionary, name, style);
}

// Returns a COPY of the style so that it doesn't get accidentially
// changed by reference.
c4m_render_style_t *
c4m_lookup_cell_style(c4m_utf8_t *name)
{
    init_style_db();

    c4m_render_style_t *entry = hatrack_dict_get(style_dictionary,
                                                 name,
                                                 NULL);

    if (!entry) {
        return NULL;
    }

    c4m_render_style_t *result = c4m_new(c4m_type_render_style());

    memcpy(result, entry, sizeof(c4m_render_style_t));
    return result;
}

void
c4m_style_init(c4m_render_style_t *style, va_list args)
{
    c4m_color_t fg_color        = -1;
    c4m_color_t bg_color        = -1;
    bool        bold            = false;
    bool        italic          = false;
    bool        strikethru      = false;
    bool        underline       = false;
    bool        duline          = false;
    bool        reverse         = false;
    bool        fit_text        = false;
    bool        disable_wrap    = false;
    double      width_pct       = -1;
    int64_t     flex_units      = -1;
    int32_t     min_size        = -1;
    int32_t     max_size        = -1;
    int32_t     top_pad         = -1;
    int32_t     bottom_pad      = -1;
    int32_t     left_pad        = -1;
    int32_t     right_pad       = -1;
    int32_t     wrap_hang       = -1;
    c4m_color_t pad_color       = 0xffffffff;
    int32_t     alignment       = -1;
    int32_t     enabled_borders = -1;
    char       *border_theme    = NULL;
    char       *tag             = NULL;

    c4m_karg_va_init(args);
    c4m_kw_int32("fg_color", fg_color);
    c4m_kw_int32("bg_color", bg_color);
    c4m_kw_bool("bold", bold);
    c4m_kw_bool("italic", italic);
    c4m_kw_bool("strike", strikethru);
    c4m_kw_bool("underline", underline);
    c4m_kw_bool("double_underline", duline);
    c4m_kw_bool("reverse", reverse);
    c4m_kw_bool("fit_text", fit_text);
    c4m_kw_bool("disable_wrap", disable_wrap);
    c4m_kw_float("width_pct", width_pct);
    c4m_kw_int64("flex_units", flex_units);
    c4m_kw_int32("min_size", min_size);
    c4m_kw_int32("max_size", max_size);
    c4m_kw_int32("top_pad", top_pad);
    c4m_kw_int32("bottom_pad", bottom_pad);
    c4m_kw_int32("left_pad", left_pad);
    c4m_kw_int32("right_pad", right_pad);
    c4m_kw_int32("wrap_hand", wrap_hang);
    c4m_kw_int32("pad_color", pad_color);
    c4m_kw_int32("alignment", alignment);
    c4m_kw_int32("enabled_borders", enabled_borders);
    c4m_kw_ptr("border_theme", border_theme);
    c4m_kw_ptr("tag", tag);

    style->name     = c4m_new_utf8(tag);
    // Use basic math to make sure overlaping cell sizing strategies
    // aren't requested in one call.
    int32_t sz_test = width_pct + flex_units + min_size + max_size + fit_text;

    if (sz_test != -5) {
        if ((width_pct != -1 && (flex_units + min_size + max_size + fit_text) != -3) || (flex_units != -1 && (min_size + max_size + (int)fit_text) != -2) || (fit_text && (min_size + max_size) != -2)) {
            C4M_CRAISE("Can't specify two cell sizing strategies.");
        }
    }

    if (wrap_hang != -1 && disable_wrap) {
        C4M_CRAISE("Cannot set 'wrap_hang' and 'disable_wrap' at once.\n");
    }

    if (fg_color != -1) {
        c4m_style_set_fg_color(style, fg_color);
    }
    if (bg_color != -1) {
        c4m_style_set_bg_color(style, bg_color);
    }
    if (bold) {
        c4m_style_bold_on(style);
    }
    if (italic) {
        c4m_style_italic_on(style);
    }
    if (strikethru) {
        c4m_style_strikethru_on(style);
    }
    if (duline) {
        c4m_style_double_underline_on(style);
    }
    else {
        if (underline) {
            c4m_style_underline_on(style);
        }
    }
    if (reverse) {
        c4m_style_reverse_on(style);
    }

    if (border_theme != NULL) {
        c4m_style_set_border_theme(style, border_theme);
    }

    if (width_pct != -1) {
        c4m_style_set_size_as_percent(style, width_pct, true);
    }

    if (flex_units != -1) {
        c4m_style_set_flex_size(style, flex_units);
    }

    if (min_size >= 0 || max_size >= 0) {
        if (min_size < 0) {
            min_size = 0;
        }
        if (max_size < 0) {
            max_size = 0x7fffffff;
        }
        c4m_style_set_size_range(style, min_size, max_size);
    }

    if (fit_text) {
        c4m_style_set_fit_to_text(style);
    }

    if (top_pad != -1) {
        c4m_style_set_top_pad(style, top_pad);
    }

    if (bottom_pad != -1) {
        c4m_style_set_bottom_pad(style, bottom_pad);
    }

    if (left_pad != -1) {
        c4m_style_set_left_pad(style, left_pad);
    }

    if (right_pad != -1) {
        c4m_style_set_right_pad(style, right_pad);
    }

    if (wrap_hang != -1) {
        c4m_style_set_wrap_hang(style, wrap_hang);
    }

    if (disable_wrap) {
        c4m_style_disable_line_wrap(style);
    }

    if (pad_color != -1) {
        c4m_set_pad_color(style, pad_color);
    }

    if (alignment != -1) {
        c4m_style_set_alignment(style, (c4m_alignment_t)alignment);
    }

    if (enabled_borders != -1) {
        c4m_style_set_borders(style,
                              (c4m_border_set_t)enabled_borders);
    }

    if (tag != NULL) {
        c4m_set_style(c4m_new_utf8(tag), style);
    }
}

c4m_render_style_t *
c4m_layer_styles(c4m_render_style_t *s1,
                 c4m_render_style_t *s2)
{
    if (!s1 || !s2) {
        return NULL;
    }
    c4m_render_style_t *res = c4m_new(c4m_type_render_style());

    if (s1 == NULL || s1 == s2) {
        if (s2 == NULL) {
            return NULL;
        }
        memcpy(res, s2, sizeof(c4m_render_style_t));
        return res;
    }
    if (s2 == NULL) {
        memcpy(res, s1, sizeof(c4m_render_style_t));
        return res;
    }

    if (s1->weight_fg >= s2->weight_fg) {
        c4m_style_set_fg_color(res, c4m_style_get_fg_color(s1));
        res->weight_fg = s1->weight_fg;
    }
    else {
        c4m_style_set_fg_color(res, c4m_style_get_fg_color(s2));
        res->weight_fg = s2->weight_fg;
    }

    if (s1->weight_bg >= s2->weight_bg) {
        c4m_style_set_bg_color(res, c4m_style_get_bg_color(s1));
        res->pad_color     = s1->pad_color;
        res->pad_color_set = s1->pad_color_set;
        res->weight_bg     = s1->weight_bg;
    }
    else {
        c4m_style_set_bg_color(res, c4m_style_get_bg_color(s2));
        res->pad_color     = s2->pad_color;
        res->pad_color_set = s2->pad_color_set;
        res->weight_bg     = s2->weight_bg;
    }

    static uint64_t mask = ~(C4M_STY_CLEAR_FLAGS | C4M_STY_FG | C4M_STY_BG);
    if (s1->weight_flags >= s2->weight_flags) {
        res->base_style |= s1->base_style & mask;
        res->weight_flags = s1->weight_flags;
    }
    else {
        res->base_style |= s2->base_style & mask;
        res->weight_flags = s2->weight_flags;
    }

    if (s1->weight_align >= s2->weight_align) {
        res->alignment    = s1->alignment;
        res->weight_align = s1->weight_align;
    }
    else {
        res->alignment    = s2->alignment;
        res->weight_align = s2->weight_align;
    }

    if (s1->weight_width >= s2->weight_width) {
        res->dim_kind     = s1->dim_kind;
        res->dims         = s1->dims;
        res->disable_wrap = s1->disable_wrap;
        res->top_pad      = s1->top_pad;
        res->bottom_pad   = s1->bottom_pad;
        res->left_pad     = s1->left_pad;
        res->right_pad    = s1->right_pad;
        res->wrap         = s1->wrap;
        res->hang_set     = s1->hang_set;
        res->tpad_set     = s1->tpad_set;
        res->bpad_set     = s1->bpad_set;
        res->lpad_set     = s1->lpad_set;
        res->rpad_set     = s1->rpad_set;
        res->weight_width = s1->weight_width;
    }
    else {
        res->dim_kind     = s2->dim_kind;
        res->dims         = s2->dims;
        res->disable_wrap = s2->disable_wrap;
        res->top_pad      = s2->top_pad;
        res->bottom_pad   = s2->bottom_pad;
        res->left_pad     = s2->left_pad;
        res->right_pad    = s2->right_pad;
        res->wrap         = s2->wrap;
        res->hang_set     = s2->hang_set;
        res->tpad_set     = s2->tpad_set;
        res->bpad_set     = s2->bpad_set;
        res->lpad_set     = s2->lpad_set;
        res->rpad_set     = s2->rpad_set;
        res->weight_width = s2->weight_width;
    }

    if (s1->weight_borders >= s2->weight_borders) {
        res->borders        = s1->borders;
        res->border_theme   = s1->border_theme;
        res->weight_borders = s1->weight_borders;
    }
    else {
        res->borders        = s2->borders;
        res->border_theme   = s2->border_theme;
        res->weight_borders = s2->weight_borders;
    }

    return res;
}

bool
c4m_style_exists(c4m_utf8_t *name)
{
    if (name == NULL) {
        return 0;
    }

    init_style_db();
    return hatrack_dict_get(style_dictionary, name, NULL) != NULL;
}

static void
static_style(char *name, c4m_render_style_t *s)
{
    c4m_render_style_t *copy = c4m_new(c4m_type_render_style());
    memcpy(copy, s, sizeof(c4m_render_style_t));
    copy->name = c4m_new_utf8(name);

    c4m_set_style(copy->name, copy);
}

void
c4m_install_default_styles()
{
    static_style("table", (c4m_render_style_t *)&default_table);
    static_style("table2", (c4m_render_style_t *)&col_borders_table);
    static_style("tr", (c4m_render_style_t *)&default_tr);
    static_style("tr.even", (c4m_render_style_t *)&default_tr_even);
    static_style("tr.odd", (c4m_render_style_t *)&default_tr_odd);
    static_style("td", (c4m_render_style_t *)&default_td);
    static_style("text", (c4m_render_style_t *)&default_td);
    static_style("th", (c4m_render_style_t *)&default_th);
    static_style("tcol", (c4m_render_style_t *)&default_tcol);
    static_style("snap", (c4m_render_style_t *)&default_snap_col);
    static_style("full_snap", (c4m_render_style_t *)&default_full_snap_col);
    static_style("flex", (c4m_render_style_t *)&default_flex_col);
    static_style("ul", (c4m_render_style_t *)&default_list_grid);
    static_style("ol", (c4m_render_style_t *)&default_ordered_list_grid);
    static_style("bullet", (c4m_render_style_t *)&default_bullet_column);
    static_style("li", (c4m_render_style_t *)&default_list_text_column);
    static_style("tree_item", (c4m_render_style_t *)&default_tree_item);
    static_style("h1", (c4m_render_style_t *)&default_h1);
    static_style("h2", (c4m_render_style_t *)&default_h2);
    static_style("h3", (c4m_render_style_t *)&default_h3);
    static_style("h4", (c4m_render_style_t *)&default_h4);
    static_style("h5", (c4m_render_style_t *)&default_h5);
    static_style("h6", (c4m_render_style_t *)&default_h6);
    static_style("table", (c4m_render_style_t *)&default_table);
    static_style("flow", (c4m_render_style_t *)&default_flow);
    static_style("error_grid", (c4m_render_style_t *)&default_error_grid);
    static_style("em", (c4m_render_style_t *)&default_em);
    static_style("callout_cell", (c4m_render_style_t *)&default_callout_cell);
    static_style("callout", (c4m_render_style_t *)&default_callout);
}

c4m_grid_t *
c4m_style_details(c4m_render_style_t *s)
{
    c4m_grid_t *grid = c4m_new(c4m_type_grid(),
                               c4m_kw("start_cols",
                                      c4m_ka(2),
                                      "header_cols",
                                      c4m_ka(1),
                                      "stripe",
                                      c4m_ka(true)));

    c4m_list_t *row  = c4m_new_table_row();
    c4m_utf8_t *nope = c4m_rich_lit("[i]Not set[/]");

    c4m_list_append(row, c4m_new_utf8("Name: "));

    if (s->name == NULL) {
        c4m_list_append(row, nope);
    }
    else {
        c4m_list_append(row, s->name);
    }

    c4m_grid_add_row(grid, row);
    row = c4m_new_table_row();

    c4m_list_append(row, c4m_new_utf8("Border Theme: "));
    c4m_border_theme_t *theme = c4m_style_get_border_theme(s);

    if (theme == NULL) {
        c4m_list_append(row, nope);
    }
    else {
        c4m_list_append(row, c4m_new_utf8(theme->name));
    }

    c4m_grid_add_row(grid, row);
    row = c4m_new_table_row();

    c4m_list_append(row, c4m_new_utf8("Base FG Color: "));

    if (!c4m_style_is_fg_color_on(s)) {
        c4m_list_append(row, nope);
    }
    else {
        c4m_list_append(row, c4m_cstr_format("{:x}", c4m_style_get_fg_color(s)));
    }

    c4m_grid_add_row(grid, row);
    row = c4m_new_table_row();

    c4m_list_append(row, c4m_new_utf8("Base BG Color: "));

    if (!c4m_style_is_bg_color_on(s)) {
        c4m_list_append(row, nope);
    }
    else {
        c4m_list_append(row, c4m_cstr_format("{:x}", c4m_style_get_bg_color(s)));
    }

    c4m_grid_add_row(grid, row);
    row = c4m_new_table_row();

    c4m_list_append(row, c4m_new_utf8("Pad Color: "));

    if (!c4m_style_is_pad_color_on(s)) {
        c4m_list_append(row, nope);
    }
    else {
        c4m_list_append(row, c4m_cstr_format("{:x}", c4m_style_get_pad_color(s)));
    }

    c4m_grid_add_row(grid, row);
    row = c4m_new_table_row();

    c4m_list_t *text_items = c4m_list(c4m_type_utf8());
    c4m_utf8_t *ti;

    if (c4m_style_get_bold(s)) {
        c4m_list_append(text_items, c4m_new_utf8("bold"));
    }

    if (c4m_style_get_italic(s)) {
        c4m_list_append(text_items, c4m_new_utf8("italic"));
    }

    if (c4m_style_get_strikethru(s)) {
        c4m_list_append(text_items, c4m_new_utf8("strikethru"));
    }

    if (c4m_style_get_underline(s)) {
        c4m_list_append(text_items, c4m_new_utf8("underline"));
    }

    if (c4m_style_get_double_underline(s)) {
        c4m_list_append(text_items, c4m_new_utf8("2xunderline"));
    }

    if (c4m_style_get_reverse(s)) {
        c4m_list_append(text_items, c4m_new_utf8("reverse"));
    }

    if (c4m_style_get_upper(s)) {
        c4m_list_append(text_items, c4m_new_utf8("upper"));
    }

    if (c4m_style_get_lower(s)) {
        c4m_list_append(text_items, c4m_new_utf8("lower"));
    }

    if (c4m_style_get_title(s)) {
        c4m_list_append(text_items, c4m_new_utf8("title"));
    }

    if (c4m_list_len(text_items) == 0) {
        ti = nope;
    }
    else {
        ti = c4m_to_utf8(c4m_str_join(text_items, c4m_new_utf8(", ")));
    }

    c4m_list_append(row, c4m_new_utf8("Text styling: "));
    c4m_list_append(row, ti);
    c4m_grid_add_row(grid, row);
    row = c4m_new_table_row();

    c4m_list_append(row, c4m_new_utf8("Cell padding: "));
    c4m_utf8_t *pads = c4m_cstr_format(
        "t: {}, b: {}, l: {}, r: {}",
        c4m_style_tpad_set(s) ? (void *)c4m_style_get_top_pad(s) : nope,
        c4m_style_bpad_set(s) ? (void *)c4m_style_get_bottom_pad(s) : nope,
        c4m_style_lpad_set(s) ? (void *)c4m_style_get_left_pad(s) : nope,
        c4m_style_rpad_set(s) ? (void *)c4m_style_get_right_pad(s) : nope);

    c4m_list_append(row, pads);
    c4m_grid_add_row(grid, row);
    row = c4m_new_table_row();

    c4m_list_append(row, c4m_new_utf8("Wrap: "));
    int64_t     wrap = c4m_style_get_wrap(s);
    c4m_utf8_t *hang = c4m_style_hang_set(s) ? c4m_new_utf8("y") : c4m_new_utf8("n");

    switch (wrap) {
    case -1:
        c4m_list_append(row, c4m_new_utf8("[i]Disabled (will overflow)[/]"));
        break;
    case 0:
        c4m_list_append(row, nope);
        break;
    default:
        c4m_list_append(row, c4m_cstr_format("{} cols (hang = {})", wrap, hang));
        break;
    }

    c4m_grid_add_row(grid, row);
    row = c4m_new_table_row();

    c4m_utf8_t *col_kind;

    switch (c4m_style_col_kind(s)) {
    case C4M_DIM_AUTO:
        col_kind = c4m_new_utf8("Auto-fit");
        break;
    case C4M_DIM_PERCENT_TRUNCATE:
        col_kind = c4m_cstr_format("{}% (truncate)");
        break;
    case C4M_DIM_PERCENT_ROUND:
        col_kind = c4m_cstr_format("{}% (round)");
        break;
    case C4M_DIM_FLEX_UNITS:
        col_kind = c4m_cstr_format("{} flex units");
        break;
    case C4M_DIM_ABSOLUTE:
        col_kind = c4m_cstr_format("{} columns");
        break;
    case C4M_DIM_ABSOLUTE_RANGE:
        col_kind = c4m_cstr_format("{} - {} columns");
        break;
    case C4M_DIM_FIT_TO_TEXT:
        col_kind = c4m_cstr_format("size to longest cell (fit-to-text)");
        break;
    default: // C4M_DIM_UNSET
        col_kind = nope;
        break;
    }

    c4m_list_append(row, c4m_new_utf8("Column setup: "));
    c4m_list_append(row, col_kind);
    c4m_grid_add_row(grid, row);

    return grid;
}

const c4m_vtable_t c4m_render_style_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)c4m_style_init,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)C4M_GC_SCAN_ALL,
        // Explicit because some compilers don't seem to always properly
        // zero it (Was sometimes crashing on a `c4m_stream_t` on my mac).
        [C4M_BI_FINALIZER]   = NULL,
    },
};

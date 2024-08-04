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
    .name         = "table",
    .borders      = C4M_BORDER_TOP | C4M_BORDER_BOTTOM | C4M_BORDER_LEFT | C4M_BORDER_RIGHT | C4M_INTERIOR_HORIZONTAL | C4M_INTERIOR_VERTICAL,
    .border_theme = (c4m_border_theme_t *)&border_bold_dash,
    .dim_kind     = C4M_DIM_AUTO,
    .alignment    = C4M_ALIGN_MID_LEFT,
};

static const c4m_render_style_t col_borders_table = {
    .name      = "table2",
    .borders   = C4M_BORDER_TOP | C4M_BORDER_BOTTOM | C4M_BORDER_LEFT | C4M_BORDER_RIGHT | C4M_INTERIOR_VERTICAL,
    .dim_kind  = C4M_DIM_AUTO,
    .alignment = C4M_ALIGN_MID_LEFT,
};

static const c4m_render_style_t default_tr = {
    .name       = "tr",
    .dim_kind   = C4M_DIM_AUTO,
    .alignment  = C4M_ALIGN_TOP_LEFT,
    .base_style = 0x2f3f3ff8f8fful | C4M_STY_BG | C4M_STY_FG,
};

static const c4m_render_style_t default_tr_even = {
    .name       = "tr.even",
    .dim_kind   = C4M_DIM_AUTO,
    .alignment  = C4M_ALIGN_TOP_LEFT,
    .base_style = 0x3f3f3ff8f8fful | C4M_STY_BG | C4M_STY_FG,
};

static const c4m_render_style_t default_tr_odd = {
    .name       = "tr.odd",
    .dim_kind   = C4M_DIM_AUTO,
    .alignment  = C4M_ALIGN_TOP_LEFT,
    .base_style = 0x5f5f5ff8f8fful | C4M_STY_BG | C4M_STY_FG,
};

static const c4m_render_style_t default_th = {
    .name       = "th",
    .base_style = C4M_STY_UPPER | 0xb3ff00 | C4M_STY_BG | C4M_STY_FG | C4M_STY_BOLD,
    .dim_kind   = C4M_DIM_AUTO,
    .alignment  = C4M_ALIGN_MID_CENTER,
};

static const c4m_render_style_t default_td = {
    .name       = "td",
    .base_style = 0,
    .left_pad   = 1,
    .right_pad  = 1,
};

static const c4m_render_style_t default_tcol = {
    .name     = "tcol",
    .dim_kind = C4M_DIM_AUTO,
};

static const c4m_render_style_t default_snap_col = {
    .name     = "snap",
    .dim_kind = C4M_DIM_FIT_TO_TEXT,
};

static const c4m_render_style_t default_full_snap_col = {
    .name      = "full_snap",
    .dim_kind  = C4M_DIM_FIT_TO_TEXT,
    .left_pad  = 0,
    .right_pad = 0,
};

static const c4m_render_style_t default_list_grid = {
    .name       = "ul",
    //.base_style = 0x2f3f3ff8f8fful | C4M_STY_BG | C4M_STY_FG,
    .bottom_pad = 1,
    .dim_kind   = C4M_DIM_AUTO,
    .alignment  = C4M_ALIGN_MID_LEFT,
};

static const c4m_render_style_t default_ordered_list_grid = {
    .name       = "ol",
    //.base_style = 0x2f3f3ff8f8fful | C4M_STY_BG | C4M_STY_FG,
    .bottom_pad = 1,
    .dim_kind   = C4M_DIM_AUTO,
    .alignment  = C4M_ALIGN_MID_LEFT,
};

static const c4m_render_style_t default_bullet_column = {
    .name       = "bullet",
    .dim_kind   = C4M_DIM_ABSOLUTE,
    //.base_style = 0x2f3f3ff8f8fful | C4M_STY_BG | C4M_STY_FG,
    .left_pad   = 1,
    .dims.units = 1,
    .alignment  = C4M_ALIGN_TOP_RIGHT,
};

static const c4m_render_style_t default_list_text_column = {
    .name      = "li",
    //.base_style = 0x2f3f3ff8f8fful | C4M_STY_BG | C4M_STY_FG,
    .dim_kind  = C4M_DIM_AUTO,
    .left_pad  = 1,
    .right_pad = 1,
    .alignment = C4M_ALIGN_TOP_LEFT,
};

static const c4m_render_style_t default_tree_item = {
    .name         = "tree_item",
    .base_style   = 0x2f3f3ff8f8fful | C4M_STY_BG | C4M_STY_FG,
    .dim_kind     = C4M_DIM_AUTO,
    .left_pad     = 1,
    .right_pad    = 1,
    .alignment    = C4M_ALIGN_TOP_LEFT,
    .disable_wrap = true,
};

static const c4m_render_style_t default_h1 = {
    .name       = "h1",
    .base_style = C4M_STY_ITALIC | C4M_STY_FG | C4M_STY_BG | C4M_STY_BOLD | 0x3434340ff2f8eUL,
    .top_pad    = 2,
    .alignment  = C4M_ALIGN_BOTTOM_CENTER,
};

static const c4m_render_style_t default_h2 = {
    .name       = "h2",
    .base_style = C4M_STY_ITALIC | C4M_STY_FG | C4M_STY_BG | C4M_STY_BOLD | 0x606060b3ff00UL,
    .top_pad    = 1,
    .alignment  = C4M_ALIGN_BOTTOM_CENTER,
};

static const c4m_render_style_t default_h3 = {
    .name       = "h3",
    .base_style = C4M_STY_ITALIC | C4M_STY_FG | C4M_STY_BG | C4M_STY_BOLD | 0x454545ee82eeUL,
    .top_pad    = 1,
    .alignment  = C4M_ALIGN_BOTTOM_CENTER,
};

static const c4m_render_style_t default_h4 = {
    .name       = "h4",
    .base_style = C4M_STY_ITALIC | C4M_STY_FG | C4M_STY_BG | C4M_STY_UL | (0xff2f8eUL << 24),
    .alignment  = C4M_ALIGN_BOTTOM_LEFT,

};

static const c4m_render_style_t default_h5 = {
    .name       = "h5",
    .base_style = C4M_STY_ITALIC | C4M_STY_FG | C4M_STY_BG | C4M_STY_UL | (0xb3ff00UL << 24),
    .alignment  = C4M_ALIGN_BOTTOM_LEFT,
};

static const c4m_render_style_t default_h6 = {
    .name       = "h6",
    .base_style = C4M_STY_ITALIC | C4M_STY_FG | C4M_STY_BG | C4M_STY_UL | (0xee82eeUL << 24),
    .alignment  = C4M_ALIGN_BOTTOM_LEFT,
};

static const c4m_render_style_t default_flow = {
    .name       = "flow",
    .base_style = 0x2f3f3ff8f8fful | C4M_STY_BG | C4M_STY_FG,
    .left_pad   = 1,
    .right_pad  = 1,
    .alignment  = C4M_ALIGN_TOP_LEFT,
};

static const c4m_render_style_t default_error_grid = {
    .name       = "error_grid",
    .base_style = 0x2f3f3ff8f8fful | C4M_STY_FG,
    .left_pad   = 0,
    .right_pad  = 0,
    .alignment  = C4M_ALIGN_TOP_LEFT,
};

static const c4m_render_style_t default_em = {
    .name       = "em",
    .base_style = C4M_STY_ITALIC | C4M_STY_FG | C4M_STY_ITALIC | 0x0ff2f8eUL,
};

static const c4m_render_style_t default_callout_cell = {
    .name         = "callout_cell",
    .top_pad      = 1,
    .bottom_pad   = 1,
    .left_pad     = 0,
    .right_pad    = 0,
    .alignment    = C4M_ALIGN_BOTTOM_CENTER,
    .dim_kind     = C4M_DIM_PERCENT_TRUNCATE,
    .dims.percent = 90,
};

static const c4m_render_style_t default_callout = {
    .name       = "callout",
    .base_style = C4M_STY_ITALIC | C4M_STY_FG | C4M_STY_BG | C4M_STY_BOLD | 0xff2f8eb3ff00UL,
    .top_pad    = 2,
    .bottom_pad = 2,
    .alignment  = C4M_ALIGN_BOTTOM_CENTER,
    .dim_kind   = C4M_DIM_FIT_TO_TEXT,
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
        style_dictionary = c4m_dict(c4m_type_utf8(), c4m_type_ref());
        c4m_gc_register_root(&style_dictionary, 1);
    }
}

void
c4m_set_style(char *name, c4m_render_style_t *style)
{
    init_style_db();
    hatrack_dict_put(style_dictionary, c4m_new_utf8(name), style);
}

// Returns a COPY of the style so that it doesn't get accidentially
// changed by reference.
c4m_render_style_t *
c4m_lookup_cell_style(char *name)
{
    init_style_db();

    c4m_render_style_t *entry = hatrack_dict_get(style_dictionary,
                                                 c4m_new_utf8(name),
                                                 NULL);

    if (!entry) {
        return NULL;
    }

    c4m_render_style_t *result = c4m_gc_alloc_mapped(c4m_render_style_t,
                                                     c4m_rs_gc_bits);
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

    style->name     = tag;
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
        c4m_set_render_style_fg_color(style, fg_color);
    }
    if (bg_color != -1) {
        c4m_set_render_style_bg_color(style, bg_color);
    }
    if (bold) {
        c4m_bold_on(style);
    }
    if (italic) {
        c4m_italic_on(style);
    }
    if (strikethru) {
        c4m_strikethru_on(style);
    }
    if (duline) {
        c4m_double_underline_on(style);
    }
    else {
        if (underline) {
            c4m_underline_on(style);
        }
    }
    if (reverse) {
        c4m_reverse_on(style);
    }

    if (border_theme != NULL) {
        c4m_set_border_theme(style, border_theme);
    }

    if (width_pct != -1) {
        c4m_set_size_as_percent(style, width_pct, true);
    }

    if (flex_units != -1) {
        c4m_set_flex_size(style, flex_units);
    }

    if (min_size >= 0 || max_size >= 0) {
        if (min_size < 0) {
            min_size = 0;
        }
        if (max_size < 0) {
            max_size = 0x7fffffff;
        }
        c4m_set_size_range(style, min_size, max_size);
    }

    if (fit_text) {
        c4m_set_fit_to_text(style);
    }

    if (top_pad != -1) {
        c4m_set_top_pad(style, top_pad);
    }

    if (bottom_pad != -1) {
        c4m_set_bottom_pad(style, bottom_pad);
    }

    if (left_pad != -1) {
        c4m_set_left_pad(style, left_pad);
    }

    if (right_pad != -1) {
        c4m_set_right_pad(style, right_pad);
    }

    if (wrap_hang != -1) {
        c4m_set_wrap_hang(style, wrap_hang);
    }

    if (disable_wrap) {
        c4m_disable_line_wrap(style);
    }

    if (pad_color != -1) {
        c4m_set_pad_color(style, pad_color);
    }

    if (alignment != -1) {
        c4m_set_alignment(style, (c4m_alignment_t)alignment);
    }

    if (enabled_borders != -1) {
        c4m_set_borders(style, (c4m_border_set_t)enabled_borders);
    }

    if (tag != NULL) {
        c4m_set_style(tag, style);
    }
}

void
c4m_layer_styles(const c4m_render_style_t *base, c4m_render_style_t *cur)
{
    // Anything not explicitly set in 'cur' will get set from base.
    if (!cur || !base) {
        return;
    }
    if (!(cur->base_style & C4M_STY_FG) && base->base_style & C4M_STY_FG) {
        c4m_set_render_style_fg_color(cur,
                                      base->base_style & ~C4M_STY_CLEAR_FG);
    }
    if (!(cur->base_style & C4M_STY_BG) && base->base_style & C4M_STY_BG) {
        c4m_set_render_style_bg_color(
            cur,
            (c4m_color_t)((base->base_style & ~C4M_STY_CLEAR_BG) >> 24));
    }
    if (base->base_style & C4M_STY_BOLD) {
        cur->base_style |= C4M_STY_BOLD;
    }
    if (base->base_style & C4M_STY_ITALIC) {
        cur->base_style |= C4M_STY_ITALIC;
    }
    if (base->base_style & C4M_STY_ST) {
        cur->base_style |= C4M_STY_ST;
    }
    if (base->base_style & C4M_STY_UL) {
        cur->base_style |= C4M_STY_UL;
    }
    if (base->base_style & C4M_STY_UUL) {
        cur->base_style |= C4M_STY_UUL;
    }
    if (base->base_style & C4M_STY_REV) {
        cur->base_style |= C4M_STY_REV;
    }
    if (base->base_style & C4M_STY_LOWER) {
        cur->base_style |= C4M_STY_LOWER;
    }
    if (base->base_style & C4M_STY_UPPER) {
        cur->base_style |= C4M_STY_UPPER;
    }

    if (cur->border_theme == NULL && base->border_theme != NULL) {
        cur->border_theme = base->border_theme;
    }

    if (!cur->pad_color_set && base->pad_color_set) {
        c4m_set_pad_color(cur, base->pad_color);
    }

    if (cur->dim_kind == C4M_DIM_UNSET && base->dim_kind != C4M_DIM_UNSET) {
        cur->dim_kind = base->dim_kind;
        cur->dims     = base->dims;
    }

    if (!cur->top_pad && !cur->tpad_set && base->tpad_set) {
        cur->top_pad  = base->top_pad;
        cur->tpad_set = 1;
    }

    if (!cur->bottom_pad && !cur->bpad_set && base->bpad_set) {
        cur->bottom_pad = base->bottom_pad;
        cur->bpad_set   = 1;
    }

    if (!cur->left_pad && !cur->lpad_set && base->lpad_set) {
        cur->left_pad = base->left_pad;
        cur->lpad_set = 1;
    }

    if (!cur->right_pad && !cur->rpad_set && base->rpad_set) {
        cur->right_pad = base->right_pad;
        cur->rpad_set  = 1;
    }

    if (!cur->hang_set && base->hang_set) {
        cur->wrap         = base->wrap;
        cur->disable_wrap = base->disable_wrap;
        cur->hang_set     = 1;
    }

    if (!cur->alignment) {
        cur->alignment = base->alignment;
    }

    if (!cur->borders) {
        cur->borders = base->borders;
    }
}

bool
c4m_style_exists(char *name)
{
    if (name == NULL) {
        return 0;
    }

    c4m_utf8_t *s = c4m_new_utf8(name);

    init_style_db();
    return hatrack_dict_get(style_dictionary, s, NULL) != NULL;
}

void
c4m_install_default_styles()
{
    init_style_db();

    c4m_set_style("table", (c4m_render_style_t *)&default_table);
    c4m_set_style("table2", (c4m_render_style_t *)&col_borders_table);
    c4m_set_style("tr", (c4m_render_style_t *)&default_tr);
    c4m_set_style("tr.even", (c4m_render_style_t *)&default_tr_even);
    c4m_set_style("tr.odd", (c4m_render_style_t *)&default_tr_odd);
    c4m_set_style("td", (c4m_render_style_t *)&default_td);
    c4m_set_style("text", (c4m_render_style_t *)&default_td);
    c4m_set_style("th", (c4m_render_style_t *)&default_th);
    c4m_set_style("tcol", (c4m_render_style_t *)&default_tcol);
    c4m_set_style("snap", (c4m_render_style_t *)&default_snap_col);
    c4m_set_style("full_snap", (c4m_render_style_t *)&default_full_snap_col);
    c4m_set_style("ul", (c4m_render_style_t *)&default_list_grid);
    c4m_set_style("ol", (c4m_render_style_t *)&default_ordered_list_grid);
    c4m_set_style("bullet", (c4m_render_style_t *)&default_bullet_column);
    c4m_set_style("li", (c4m_render_style_t *)&default_list_text_column);
    c4m_set_style("tree_item", (c4m_render_style_t *)&default_tree_item);
    c4m_set_style("h1", (c4m_render_style_t *)&default_h1);
    c4m_set_style("h2", (c4m_render_style_t *)&default_h2);
    c4m_set_style("h3", (c4m_render_style_t *)&default_h3);
    c4m_set_style("h4", (c4m_render_style_t *)&default_h4);
    c4m_set_style("h5", (c4m_render_style_t *)&default_h5);
    c4m_set_style("h6", (c4m_render_style_t *)&default_h6);
    c4m_set_style("table", (c4m_render_style_t *)&default_table);
    c4m_set_style("flow", (c4m_render_style_t *)&default_flow);
    c4m_set_style("error_grid", (c4m_render_style_t *)&default_error_grid);
    c4m_set_style("em", (c4m_render_style_t *)&default_em);
    c4m_set_style("callout_cell", (c4m_render_style_t *)&default_callout_cell);
    c4m_set_style("callout", (c4m_render_style_t *)&default_callout);
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

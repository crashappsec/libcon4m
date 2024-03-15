#include <con4m.h>


static hatrack_dict_t *style_dictionary = NULL;

const border_theme_t border_ascii = {
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
    .next_style      = NULL
};

const border_theme_t border_asterisk = {
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
    .next_style      = (border_theme_t *)&border_ascii
};

const border_theme_t border_bold_dash2 = {
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
    .next_style      = (border_theme_t *)&border_asterisk
};

const border_theme_t border_dash2 = {
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
    .next_style      = (border_theme_t *)&border_bold_dash2
};


const border_theme_t border_bold_dash = {
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
    .next_style      = (border_theme_t *)&border_dash2
};

const border_theme_t border_dash =  {
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
    .next_style      = (border_theme_t *)&border_bold_dash
};

const border_theme_t border_double = {
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
    .next_style      = (border_theme_t *)&border_dash
};

const border_theme_t border_bold =  {
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
    .next_style      = (border_theme_t *)&border_double
};

const border_theme_t border_plain = {
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
    .next_style      = (border_theme_t *)&border_bold
};

const border_theme_t *registered_borders = (border_theme_t *)&border_plain;

// Used for border drawing and background (pad color).
static const render_style_t default_table = {
    .base_style.bf.fg_color_on = 1,
    .base_style.bf.bg_color_on = 1,
    .base_style.bf.fg_color    = 0xf8f8ff, // Ghost white
    .base_style.bf.bg_color    = 0x2f4f4f, // Dark slate grey.
    .borders                   = BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT |
                                 BORDER_RIGHT | INTERIOR_HORIZONTAL |
                                 INTERIOR_VERTICAL,
    .border_theme              = (border_theme_t *)&border_bold_dash,
    .left_pad                  = 1,
    .right_pad                 = 1,
    .dim_kind                  = DIM_AUTO,
    .alignment                 = ALIGN_MID_LEFT
};

static const render_style_t default_tr = {
    .base_style.style          = 0x00062f4f4ff8f8ff,
    .dim_kind                  = DIM_AUTO,
    .alignment                 = ALIGN_TOP_LEFT
};

static const render_style_t default_th = {
    .base_style.bf.fg_color_on = 1,
    .base_style.bf.bg_color_on = 1,
    .base_style.bf.fg_color    = 0xb3ff00, // Atomic lime
    .base_style.bf.bg_color    = 0x000000,
    .left_pad                  = 1,
    .right_pad                 = 1,
    .dim_kind                  = DIM_AUTO,
    .alignment                 = ALIGN_TOP_LEFT
};

static const render_style_t default_td = {
    .base_style.style = 0,
};

static const render_style_t default_tcol = {
    .dim_kind = DIM_AUTO
};

static const render_style_t default_list_grid = {
    .left_pad   = 2,
    .right_pad  = 1,
    .bottom_pad = 1,
    .dim_kind   = DIM_AUTO,
    .alignment  = ALIGN_MID_LEFT
};

static const render_style_t default_bullet_column = {
    .dim_kind   = DIM_FIT_TO_TEXT,
    .left_pad   = 0,
    .dims.units = 2
};

static const render_style_t default_list_text_column = {
    .dim_kind   = DIM_AUTO,
    .left_pad   = 1,
    .right_pad  = 1,
    .alignment  = ALIGN_TOP_LEFT
};

static const render_style_t default_h1 = {
    .base_style.bf.italic      = 1,
    .base_style.bf.fg_color_on = 1,
    .base_style.bf.bg_color_on = 1,
    .base_style.bf.fg_color    = 0x000000, // black
    .base_style.bf.bg_color    = 0xb3ff00, // Atomic lime
    .base_style.bf.underline   = NEW_UL_ON,
    .alignment                 = ALIGN_TOP_CENTER,
    .top_pad                   = 1
};

static const render_style_t default_h2 = {
    .base_style.bf.italic      = 1,
    .base_style.bf.fg_color_on = 1,
    .base_style.bf.bg_color_on = 1,
    .base_style.bf.fg_color    = 0x000000, // black
    .base_style.bf.bg_color    = 0xff2f8e, // Jazzberry
    .base_style.bf.underline   = NEW_UL_ON,
    .alignment                 = ALIGN_TOP_CENTER,
    .top_pad                   = 1
};

// Third word of render styles is a pointer.
const uint64_t rs_pmap[2] = { 0x1, 0x8000000000000000 };

static inline void
init_style_db()
{
    if (style_dictionary == NULL) {
	style_dictionary = gc_alloc(hatrack_dict_t);
	hatrack_dict_init(style_dictionary, HATRACK_DICT_KEY_TYPE_CSTR);
    }
}

void
set_style(char *name, render_style_t *style)
{
    init_style_db();
    hatrack_dict_put(style_dictionary, name, style);
}

// Returns a COPY of the style so that it doesn't get accidentially
// changed by reference.
render_style_t *
lookup_cell_style(char *name)
{
    init_style_db();

    int ok = false;

    render_style_t *entry  = hatrack_dict_get(style_dictionary, name, &ok);

    if (!entry) {
	return NULL;
    }



    render_style_t *result = gc_alloc_mapped(render_style_t, &rs_pmap[0]);
    memcpy(result, entry, sizeof(render_style_t));
    return result;
}

void
con4m_style_init(render_style_t *style, va_list args)
{
    DECLARE_KARGS(
	color_t fg_color        = -1;
	color_t bg_color        = -1;
	int32_t bold            = -1;
	int32_t italic          = -1;
	int32_t strikethru      = -1;
	int32_t underline       = -1;
	int32_t inverse         = -1;
	double  width_pct       = -1;
	int64_t flex_units      = -1;
	int32_t min_size        = -1;
	int32_t max_size        = -1;
	int32_t fit_text        = -1;
	int32_t top_pad         = -1;
	int32_t bottom_pad      = -1;
	int32_t left_pad        = -1;
	int32_t right_pad       = -1;
	int32_t wrap_hang       = -1;
	int32_t disable_wrap    = -1;
	color_t pad_color       = 0xffffffff;
	int32_t alignment       = -1;
	char   *border_theme    = NULL;
	int32_t enabled_borders = -1;
	char *  tag             = NULL;

	);
    method_kargs(args, fg_color, bg_color, bold, italic, strikethru, underline,
		 inverse, width_pct, flex_units, min_size, max_size, fit_text,
		 left_pad, right_pad, top_pad, bottom_pad, wrap_hang,
		 disable_wrap, pad_color, alignment, border_theme,
		 enabled_borders, tag);

    // Use basic math to make sure overlaping cell sizing strategies
    // aren't requested in one call.
    int32_t sz_test = width_pct + flex_units + min_size + max_size + fit_text;

    if (sz_test != -5) {
	if ((width_pct != -1 &&
	     (flex_units + min_size + max_size + fit_text) != -4) ||
	    (flex_units != -1 && (min_size + max_size + fit_text) != -3) ||
	    (fit_text != -1 && (min_size + max_size) != -2)) {
              printf("Future exception: can't specify two cell "
		     "sizing strategies.\n");
  	      abort();
	}
    }

    if (wrap_hang != -1 && disable_wrap != -1) {
	printf("Cannot set 'wrap_hang' and 'disable_wrap' at once.\n");
	abort();
    }

    if (fg_color != -1) {
	set_fg_color(style, fg_color);
    }
    if (bg_color != -1) {
	set_bg_color(style, bg_color);
    }
    if (bold > 0) {
	bold_on(style);
    }
    if (italic > 0) {
	italic_on(style);
    }
    if (strikethru > 0) {
	strikethru_on(style);
    }
    if (underline == NEW_UL_ON) {
	underline_on(style);
    }
    else {
	if (underline == NEW_UL_DOUBLE) {
	    double_underline_on(style);
	}
    }
    if (inverse > 0) {
	inverse_on(style);
    }

    if (border_theme != NULL) {
	set_border_theme(style, border_theme);
    }

    if (width_pct != -1) {
	set_size_as_percent(style, width_pct, true);
    }

    if (flex_units != -1) {
	set_flex_size(style, flex_units);
    }

    if (min_size >= 0 || max_size >= 0) {
	if (min_size < 0) {
	    min_size = 0;
	}
	if (max_size < 0) {
	    max_size = 0x7fffffff;
	}
	set_size_range(style, min_size, max_size);
    }

    if (fit_text > 0) {
	set_fit_to_text(style);
    }

    if (top_pad != -1) {
	set_top_pad(style, top_pad);
    }

    if (bottom_pad != -1) {
	set_bottom_pad(style, bottom_pad);
    }

    if (left_pad != -1) {
	set_left_pad(style, left_pad);
    }

    if (right_pad != -1) {
	set_right_pad(style, right_pad);
    }

    if (wrap_hang != -1) {
	set_wrap_hang(style, wrap_hang);
    }

    if (disable_wrap > 0) {
	disable_line_wrap(style);
    }

    if (pad_color != -1) {
	set_pad_color(style, pad_color);
    }

    if (alignment != -1) {
	set_alignment(style, (alignment_t)alignment);
    }

    if (enabled_borders != -1) {
	set_borders(style, (border_set_t)enabled_borders);
    }

    if (tag != NULL) {
	set_style(tag, style);
    }
}

void
layer_styles(const render_style_t *base, render_style_t *cur)
{
    // Anything not explicitly set in 'cur' will get set from base.
    if (!cur->base_style.bf.fg_color_on && base->base_style.bf.fg_color_on) {
	set_fg_color(cur, base->base_style.bf.fg_color);
    }
    if (!cur->base_style.bf.bg_color_on && base->base_style.bf.bg_color_on) {
	set_bg_color(cur, base->base_style.bf.bg_color);
    }
    if (base->base_style.bf.bold) {
	cur->base_style.bf.bold = 1;
    }
    if (base->base_style.bf.italic) {
	cur->base_style.bf.italic = 1;
    }
    if (base->base_style.bf.strikethru) {
	cur->base_style.bf.strikethru = 1;
    }
    if (!cur->base_style.bf.underline) {
	cur->base_style.bf.underline = base->base_style.bf.underline;
    }
    if (base->base_style.bf.inverse) {
	cur->base_style.bf.inverse = 1;
    }
    if (!cur->base_style.bf.casing) {
	cur->base_style.bf.casing = base->base_style.bf.casing;
    }

    if (cur->border_theme == NULL && base->border_theme != NULL) {
	cur->border_theme = base->border_theme;
    }

    if (!cur->pad_color_set && base->pad_color_set) {
	set_pad_color(cur, base->pad_color);
    }

    if (cur->dim_kind == DIM_UNSET && base->dim_kind != DIM_UNSET) {
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

void
install_default_styles()
{
    init_style_db();

    set_style("table", (render_style_t *)&default_table);
    set_style("tr", (render_style_t *)&default_tr);
    set_style("td", (render_style_t *)&default_td);
    set_style("th", (render_style_t *)&default_th);
    set_style("tcol", (render_style_t *)&default_tcol);
    set_style("ol", (render_style_t *)&default_list_grid);
    set_style("ul", (render_style_t *)&default_list_grid);
    set_style("bullet", (render_style_t *)&default_bullet_column);
    set_style("li", (render_style_t *)&default_list_text_column);
    set_style("h1", (render_style_t *)&default_h1);
    set_style("h2", (render_style_t *)&default_h2);
    set_style("table", (render_style_t *)&default_table);
}

const con4m_vtable render_style_vtable = {
    .num_entries = 1,
    .methods     = {
	(con4m_vtable_entry)con4m_style_init
    }
};

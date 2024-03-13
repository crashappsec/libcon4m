#include <con4m.h>


static hatrack_dict_t *style_dictionary = NULL;

extern const border_theme_t border_bold_dash;

// Used for border drawing and background (pad color).
static const render_style_t default_table = {
    .base_style.bf.fg_color_on = 1,
    .base_style.bf.bg_color_on = 1,
    .base_style.bf.fg_color    = 0xf8f8ff, // Ghost white
    .base_style.bf.bg_color    = 0x2f4f4f, // Dark slate grey.
    .borders                   = BORDER_TOP | BORDER_BOTTOM |
                                 INTERIOR_VERTICAL | BORDER_LEFT | BORDER_RIGHT,
    .border_theme              = (border_theme_t *)&border_bold_dash,
    .left_pad                  = 1,
    .right_pad                 = 1,
    .bottom_pad                = 1,
    .dim_kind                  = DIM_AUTO,
    .alignment                 = ALIGN_MID_LEFT
};

static const render_style_t default_tr = {
    .base_style.bf.fg_color_on = 1,
    .base_style.bf.bg_color_on = 1,
    .base_style.bf.fg_color    = 0xf8f8ff, // Ghost white
    .base_style.bf.bg_color    = 0x2f4f4f, // Dark slate grey.
    .left_pad                  = 1,
    .right_pad                 = 1,
    .dim_kind                  = DIM_AUTO,
    .alignment                 = ALIGN_TOP_LEFT
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
    .left_pad   = 1,
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

    _Bool err = false;

    render_style_t *result = gc_alloc_mapped(render_style_t, &rs_pmap[0]);
    render_style_t *entry  = hatrack_dict_get(style_dictionary, name, &err);

    if (err) {
	return entry;
    }

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
	float   width_pct       = -1;
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

	);
    method_kargs(args, fg_color, bg_color, bold, italic, strikethru, underline,
		 inverse, width_pct, flex_units, min_size, max_size, fit_text,
		 left_pad, right_pad, top_pad, bottom_pad, wrap_hang,
		 disable_wrap, pad_color, alignment, border_theme,
		 enabled_borders);

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

    if (fg_color) {
	set_fg_color(style, fg_color);
    }
    if (bg_color) {
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

    if (pad_color != 0xffffffff) {
	set_pad_color(style, pad_color);
    }

    if (alignment != -1) {
	set_alignment(style, (alignment_t)alignment);
    }

    if (enabled_borders != -1) {
	set_borders(style, (border_set_t)enabled_borders);
    }
}

void
install_default_styles()
{
    init_style_db();

    set_style("table", (render_style_t *)&default_table);
    set_style("tr", (render_style_t *)&default_tr);
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

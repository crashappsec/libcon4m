#include <con4m.h>

static col_props_t global_cell_defaults = {
    .color = FG_COLOR_ON | BG_COLOR_ON | (0x555555UL << 24) | 0xefefef,
    .pad   = {
	.left = 1,
	.right = 1,
	.top = 0,
	.bottom = 0
    },
    .alignment = ALIGN_TOP_LEFT,
    .dimensions = {
	.kind = DIM_AUTO
    },
    .wrap = 0,
    .text_style_override = BOLD_ON | INV_ON | ST_ON | ITALIC_ON |
    UL_ON | UL_DOUBLE | TITLE_CASE
};

static const border_style_t border_ascii = {
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

static const border_style_t border_asterisk = {
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
    .next_style      = (border_style_t *)&border_ascii
};

static const border_style_t border_bold_dash2 = {
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
    .next_style      = (border_style_t *)&border_asterisk
};

static const border_style_t border_dash2 = {
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
    .next_style      = (border_style_t *)&border_bold_dash2
};


static const border_style_t border_bold_dash = {
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
    .next_style      = (border_style_t *)&border_dash2
};

static const border_style_t border_dash =  {
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
    .next_style      = (border_style_t *)&border_bold_dash
};

static const border_style_t border_double = {
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
    .next_style      = (border_style_t *)&border_dash
};

static const border_style_t border_bold =  {
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
    .next_style      = (border_style_t *)&border_double
};

static const border_style_t  border_plain = {
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
    .next_style      = (border_style_t *)&border_bold
};

static border_style_t *registered_borders = (border_style_t *)&border_plain;

static inline void
internal_set_pad(grid_t *grid, int8_t left, int8_t right, int8_t top,
		 int8_t bottom)
{
    if (left >= 0) {
	grid->outer_pad.left  = left;
    }
    if (right >= 0) {
	grid->outer_pad.right  = right;
    }
    if (top >= 0) {
	grid->outer_pad.top  = top;
    }
    if (bottom >= 0) {
	grid->outer_pad.top  = bottom;
    }
}

static void
props_init(row_or_col_props_t *props, va_list args)
{
    // Begin keyword arugments
}

static void
c4grid_init(grid_t *grid, va_list args)
{
    // Begin keyword arguments.
    uint16_t      rows         = 0;
    uint16_t      cols         = 0;
    uint16_t      spare_rows   = 16;
    flexarray_t  *contents     = NULL;
    border_set_t  borders      = 0;
    style_t       border_color = 0;
    str_t        *border_style = NULL;
    row_props_t  *row_defaults = NULL;
    col_props_t  *col_defaults = &global_cell_defaults;
    int8_t        left         = -1;
    int8_t        right        = -1;
    int8_t        top          = -1;
    int8_t        bottom       = -1;

    method_kargs(args, rows, cols, spare_rows, contents, border_style,
		 border_color, row_defaults, col_defaults, borders,
		 left, right, top, bottom);
    // End keyword arguments.

    grid->spare_rows      = spare_rows;

    if (contents != NULL) {
	// NOTE: ignoring num_rows and num_cols; could throw an
	// exception here.
	grid_set_all_contents(grid, contents);
    }

    else {
	grid->num_rows = rows;
	grid->num_cols = cols;
    }
    grid->default_row_properties = row_defaults;
    grid->default_col_properties = col_defaults;

    internal_set_pad(grid, left, right, top, bottom);
    set_enabled_borders(grid, borders);

    if (border_style != NULL) {
	grid_set_border_style(grid, (str_t *)border_style);
    }
}

const con4m_vtable grid_vtable  = {
    .num_entries = 1,
    .methods     = {
	(con4m_vtable_entry)c4grid_init
    }
};

void
_grid_set_outer_pad(grid_t *grid, ...)
{
    // Begin keyword args
    int8_t left   = -1;
    int8_t right  = -1;
    int8_t top    = -1;
    int8_t bottom = -1;

    kargs(grid, left, right, top, bottom);

    // End keyword args.

    internal_set_pad(grid, left, right, top, bottom);
}

bool
grid_set_border_style(grid_t *grid, str_t *name)
{
    border_style_t *cur = registered_borders;

    // If we got passed a u32 make sure it's U8.
    name = force_utf8(name);

    while (cur != NULL) {
	if (!strcmp((char *)name, cur->name)) {
	    grid->border_style = cur;
	    return true;
	}
	cur = cur->next_style;
    }
    return false;
}


// Contents currently must be a list[list[object_t]].  Supply
// properties separately; if you want something that spans you should
// instead
void
grid_set_all_contents(grid_t *g, flexarray_t *contents)
{
    flex_view_t *rows       = flexarray_view(contents);
    uint64_t     nrows      = flexarray_view_len(rows);
    flex_view_t **rowviews  = (flex_view_t **)gc_array_alloc(flex_view_t *,
							     nrows);
    uint64_t     ncols      = 0;
    _Bool        stop       = false;

    for (uint64_t i = 0; i < nrows; i++) {
	flex_view_t *row  = (flex_view_t *)flexarray_view_next(rowviews[i],
							       &stop);
	uint64_t     rlen = flexarray_view_len(row);
	rowviews[i]       = row;

	if (rlen > ncols) {
	    ncols = rlen;
	}
    }

    size_t num_cells = (nrows + g->spare_rows) * ncols;
    g->cells         = gc_array_alloc(renderable_t *, num_cells);

    for (uint64_t i = 0; i < nrows; i++) {

	uint64_t viewlen = flexarray_view_len(rowviews[i]);

	for (uint64_t j = 0; j < viewlen; j++) {
	    renderable_t *cell = gc_alloc(renderable_t);
	    cell->raw_item     = flexarray_view_next(rowviews[i], &stop);
	    g->cells[i][j]     = cell;
	}
    }
}

void
grid_set_row_properties(grid_t *grid, row_props_t *props, uint64_t rowix)
{
    if (rowix < grid->num_rows) {
	for (int i = 0; i < grid->num_cols; i++) {
	    grid->cells[rowix][i]->row_props = props;
	}
    }
}

void
grid_set_col_properties(grid_t *grid, col_props_t *props, uint64_t colix)
{
    if (colix < grid->num_cols) {
	for (int i = 0; i < grid->num_rows; i++) {
	    grid->cells[i][colix]->col_props = props;
	}
    }
}

// TODO:
// 1. A tag-based interface for styles to make it easy to tweak styles,
//    and to be able to use common html style names, etc. Should remove
//    overrides, etc.
// 2. Test / debug flex / pct columns.
// 3. Enable nested tables.
// 4. Change sizing to work on renderable width not codepoints.
// 5. Add the ability to add rows or cells easily.
// 6. Test all the options and look at any render bugs remaining.
// 7. Now we're ready to add a more generic `print()`.
// 8. I'd like to do the debug console soon.

// Not soon, but should eventually get done: row spans (column spans
// are there).

#include <con4m.h>

static col_props_t global_cell_defaults = {
    .style = FG_COLOR_ON | BG_COLOR_ON | (0x555555UL << 24) | 0x00efef,
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

static const border_theme_t border_ascii = {
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

static const border_theme_t border_asterisk = {
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

static const border_theme_t border_bold_dash2 = {
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

static const border_theme_t border_dash2 = {
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


static const border_theme_t border_bold_dash = {
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

static const border_theme_t border_dash =  {
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

static const border_theme_t border_double = {
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

static const border_theme_t border_bold =  {
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

static const border_theme_t border_plain = {
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

static border_theme_t *registered_borders = (border_theme_t *)&border_plain;

static inline void
alloc_grid_props(grid_t *grid)
{
    grid->all_row_props = gc_array_alloc(row_props_t, grid->num_rows);
    grid->all_col_props = gc_array_alloc(col_props_t, grid->num_cols);
}

static inline border_theme_t *
get_border_theme(grid_t *grid)
{
    if (!grid->border_theme) {
	return registered_borders;
    }

    return grid->border_theme;
}

static inline row_props_t *
get_cell_row_props(renderable_t *cell)
{
    if (!cell->row_props) {
	return gc_alloc(row_props_t);
    }
    return cell->row_props;
}

static inline col_props_t *
get_cell_col_props(renderable_t *cell)
{
    if (!cell->col_props) {
	return gc_alloc(col_props_t);
    }
    return cell->col_props;
}

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

static inline str_t *
styled_repeat(codepoint_t c, uint32_t width, style_t style)
{
    str_t *result = c4str_repeat(c, width);

    if (c4str_len(result) != 0) {
	c4str_apply_style(result, style);
    }

    return result;
}

static inline str_t *
get_styled_pad(uint32_t width, style_t style)
{
    return styled_repeat(' ', width, style);
}


static xlist_t *
pad_vertically(grid_t *grid, xlist_t *list, int32_t height, int32_t width,
	       style_t style)
{
    int32_t  len  = xlist_len(list);
    int32_t  diff = height - len;
    str_t   *pad;
    xlist_t *res;

    if (len == 0) {
	pad = get_styled_pad(width, style);
    }
    else {
	pad           = c4str_repeat(' ', width);
	real_str_t *l = to_internal((str_t *)xlist_get(list, len - 1, NULL));
	real_str_t *p = to_internal(pad);

	p->styling = l->styling;
    }
    switch (grid->outer_alignment & VERTICAL_MASK) {
    case ALIGN_BOTTOM:
	res = con4m_new(T_XLIST, "length", height);

	for (int i = 0; i < diff; i++) {
	    xlist_append(res, pad);
	}
	xlist_plus_eq(res, list);
	return res;

    case ALIGN_MIDDLE:
	res = con4m_new(T_XLIST, "length", height);

	for (int i = 0; i < diff / 2; i++) {
	    xlist_append(res, pad);
	}

	xlist_plus_eq(res, list);

	for (int i = 0; i < diff / 2; i++) {
	    xlist_append(res, pad);
	}

	if (diff % 2 != 0) {
	    xlist_append(res, pad);
	}

	return res;
    default:
	for (int i = 0; i < diff; i++) {
	    xlist_append(list, pad);
	}
	return list;
    }
}

static void
props_init(row_or_col_props_t *props, va_list args)
{
    DECLARE_KARGS(
	style_t      style               = STYLE_INVALID;
	int32_t      left                = -1;
	int32_t      right               = -1;
	int32_t      top                 = -1;
	int32_t      bottom              = -1;
	alignment_t  alignment           = ALIGN_IGNORE;
	dimspec_t   *dimensions          = NULL;
	int32_t      wrap                = 0;
	int32_t      text_style_override = 0;
	);

    method_kargs(args, style, left, right, top, bottom, alignment,
		 dimensions, wrap, text_style_override);

    props->style               = style;
    props->pad.left            = left;
    props->pad.right           = right;
    props->pad.top             = top;
    props->pad.bottom          = bottom;
    props->alignment           = alignment;
    props->dimensions          = *dimensions;
    props->wrap                = wrap;
    props->text_style_override = text_style_override;
}

static void
dimspec_init(dimspec_t *dimensions, va_list args)
{
    DECLARE_KARGS(
	dimspec_kind_t kind   = DIM_AUTO;
	float          pct    = -1;
	int64_t        units  = -1;
	int32_t        min_sz = -1;
	int32_t        max_sz = -1;
	);

    method_kargs(args, kind, pct, units, min_sz, max_sz);
    // TODO: error checking with exception handling.

    dimensions->kind    = kind;

    switch (kind) {
    case DIM_PERCENT_ROUND:
    case DIM_PERCENT_TRUNCATE:
	dimensions->dims.percent = pct;
	break;
    case DIM_FLEX_UNITS:
    case DIM_ABSOLUTE:
	dimensions->dims.units = units;
	break;
    case DIM_ABSOLUTE_RANGE:
	dimensions->dims.range[0] = min_sz;
	dimensions->dims.range[1] = max_sz;
	break;
    default:
	break;
    }
}

static inline void
internal_renderable_init(renderable_t *item,      object_t     obj,
			 uint16_t      start_col, uint16_t     start_row,
			 style_t       text_style_override)
{
    item->raw_item    = obj;
    item->start_col   = start_col;
    item->end_col     = start_col + 1;
    item->start_row   = start_row;
    item->end_row     = start_row + 1;
    item->overridable = text_style_override;
}
static void
renderable_init(renderable_t *item, va_list args)
{
    DECLARE_KARGS(
	object_t    *obj                 = NULL;
	int32_t      start_col           = 0;
	int32_t      start_row           = 0;
	style_t      text_style_override = 0; // Inherits from row / col too.
	);

    method_kargs(args, obj, start_col, start_row, text_style_override);
    internal_renderable_init(item, obj, (uint16_t)start_col,
			     (uint16_t)start_row, text_style_override);
}

static void
grid_init(grid_t *grid, va_list args)
{
    DECLARE_KARGS(
	uint32_t      rows         = 1;
	uint32_t      cols         = 1;
	uint32_t      spare_rows   = 16;
	flexarray_t  *contents     = NULL;
	uint32_t      borders      = 0x3f;
	style_t       border_style = FG_COLOR_ON | BG_COLOR_ON |
	                             (0x555555UL << 24) | 0xff007fUL;
	style_t       pad_style    = FG_COLOR_ON | BG_COLOR_ON;
	str_t        *border_theme = NULL;
	row_props_t  *row_defaults = NULL;
	col_props_t  *col_defaults = &global_cell_defaults;
	int32_t       left         = -1;
	int32_t       right        = -1;
	int32_t       top          = -1;
	int32_t       bottom       = -1;

	);

    method_kargs(args, rows, cols, spare_rows, contents, borders,
		 border_style, pad_style, border_theme, row_defaults,
		 col_defaults, left, right, top, bottom);

    grid->spare_rows = (uint16_t)spare_rows;

    if (contents != NULL) {
	// NOTE: ignoring num_rows and num_cols; could throw an
	// exception here.
	grid_set_all_contents(grid, contents);
    }

    else {
	grid->num_rows   = (uint16_t)rows;
	grid->num_cols   = (uint16_t)cols;
	size_t num_cells = (rows + spare_rows) * cols;
	grid->cells      = gc_array_alloc(renderable_t *, num_cells);

    }

    alloc_grid_props(grid);

    grid->default_row_properties = row_defaults;
    grid->default_col_properties = col_defaults;

    internal_set_pad(grid, (int8_t)left, (int8_t)right,
		     (int8_t)top, (int8_t)bottom);
    set_enabled_borders(grid, (border_set_t)borders);

    if (border_theme != NULL) {
	grid_set_border_theme(grid, (str_t *)border_theme);
    }

    grid->width        = GRID_TERMINAL_DIM;
    grid->height       = GRID_UNBOUNDED_DIM;
    grid->border_color = border_style;
    grid->pad_color    = pad_style;
}

void
_grid_set_outer_pad(grid_t *grid, ...)
{
    DECLARE_KARGS(
	int8_t left   = -1;
	int8_t right  = -1;
	int8_t top    = -1;
	int8_t bottom = -1;
	);

    kargs(grid, left, right, top, bottom);

    internal_set_pad(grid, left, right, top, bottom);
}

bool
grid_set_border_theme(grid_t *grid, str_t *name)
{
    border_theme_t *cur = registered_borders;

    // If we got passed a u32 make sure it's U8.
    name = force_utf8(name);

    while (cur != NULL) {
	if (!strcmp((char *)name, cur->name)) {
	    grid->border_theme = cur;
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
    g->num_rows      = nrows;
    g->num_cols      = ncols;

    for (uint64_t i = 0; i < nrows; i++) {

	uint64_t viewlen = flexarray_view_len(rowviews[i]);

	for (uint64_t j = 0; j < viewlen; j++) {
	    renderable_t *cell     = con4m_new(T_RENDERABLE);
	    cell->raw_item         = flexarray_view_next(rowviews[i], &stop);
	    *cell_address(g, i, j) = cell;
	}
    }
}

void
_grid_add_col_span(grid_t *grid, object_t item, int64_t row, ...)
{
    DECLARE_KARGS(
	int64_t  start_col           = 0;
	int64_t  num_cols            = -1;
	style_t  text_style_override = 0;
	str_t   *string              = NULL;
	);
    kargs(row, start_col, num_cols, text_style_override, string);

    if (row >= grid->num_rows || row < 0) {
	return; // Later, throw an exception.
    }

    int64_t end_col;

    if (num_cols == -1) {
	end_col = grid->num_cols - 1;
    }
    else {
	end_col = min(start_col + num_cols - 1, grid->num_cols - 1);
    }

    renderable_t *box = con4m_new(
	T_RENDERABLE,
	"obj",                 item,
	"start_row",           row,
	"start_col",           start_col,
	"text_style_override", text_style_override);

    box->end_col = end_col;

    for (int64_t col = start_col; col < end_col; col++) {
	*cell_address(grid, row, col) = box;
    }

    if (string != NULL) {
	box->raw_item = to_internal(string);
    }
}

void
_grid_add_row_span(grid_t *grid, object_t item, int64_t col, ...)
{
    DECLARE_KARGS(
	int64_t  start_row           = 0;
	int64_t  num_rows            = -1;
	style_t  text_style_override = 0;
	str_t   *string              = NULL;
	);
    kargs(col, start_row, num_rows, text_style_override, string);

    if (col >= grid->num_cols || col < 0) {
	return; // Later, throw an exception.
    }

    int64_t end_row;

    if (num_rows == -1) {
	end_row = grid->num_rows;
    }
    else {
	end_row = min(start_row + num_rows, grid->num_rows);
    }

    renderable_t *box = con4m_new(
	T_RENDERABLE,
	"obj",                 item,
	"start_row",           start_row,
	"start_col",           col,
	"text_style_override", text_style_override);

    box->end_row = end_row;

    for (int64_t row = start_row; row < end_row; row++) {
	*cell_address(grid, row, col) = box;
    }

    if (string != NULL) {
	box->raw_item = to_internal(string);
    }
}

static inline col_props_t *
lookup_grid_col_props(grid_t *grid, int i)
{
    if (!grid->all_col_props) {
	return grid->default_col_properties;
    }

    col_props_t *res = grid->all_col_props[i];

    if (res == NULL) {
	return grid->default_col_properties;
    }
    return res;
}

static inline row_props_t *
lookup_grid_row_props(grid_t *grid, int i)
{
    if (!grid->all_row_props) {
	return grid->default_row_properties;
    }

    row_props_t *res = grid->all_row_props[i];

    if (res == NULL) {
	return grid->default_row_properties;
    }
    return res;
}

static inline int16_t
get_column_render_overhead(grid_t *grid)
{
    int16_t result = grid->outer_pad.left + grid->outer_pad.right;

    if (grid->enabled_borders & BORDER_LEFT) {
	result += 1;
    }

    if (grid->enabled_borders & BORDER_RIGHT) {
	result += 1;
    }

    if (grid->enabled_borders & INTERIOR_VERTICAL) {
	result += grid->num_cols - 1;
    }

    return result;
}

// Here, render width indicates the actual dimensions that rendering
// will produce, knowing that it might be less than or greater than the
// desired width (which we'll handle by padding or truncating).

static int16_t *
calculate_col_widths(grid_t *grid, int16_t width, int16_t *render_width)
{
    size_t       term_width;
    int16_t     *result = gc_array_alloc(uint16_t, grid->num_cols);
    int16_t      sum    = get_column_render_overhead(grid);
    col_props_t *props;

    if (width == GRID_USE_STORED) {
	width = grid->width;
    }

    if (width == GRID_TERMINAL_DIM) {
	terminal_dimensions(&term_width, NULL);
	if (term_width == 0) {
	    term_width = 80;
	}
	width = (int16_t)term_width;
    }

    if (width == GRID_UNBOUNDED_DIM) {

	result = gc_array_alloc(uint16_t, grid->num_cols);

	for (int i = 0; i < grid->num_cols; i++) {
	    props = lookup_grid_col_props(grid, i);

	    switch (props->dimensions.kind) {
	    case DIM_ABSOLUTE:
		result[i] = (uint16_t)props->dimensions.dims.units;
		sum      += result[i];
		break;
	    case DIM_ABSOLUTE_RANGE:
		result[i] = (uint16_t)props->dimensions.dims.range[1];
		sum      += result[i];
		break;
	    default:
		abort(); // TODO: throw an exception.
	    }
	}

	*render_width = sum;
	return result;
    }

    // Width is fixed; start by substracting out what we'll need
    // for OUTER padding and for any borders, which is stored in `sum`.
    //
    // `remaining` counts how many bytes of space remaining we actually
    // have to allocate.
    int16_t remaining = width - sum;

    // Pass 1, for anything that has a fixed width, subtract it out of
    // the total remaining. For absolute ranges, use the min value.
    // For percentages, calculate the percentage based on the absolute
    // width.
    //
    // For auto and flex, we see what's left over at the end, but
    // we do count up how many 'flex' units, where 'auto' always
    // gives a flex unit of 1.

    uint64_t flex_units = 0;
    int16_t  num_flex   = 0;
    bool     has_range  = false;
    uint16_t cur;
    float    pct;

    for (int i = 0; i < grid->num_cols; i++) {

	props = lookup_grid_col_props(grid, i);

	switch (props->dimensions.kind) {
	case DIM_ABSOLUTE:
	    cur        = (uint16_t)props->dimensions.dims.units;
	    result[i]  = cur;
	    sum       += cur;
	    remaining -= cur;
	    continue;
	case DIM_ABSOLUTE_RANGE:
	    has_range  = true;
            cur        = (uint16_t)props->dimensions.dims.range[0];
	    result[i]  = cur;
	    sum       += cur;
	    remaining -= cur;
	    continue;
	case DIM_PERCENT_TRUNCATE:
	    pct        = (props->dimensions.dims.percent / 100);
	    cur        = (uint16_t)(pct * remaining);
	    result[i]  = cur;
	    sum       += cur;
	    remaining -= cur;
	    continue;
	case DIM_PERCENT_ROUND:
	    pct        = (props->dimensions.dims.percent / 100);
	    cur        = (uint16_t)(0.5 + (pct * remaining));
	    result[i]  = cur;
	    sum       += cur;
	    remaining -= cur;
	    continue;
	case DIM_AUTO:
	    flex_units += 1;
	    num_flex   += 1;
	    continue;
	case DIM_FLEX_UNITS:
	    flex_units += props->dimensions.dims.units;

	    // We don't count this if it's set to 0.
	    if (props->dimensions.dims.units != 0) {
		num_flex += 1;
	    }
	    continue;
	}
    }

    // If we have nothing left, we are done.
    if (remaining <= 0) {
	*render_width = sum;
	return result;
    }

    // Second pass only occurs if 'has_range' is true.  If it is, we
    // try to give ranged cells their maximum, up to the remaining width.
    if (has_range) {
	for (int i = 0; i < grid->num_cols; i++) {
	    if (props->dimensions.kind != DIM_ABSOLUTE_RANGE) {
		continue;
	    }
	    int32_t desired = props->dimensions.dims.range[1] -
		props->dimensions.dims.range[0];

	    if (desired <= 0) {
		continue;
	    }
	    cur        = min((uint16_t)desired, (uint16_t)remaining);
	    sum       += cur;
	    remaining -= cur;
	    result[i] += cur;
	    if (remaining == 0) {
		*render_width = sum;
		return result;
	    }
	}
    }

    if (!flex_units || remaining == 0) {
	*render_width = sum;
	return result;
    }

    // Third and final pass is the flex pass. Here, we'll give an even
    // amount to each flex unit, rounding down to the character; if
    // the rounding error gives us leftover space, we give it all to
    // the final column (which is why we're tracking the # remaining).
    float flex_width = remaining / (float)flex_units;

    for (int i = 0; i < grid->num_cols; i++) {
	uint64_t units = 1;

	switch (props->dimensions.kind) {
	case DIM_FLEX_UNITS:
	    units = props->dimensions.dims.units;
	    if (units == 0) {
		continue;
	    }
	    /* fallthrough; */
	case DIM_AUTO:
	    if (--num_flex == 0) {
		result[i] += remaining;
		sum       += remaining;

		*render_width = sum;
		return result;
	    }
	    cur        = (uint16_t)(units * flex_width);
	    result[i]  = cur;
	    sum       += cur;
	    remaining -= cur;
	default:
	    continue;
	}
    }

    *render_width = sum;
    return result;
}

static inline style_t
grid_blend_color(style_t style1, style_t style2)
{
    // We simply do a linear average of the colors.
    return ((style1 & ~FG_COLOR_MASK) + (style2 & ~FG_COLOR_MASK)) >> 1;
}

// This takes what's passed in about default style info, and preps the
// default style for a single cell.
//
// the row and column defaults are DEFAULTS; anything set in the cell
// will take precedence.
//
// Also note that the actual text might be allowed to override style
// here. By default, it does not, and that overriding doesn't happen
// in this function. This function just decides the default for the cell.
//
// The task is easy if there's only one available thing set. But if we
// have no cell, but we do have both row and column styles, we will
// try to intellegently merge. For instance, we will BLEND any colors
// both specify (in the future we will probably add an layer ordering
// and an alpha value here, but for now we just do 50%).

static inline style_t
get_cell_style(renderable_t *cell)
{
    if (cell->applied_style != 0) {
	return cell->applied_style;

    }
    style_t row_style = get_cell_row_props(cell)->style;
    style_t col_style = get_cell_col_props(cell)->style;

    // If nobody has colors on, OR-ing the two things together
    // works pretty well (the only real issue is the possibility
    // of having both single and double undeline bits set).
    style_t result = row_style | col_style;
    // Here we pull out the bits they both have set, so we can check
    // if we need to blend colors.
    style_t dupes  = row_style & col_style;
    if (dupes & FG_COLOR_ON) {
	// Clear the color; we're going to have to blend it.
	result &= FG_COLOR_MASK;
	result |= grid_blend_color(row_style, col_style);
    }
    if (dupes & BG_COLOR_ON) {
	result &= BG_COLOR_MASK;
	result |= (grid_blend_color(row_style >> 24, col_style >> 24) << 24);
    }

    cell->applied_style = result;

    return result;
}

static inline style_t
get_overridable_style(renderable_t *cell)
{
    row_props_t *rprops = get_cell_row_props(cell);
    col_props_t *cprops = get_cell_col_props(cell);

    style_t r_override = 0;
    style_t c_override = 0;

    if (rprops == NULL && cprops == NULL) {
	return global_cell_defaults.text_style_override | cell->overridable;
    }

    if (rprops != NULL) r_override = rprops->text_style_override;
    if (cprops != NULL) c_override = cprops->text_style_override;

    return ~(r_override | c_override | cell->overridable);
}

// Here, we generally will take from the cell's style, but there can
// be overrides, and if there are, we use them.
static inline style_t
grid_new_text_style(renderable_t *cell, style_t text_style)
{
    style_t cell_style  = get_cell_style(cell);
    style_t overrides   = get_overridable_style(cell);
    style_t to_override = overrides & text_style & ~FLAG_MASK;

    if (to_override & FG_COLOR_ON) {
	cell_style &= FG_COLOR_MASK;
	cell_style |= text_style & ~FG_COLOR_MASK;
    }

    if (to_override & BG_COLOR_ON) {
	cell_style &= BG_COLOR_MASK;
	cell_style |= text_style & ~BG_COLOR_MASK;
    }

    return cell_style | to_override;
}

static inline
int8_t get_wrap(renderable_t *cell)
{
    int8_t res = cell->wrap_override;

    if (res) {
	return res;
    }

    row_props_t *rprops = get_cell_row_props(cell);
    col_props_t *cprops = get_cell_col_props(cell);

    if (!rprops && !cprops) return 0;
    if (!rprops) return cprops->wrap;
    if (!cprops) return rprops->wrap;
    return max(cprops->wrap, rprops->wrap);
}

#define def_pad_func(pad_location)                                             \
    static inline uint8_t                                                      \
    get_ ## pad_location ## _pad(renderable_t *cell) { 	                       \
	row_props_t *rprops = get_cell_row_props(cell);                        \
        col_props_t *cprops = get_cell_col_props(cell);			       \
        if (cell->pad_overrides.pad_location > 0) {                            \
            return cell->pad_overrides.pad_location;                           \
        }                                                                      \
	if (!rprops && !cprops) return 0;                                      \
	if (!rprops) return cprops->pad.pad_location;                          \
	if (!cprops) return rprops->pad.pad_location;                          \
	return max(cprops->pad.pad_location, rprops->pad.pad_location);        \
    }

def_pad_func(top);
def_pad_func(bottom);
def_pad_func(left);
def_pad_func(right);

// Flag indicates whether to prioritize the horizontal alignment,
// which we prefer to take from columns.
static inline alignment_t
get_alignment(renderable_t *cell, bool horizontal)
{
    if (cell->alignment_overrides) {
	return cell->alignment_overrides;
    }

    if (horizontal) {
	if (cell->col_props) {
	    return cell->col_props->alignment;
	}
	if (cell->row_props) {
	    return cell->row_props->alignment;
	}
    }
    else {
	if (cell->row_props) {
	    return cell->row_props->alignment;
	}
	if (cell->col_props) {
	    return cell->col_props->alignment;
	}
    }
    return ALIGN_MID_LEFT;
}

static inline str_t *
pad_and_style_line(renderable_t *cell, int16_t width, str_t *line)
{

    alignment_t align = get_alignment(cell, true) & HORIZONTAL_MASK;
    int64_t     len   = c4str_len(line);
    uint8_t     lnum  = get_left_pad(cell);
    uint8_t     rnum  = get_right_pad(cell);
    int64_t     diff  = width - len - lnum - rnum;
    str_t      *lpad;
    str_t      *rpad;

    if (diff > 0) {
	switch (align) {
	case ALIGN_RIGHT:
	    lnum += diff;
	    break;
	case ALIGN_CENTER:
	    lnum += (diff / 2);
	    rnum += (diff / 2);

	    if (diff % 2 == 1) {
		rnum += 1;
	    }
	    break;
	default:
	    rnum += diff;
	    break;
	}
    }

    style_t     cell_style = get_cell_style(cell);
    style_t     lpad_style = cell_style;
    style_t     rpad_style = cell_style;
    str_t      *copy       = c4str_copy(line);
    real_str_t *real       = to_internal(copy);

    style_gaps(real, cell_style);

    int last_style = real->styling->num_entries - 1;

    lpad_style = real->styling->styles[0].info;
    rpad_style = real->styling->styles[last_style].info;


    lpad = get_styled_pad(lnum, lpad_style);
    rpad = get_styled_pad(rnum, rpad_style);

    str_t *result = c4str_concat(c4str_concat(lpad, copy), rpad);


    return result;
}

static inline uint16_t
str_render_cell(str_t *s, renderable_t *cell, int16_t width, int16_t height)
{
    int8_t  wrap              = get_wrap(cell);
    break_info_t *line_starts = wrap_text(s, width - 1, wrap);

    xlist_t *res = con4m_new(T_XLIST);
    str_t   *line;
    str_t   *pad_line = pad_and_style_line(cell, width, empty_string());
    int      i;

    for (i = 0; i < get_top_pad(cell); i++) {
	xlist_append(res, pad_line);
    }

    for (i = 0; i < line_starts->num_breaks - 1; i++) {
	line = c4str_slice(s, line_starts->breaks[i],
				  line_starts->breaks[i + 1]);
	line = c4str_strip(line);
	xlist_append(res, pad_and_style_line(cell, width, line));
    }

    if (i == (line_starts->num_breaks - 1)) {
	int b = line_starts->breaks[i];
	line  = c4str_slice(s, b, c4str_len(s));
	line = c4str_strip(line);
	xlist_append(res, pad_and_style_line(cell, width, line));
    }

    for (i = 0; i < get_bottom_pad(cell); i++) {
	xlist_append(res, pad_line);
    }

    cell->render_cache = res;

    return xlist_len(res);
}

// Renders to the exact width, and via the height. For now, we're just going
// to handle text objects, and then sub-grids.
static inline uint16_t
render_to_cache(renderable_t *cell, int16_t width, int16_t height)
{
    switch (get_base_type(cell->raw_item)) {
    case T_STR:
    case T_UTF32:
    {
	real_str_t *r = (real_str_t *)cell->raw_item;
	if (cell->end_col - cell->start_col != 1) {
	    str_render_cell(force_utf32((str_t *)r->data),
			       cell, width, height);
	}
	return str_render_cell(force_utf32((str_t *)r->data),
			       cell, width, height);
    }
    case T_GRID:
	// Not done yet.
    default:
	abort();
    }

    return 0;
}

static inline void
grid_add_blank_cell(grid_t *grid, uint16_t row, uint16_t col, int16_t width,
		    int16_t height)
{
    renderable_t *cell = con4m_new(T_RENDERABLE,
				   "start_row", row,
				   "start_col", col);

    cell->raw_item  = to_internal((str_t *)empty_string());

    // TODO-- all_row_props aren't being set right, or
    // else this would be grey.
    cell->row_props = (row_props_t *)&grid->all_row_props[row];
    cell->col_props = (col_props_t *)&grid->all_col_props[col];


    if (!cell->row_props) {
	cell->row_props = grid->default_row_properties;
    }
    if (!cell->col_props) {
	cell->col_props = grid->default_col_properties;
    }

    *cell_address(grid, row, col) = cell;

    render_to_cache(cell, width, -1);
}

static inline int16_t *
grid_pre_render(grid_t *grid, int16_t *col_widths)
{
    row_props_t *row_props;
    col_props_t *col_props;

    int16_t *row_heights = gc_array_alloc(int16_t *, grid->num_rows);

    // Run through and tell the individual items to render.
    // For now we tell them all to render to whatever height.
    for (int16_t i = 0; i < grid->num_rows; i++) {
	int16_t row_height = 1;
	int16_t width;
	int16_t cell_height;

	for (int16_t j = 0; j < grid->num_cols; j++) {
	    renderable_t *cell = *cell_address(grid, i, j);

	    if (cell == NULL) {
		continue;
	    }

	    if (cell->start_row != i || cell->start_col != j) {
		continue;
	    }

	    width = 0;

	    for (int16_t k = j; k < cell->end_col; k++) {
		width += col_widths[k];
	    }

	    // Make sure to account for borders in spans.
	    if (grid->enabled_borders & INTERIOR_VERTICAL) {
		width += cell->end_col - j - 1;
	    }

	    cell->render_width = width;

	    row_props = lookup_grid_row_props(grid, i);
	    col_props = lookup_grid_col_props(grid, j);

	    if (row_props == NULL) {
		row_props = grid->default_row_properties;
	    }

	    if (col_props == NULL) {
		col_props = grid->default_col_properties;
	    }

	    cell->row_props = row_props;
	    cell->col_props = col_props;
	    cell_height     = render_to_cache(cell, width, -1);


	    if (cell_height > row_height) {
		row_height = cell_height;
	    }
	}

	row_heights[i] = row_height;

	for (int16_t j = 0; j < grid->num_cols; j++) {
	    renderable_t *cell = *cell_address(grid, i, j);

	    if (cell == NULL) {
		grid_add_blank_cell(grid, i, j, col_widths[j], cell_height);
		continue;
	    }

	    if (cell->start_row != i || cell->start_col != j) {
		continue;
	    }
	    // TODO: handle vertical spans properly; this does
	    // not.  Right now we're assuming all heights are
	    // dynamic to the longest content.
	    cell->render_cache = pad_vertically(grid, cell->render_cache,
						row_height, cell->render_width,
						get_cell_style(cell));
	}

    }
    return row_heights;
}

static inline style_t
get_border_color(grid_t *grid)
{
    return grid->border_color;
}

static inline style_t
get_pad_color(grid_t *grid)
{

    if (grid->pad_color) {
	return grid->pad_color;
    }

    return get_border_color(grid);
}

static inline void
grid_add_top_pad(grid_t *grid, xlist_t *lines, int16_t width) {
    int top = grid->outer_pad.top;

    if (!top) {
	return;
    }

    str_t *pad = get_styled_pad(width, get_pad_color(grid));

    for (int i = 0; i < top; i++) {
	xlist_append(lines, pad);
    }
}

static inline void
grid_add_bottom_pad(grid_t *grid, xlist_t *lines, int16_t width)
{
    int bottom = grid->outer_pad.bottom;

    if (!bottom) {
	return;
    }

    str_t *pad = get_styled_pad(width, get_pad_color(grid));

    for (int i = 0; i < bottom; i++) {
	xlist_append(lines, pad);
    }
}

static inline void
grid_add_top_border(grid_t *grid, xlist_t *lines, int16_t *col_widths)
{
    int32_t         border_width = 0;
    int             vertical_borders;
    border_theme_t *draw_chars;
    str_t          *s, *lpad, *rpad;
    int32_t        *p;
    style_t         pad_color;


    if (!(grid->enabled_borders & BORDER_TOP)) {
	return;
    }

    draw_chars = get_border_theme(grid);

    if (draw_chars == NULL) {
	draw_chars = registered_borders;
    }

    for (int i = 0; i < grid->num_cols; i++) {
	border_width += col_widths[i];
    }

    if (grid->enabled_borders & BORDER_LEFT) {
	border_width++;
    }

    if (grid->enabled_borders & BORDER_RIGHT) {
	border_width++;
    }

    vertical_borders = grid->enabled_borders & INTERIOR_VERTICAL;

    if (vertical_borders) {
	border_width += grid->num_cols - 1;
    }

    s = (str_t *)con4m_new(T_UTF32, "length", border_width);
    p = (int32_t *)s;
    (to_internal(s))->codepoints = ~border_width;

    if (grid->enabled_borders & BORDER_LEFT) {
	*p++ = draw_chars->upper_left;
    }

    for (int i = 0; i < grid->num_cols; i++) {
	for (int j = 0; j < col_widths[i]; j++) {
	    *p++ = draw_chars->horizontal_rule;
	}

	if (vertical_borders && (i + 1 != grid->num_cols)) {
	    *p++ = draw_chars->top_t;
	}
    }

    if (grid->enabled_borders & BORDER_RIGHT) {
	*p++ = draw_chars->upper_right;
    }

    c4str_apply_style(s, get_border_color(grid));

    pad_color = get_pad_color(grid);
    lpad      = get_styled_pad(grid->outer_pad.left, pad_color);
    rpad      = get_styled_pad(grid->outer_pad.right, pad_color);

    xlist_append(lines, c4str_concat(c4str_concat(lpad, s), rpad));
}

static inline void
grid_add_bottom_border(grid_t *grid, xlist_t *lines, int16_t *col_widths)
{
    int32_t         border_width = 0;
    int             vertical_borders;
    border_theme_t *draw_chars;
    str_t          *s, *lpad, *rpad;
    int32_t        *p;
    style_t         pad_color;

    if (!(grid->enabled_borders & BORDER_BOTTOM)) {
	return;
    }

    draw_chars = get_border_theme(grid);

    if (draw_chars == NULL) {
	draw_chars = registered_borders;
    }

    for (int i = 0; i < grid->num_cols; i++) {
	border_width += col_widths[i];
    }

    if (grid->enabled_borders & BORDER_LEFT) {
	border_width++;
    }

    if (grid->enabled_borders & BORDER_RIGHT) {
	border_width++;
    }

    vertical_borders = grid->enabled_borders & INTERIOR_VERTICAL;

    if (vertical_borders) {
	border_width += grid->num_cols - 1;
    }

    s = (str_t *)con4m_new(T_UTF32, "length", border_width);
    p = (int32_t *)s;
    (to_internal(s))->codepoints = ~border_width;

    if (grid->enabled_borders & BORDER_LEFT) {
	*p++ = draw_chars->lower_left;
    }

    for (int i = 0; i < grid->num_cols; i++) {
	for (int j = 0; j < col_widths[i]; j++) {
	    *p++ = draw_chars->horizontal_rule;
	}

	if (vertical_borders && (i + 1 != grid->num_cols)) {
	    *p++ = draw_chars->bottom_t;
	}
    }

    if (grid->enabled_borders & BORDER_RIGHT) {
	*p++ = draw_chars->lower_right;
    }

    c4str_apply_style(s, get_border_color(grid));

    pad_color = get_pad_color(grid);
    lpad      = get_styled_pad(grid->outer_pad.left, pad_color);
    rpad      = get_styled_pad(grid->outer_pad.right, pad_color);

    xlist_append(lines, c4str_concat(c4str_concat(lpad, s), rpad));
}

static inline void
grid_add_horizontal_rule(grid_t *grid, xlist_t *lines, int16_t *col_widths)
{
    int32_t         border_width = 0;
    int             vertical_borders;
    border_theme_t *draw_chars;
    str_t          *s, *lpad, *rpad;
    int32_t        *p;
    style_t         pad_color;

    if (!(grid->enabled_borders & INTERIOR_HORIZONTAL)) {
	return;
    }

    draw_chars = get_border_theme(grid);

    if (draw_chars == NULL) {
	draw_chars = registered_borders;
    }

    for (int i = 0; i < grid->num_cols; i++) {
	border_width += col_widths[i];
    }

    if (grid->enabled_borders & BORDER_LEFT) {
	border_width++;
    }

    if (grid->enabled_borders & BORDER_RIGHT) {
	border_width++;
    }

    vertical_borders = grid->enabled_borders & INTERIOR_VERTICAL;

    if (vertical_borders) {
	border_width += grid->num_cols - 1;
    }

    s = (str_t *)con4m_new(T_UTF32, "length", border_width);
    p = (int32_t *)s;
    (to_internal(s))->codepoints = ~border_width;

    if (grid->enabled_borders & BORDER_LEFT) {
	*p++ = draw_chars->left_t;
    }

    for (int i = 0; i < grid->num_cols; i++) {
	for (int j = 0; j < col_widths[i]; j++) {
	    *p++ = draw_chars->horizontal_rule;
	}

	if (vertical_borders && (i + 1 != grid->num_cols)) {
	    *p++ = draw_chars->cross;
	}
    }

    if (grid->enabled_borders & BORDER_RIGHT) {
	*p++ = draw_chars->right_t;
    }

    c4str_apply_style(s, get_border_color(grid));

    pad_color = get_pad_color(grid);
    lpad      = get_styled_pad(grid->outer_pad.left, pad_color);
    rpad      = get_styled_pad(grid->outer_pad.right, pad_color);

    xlist_append(lines, c4str_concat(c4str_concat(lpad, s), rpad));
}


static inline xlist_t *
grid_add_left_pad(grid_t *grid, int height)
{
    xlist_t *res  = con4m_new(T_XLIST, "length", height);
    str_t   *lpad = empty_string();

    if (grid->outer_pad.left > 0) {
	lpad = get_styled_pad(grid->outer_pad.left, get_pad_color(grid));
    }

    for (int i = 0; i < height; i++) {
	xlist_append(res, lpad);
    }

    return res;
}

static inline void
grid_add_right_pad(grid_t *grid, xlist_t *lines)
{
    if (grid->outer_pad.right <= 0) {
	return;
    }

    str_t *rpad = get_styled_pad(grid->outer_pad.right, get_pad_color(grid));

    for (int i = 0; i < xlist_len(lines); i++) {
	str_t *s = (str_t *)xlist_get(lines, i, NULL);
	xlist_set(lines, i, c4str_concat(s, rpad));
    }
}

static inline void
add_vertical_bar(grid_t *grid, xlist_t *lines, border_set_t to_match)
{
    if (!(grid->enabled_borders & to_match)) {
	return;
    }

    border_theme_t *border_theme = get_border_theme(grid);
    style_t         border_color = get_border_color(grid);
    str_t          *bar;

    bar = styled_repeat(border_theme->vertical_rule, 1, border_color);

    for (int i = 0; i < xlist_len(lines); i++) {
	str_t *s = (str_t *)xlist_get(lines, i, NULL);
	xlist_set(lines, i, c4str_concat(s, bar));
    }
}

static inline void
grid_add_left_border(grid_t *grid, xlist_t *lines)
{
    add_vertical_bar(grid, lines, BORDER_LEFT);
}

static inline void
grid_add_right_border(grid_t *grid, xlist_t *lines)
{
    add_vertical_bar(grid, lines, BORDER_RIGHT);
}

static inline void
grid_add_vertical_rule(grid_t *grid, xlist_t *lines)
{
    add_vertical_bar(grid, lines, BORDER_RIGHT);
}

static void
crop_vertically(grid_t *grid, xlist_t *lines, int32_t height)
{
    int32_t diff = height - xlist_len(lines);

    switch (grid->outer_alignment & VERTICAL_MASK) {
    case ALIGN_BOTTOM:
	for (int i = 0; i < height; i++) {
	    xlist_set(lines, i, xlist_get(lines, i + diff, NULL));
	}
	break;
    case ALIGN_MIDDLE:
	for (int i = 0; i < height; i++) {
	    xlist_set(lines, i, xlist_get(lines, i + (diff >> 1), NULL));
	}
	break;
    default:
	break;
    }

    lines->length = height;
}

static inline str_t *
align_and_crop_grid_line(str_t *line, int32_t width, alignment_t align,
			 style_t pad_style)
{
    // Called on one grid line if we need to align or crop it.
    int32_t diff = width - c4str_render_len(line);
    str_t  *pad;
    if (diff > 0) {
	// We need to pad. Here, we use the alignment info.
	switch (align & HORIZONTAL_MASK) {

	case ALIGN_RIGHT:
	    pad = get_styled_pad(diff, pad_style);
	    return c4str_concat(pad, line);
	case ALIGN_CENTER:
	{
	    pad = get_styled_pad(diff / 2, pad_style);
	    line = c4str_concat(pad, line);
	    if (diff % 2 != 0) {
		pad = get_styled_pad(1 + diff / 2, pad_style);
	    }
	    return c4str_concat(line, pad);
	}
	default:
	    pad = get_styled_pad(diff, pad_style);
	    return c4str_concat(line, pad);
	}
    }
    else {
	// We need to crop. For now, we ONLY crop from the right.
	return c4str_truncate(line, (int64_t)width, "use_render_width", 1);
    }
}

static xlist_t *
align_and_crop_grid(grid_t *grid, xlist_t *lines, int32_t width, int32_t height)
{
    int         num_lines = xlist_len(lines);
    alignment_t alignment = grid->outer_alignment;
    style_t     style     = get_pad_color(grid);

    // For now, width must always be set. Won't be true for height.

    for (int i = 0; i < num_lines; i++) {
	str_t *s = (str_t *)xlist_get(lines, i, NULL);
	if (c4str_len(s) == width) {
	    continue;
	}

	str_t *l = align_and_crop_grid_line(s, width, alignment, style);
	xlist_set(lines, i, l);
    }

    if (height != -1) {
	if (num_lines > height) {
	    crop_vertically(grid, lines, height);
	}
	else {
	    if (num_lines < height) {
		lines = pad_vertically(grid, lines, height, width, style);
	    }
	}
    }

    return lines;
}

static inline bool
grid_add_cell_contents(grid_t *grid, xlist_t *lines, uint16_t r, uint16_t c,
		       int16_t *col_widths, int16_t *row_heights)
{
    // This is the one that fills a single cell.  Returns true if the
    // caller should render vertical interior borders (if wanted). The
    // caller will be on its own in figuring out borders for spans
    // though.

    renderable_t *cell = *cell_address(grid, r, c);
    int           i;

    if (cell->end_col - cell->start_col == 1 &&
	cell->end_row - cell->start_row == 1) {
	for (i = 0; i < xlist_len(lines); i++) {
	    str_t *s     = (str_t *)xlist_get(lines, i, NULL);
	    str_t *piece = (str_t *)xlist_get(cell->render_cache, i, NULL);
	    if (!c4str_len(piece)) {
		piece = get_styled_pad(col_widths[i],  0);
	    }
	    xlist_set(lines, i, c4str_concat(s, piece));
	}
	return true;
    }

    // For spans, just return the one block of the grid, along with
    // any interior borders.
    uint16_t row_offset   = r - cell->start_row;
    uint16_t col_offset   = c - cell->start_col;
    int      start_width  = 0;
    int      start_height = 0;

    if (grid->enabled_borders & INTERIOR_VERTICAL) {
	start_width += col_offset;
    }

    if (grid->enabled_borders & INTERIOR_HORIZONTAL) {
	start_height += row_offset;
    }

    for (i = cell->start_col; i < c; i++) {
	start_width += col_widths[i];
    }

    for (i = cell->start_row; i < r; i++) {
	start_height += row_heights[i];
    }

    int num_rows = row_heights[r];
    int num_cols = col_widths[c];

    if ((grid->enabled_borders & INTERIOR_HORIZONTAL) &&
	r + 1 != cell->end_row) {
	num_rows += 1;
    }

    if ((grid->enabled_borders & INTERIOR_VERTICAL) &&
	r + 1 != cell->end_col) {
	num_cols += 1;

    }
    for (i = row_offset; i < row_offset + num_rows; i++) {
	str_t *s     = (str_t *)xlist_get(lines, i, NULL);
	str_t *piece = (str_t *)xlist_get(cell->render_cache, i, NULL);

	piece = c4str_slice(piece, start_width, start_width + num_cols);
	str_t *line = c4str_concat(s, piece);
	xlist_set(lines, i, line);
    }

    return c + 1 == cell->end_col;
}

xlist_t *
_grid_render(grid_t *grid, ...)
{
    // There's a lot of work in here, so I'm keeping the high-level
    // algorithm in this function as simple as possible.  Note that we
    // currently build up one big output string, but I'd also like to
    // have a slight variant that writes to a FILE *.
    //
    // A single streaming implementation doesn't really work, since
    // when writing to a FILE *, we would render the ansi codes as we
    // go.
    DECLARE_KARGS(
	int64_t width  = -1;
	int64_t height = -1;
	);

    kargs(grid, width, height);

    if ((width == -1 && height == -1) || width < 1) {
	width = max(terminal_width(), 20);
    }

    int16_t    *col_widths  = calculate_col_widths(grid, width, &grid->width);
    int16_t    *row_heights = grid_pre_render(grid, col_widths);

    // Right now, we're not going to do the final padding and row
    // heights; we'll just do the padding at the end, and pad all rows
    // out to whatever they render.
    //
    // grid_compute_full_padding(grid, width, height);
    // grid_finalize_row_heights(grid, row_heights, height);

    // Now it's time to output. Each cell will have pre-rendered, even
    // if there are big spans.  So we go through the grid and ask each
    // cell to give us back data, one cell at a time.
    //
    // Span cells know how to return just the contents for the one
    // pard of the grid we're interested in.
    //
    // We also are responsible for outside padding borders here, but
    // we don't draw borders if they would be in the interior of span
    // cells. The function abstractions will do the checking to see if
    // they should do anything.

    uint16_t h_alloc = grid->num_rows + 1 + grid->outer_pad.top +
	grid->outer_pad.bottom;

    for (int i = 0; i < grid->num_rows; i++) {
	h_alloc += row_heights[i];
    }

    xlist_t *result = con4m_new(T_XLIST, "length", h_alloc);

    grid_add_top_pad(grid, result, width);
    grid_add_top_border(grid, result, col_widths);

    for (int i = 0; i < grid->num_rows; i++) {
	xlist_t *row = grid_add_left_pad(grid, row_heights[i]);
	grid_add_left_border(grid, row);

	for (int j = 0; j < grid->num_cols; j++) {
	    bool vertical_ok = grid_add_cell_contents(grid, row, i, j,
						      col_widths, row_heights);

	    if (vertical_ok && (j + 1 < grid->num_cols)) {
		grid_add_vertical_rule(grid, row);
	    }
	}

	grid_add_right_border(grid, row);
	grid_add_right_pad(grid, row);

	xlist_plus_eq(result, row);

	if (i + 1 < grid->num_rows) {
	    grid_add_horizontal_rule(grid, result, col_widths);
	}
    }

    grid_add_bottom_border(grid, result, col_widths);
    grid_add_bottom_pad(grid, result, width);

    return align_and_crop_grid(grid, result, width, height);
}

str_t *
grid_to_str(grid_t *g, to_str_use_t how)
{
    xlist_t *l = grid_render(g);

    return c4str_join(l, c4str_newline(), "add_trailing", true);
}

grid_t *
ordered_list(flexarray_t *items)
{
    flex_view_t *view         = flexarray_view(items);
    uint64_t     n            = flexarray_view_len(view);
    col_props_t *bullet_props = gc_alloc(col_props_t);
    col_props_t *text_props   = gc_alloc(col_props_t);
    str_t       *dot          = c4str_repeat('.', 1);
    grid_t      *res          = con4m_new(T_GRID, "rows", n, "cols", 2,
					  "borders", 0);

    bullet_props->pad.left              = 1;
    bullet_props->alignment             = ALIGN_TOP_RIGHT;
    bullet_props->dimensions.kind       = DIM_ABSOLUTE;
    bullet_props->dimensions.dims.units = (64 - __builtin_clzll(n));
    text_props->pad.left                = 1;
    text_props->pad.right               = 1;
    text_props->dimensions.kind         = DIM_ABSOLUTE;
    text_props->dimensions.dims.units   = terminal_width() - (64 - __builtin_clzll(n));
    text_props->alignment               = ALIGN_TOP_LEFT;
//    text_props->dimensions.kind         = DIM_AUTO;

    for (int i = 0; i < n; i++) {
	bool        status;
	str_t      *s         = c4str_concat(c4str_from_int(i + 1), dot);
	real_str_t *list_item = flexarray_view_next(view, &status);

	grid_set_cell_contents(res, i, 0, to_internal(s));
	grid_set_cell_contents(res, i, 1, list_item);
    }

    res->all_col_props[0] = bullet_props;
    res->all_col_props[1] = text_props;

    return res;
}

grid_t *
_unordered_list(flexarray_t *items, ...)
{
    DECLARE_KARGS(
	codepoint_t bullet = 0x2022;
	);

    kargs(items, bullet);

    flex_view_t *view         = flexarray_view(items);
    uint64_t     n            = flexarray_view_len(view);
    col_props_t *bullet_props = gc_alloc(col_props_t);
    col_props_t *text_props   = gc_alloc(col_props_t);
    grid_t      *res          = con4m_new(T_GRID, "rows", n, "cols", 2,
					  "borders", 0);
    real_str_t  *bull_str     = to_internal(c4str_repeat(bullet, 1));

    bullet_props->pad.left              = 1;
    bullet_props->dimensions.kind       = DIM_ABSOLUTE;
    bullet_props->dimensions.dims.units = 2;
    text_props->pad.left                = 1;
    text_props->pad.right               = 1;
    text_props->dimensions.kind         = DIM_ABSOLUTE;
    text_props->dimensions.dims.units   = terminal_width() - 2;
    text_props->alignment               = ALIGN_TOP_LEFT;
//    text_props->dimensions.kind         = DIM_AUTO;

    for (int i = 0; i < n; i++) {
	bool        status;
	real_str_t *list_item = flexarray_view_next(view, &status);

	grid_set_cell_contents(res, i, 0, bull_str);
	grid_set_cell_contents(res, i, 1, list_item);
    }

    res->all_col_props[0] = bullet_props;
    res->all_col_props[1] = text_props;

    return res;
}

const con4m_vtable grid_vtable  = {
    .num_entries = 2,
    .methods     = {
	(con4m_vtable_entry)grid_init,
	(con4m_vtable_entry)grid_to_str
    }
};

const con4m_vtable dimensions_vtable = {
    .num_entries = 1,
    .methods     = {
	(con4m_vtable_entry)dimspec_init
    }
};

const con4m_vtable gridprops_vtable = {
    .num_entries = 1,
    .methods     = {
	(con4m_vtable_entry)props_init
    }
};

const con4m_vtable renderable_vtable = {
    .num_entries = 1,
    .methods     = {
	(con4m_vtable_entry)renderable_init
    }
};

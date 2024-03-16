int debug = 0;

// TODO for partiy:
// 1. Search.
// 2. Alternating row color striping option for tables.

// Then:
// 1. Add the ability to add rows or cells easily (and max col width?)
// 2. Now we're ready to add a more generic `print()`.
// 3. Deal w/ newlines for fit-to-text (string split).
// 4. I'd like to do the debug console soon-ish.

// Not soon, but should eventually get done:
// 1. Row spans (column spans are there; row spans only stubbed).
// 2. Style doesn't pick up properly w/ col spans ending exactly on middle.
// 3. Also not soon, but should consider allowing style info to "resolve"
//    as a better way to fix issues w/ split.

#include <con4m.h>

#define SPAN_NONE  0
#define SPAN_HERE  1
#define SPAN_BELOW 2


static inline
render_style_t *
grid_style(grid_t *grid) {
    return grid->self->current_style;
}

extern void
apply_container_style(renderable_t *item, char *tag)

{
    render_style_t *tag_style = lookup_cell_style(tag);
    if (!tag_style) {
	return;
    }

    if (item->current_style == NULL) {
	item->current_style = tag_style;
    }
    else {
	layer_styles(tag_style, item->current_style);
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
pad_lines_vertically(render_style_t *gs, xlist_t *list, int32_t height,
		     int32_t width)
{
    int32_t         len  = xlist_len(list);
    int32_t         diff = height - len;
    str_t          *pad;
    xlist_t        *res;

    if (len == 0) {
	pad = get_styled_pad(width, get_pad_style(gs));
    }
    else {
	pad           = c4str_repeat(' ', width);
	real_str_t *l = to_internal((str_t *)xlist_get(list, len - 1, NULL));
	real_str_t *p = to_internal(pad);

	p->styling = l->styling;
    }
    switch (gs->alignment) {
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
renderable_init(renderable_t *item, va_list args)
{
    DECLARE_KARGS(
	object_t    *obj                 = NULL;
	char        *tag                 = NULL;
	);

    method_kargs(args, obj, tag);

    item->raw_item = obj;

    if (tag != NULL) {
	apply_container_style(item, tag);
    }
}

void
install_renderable(grid_t *grid, renderable_t *cell,
		   int start_row, int end_row, int start_col, int end_col)
{
    int i, j;

    cell->start_col = start_col;
    cell->end_col   = end_col;
    cell->start_row = start_row;
    cell->end_row   = end_row;

    for (i = start_row; i < end_row; i++) {
	for (j = start_col; j < end_col; j++) {
	    *cell_address(grid, i, j) = cell;
	}
    }

    if (i < grid->header_rows || j < grid->header_cols) {
	apply_container_style(cell, get_th_tag(grid));
    }
    else {
	apply_container_style(cell, get_td_tag(grid));
    }
}

static void
grid_init(grid_t *grid, va_list args)
{
    DECLARE_KARGS(
	uint32_t      start_rows    = 1;
	uint32_t      start_cols    = 1;
	uint32_t      spare_rows    = 16;
	flexarray_t  *contents      = NULL;
	char         *container_tag = "table";
	char         *th_tag        = NULL;
	char         *td_tag        = NULL;
	int           header_rows   = 0;
	int           header_cols   = 0;
	);

    method_kargs(args, start_rows, start_cols, spare_rows, contents,
		 container_tag, th_tag, td_tag, header_rows, header_cols);

    grid->spare_rows    = (uint16_t)spare_rows;
    grid->width         = GRID_TERMINAL_DIM;
    grid->height        = GRID_UNBOUNDED_DIM;
    grid->td_tag_name   = td_tag;
    grid->th_tag_name   = th_tag;

    if (contents != NULL) {
	// NOTE: ignoring num_rows and num_cols; could throw an
	// exception here.
	grid_set_all_contents(grid, contents);
    }

    else {
	grid->num_rows   = (uint16_t)start_rows;
	grid->num_cols   = (uint16_t)start_cols;
	size_t num_cells = (start_rows + spare_rows) * start_cols;
	grid->cells      = gc_array_alloc(renderable_t *, num_cells);
    }

    renderable_t *self = con4m_new(T_RENDERABLE, "tag", container_tag,
				   "obj", grid);
    grid->self = self;

    grid->col_props = gc_array_alloc(render_style_t *, grid->num_cols);
    grid->row_props = gc_array_alloc(render_style_t *, grid->num_rows +
				     spare_rows);
}

static inline render_style_t *
get_row_props(grid_t *grid, int row)
{
    if (!grid->row_props[row]) {
	return lookup_cell_style("tr");
    }
    else {
	return grid->row_props[row];
    }
}

static inline render_style_t *
get_col_props(grid_t *grid, int col)
{
    if (!grid->col_props[col]) {
	return lookup_cell_style("td");
    }
    else {
	return grid->col_props[col];
    }
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
	    object_t      item = flexarray_view_next(rowviews[i], &stop);
	    renderable_t *cell = con4m_new(T_RENDERABLE, "obj", item);

	    install_renderable(g, cell, i, i + 1, j, j + 1);
	}
    }
}

void
grid_add_col_span(grid_t *grid, renderable_t *contents, int64_t row,
		  int64_t start_col, int64_t num_cols)
{
    int64_t end_col;

    if (num_cols == -1) {
	end_col = grid->num_cols;
    }
    else {
	end_col = min(start_col + num_cols, grid->num_cols);
    }

    if (row >= grid->num_rows || row < 0 || start_col < 0 ||
	(start_col + num_cols) > grid->num_cols) {
	return; // Later, throw an exception.
    }

    install_renderable(grid, contents, row, row + 1, start_col, end_col);
}

void
grid_add_row_span(grid_t *grid, renderable_t *contents, int64_t col,
		  int64_t start_row, int64_t num_rows)
{
    int64_t end_row;

    if (num_rows == -1) {
	end_row = grid->num_rows - 1;
    }
    else {
	end_row = min(start_row + num_rows - 1, grid->num_rows - 1);
    }

    if (col >= grid->num_cols || col < 0 || start_row < 0 ||
	(start_row + num_rows) > grid->num_rows) {
	return; // Later, throw an exception.
    }

    install_renderable(grid, contents, start_row, end_row, col, col + 1);
}

static inline int16_t
get_column_render_overhead(grid_t *grid)
{
    render_style_t *gs  = grid_style(grid);

    int16_t result = gs->left_pad + gs->right_pad;

    if (gs->borders & BORDER_LEFT) {
	result += 1;
    }

    if (gs->borders & BORDER_RIGHT) {
	result += 1;
    }

    if (gs->borders & INTERIOR_VERTICAL) {
	result += grid->num_cols - 1;
    }

    return result;
}

static inline int
column_text_width(grid_t *grid, int col)
{
    // For now, this doesn't take newlines into acccount.
    int         max_width = 0;
    real_str_t *s;

    for (int i = 0; i < grid->num_rows; i++) {
	renderable_t *cell = *cell_address(grid, i, col);

	// Skip any spans except one-cell vertical spans..
	if (!cell || cell->start_row != i || cell->start_col != col ||
	    cell->end_col != col + 1) {
	    continue;
	}
	switch (get_base_type(cell->raw_item)) {
	case T_STR:
	case T_UTF32:
	    s     = (real_str_t *)cell->raw_item;
	    int n = internal_num_cp(s);
	    if (n > max_width) {
		max_width = n;
	    }
	default:
	    continue;
	}
    }
    return max_width;
}
// Here, render width indicates the actual dimensions that rendering
// will produce, knowing that it might be less than or greater than the
// desired width (which we'll handle by padding or truncating).

static int16_t *
calculate_col_widths(grid_t *grid, int16_t width, int16_t *render_width)
{
    size_t          term_width;
    int16_t        *result      = gc_array_alloc(uint16_t, grid->num_cols);
    int16_t         sum         = get_column_render_overhead(grid);
    render_style_t *props;

    for (int i = 0; i < grid->num_cols; i ++) {
	props = get_col_props(grid, i);
    }
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
	    props = get_col_props(grid, i);

	    switch (props->dim_kind) {
	    case DIM_ABSOLUTE:
		result[i] = (uint16_t)props->dims.units;
		sum      += result[i];
		break;
	    case DIM_ABSOLUTE_RANGE:
		result[i] = (uint16_t)props->dims.range[1];
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
	props = get_col_props(grid, i);

	switch (props->dim_kind) {
	case DIM_ABSOLUTE:
	    cur        = (uint16_t)props->dims.units;
	    result[i]  = cur;
	    sum       += cur;
	    remaining -= cur;
	    continue;
	case DIM_ABSOLUTE_RANGE:
	    has_range  = true;
            cur        = (uint16_t)props->dims.range[0];
	    result[i]  = cur;
	    sum       += cur;
	    remaining -= cur;
	    continue;
	case DIM_PERCENT_TRUNCATE:
	    pct        = (props->dims.percent / 100);
	    cur        = (uint16_t)(pct * width);
	    result[i]  = cur;
	    sum       += cur;
	    remaining -= cur;
	    continue;
	case DIM_PERCENT_ROUND:
	    pct        = (props->dims.percent + 0.5) / 100;
	    cur        = (uint16_t)(pct * width);
	    result[i]  = cur;
	    sum       += cur;
	    remaining -= cur;
	    continue;
	case DIM_FIT_TO_TEXT:
	    cur        = column_text_width(grid, i);
	    result[i]  = cur;
	    sum       += cur;
	    remaining -= cur;

	    continue;
	case DIM_UNSET:
	case DIM_AUTO:
	    flex_units += 1;
	    num_flex   += 1;
	    continue;
	case DIM_FLEX_UNITS:
	    flex_units += props->dims.units;

	    // We don't count this if it's set to 0.
	    if (props->dims.units != 0) {
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
	    props = get_col_props(grid, i);

	    if (props->dim_kind != DIM_ABSOLUTE_RANGE) {
		continue;
	    }
	    int32_t desired = props->dims.range[1] - props->dims.range[0];

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
	props = get_col_props(grid, i);

	uint64_t units = 1;

	switch (props->dim_kind) {
	case DIM_FLEX_UNITS:
	    units = props->dims.units;
	    if (units == 0) {
		continue;
	    }
	    /* fallthrough; */
	case DIM_UNSET:
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
	    continue;
	default:
	    continue;
	}
    }

    *render_width = sum;
    return result;
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

static inline str_t *
pad_and_style_line(grid_t *grid, renderable_t *cell, int16_t width, str_t *line)
{
    alignment_t align = cell->current_style->alignment & HORIZONTAL_MASK;
    int64_t     len   = c4str_render_len(line);
    uint8_t     lnum  = cell->current_style->left_pad;
    uint8_t     rnum  = cell->current_style->right_pad;
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


    style_t     cell_style = cell->current_style->base_style;
    style_t     lpad_style = get_pad_style(cell->current_style);
    style_t     rpad_style = lpad_style;
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
str_render_cell(grid_t *grid, str_t *s, renderable_t *cell, int16_t width,
		int16_t height)
{
    render_style_t *col_style   = get_col_props(grid, cell->start_col);
    render_style_t *row_style   = get_row_props(grid, cell->start_row);
    render_style_t *cs          = copy_render_style(cell->current_style);
    xlist_t        *res         = con4m_new(T_XLIST);

    layer_styles(col_style, cs);
    layer_styles(row_style, cs);

    cell->current_style = cs;

    int           pad       = cs->left_pad + cs->right_pad;
    break_info_t *line_starts;
    str_t        *pad_line = pad_and_style_line(grid, cell, width, empty_string());
    str_t        *line;
    int           i;

    for (i = 0; i < cs->top_pad; i++) {
	xlist_append(res, pad_line);
    }

    if (cs->disable_wrap) {
	flexarray_t *f = c4str_split(s, c4str_newline());
	int err;

	for (i = 0; i < flexarray_len(f); i++) {
	    str_t *s = (str_t *)flexarray_get(f, i, &err);
	    if (s == NULL) {
		break;
	    }
	    xlist_append(res, c4str_truncate(s, width, "use_render_width", 1));
	}
    }
    else {
	line_starts = wrap_text(s, width - pad, cs->wrap);
	for (i = 0; i < line_starts->num_breaks - 1; i++) {
	    line = c4str_slice(s, line_starts->breaks[i],
			       line_starts->breaks[i + 1]);
	    line = c4str_strip(line);
	    xlist_append(res, pad_and_style_line(grid, cell, width, line));
	}

	if (i == (line_starts->num_breaks - 1)) {
	    int b = line_starts->breaks[i];
	    line = c4str_slice(s, b, c4str_len(s));
	    line = c4str_strip(line);
	    xlist_append(res, pad_and_style_line(grid, cell, width, line));
	}
    }

    for (i = 0; i < cs->bottom_pad; i++) {
	xlist_append(res, pad_line);
    }

    cell->render_cache = res;

    return xlist_len(res);
}

// Renders to the exact width, and via the height. For now, we're just going
// to handle text objects, and then sub-grids.
static inline uint16_t
render_to_cache(grid_t *grid, renderable_t *cell, int16_t width, int16_t height)
{
    assert (cell->raw_item != NULL);

    switch (get_base_type(cell->raw_item)) {
    case T_STR:
    case T_UTF32:
    {
	real_str_t *r = (real_str_t *)cell->raw_item;
	if (cell->end_col - cell->start_col != 1) {
	    str_render_cell(grid, force_utf32((str_t *)r->data),
			    cell, width, height);
	}
	return str_render_cell(grid, force_utf32((str_t *)r->data),
			       cell, width, height);
    }
    case T_GRID:
	cell->render_cache = grid_render(cell->raw_item, "width", width,
	                                 "height", height);
	return xlist_len(cell->render_cache);

    default:
	printf("Type is not grid-renderable.\n");
	abort();
    }

    return 0;
}

static inline void
grid_add_blank_cell(grid_t *grid, uint16_t row, uint16_t col, int16_t width,
		    int16_t height)
{
    real_str_t   *empty = to_internal((str_t *)empty_string());
    renderable_t *cell  = con4m_new(T_RENDERABLE, "obj", empty);

    install_renderable(grid, cell, row, row + 1, col, col + 1);
    render_to_cache(grid, cell, width, height);
}

static inline int16_t *
grid_pre_render(grid_t *grid, int16_t *col_widths)
{
    int16_t        *row_heights = gc_array_alloc(int16_t *, grid->num_rows);
    render_style_t *gs          = grid_style(grid);

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
	    if (gs->borders & INTERIOR_VERTICAL) {
		width += cell->end_col - j - 1;
	    }

	    cell->render_width = width;
	    cell_height        = render_to_cache(grid, cell, width, -1);

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
	    cell->render_cache = pad_lines_vertically(gs, cell->render_cache,
						row_height, cell->render_width);
	}

    }
    return row_heights;
}

static inline void
grid_add_top_pad(grid_t *grid, xlist_t *lines, int16_t width) {
    render_style_t *gs  = grid_style(grid);
    int             top = gs->top_pad;

    if (!top) {
	return;
    }

    str_t *pad = get_styled_pad(width, get_pad_style(gs));

    for (int i = 0; i < top; i++) {
	xlist_append(lines, pad);
    }
}

static inline void
grid_add_bottom_pad(grid_t *grid, xlist_t *lines, int16_t width) {
    render_style_t *gs     = grid_style(grid);
    int             bottom = gs->bottom_pad;

    if (!bottom) {
	return;
    }

    str_t *pad = get_styled_pad(width, get_pad_style(gs));

    for (int i = 0; i < bottom; i++) {
	xlist_append(lines, pad);
    }
}

static inline int
find_spans(grid_t *grid, int row, int col)
{
    int result = SPAN_NONE;

    if (col + 1 == grid->num_rows) {
	return result;
    }

    renderable_t *cell = *cell_address(grid, row, col + 1);
    if (cell->start_col != col + 1) {
	result |= SPAN_HERE;
    }


    if (row != grid->num_rows - 1) {
	renderable_t *cell = *cell_address(grid, row + 1, col + 1);
	if (cell->start_col != col + 1) {
	    result |= SPAN_BELOW;
	}
    }

    return result;
}

static inline void
grid_add_top_border(grid_t *grid, xlist_t *lines, int16_t *col_widths)
{
    render_style_t *gs           = grid_style(grid);
    int32_t         border_width = 0;
    int             vertical_borders;
    border_theme_t *draw_chars;
    str_t          *s, *lpad, *rpad;
    int32_t        *p;
    style_t         pad_color;


    if (!(gs->borders & BORDER_TOP)) {
	return;
    }

    draw_chars = get_border_theme(gs);

    for (int i = 0; i < grid->num_cols; i++) {
	border_width += col_widths[i];
    }

    if (gs->borders & BORDER_LEFT) {
	border_width++;
    }

    if (gs->borders & BORDER_RIGHT) {
	border_width++;
    }

    vertical_borders = gs->borders & INTERIOR_VERTICAL;

    if (vertical_borders) {
	border_width += grid->num_cols - 1;
    }

    s = (str_t *)con4m_new(T_UTF32, "length", border_width);
    p = (int32_t *)s;

    (to_internal(s))->codepoints = ~border_width;

    if (gs->borders & BORDER_LEFT) {
	*p++ = draw_chars->upper_left;
    }

    for (int i = 0; i < grid->num_cols; i++) {
	for (int j = 0; j < col_widths[i]; j++) {
	    *p++ = draw_chars->horizontal_rule;
	}

	if (vertical_borders && (i + 1 != grid->num_cols)) {
	    if (find_spans(grid, 0, i) == SPAN_HERE) {
		*p++ = draw_chars->horizontal_rule;
	    }
	    else {
		*p++ = draw_chars->top_t;
	    }
	}
    }

    if (gs->borders & BORDER_RIGHT) {
	*p++ = draw_chars->upper_right;
    }

    c4str_apply_style(s, get_string_style(gs));

    pad_color = get_pad_style(gs);
    lpad      = get_styled_pad(gs->left_pad, pad_color);
    rpad      = get_styled_pad(gs->right_pad, pad_color);

    xlist_append(lines, c4str_concat(c4str_concat(lpad, s), rpad));
}

static inline void
grid_add_bottom_border(grid_t *grid, xlist_t *lines, int16_t *col_widths)
{
    render_style_t *gs           = grid_style(grid);
    int32_t         border_width = 0;
    int             vertical_borders;
    border_theme_t *draw_chars;
    str_t          *s, *lpad, *rpad;
    int32_t        *p;
    style_t         pad_color;

    if (!(gs->borders & BORDER_BOTTOM)) {
	return;
    }

    draw_chars = get_border_theme(gs);

    for (int i = 0; i < grid->num_cols; i++) {
	border_width += col_widths[i];
    }

    if (gs->borders & BORDER_LEFT) {
	border_width++;
    }

    if (gs->borders & BORDER_RIGHT) {
	border_width++;
    }

    vertical_borders = gs->borders & INTERIOR_VERTICAL;

    if (vertical_borders) {
	border_width += grid->num_cols - 1;
    }

    s = (str_t *)con4m_new(T_UTF32, "length", border_width);
    p = (int32_t *)s;
    (to_internal(s))->codepoints = ~border_width;

    if (gs->borders & BORDER_LEFT) {
	*p++ = draw_chars->lower_left;
    }

    for (int i = 0; i < grid->num_cols; i++) {
	for (int j = 0; j < col_widths[i]; j++) {
	    *p++ = draw_chars->horizontal_rule;
	}

	if (vertical_borders && (i + 1 != grid->num_cols)) {
	    if (find_spans(grid, grid->num_rows - 1, i) == SPAN_HERE) {
		*p++ = draw_chars->horizontal_rule;
	    }
	    else {
		*p++ = draw_chars->bottom_t;
	    }

	}
    }

    if (gs->borders & BORDER_RIGHT) {
	*p++ = draw_chars->lower_right;
    }

    c4str_apply_style(s, get_string_style(gs));

    pad_color = get_pad_style(gs);
    lpad      = get_styled_pad(gs->left_pad, pad_color);
    rpad      = get_styled_pad(gs->right_pad, pad_color);

    xlist_append(lines, c4str_concat(c4str_concat(lpad, s), rpad));
}

static inline void
grid_add_horizontal_rule(grid_t *grid, int row, xlist_t *lines, int16_t *col_widths)
{
    render_style_t *gs           = grid_style(grid);
    int32_t         border_width = 0;
    int             vertical_borders;
    border_theme_t *draw_chars;
    str_t          *s, *lpad, *rpad;
    int32_t        *p;
    style_t         pad_color;

    if (!(gs->borders & INTERIOR_HORIZONTAL)) {
	return;
    }

    draw_chars = get_border_theme(gs);

    for (int i = 0; i < grid->num_cols; i++) {
	border_width += col_widths[i];
    }

    if (gs->borders & BORDER_LEFT) {
	border_width++;
    }

    if (gs->borders & BORDER_RIGHT) {
	border_width++;
    }

    vertical_borders = gs->borders & INTERIOR_VERTICAL;

    if (vertical_borders) {
	border_width += grid->num_cols - 1;
    }

    s = (str_t *)con4m_new(T_UTF32, "length", border_width);
    p = (int32_t *)s;
    (to_internal(s))->codepoints = ~border_width;

    if (gs->borders & BORDER_LEFT) {
	*p++ = draw_chars->left_t;
    }

    for (int i = 0; i < grid->num_cols; i++) {
	for (int j = 0; j < col_widths[i]; j++) {
	    *p++ = draw_chars->horizontal_rule;
	}

	if (vertical_borders && (i + 1 != grid->num_cols)) {
	    switch(find_spans(grid, row, i)) {
	    case SPAN_NONE:
		*p++ = draw_chars->cross;
		break;
	    case SPAN_HERE:
		*p++ = draw_chars->top_t;
		break;
	    case SPAN_BELOW:
		*p++ = draw_chars->bottom_t;
		break;
	    default:
		*p++ = draw_chars->horizontal_rule;
		break;
	    }
	}
    }

    if (gs->borders & BORDER_RIGHT) {
	*p++ = draw_chars->right_t;
    }

    c4str_apply_style(s, get_string_style(gs));

    pad_color = get_pad_style(gs);
    lpad      = get_styled_pad(gs->left_pad, pad_color);
    rpad      = get_styled_pad(gs->right_pad, pad_color);

    xlist_append(lines, c4str_concat(c4str_concat(lpad, s), rpad));
}


static inline xlist_t *
grid_add_left_pad(grid_t *grid, int height)
{
    render_style_t *gs   = grid_style(grid);
    xlist_t        *res  = con4m_new(T_XLIST, "length", height);
    str_t          *lpad = empty_string();

    if (gs->left_pad > 0) {
	lpad = get_styled_pad(gs->left_pad, get_pad_style(gs));
    }

    for (int i = 0; i < height; i++) {
	xlist_append(res, lpad);
    }

    return res;
}

static inline void
grid_add_right_pad(grid_t *grid, xlist_t *lines)
{
    render_style_t *gs   = grid_style(grid);

    if (gs->right_pad <= 0) {
	return;
    }

    str_t *rpad = get_styled_pad(gs->right_pad, get_pad_style(gs));

    for (int i = 0; i < xlist_len(lines); i++) {
	str_t *s = (str_t *)xlist_get(lines, i, NULL);
	xlist_set(lines, i, c4str_concat(s, rpad));
    }
}

static inline void
add_vertical_bar(grid_t *grid, xlist_t *lines, border_set_t to_match)
{
    render_style_t *gs = grid_style(grid);

    if (!(gs->borders & to_match)) {
	return;
    }

    border_theme_t *border_theme = get_border_theme(gs);
    style_t         border_color = get_string_style(gs);
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
    render_style_t *gs   = grid_style(grid);
    int32_t         diff = height - xlist_len(lines);

    switch (gs->alignment & VERTICAL_MASK) {
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
align_and_crop_grid_line(grid_t *grid, str_t *line, int32_t width)
{
    render_style_t *gs        = grid_style(grid);
    alignment_t     align     = gs->alignment;
    style_t         pad_style = get_pad_style(gs);

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
    int num_lines = xlist_len(lines);

    // For now, width must always be set. Won't be true for height.

    for (int i = 0; i < num_lines; i++) {
	str_t *s = (str_t *)xlist_get(lines, i, NULL);
	if (c4str_render_len(s) == width) {
	    continue;
	}

	str_t *l = align_and_crop_grid_line(grid, s, width);
	xlist_set(lines, i, l);
    }

    if (height != -1) {
	if (num_lines > height) {
	    crop_vertically(grid, lines, height);
	}
	else {
	    if (num_lines < height) {
		lines = pad_lines_vertically(grid_style(grid), lines,
					     height, width);
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
		style_t pad_style = get_pad_style(grid_style(grid));
		piece = get_styled_pad(col_widths[i],  pad_style);
	    }
	    xlist_set(lines, i, c4str_concat(s, piece));
	}
	return true;
    }

    // For spans, just return the one block of the grid, along with
    // any interior borders.
    uint16_t        row_offset   = r - cell->start_row;
    uint16_t        col_offset   = c - cell->start_col;
    int             start_width  = 0;
    int             start_height = 0;
    render_style_t *gs           = grid_style(grid);

    if (gs->borders & INTERIOR_VERTICAL) {
	start_width += col_offset;
    }

    if (gs->borders & INTERIOR_HORIZONTAL) {
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

    if ((gs->borders & INTERIOR_HORIZONTAL) &&
	r + 1 != cell->end_row) {
	num_rows += 1;
    }

    if ((gs->borders & INTERIOR_VERTICAL) &&
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

    if (width == -1) {
	width = terminal_width();
	width = max(terminal_width(), 20);
    }

    if (width == 0) {
	return con4m_new(T_XLIST, "length", 0);
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

    render_style_t *gs = grid_style(grid);
    uint16_t h_alloc   = grid->num_rows + 1 + gs->top_pad + gs->bottom_pad;

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
	    grid_add_horizontal_rule(grid, i, result, col_widths);
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
_ordered_list(flexarray_t *items, ...)
{
    DECLARE_KARGS(
	char       *bullet_style = "bullet";
	char       *item_style   = "li";
	);

    kargs(items, bullet_style, item_style);

    flex_view_t *view         = flexarray_view(items);
    int64_t     n             = flexarray_view_len(view);
    str_t       *dot          = c4str_repeat('.', 1);
    grid_t      *res          = con4m_new(T_GRID, "start_rows", n,
					  "start_cols", 2,
					  "container_tag", "ol");


    render_style_t *bp    = lookup_cell_style(bullet_style);
    float           log   = log10((float)n);
    int             width = (int)(log + .5) + 1 + 1;
    // Above, one + 1 is because log returns one less than what we
    // need for the int with, and the other is for the period /
    // bullet.

    width         += bp->left_pad + bp->right_pad;
    bp->dims.units = width;

    res->col_props[0] = bp;
    set_column_style(res, 1, item_style);

    for (int i = 0; i < n; i++) {
	int           status;
	str_t        *s         = c4str_concat(c4str_from_int(i + 1), dot);
	real_str_t   *list_item = flexarray_view_next(view, (bool *)&status);
	renderable_t *li        = con4m_new(T_RENDERABLE, "obj", list_item,
					    "tag", item_style);
	grid_set_cell_contents(res, i, 0, to_str_renderable(s, bullet_style));
	grid_set_cell_contents(res, i, 1, li);
    }
    return res;
}

grid_t *
_unordered_list(flexarray_t *items, ...)
{
    DECLARE_KARGS(
	char       *bullet_style = "bullet";
	char       *item_style   = "li";
	codepoint_t bullet = 0x2022;
	);

    kargs(items, bullet_style, item_style, bullet);

    flex_view_t *view       = flexarray_view(items);
    int64_t      n          = flexarray_view_len(view);
    grid_t      *res        = con4m_new(T_GRID, "start_rows", n,
					"start_cols", 2,
					"container_tag", "ul");
    str_t       *bull_str   = c4str_repeat(bullet, 1);

    render_style_t *bp = lookup_cell_style(bullet_style);
    bp->dims.units    += bp->left_pad + bp->right_pad;

    res->col_props[0] = bp;
    set_column_style(res, 1, item_style);

    for (int i = 0; i < n; i++) {
	int           status;
	real_str_t   *list_item = flexarray_view_next(view, (bool *)&status);
	renderable_t *li        = con4m_new(T_RENDERABLE, "obj", list_item,
					    "tag", item_style);

	grid_set_cell_contents(res, i, 0,
			       to_str_renderable(bull_str, bullet_style));
	grid_set_cell_contents(res, i, 1, li);
    }

    return res;
}

grid_t *
grid_flow(uint64_t items, ...)
{
    va_list contents;

    grid_t *res = con4m_new(T_GRID, "start_rows", items, "start_cols", 1,
			    "container_tag", "flow");

    va_start(contents, items);
    for (uint64_t i = 0; i < items; i++) {
	grid_set_cell_contents(res, i, 0, (object_t)va_arg(contents, object_t));
    }

    return res;
}


const con4m_vtable grid_vtable  = {
    .num_entries = 2,
    .methods     = {
	(con4m_vtable_entry)grid_init,
	(con4m_vtable_entry)grid_to_str
    }
};

const con4m_vtable renderable_vtable = {
    .num_entries = 1,
    .methods     = {
	(con4m_vtable_entry)renderable_init
    }
};

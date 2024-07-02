// TODO
// 1. Search.
// 2. Now we're ready to add a more generic `print()`.
// 3. I'd like to do the debug console soon-ish.

// Not soon, but should eventually get done:
// 1. Row spans (column spans are there; row spans only stubbed).
// 2. Style doesn't pick up properly w/ col spans ending exactly on middle.
// 3. Also not soon, but should consider allowing style info to "resolve"
//    as a better way to fix issues w/ split.

#include "con4m.h"

#define SPAN_NONE  0
#define SPAN_HERE  1
#define SPAN_BELOW 2

static inline c4m_render_style_t *
grid_style(c4m_grid_t *grid)
{
    if (!grid->self->current_style) {
        grid->self->current_style = c4m_lookup_cell_style("table");
    }

    return grid->self->current_style;
}

void
c4m_apply_container_style(c4m_renderable_t *item, char *tag)

{
    c4m_render_style_t *tag_style = c4m_lookup_cell_style(tag);
    if (!tag_style) {
        return;
    }

    if (item->current_style == NULL) {
        item->current_style = tag_style;
    }
    else {
        c4m_layer_styles(tag_style, item->current_style);
    }
}

static inline c4m_utf32_t *
styled_repeat(c4m_codepoint_t c, uint32_t width, c4m_style_t style)
{
    c4m_utf32_t *result = c4m_utf32_repeat(c, width);

    if (c4m_str_codepoint_len(result) != 0) {
        c4m_str_set_style(result, style);
    }

    return result;
}

static inline c4m_utf32_t *
get_styled_pad(uint32_t width, c4m_style_t style)
{
    return styled_repeat(' ', width, style);
}

static c4m_list_t *
pad_lines_vertically(c4m_render_style_t *gs,
                     c4m_list_t         *list,
                     int32_t             height,
                     int32_t             width)
{
    int32_t      len  = c4m_list_len(list);
    int32_t      diff = height - len;
    c4m_utf32_t *pad;
    c4m_list_t  *res;

    if (len == 0) {
        pad = get_styled_pad(width, c4m_get_pad_style(gs));
    }
    else {
        pad            = c4m_utf32_repeat(' ', width);
        c4m_utf32_t *l = c4m_to_utf32(c4m_list_get(list, len - 1, NULL));

        pad->styling = l->styling;
    }
    switch (gs->alignment) {
    case C4M_ALIGN_BOTTOM:
        res = c4m_new(c4m_type_list(c4m_type_utf32()),
                      c4m_kw("length", c4m_ka(height)));

        for (int i = 0; i < diff; i++) {
            c4m_list_append(res, pad);
        }
        c4m_list_plus_eq(res, list);
        return res;

    case C4M_ALIGN_MIDDLE:
        res = c4m_new(c4m_type_list(c4m_type_utf32()),
                      c4m_kw("length", c4m_ka(height)));

        for (int i = 0; i < diff / 2; i++) {
            c4m_list_append(res, pad);
        }

        c4m_list_plus_eq(res, list);

        for (int i = 0; i < diff / 2; i++) {
            c4m_list_append(res, pad);
        }

        if (diff % 2 != 0) {
            c4m_list_append(res, pad);
        }

        return res;
    default:
        for (int i = 0; i < diff; i++) {
            c4m_list_append(list, pad);
        }
        return list;
    }
}

static void
renderable_init(c4m_renderable_t *item, va_list args)
{
    c4m_obj_t *obj = NULL;
    char      *tag = NULL;

    c4m_karg_va_init(args);

    c4m_kw_ptr("obj", obj);
    c4m_kw_ptr("tag", tag);

    item->raw_item = obj;

    if (tag != NULL) {
        c4m_apply_container_style(item, tag);
    }
}

bool
c4m_install_renderable(c4m_grid_t       *grid,
                       c4m_renderable_t *cell,
                       int               start_row,
                       int               end_row,
                       int               start_col,
                       int               end_col)
{
    int i, j = 0;

    cell->start_col = start_col;
    cell->end_col   = end_col;
    cell->start_row = start_row;
    cell->end_row   = end_row;

    if (start_col < 0 || start_col >= grid->num_cols) {
        return false;
    }
    if (start_row < 0 || start_row >= grid->num_rows) {
        return false;
    }

    for (i = start_row; i < end_row; i++) {
        for (j = start_col; j < end_col; j++) {
            *c4m_cell_address(grid, i, j) = cell;
        }
    }

    if (i < grid->header_rows || j < grid->header_cols) {
        c4m_apply_container_style(cell, c4m_get_th_tag(grid));
    }
    else {
        c4m_apply_container_style(cell, c4m_get_td_tag(grid));
    }

    return true;
}

void
c4m_expand_columns(c4m_grid_t *grid, uint64_t num)
{
    uint16_t           new_cols = grid->num_cols + num;
    size_t             sz       = new_cols * (grid->num_rows + grid->spare_rows);
    c4m_renderable_t **cells    = c4m_gc_array_alloc(c4m_renderable_t *, sz);
    c4m_renderable_t **p        = grid->cells;

    for (int i = 0; i < grid->num_rows; i++) {
        for (int j = 0; j < grid->num_cols; j++) {
            cells[(i * new_cols) + j] = *p++;
        }
    }

    // This needs a lock.
    grid->cells    = cells;
    grid->num_cols = new_cols;
}

void
c4m_grid_expand_rows(c4m_grid_t *grid, uint64_t num)
{
    if (num <= grid->spare_rows) {
        grid->num_rows += num;
        grid->spare_rows -= num;
        return;
    }

    int                old_num  = grid->num_rows * grid->num_cols;
    uint16_t           new_rows = grid->num_rows + num;
    size_t             sz       = grid->num_cols * (new_rows + grid->spare_rows);
    c4m_renderable_t **cells    = c4m_gc_array_alloc(c4m_renderable_t *, sz);
    for (int i = 0; i < old_num; i++) {
        cells[i] = grid->cells[i];
    }

    grid->cells    = cells;
    grid->num_rows = new_rows;
}

void
c4m_grid_add_row(c4m_grid_t *grid, c4m_obj_t container)
{
    if (grid->row_cursor == grid->num_rows) {
        c4m_grid_expand_rows(grid, 1);
    }
    if (grid->col_cursor != 0) {
        grid->row_cursor++;
        grid->col_cursor = 0;
    }

    switch (c4m_base_type(container)) {
    case C4M_T_RENDERABLE:
        c4m_install_renderable(grid,
                               (c4m_renderable_t *)container,
                               grid->row_cursor,
                               grid->row_cursor + 1,
                               0,
                               grid->num_cols);
        grid->row_cursor++;
        return;

    case C4M_T_GRID:
    case C4M_T_UTF8:
    case C4M_T_UTF32: {
        c4m_renderable_t *r = c4m_new(c4m_type_renderable(),
                                      c4m_kw("obj",
                                             c4m_ka(container),
                                             "tag",
                                             c4m_ka("td")));
        c4m_install_renderable(grid,
                               r,
                               grid->row_cursor,
                               grid->row_cursor + 1,
                               0,
                               grid->num_cols);
        grid->row_cursor++;
        return;
    }
    case C4M_T_FLIST:;
        flex_view_t *items = flexarray_view((flexarray_t *)container);

        for (int i = 0; i < grid->num_cols; i++) {
            int       err = false;
            c4m_obj_t x   = flexarray_view_next(items, &err);
            if (err || x == NULL) {
                x = (c4m_obj_t)c4m_to_utf32(c4m_empty_string());
            }
            c4m_grid_set_cell_contents(grid, grid->row_cursor, i, x);
        }
        return;
    case C4M_T_XLIST:
        for (int i = 0; i < grid->num_cols; i++) {
            c4m_obj_t x = c4m_list_get((c4m_list_t *)container, i, NULL);
            if (x == NULL) {
                x = (c4m_obj_t)c4m_new(c4m_type_utf8(),
                                       c4m_kw("cstring", c4m_ka(" ")));
            }
            c4m_grid_set_cell_contents(grid, grid->row_cursor, i, x);
        }
        return;

    default:
        C4M_CRAISE("Invalid item type for grid.");
    }
}

static void
grid_init(c4m_grid_t *grid, va_list args)
{
    int32_t     start_rows    = 1;
    int32_t     start_cols    = 1;
    int32_t     spare_rows    = 16;
    c4m_list_t *contents      = NULL;
    char       *container_tag = "table";
    char       *th_tag        = NULL;
    char       *td_tag        = NULL;
    int32_t     header_rows   = 0;
    int32_t     header_cols   = 0;
    bool        stripe        = false;

    c4m_karg_va_init(args);
    c4m_kw_int32("start_rows", start_rows);
    c4m_kw_int32("start_cols", start_cols);
    c4m_kw_int32("spare_rows", spare_rows);
    c4m_kw_ptr("contents", contents);
    c4m_kw_ptr("container_tag", container_tag);
    c4m_kw_ptr("th_tag", th_tag);
    c4m_kw_ptr("td_tag", td_tag);
    c4m_kw_int32("header_rows", header_rows);
    c4m_kw_int32("header_cols", header_cols);
    c4m_kw_bool("stripe", stripe);

    if (start_rows < 1) {
        start_rows = 1;
    }

    if (start_cols < 1) {
        start_cols = 1;
    }

    if (spare_rows < 0) {
        spare_rows = 0;
    }

    grid->spare_rows  = (uint16_t)spare_rows;
    grid->width       = C4M_GRID_TERMINAL_DIM;
    grid->height      = C4M_GRID_UNBOUNDED_DIM;
    grid->td_tag_name = td_tag;
    grid->th_tag_name = th_tag;

    if (stripe) {
        grid->stripe = 1;
    }

    if (contents != NULL) {
        // NOTE: ignoring num_rows and num_cols; could throw an
        // exception here.
        c4m_grid_set_all_contents(grid, contents);
    }

    else {
        grid->num_rows   = (uint16_t)start_rows;
        grid->num_cols   = (uint16_t)start_cols;
        size_t num_cells = (start_rows + spare_rows) * start_cols;
        grid->cells      = c4m_gc_array_alloc(c4m_renderable_t *, num_cells);
    }

    if (!c4m_style_exists(container_tag)) {
        container_tag = "table";
    }

    if (!c4m_style_exists(td_tag)) {
        td_tag = "td";
    }

    if (!c4m_style_exists(th_tag)) {
        td_tag = "th";
    }

    c4m_renderable_t *self = c4m_new(c4m_type_renderable(),
                                     c4m_kw("tag",
                                            c4m_ka(container_tag),
                                            "obj",
                                            c4m_ka(grid)));
    grid->self             = self;

    grid->col_props = NULL;
    grid->row_props = NULL;

    grid->header_rows = header_rows;
    grid->header_cols = header_cols;
}

static inline c4m_render_style_t *
get_row_props(c4m_grid_t *grid, int row)
{
    c4m_render_style_t *result;

    if (grid->row_props != NULL) {
        result = hatrack_dict_get(grid->row_props, (void *)(int64_t)row, NULL);
        if (result != NULL) {
            return result;
        }
    }

    if (grid->stripe) {
        if (row % 2) {
            return c4m_lookup_cell_style("tr.even");
        }
        else {
            return c4m_lookup_cell_style("tr.odd");
        }
    }
    else {
        return c4m_lookup_cell_style("tr");
    }
}

static inline c4m_render_style_t *
get_col_props(c4m_grid_t *grid, int col)
{
    c4m_render_style_t *result;

    if (grid->col_props != NULL) {
        result = hatrack_dict_get(grid->col_props, (void *)(int64_t)col, NULL);

        if (result != NULL) {
            return result;
        }
    }

    return c4m_lookup_cell_style("td");
}

// Contents currently must be a list[list[c4m_obj_t]].  Supply
// properties separately; if you want something that spans you should
// instead
void
c4m_grid_set_all_contents(c4m_grid_t *g, c4m_list_t *rows)
{
    int64_t  nrows = c4m_list_len(rows);
    uint64_t ncols = 0;

    for (int64_t i = 0; i < nrows; i++) {
        c4m_list_t *row = (c4m_list_t *)(c4m_list_get(rows, i, NULL));

        uint64_t rlen = c4m_list_len(row);

        if (rlen > ncols) {
            ncols = rlen;
        }
    }

    size_t num_cells = (nrows + g->spare_rows) * ncols;
    g->cells         = c4m_gc_array_alloc(c4m_renderable_t *, num_cells);
    g->num_rows      = nrows;
    g->num_cols      = ncols;

    for (int64_t i = 0; i < nrows; i++) {
        c4m_list_t *view    = c4m_list_get(rows, i, NULL);
        uint64_t    viewlen = c4m_list_len(view);

        for (uint64_t j = 0; j < viewlen; j++) {
            c4m_obj_t         item = c4m_list_get(view, j, NULL);
            c4m_renderable_t *cell = c4m_new(c4m_type_renderable(),
                                             c4m_kw("obj", c4m_ka(item)));

            c4m_install_renderable(g, cell, i, i + 1, j, j + 1);
        }
    }
}

void
c4m_grid_add_col_span(c4m_grid_t       *grid,
                      c4m_renderable_t *contents,
                      int64_t           row,
                      int64_t           start_col,
                      int64_t           num_cols)
{
    int64_t end_col;

    if (num_cols == -1) {
        end_col = grid->num_cols;
    }
    else {
        end_col = c4m_min(start_col + num_cols, grid->num_cols);
    }

    if (row >= grid->num_rows || row < 0 || start_col < 0 || (start_col + num_cols) > grid->num_cols) {
        return; // Later, throw an exception.
    }

    c4m_install_renderable(grid, contents, row, row + 1, start_col, end_col);
}

void
c4m_grid_add_row_span(c4m_grid_t       *grid,
                      c4m_renderable_t *contents,
                      int64_t           col,
                      int64_t           start_row,
                      int64_t           num_rows)
{
    int64_t end_row;

    if (num_rows == -1) {
        end_row = grid->num_rows - 1;
    }
    else {
        end_row = c4m_min(start_row + num_rows - 1, grid->num_rows - 1);
    }

    if (col >= grid->num_cols || col < 0 || start_row < 0 || (start_row + num_rows) > grid->num_rows) {
        return; // Later, throw an exception.
    }

    c4m_install_renderable(grid, contents, start_row, end_row, col, col + 1);
}

static inline int16_t
get_column_render_overhead(c4m_grid_t *grid)
{
    c4m_render_style_t *gs     = grid_style(grid);
    int16_t             result = gs->left_pad + gs->right_pad;

    if (gs->borders & C4M_BORDER_LEFT) {
        result += 1;
    }

    if (gs->borders & C4M_BORDER_RIGHT) {
        result += 1;
    }

    if (gs->borders & C4M_INTERIOR_VERTICAL) {
        result += grid->num_cols - 1;
    }

    return result;
}

static inline int
column_text_width(c4m_grid_t *grid, int col)
{
    int        max_width = 0;
    c4m_str_t *s;

    for (int i = 0; i < grid->num_rows; i++) {
        c4m_renderable_t *cell = *c4m_cell_address(grid, i, col);

        // Skip any spans except one-cell vertical spans..
        if (!cell || cell->start_row != i || cell->start_col != col || cell->end_col != col + 1) {
            continue;
        }
        switch (c4m_base_type(cell->raw_item)) {
        case C4M_T_UTF8:
        case C4M_T_UTF32:
            s = (c4m_str_t *)cell->raw_item;

            c4m_list_t *arr = c4m_str_xsplit(s, c4m_str_newline());
            int         len = c4m_list_len(arr);

            for (int j = 0; j < len; j++) {
                c4m_utf32_t *item = c4m_to_utf32(c4m_list_get(arr, j, NULL));
                if (item == NULL) {
                    break;
                }
                int cur = c4m_str_render_len(item);
                if (cur > max_width) {
                    max_width = cur;
                }
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
calculate_col_widths(c4m_grid_t *grid, int16_t width, int16_t *render_width)
{
    size_t              term_width;
    int16_t            *result = c4m_gc_array_value_alloc(uint16_t,
                                               grid->num_cols);
    int16_t             sum    = get_column_render_overhead(grid);
    c4m_render_style_t *props;

    for (int i = 0; i < grid->num_cols; i++) {
        props = get_col_props(grid, i);
    }
    if (width == C4M_GRID_USE_STORED) {
        width = grid->width;
    }

    if (width == C4M_GRID_TERMINAL_DIM) {
        c4m_terminal_dimensions(&term_width, NULL);
        if (term_width == 0) {
            term_width = 80;
        }
        width = (int16_t)term_width;
    }

    if (width == C4M_GRID_UNBOUNDED_DIM) {
        result = c4m_gc_array_alloc(uint16_t, grid->num_cols);

        for (int i = 0; i < grid->num_cols; i++) {
            props = get_col_props(grid, i);

            switch (props->dim_kind) {
            case C4M_DIM_ABSOLUTE:
                result[i] = (uint16_t)props->dims.units;
                sum += result[i];
                break;
            case C4M_DIM_ABSOLUTE_RANGE:
                result[i] = (uint16_t)props->dims.range[1];
                sum += result[i];
                break;
            default:
                C4M_CRAISE("Invalid col spec for unbounded width.");
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
        case C4M_DIM_ABSOLUTE:
            cur       = (uint16_t)props->dims.units;
            result[i] = cur;
            sum += cur;
            remaining -= cur;
            continue;
        case C4M_DIM_ABSOLUTE_RANGE:
            has_range = true;
            cur       = (uint16_t)props->dims.range[0];
            result[i] = cur;
            sum += cur;
            remaining -= cur;
            continue;
        case C4M_DIM_PERCENT_TRUNCATE:
            pct       = (props->dims.percent / 100);
            cur       = (uint16_t)(pct * width);
            result[i] = cur;
            sum += cur;
            remaining -= cur;
            continue;
        case C4M_DIM_PERCENT_ROUND:
            pct       = (props->dims.percent + 0.5) / 100;
            cur       = (uint16_t)(pct * width);
            result[i] = cur;
            sum += cur;
            remaining -= cur;
            continue;
        case C4M_DIM_FIT_TO_TEXT:
            cur = column_text_width(grid, i);
            // Assume minimal padding needed.
            cur += 2;
            result[i] = cur;
            sum += cur;
            remaining -= cur;
            continue;
        case C4M_DIM_UNSET:
        case C4M_DIM_AUTO:
            flex_units += 1;
            num_flex += 1;
            continue;
        case C4M_DIM_FLEX_UNITS:
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

            if (props->dim_kind != C4M_DIM_ABSOLUTE_RANGE) {
                continue;
            }
            int32_t desired = props->dims.range[1] - props->dims.range[0];

            if (desired <= 0) {
                continue;
            }
            cur = c4m_min((uint16_t)desired, (uint16_t)remaining);
            sum += cur;
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
        case C4M_DIM_FLEX_UNITS:
            units = props->dims.units;
            if (units == 0) {
                continue;
            }
            /* fallthrough */
        case C4M_DIM_UNSET:
        case C4M_DIM_AUTO:
            if (--num_flex == 0) {
                result[i] += remaining;
                sum += remaining;

                *render_width = sum;
                return result;
            }
            cur       = (uint16_t)(units * flex_width);
            result[i] = cur;
            sum += cur;
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

static inline c4m_utf32_t *
pad_and_style_line(c4m_grid_t       *grid,
                   c4m_renderable_t *cell,
                   int16_t           width,
                   c4m_utf32_t      *line)
{
    c4m_alignment_t align = cell->current_style->alignment & C4M_HORIZONTAL_MASK;
    int64_t         len   = c4m_str_render_len(line);
    uint8_t         lnum  = cell->current_style->left_pad;
    uint8_t         rnum  = cell->current_style->right_pad;
    int64_t         diff  = width - len - lnum - rnum;
    c4m_utf32_t    *lpad;
    c4m_utf32_t    *rpad;

    if (diff > 0) {
        switch (align) {
        case C4M_ALIGN_RIGHT:
            lnum += diff;
            break;
        case C4M_ALIGN_CENTER:
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

    c4m_style_t  cell_style = cell->current_style->base_style;
    c4m_style_t  lpad_style = c4m_get_pad_style(cell->current_style);
    c4m_style_t  rpad_style = lpad_style;
    c4m_utf32_t *copy       = c4m_str_copy(line);

    c4m_style_gaps(copy, cell_style);

    int last_style = copy->styling->num_entries - 1;

    lpad_style = copy->styling->styles[0].info;
    rpad_style = copy->styling->styles[last_style].info;

    lpad = get_styled_pad(lnum, lpad_style);
    rpad = get_styled_pad(rnum, rpad_style);

    return c4m_str_concat(c4m_str_concat(lpad, copy), rpad);
}

static inline uint16_t
str_render_cell(c4m_grid_t       *grid,
                c4m_utf32_t      *s,
                c4m_renderable_t *cell,
                int16_t           width,
                int16_t           height)
{
    c4m_render_style_t *col_style = get_col_props(grid, cell->start_col);
    c4m_render_style_t *row_style = get_row_props(grid, cell->start_row);
    c4m_render_style_t *cs        = c4m_copy_render_style(cell->current_style);
    c4m_list_t         *res       = c4m_new(c4m_type_list(c4m_type_utf32()));

    c4m_layer_styles(col_style, cs);
    c4m_layer_styles(row_style, cs);

    cell->current_style = cs;

    int               pad      = cs->left_pad + cs->right_pad;
    c4m_utf32_t      *pad_line = pad_and_style_line(grid,
                                               cell,
                                               width,
                                               c4m_empty_string());
    c4m_break_info_t *line_starts;
    c4m_utf32_t      *line;
    int               i;

    for (i = 0; i < cs->top_pad; i++) {
        c4m_list_append(res, pad_line);
    }

    if (cs->disable_wrap) {
        c4m_list_t *arr = c4m_str_xsplit(s, c4m_str_newline());
        bool        err;

        for (i = 0; i < c4m_list_len(arr); i++) {
            c4m_utf32_t *one = c4m_to_utf32(c4m_list_get(arr, i, &err));
            if (one == NULL) {
                break;
            }
            c4m_list_append(res,
                            c4m_str_truncate(one,
                                             width,
                                             c4m_kw("use_render_width",
                                                    c4m_ka(true))));
        }
    }
    else {
        line_starts = c4m_wrap_text(s, width - pad, cs->wrap);
        for (i = 0; i < line_starts->num_breaks - 1; i++) {
            line = c4m_str_slice(s,
                                 line_starts->breaks[i],
                                 line_starts->breaks[i + 1]);
            line = c4m_str_strip(line);
            c4m_list_append(res, pad_and_style_line(grid, cell, width, line));
        }

        if (i == (line_starts->num_breaks - 1)) {
            int b = line_starts->breaks[i];
            line  = c4m_str_slice(s, b, c4m_str_codepoint_len(s));
            line  = c4m_str_strip(line);
            c4m_list_append(res, pad_and_style_line(grid, cell, width, line));
        }
    }

    for (i = 0; i < cs->bottom_pad; i++) {
        c4m_list_append(res, pad_line);
    }

    cell->render_cache = res;

    return c4m_list_len(res);
}

// Renders to the exact width, and via the height. For now, we're just going
// to handle text objects, and then sub-grids.
static inline uint16_t
render_to_cache(c4m_grid_t       *grid,
                c4m_renderable_t *cell,
                int16_t           width,
                int16_t           height)
{
    assert(cell->raw_item != NULL);

    switch (c4m_base_type(cell->raw_item)) {
    case C4M_T_UTF8:
    case C4M_T_UTF32: {
        c4m_str_t *r = (c4m_str_t *)cell->raw_item;
        if (cell->end_col - cell->start_col != 1) {
            return str_render_cell(grid, c4m_to_utf32(r), cell, width, height);
        }
        else {
            return str_render_cell(grid, c4m_to_utf32(r), cell, width, height);
        }
    }

    case C4M_T_GRID:
        cell->render_cache = c4m_grid_render(cell->raw_item,
                                             c4m_kw("width",
                                                    c4m_ka(width),
                                                    "height",
                                                    c4m_ka(height)));
        return c4m_list_len(cell->render_cache);

    default:
        C4M_CRAISE("Type is not grid-renderable.");
    }

    return 0;
}

static inline void
grid_add_blank_cell(c4m_grid_t *grid,
                    uint16_t    row,
                    uint16_t    col,
                    int16_t     width,
                    int16_t     height)
{
    c4m_utf32_t      *empty = c4m_to_utf32(c4m_empty_string());
    c4m_renderable_t *cell  = c4m_new(c4m_type_renderable(),
                                     c4m_kw("obj",
                                            c4m_ka(empty)));

    c4m_install_renderable(grid, cell, row, row + 1, col, col + 1);
    render_to_cache(grid, cell, width, height);
}

static inline int16_t *
grid_pre_render(c4m_grid_t *grid, int16_t *col_widths)
{
    int16_t            *row_heights = c4m_gc_array_value_alloc(int16_t *,
                                                    grid->num_rows);
    c4m_render_style_t *gs          = grid_style(grid);

    // Run through and tell the individual items to render.
    // For now we tell them all to render to whatever height.
    for (int16_t i = 0; i < grid->num_rows; i++) {
        int16_t row_height = 1;
        int16_t width;
        int16_t cell_height = 0;

        for (int16_t j = 0; j < grid->num_cols; j++) {
            c4m_renderable_t *cell = *c4m_cell_address(grid, i, j);

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
            if (gs->borders & C4M_INTERIOR_VERTICAL) {
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
            c4m_renderable_t *cell = *c4m_cell_address(grid, i, j);

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
            cell->render_cache = pad_lines_vertically(gs,
                                                      cell->render_cache,
                                                      row_height,
                                                      cell->render_width);
        }
    }
    return row_heights;
}

static inline void
grid_add_top_pad(c4m_grid_t *grid, c4m_list_t *lines, int16_t width)
{
    c4m_render_style_t *gs  = grid_style(grid);
    int                 top = gs->top_pad;

    if (!top) {
        return;
    }

    c4m_utf32_t *pad = get_styled_pad(width, c4m_get_pad_style(gs));

    for (int i = 0; i < top; i++) {
        c4m_list_append(lines, pad);
    }
}

static inline void
grid_add_bottom_pad(c4m_grid_t *grid, c4m_list_t *lines, int16_t width)
{
    c4m_render_style_t *gs     = grid_style(grid);
    int                 bottom = gs->bottom_pad;

    if (!bottom) {
        return;
    }

    c4m_utf32_t *pad = get_styled_pad(width, c4m_get_pad_style(gs));

    for (int i = 0; i < bottom; i++) {
        c4m_list_append(lines, pad);
    }
}

static inline int
find_spans(c4m_grid_t *grid, int row, int col)
{
    int result = SPAN_NONE;

    if (col + 1 == grid->num_rows) {
        return result;
    }

    c4m_renderable_t *cell = *c4m_cell_address(grid, row, col + 1);
    if (cell->start_col != col + 1) {
        result |= SPAN_HERE;
    }

    if (row != grid->num_rows - 1) {
        c4m_renderable_t *cell = *c4m_cell_address(grid, row + 1, col + 1);
        if (cell->start_col != col + 1) {
            result |= SPAN_BELOW;
        }
    }

    return result;
}

static inline void
grid_add_top_border(c4m_grid_t *grid, c4m_list_t *lines, int16_t *col_widths)
{
    c4m_render_style_t *gs           = grid_style(grid);
    int32_t             border_width = 0;
    int                 vertical_borders;
    c4m_border_theme_t *draw_chars;
    c4m_utf32_t        *s, *lpad, *rpad;
    c4m_codepoint_t    *p;
    c4m_style_t         pad_color;

    if (!(gs->borders & C4M_BORDER_TOP)) {
        return;
    }

    draw_chars = c4m_get_border_theme(gs);

    for (int i = 0; i < grid->num_cols; i++) {
        border_width += col_widths[i];
    }

    if (gs->borders & C4M_BORDER_LEFT) {
        border_width++;
    }

    if (gs->borders & C4M_BORDER_RIGHT) {
        border_width++;
    }

    vertical_borders = gs->borders & C4M_INTERIOR_VERTICAL;

    if (vertical_borders) {
        border_width += grid->num_cols - 1;
    }

    s = c4m_new(c4m_type_utf32(), c4m_kw("length", c4m_ka(border_width)));
    p = (c4m_codepoint_t *)s->data;

    s->codepoints = border_width;

    if (gs->borders & C4M_BORDER_LEFT) {
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

    if (gs->borders & C4M_BORDER_RIGHT) {
        *p++ = draw_chars->upper_right;
    }

    c4m_str_set_style(s, c4m_str_style(gs));

    pad_color = c4m_get_pad_style(gs);
    lpad      = get_styled_pad(gs->left_pad, pad_color);
    rpad      = get_styled_pad(gs->right_pad, pad_color);

    c4m_list_append(lines, c4m_str_concat(c4m_str_concat(lpad, s), rpad));
}

static inline void
grid_add_bottom_border(c4m_grid_t *grid,
                       c4m_list_t *lines,
                       int16_t    *col_widths)
{
    c4m_render_style_t *gs           = grid_style(grid);
    int32_t             border_width = 0;
    int                 vertical_borders;
    c4m_border_theme_t *draw_chars;
    c4m_utf32_t        *s, *lpad, *rpad;
    c4m_codepoint_t    *p;
    c4m_style_t         pad_color;

    if (!(gs->borders & C4M_BORDER_BOTTOM)) {
        return;
    }

    draw_chars = c4m_get_border_theme(gs);

    for (int i = 0; i < grid->num_cols; i++) {
        border_width += col_widths[i];
    }

    if (gs->borders & C4M_BORDER_LEFT) {
        border_width++;
    }

    if (gs->borders & C4M_BORDER_RIGHT) {
        border_width++;
    }

    vertical_borders = gs->borders & C4M_INTERIOR_VERTICAL;

    if (vertical_borders) {
        border_width += grid->num_cols - 1;
    }

    s = c4m_new(c4m_type_utf32(), c4m_kw("length", c4m_ka(border_width)));
    p = (c4m_codepoint_t *)s->data;

    s->codepoints = border_width;

    if (gs->borders & C4M_BORDER_LEFT) {
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

    if (gs->borders & C4M_BORDER_RIGHT) {
        *p++ = draw_chars->lower_right;
    }

    c4m_str_set_style(s, c4m_str_style(gs));

    pad_color = c4m_get_pad_style(gs);
    lpad      = get_styled_pad(gs->left_pad, pad_color);
    rpad      = get_styled_pad(gs->right_pad, pad_color);

    c4m_list_append(lines, c4m_str_concat(c4m_str_concat(lpad, s), rpad));
}

static inline void
grid_add_horizontal_rule(c4m_grid_t *grid,
                         int         row,
                         c4m_list_t *lines,
                         int16_t    *col_widths)
{
    c4m_render_style_t *gs           = grid_style(grid);
    int32_t             border_width = 0;
    int                 vertical_borders;
    c4m_border_theme_t *draw_chars;
    c4m_utf32_t        *s, *lpad, *rpad;
    c4m_codepoint_t    *p;
    c4m_style_t         pad_color;

    if (!(gs->borders & C4M_INTERIOR_HORIZONTAL)) {
        return;
    }

    draw_chars = c4m_get_border_theme(gs);

    for (int i = 0; i < grid->num_cols; i++) {
        border_width += col_widths[i];
    }

    if (gs->borders & C4M_BORDER_LEFT) {
        border_width++;
    }

    if (gs->borders & C4M_BORDER_RIGHT) {
        border_width++;
    }

    vertical_borders = gs->borders & C4M_INTERIOR_VERTICAL;

    if (vertical_borders) {
        border_width += grid->num_cols - 1;
    }

    s = c4m_new(c4m_type_utf32(), c4m_kw("length", c4m_ka(border_width)));
    p = (c4m_codepoint_t *)s->data;

    s->codepoints = border_width;

    if (gs->borders & C4M_BORDER_LEFT) {
        *p++ = draw_chars->left_t;
    }

    for (int i = 0; i < grid->num_cols; i++) {
        for (int j = 0; j < col_widths[i]; j++) {
            *p++ = draw_chars->horizontal_rule;
        }

        if (vertical_borders && (i + 1 != grid->num_cols)) {
            switch (find_spans(grid, row, i)) {
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

    if (gs->borders & C4M_BORDER_RIGHT) {
        *p++ = draw_chars->right_t;
    }

    c4m_str_set_style(s, c4m_str_style(gs));

    pad_color = c4m_get_pad_style(gs);
    lpad      = get_styled_pad(gs->left_pad, pad_color);
    rpad      = get_styled_pad(gs->right_pad, pad_color);

    c4m_list_append(lines, c4m_str_concat(c4m_str_concat(lpad, s), rpad));
}

static inline c4m_list_t *
grid_add_left_pad(c4m_grid_t *grid, int height)
{
    c4m_render_style_t *gs   = grid_style(grid);
    c4m_list_t         *res  = c4m_new(c4m_type_list(c4m_type_utf32()),
                              c4m_kw("length", c4m_ka(height)));
    c4m_utf32_t        *lpad = c4m_empty_string();

    if (gs->left_pad > 0) {
        lpad = get_styled_pad(gs->left_pad, c4m_get_pad_style(gs));
    }

    for (int i = 0; i < height; i++) {
        c4m_list_append(res, lpad);
    }

    return res;
}

static inline void
grid_add_right_pad(c4m_grid_t *grid, c4m_list_t *lines)
{
    c4m_render_style_t *gs = grid_style(grid);

    if (gs->right_pad <= 0) {
        return;
    }

    c4m_utf32_t *rpad = get_styled_pad(gs->right_pad, c4m_get_pad_style(gs));

    for (int i = 0; i < c4m_list_len(lines); i++) {
        c4m_utf32_t *s = c4m_to_utf32(c4m_list_get(lines, i, NULL));
        c4m_list_set(lines, i, c4m_str_concat(s, rpad));
    }
}

static inline void
add_vertical_bar(c4m_grid_t      *grid,
                 c4m_list_t      *lines,
                 c4m_border_set_t to_match)
{
    c4m_render_style_t *gs = grid_style(grid);

    if (!(gs->borders & to_match)) {
        return;
    }

    c4m_border_theme_t *border_theme = c4m_get_border_theme(gs);
    c4m_style_t         border_color = c4m_str_style(gs);
    c4m_utf32_t        *bar;

    bar = styled_repeat(border_theme->vertical_rule, 1, border_color);

    for (int i = 0; i < c4m_list_len(lines); i++) {
        c4m_utf32_t *s = c4m_to_utf32(c4m_list_get(lines, i, NULL));
        c4m_list_set(lines, i, c4m_str_concat(s, bar));
    }
}

static inline void
grid_add_left_border(c4m_grid_t *grid, c4m_list_t *lines)
{
    add_vertical_bar(grid, lines, C4M_BORDER_LEFT);
}

static inline void
grid_add_right_border(c4m_grid_t *grid, c4m_list_t *lines)
{
    add_vertical_bar(grid, lines, C4M_BORDER_RIGHT);
}

static inline void
grid_add_vertical_rule(c4m_grid_t *grid, c4m_list_t *lines)
{
    add_vertical_bar(grid, lines, C4M_BORDER_RIGHT);
}

static void
crop_vertically(c4m_grid_t *grid, c4m_list_t *lines, int32_t height)
{
    c4m_render_style_t *gs   = grid_style(grid);
    int32_t             diff = height - c4m_list_len(lines);

    switch (gs->alignment & C4M_VERTICAL_MASK) {
    case C4M_ALIGN_BOTTOM:
        for (int i = 0; i < height; i++) {
            c4m_list_set(lines, i, c4m_list_get(lines, i + diff, NULL));
        }
        break;
    case C4M_ALIGN_MIDDLE:
        for (int i = 0; i < height; i++) {
            c4m_list_set(lines,
                         i,
                         c4m_list_get(lines, i + (diff >> 1), NULL));
        }
        break;
    default:
        break;
    }

    lines->length = height;
}

static inline c4m_utf32_t *
align_and_crop_grid_line(c4m_grid_t *grid, c4m_utf32_t *line, int32_t width)
{
    c4m_render_style_t *gs        = grid_style(grid);
    c4m_alignment_t     align     = gs->alignment;
    c4m_style_t         pad_style = c4m_get_pad_style(gs);

    // Called on one grid line if we need to align or crop it.
    int32_t      diff = width - c4m_str_render_len(line);
    c4m_utf32_t *pad;

    if (diff > 0) {
        // We need to pad. Here, we use the alignment info.
        switch (align & C4M_HORIZONTAL_MASK) {
        case C4M_ALIGN_RIGHT:
            pad = get_styled_pad(diff, pad_style);
            return c4m_str_concat(pad, line);
        case C4M_ALIGN_CENTER: {
            pad  = get_styled_pad(diff / 2, pad_style);
            line = c4m_str_concat(pad, line);
            if (diff % 2 != 0) {
                pad = get_styled_pad(1 + diff / 2, pad_style);
            }
            return c4m_str_concat(line, pad);
        }
        default:
            pad = get_styled_pad(diff, pad_style);
            return c4m_str_concat(line, pad);
        }
    }
    else {
        // We need to crop. For now, we ONLY crop from the right.
        return c4m_str_truncate(line,
                                (int64_t)width,
                                c4m_kw("use_render_width",
                                       c4m_ka(1)));
    }
}

static c4m_list_t *
align_and_crop_grid(c4m_grid_t *grid,
                    c4m_list_t *lines,
                    int32_t     width,
                    int32_t     height)
{
    int num_lines = c4m_list_len(lines);

    // For now, width must always be set. Won't be true for height.

    for (int i = 0; i < num_lines; i++) {
        c4m_utf32_t *s = c4m_to_utf32(c4m_list_get(lines, i, NULL));
        if (c4m_str_render_len(s) == width) {
            continue;
        }

        c4m_utf32_t *l = align_and_crop_grid_line(grid, s, width);
        c4m_list_set(lines, i, l);
    }

    if (height != -1) {
        if (num_lines > height) {
            crop_vertically(grid, lines, height);
        }
        else {
            if (num_lines < height) {
                lines = pad_lines_vertically(grid_style(grid),
                                             lines,
                                             height,
                                             width);
            }
        }
    }

    return lines;
}

static inline bool
grid_add_cell_contents(c4m_grid_t *grid,
                       c4m_list_t *lines,
                       uint16_t    r,
                       uint16_t    c,
                       int16_t    *col_widths,
                       int16_t    *row_heights)
{
    // This is the one that fills a single cell.  Returns true if the
    // caller should render vertical interior borders (if wanted). The
    // caller will be on its own in figuring out borders for spans
    // though.

    c4m_renderable_t *cell = *c4m_cell_address(grid, r, c);
    int               i;

    if (cell->end_col - cell->start_col == 1 && cell->end_row - cell->start_row == 1) {
        for (i = 0; i < c4m_list_len(lines); i++) {
            c4m_utf32_t *s     = c4m_to_utf32(c4m_list_get(lines, i, NULL));
            c4m_utf32_t *piece = c4m_to_utf32(c4m_list_get(cell->render_cache,
                                                           i,
                                                           NULL));
            if (!c4m_str_codepoint_len(piece)) {
                c4m_style_t pad_style = c4m_get_pad_style(grid_style(grid));
                piece                 = get_styled_pad(col_widths[i],
                                       pad_style);
            }
            c4m_list_set(lines, i, c4m_str_concat(s, piece));
        }
        return true;
    }

    // For spans, just return the one block of the grid, along with
    // any interior borders.
    uint16_t            row_offset   = r - cell->start_row;
    uint16_t            col_offset   = c - cell->start_col;
    int                 start_width  = 0;
    int                 start_height = 0;
    c4m_render_style_t *gs           = grid_style(grid);

    if (gs->borders & C4M_INTERIOR_VERTICAL) {
        start_width += col_offset;
    }

    if (gs->borders & C4M_INTERIOR_HORIZONTAL) {
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

    if ((gs->borders & C4M_INTERIOR_HORIZONTAL) && r + 1 != cell->end_row) {
        num_rows += 1;
    }

    if ((gs->borders & C4M_INTERIOR_VERTICAL) && r + 1 != cell->end_col) {
        num_cols += 1;
    }
    for (i = row_offset; i < row_offset + num_rows; i++) {
        c4m_utf32_t *s     = c4m_to_utf32(c4m_list_get(lines, i, NULL));
        c4m_utf32_t *piece = c4m_to_utf32(c4m_list_get(cell->render_cache,
                                                       i,
                                                       NULL));

        piece             = c4m_str_slice(piece,
                              start_width,
                              start_width + num_cols);
        c4m_utf32_t *line = c4m_str_concat(s, piece);
        c4m_list_set(lines, i, line);
    }

    // This silences a warning... I know I'm not using start_height
    // yet, but the compiler won't shut up about it! So here, I'm
    // using it now, are you happy???
    return ((c + 1) ^ start_height) == ((cell->end_col) ^ start_height);
}

c4m_list_t *
_c4m_grid_render(c4m_grid_t *grid, ...)
{
    // There's a lot of work in here, so I'm keeping the high-level
    // algorithm in this function as simple as possible.  Note that we
    // currently build up one big output string, but I'd also like to
    // have a slight variant that writes to a FILE *.
    //
    // A single streaming implementation doesn't really work, since
    // when writing to a FILE *, we would render the ansi codes as we
    // go.

    int64_t width  = -1;
    int64_t height = -1;

    c4m_karg_only_init(grid);
    c4m_kw_int64("width", width);
    c4m_kw_int64("height", height);

    if (width == -1) {
        width = c4m_terminal_width();
        width = c4m_max(width, 20);
    }

    if (width == 0) {
        return c4m_new(c4m_type_list(c4m_type_utf32()),
                       c4m_kw("length", c4m_ka(0)));
    }

    int16_t *col_widths  = calculate_col_widths(grid, width, &grid->width);
    int16_t *row_heights = grid_pre_render(grid, col_widths);

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

    c4m_render_style_t *gs      = grid_style(grid);
    uint16_t            h_alloc = grid->num_rows + 1 + gs->top_pad + gs->bottom_pad;

    for (int i = 0; i < grid->num_rows; i++) {
        h_alloc += row_heights[i];
    }

    c4m_list_t *result = c4m_new(c4m_type_list(c4m_type_utf32()),
                                 c4m_kw("length", c4m_ka(h_alloc)));

    grid_add_top_pad(grid, result, width);
    grid_add_top_border(grid, result, col_widths);

    for (int i = 0; i < grid->num_rows; i++) {
        c4m_list_t *row = grid_add_left_pad(grid, row_heights[i]);
        grid_add_left_border(grid, row);

        for (int j = 0; j < grid->num_cols; j++) {
            bool vertical_ok = grid_add_cell_contents(grid,
                                                      row,
                                                      i,
                                                      j,
                                                      col_widths,
                                                      row_heights);

            if (vertical_ok && (j + 1 < grid->num_cols)) {
                grid_add_vertical_rule(grid, row);
            }
        }

        grid_add_right_border(grid, row);
        grid_add_right_pad(grid, row);

        c4m_list_plus_eq(result, row);

        if (i + 1 < grid->num_rows) {
            grid_add_horizontal_rule(grid, i, result, col_widths);
        }
    }

    grid_add_bottom_border(grid, result, col_widths);
    grid_add_bottom_pad(grid, result, width);

    return align_and_crop_grid(grid, result, width, height);
}

c4m_utf32_t *
c4m_grid_to_str(c4m_grid_t *g)
{
    c4m_list_t *l = c4m_grid_render(g);
    // join will force utf32 on the newline.
    return c4m_str_join(l,
                        c4m_str_newline(),
                        c4m_kw("add_trailing", c4m_ka(true)));
}

c4m_grid_t *
_c4m_ordered_list(c4m_list_t *items, ...)
{
    char *bullet_style = "bullet";
    char *item_style   = "li";

    c4m_karg_only_init(items);
    c4m_kw_ptr("bullet_style", bullet_style);
    c4m_kw_ptr("item_style", item_style);

    items = c4m_list_copy(items);

    int64_t      n   = c4m_list_len(items);
    c4m_utf32_t *dot = c4m_utf32_repeat('.', 1);
    c4m_grid_t  *res = c4m_new(c4m_type_grid(),
                              c4m_kw("start_rows",
                                     c4m_ka(n),
                                     "start_cols",
                                     c4m_ka(2),
                                     "container_tag",
                                     c4m_ka("ol")));

    c4m_render_style_t *bp    = c4m_lookup_cell_style(bullet_style);
    float               log   = log10((float)n);
    int                 width = (int)(log + .5) + 1 + 1;
    // Above, one + 1 is because log returns one less than what we
    // need for the int with, and the other is for the period /
    // bullet.

    width += bp->left_pad + bp->right_pad;
    bp->dims.units = width;

    c4m_set_column_props(res, 0, bp);
    c4m_set_column_style(res, 1, item_style);

    for (int i = 0; i < n; i++) {
        c4m_utf32_t      *s         = c4m_str_concat(c4m_str_from_int(i + 1),
                                        dot);
        c4m_utf32_t      *list_item = c4m_to_utf32(c4m_list_get(items,
                                                           i,
                                                           NULL));
        c4m_renderable_t *li        = c4m_new(c4m_type_renderable(),
                                       c4m_kw("obj",
                                              c4m_ka(list_item),
                                              "tag",
                                              c4m_ka(item_style)));
        c4m_grid_set_cell_contents(res,
                                   i,
                                   0,
                                   c4m_to_str_renderable(s, bullet_style));
        c4m_grid_set_cell_contents(res, i, 1, li);
    }
    return res;
}

c4m_grid_t *
_c4m_unordered_list(c4m_list_t *items, ...)
{
    char           *bullet_style = "bullet";
    char           *item_style   = "li";
    c4m_codepoint_t bullet       = 0x2022;

    c4m_karg_only_init(items);
    c4m_kw_ptr("bullet_style", bullet_style);
    c4m_kw_ptr("item_style", item_style);
    c4m_kw_codepoint("bullet", bullet);

    items = c4m_list_copy(items);

    int64_t      n        = c4m_list_len(items);
    c4m_grid_t  *res      = c4m_new(c4m_type_grid(),
                              c4m_kw("start_rows",
                                     c4m_ka(n),
                                     "start_cols",
                                     c4m_ka(2),
                                     "container_tag",
                                     c4m_ka("ul")));
    c4m_utf32_t *bull_str = c4m_utf32_repeat(bullet, 1);

    c4m_render_style_t *bp = c4m_lookup_cell_style(bullet_style);
    bp->dims.units += bp->left_pad + bp->right_pad;

    c4m_set_column_props(res, 0, bp);
    c4m_set_column_style(res, 1, item_style);

    for (int i = 0; i < n; i++) {
        c4m_utf32_t      *list_item = c4m_to_utf32(c4m_list_get(items,
                                                           i,
                                                           NULL));
        c4m_renderable_t *li        = c4m_new(c4m_type_renderable(),
                                       c4m_kw("obj",
                                              c4m_ka(list_item),
                                              "tag",
                                              c4m_ka(item_style)));

        c4m_grid_set_cell_contents(res,
                                   i,
                                   0,
                                   c4m_to_str_renderable(bull_str,
                                                         bullet_style));
        c4m_grid_set_cell_contents(res, i, 1, li);
    }

    return res;
}

c4m_grid_t *
c4m_grid_flow(uint64_t items, ...)
{
    va_list contents;

    c4m_grid_t *res = c4m_new(c4m_type_grid(),
                              c4m_kw("start_rows",
                                     c4m_ka(items),
                                     "start_cols",
                                     c4m_ka(1),
                                     "container_tag",
                                     c4m_ka("flow")));

    va_start(contents, items);
    for (uint64_t i = 0; i < items; i++) {
        c4m_grid_set_cell_contents(res,
                                   i,
                                   0,
                                   (c4m_obj_t)va_arg(contents, c4m_obj_t));
    }
    va_end(contents);

    return res;
}

c4m_grid_t *
c4m_grid_horizontal_flow(c4m_list_t *items,
                         uint64_t    max_columns,
                         uint64_t    total_width,
                         char       *table_style,
                         char       *cell_style)
{
    uint64_t list_len   = c4m_list_len(items);
    uint64_t start_cols = c4m_min(list_len, max_columns);
    uint64_t start_rows = (list_len + start_cols - 1) / start_cols;

    if (table_style == NULL) {
        table_style = "flow";
    }

    if (cell_style == NULL) {
        cell_style = "td";
    }

    c4m_grid_t *res = c4m_new(c4m_type_grid(),
                              c4m_kw("start_rows",
                                     c4m_ka(start_rows),
                                     "start_cols",
                                     c4m_ka(start_cols),
                                     "container_tag",
                                     c4m_ka(table_style),
                                     "td_tag",
                                     c4m_ka(cell_style)));

    for (uint64_t i = 0; i < list_len; i++) {
        int row = i / start_cols;
        int col = i % start_cols;

        c4m_grid_set_cell_contents(res,
                                   row,
                                   col,
                                   c4m_list_get(items, i, NULL));
    }

    return res;
}

static void
c4m_grid_marshal(c4m_grid_t   *grid,
                 c4m_stream_t *s,
                 c4m_dict_t   *memos,
                 int64_t      *mid)
{
    int num_cells = grid->num_rows * grid->num_cols;

    c4m_marshal_u16(grid->num_cols, s);
    c4m_marshal_u16(grid->num_rows, s);
    c4m_marshal_u16(grid->spare_rows, s);
    c4m_marshal_i16(grid->width, s);
    c4m_marshal_i16(grid->height, s);
    c4m_marshal_u16(grid->col_cursor, s);
    c4m_marshal_u16(grid->row_cursor, s);
    c4m_marshal_i8(grid->header_cols, s);
    c4m_marshal_i8(grid->header_rows, s);
    c4m_marshal_i8(grid->stripe, s);
    c4m_marshal_cstring(grid->td_tag_name, s);
    c4m_marshal_cstring(grid->th_tag_name, s);

    c4m_sub_marshal(grid->col_props, s, memos, mid);
    c4m_sub_marshal(grid->row_props, s, memos, mid);

    for (int i = 0; i < num_cells; i++) {
        c4m_sub_marshal((c4m_renderable_t *)grid->cells[i], s, memos, mid);
    }

    c4m_sub_marshal(grid->self, s, memos, mid);
}

static void
c4m_grid_unmarshal(c4m_grid_t *grid, c4m_stream_t *s, c4m_dict_t *memos)
{
    grid->num_cols    = c4m_unmarshal_u16(s);
    grid->num_rows    = c4m_unmarshal_u16(s);
    grid->spare_rows  = c4m_unmarshal_u16(s);
    grid->width       = c4m_unmarshal_i16(s);
    grid->height      = c4m_unmarshal_i16(s);
    grid->col_cursor  = c4m_unmarshal_u16(s);
    grid->row_cursor  = c4m_unmarshal_u16(s);
    grid->header_cols = c4m_unmarshal_i8(s);
    grid->header_rows = c4m_unmarshal_i8(s);
    grid->stripe      = c4m_unmarshal_i8(s);
    grid->td_tag_name = c4m_unmarshal_cstring(s);
    grid->th_tag_name = c4m_unmarshal_cstring(s);
    grid->col_props   = c4m_sub_unmarshal(s, memos);
    grid->row_props   = c4m_sub_unmarshal(s, memos);

    size_t num_cells = (grid->num_rows + grid->spare_rows) * grid->num_cols;
    grid->cells      = c4m_gc_array_alloc(c4m_renderable_t *, num_cells);

    num_cells = grid->num_rows * grid->num_cols;

    for (size_t i = 0; i < num_cells; i++) {
        grid->cells[i] = c4m_sub_unmarshal(s, memos);
    }

    grid->self = c4m_sub_unmarshal(s, memos);
}

static void
c4m_renderable_marshal(c4m_renderable_t *r,
                       c4m_stream_t     *s,
                       c4m_dict_t       *memos,
                       int64_t          *mid)
{
    c4m_sub_marshal(r->raw_item, s, memos, mid);
    c4m_marshal_cstring(r->container_tag, s);
    c4m_sub_marshal(r->current_style, s, memos, mid);
    c4m_marshal_u16(r->start_col, s);
    c4m_marshal_u16(r->start_row, s);
    c4m_marshal_u16(r->end_col, s);
    c4m_marshal_u16(r->end_row, s);
    // We 100% skip the render cache.
    c4m_marshal_u16(r->render_width, s);
    c4m_marshal_u16(r->render_height, s);
}

static void
c4m_renderable_unmarshal(c4m_renderable_t *r,
                         c4m_stream_t     *s,
                         c4m_dict_t       *memos)
{
    r->raw_item      = c4m_sub_unmarshal(s, memos);
    r->container_tag = c4m_unmarshal_cstring(s);
    r->current_style = c4m_sub_unmarshal(s, memos);
    r->start_col     = c4m_unmarshal_u16(s);
    r->start_row     = c4m_unmarshal_u16(s);
    r->end_col       = c4m_unmarshal_u16(s);
    r->end_row       = c4m_unmarshal_u16(s);
    r->render_width  = c4m_unmarshal_u16(s);
    r->render_height = c4m_unmarshal_u16(s);
}

// For instantiating w/o varargs.
c4m_grid_t *
c4m_grid(int32_t start_rows,
         int32_t start_cols,
         char   *table_tag,
         char   *th_tag,
         char   *td_tag,
         int     header_rows,
         int     header_cols,
         int     s)
{
    return c4m_new(c4m_type_grid(),
                   c4m_kw("start_rows",
                          c4m_ka(start_rows),
                          "start_cols",
                          c4m_ka(start_cols),
                          "container_tag",
                          c4m_ka(table_tag),
                          "th_tag",
                          c4m_ka(th_tag),
                          "td_tag",
                          c4m_ka(td_tag),
                          "header_rows",
                          c4m_ka(header_rows),
                          "header_cols",
                          c4m_ka(header_cols),
                          "stripe",
                          c4m_ka(s)));
}

typedef struct {
    c4m_codepoint_t  pad;
    c4m_codepoint_t  tchar;
    c4m_codepoint_t  lchar;
    c4m_codepoint_t  hchar;
    c4m_codepoint_t  vchar;
    int              vpad;
    int              ipad;
    int              no_nl;
    c4m_style_t      style;
    char            *tag;
    c4m_codepoint_t *padstr;
    int              pad_ix;
    c4m_grid_t      *grid;
    c4m_utf8_t      *nl;
    bool             root;
} tree_fmt_t;

static void
build_tree_output(c4m_tree_node_t *node, tree_fmt_t *info, bool last)
{
    if (node == NULL) {
        return;
    }

    c4m_str_t       *line = c4m_tree_get_contents(node);
    int              i;
    c4m_codepoint_t *prev_pad = info->padstr;
    int              last_len = info->pad_ix;

    if (line != NULL) {
        if (info->no_nl) {
            int64_t ix = c4m_str_find(line, info->nl);

            if (ix != -1) {
                line = c4m_str_slice(line, 0, ix);
                line = c4m_str_concat(line, c4m_utf32_repeat(0x2026, 1));
            }
        }
    }
    else {
        line = c4m_utf32_repeat(0x2026, 1);
    }

    if (!info->root) {
        info->pad_ix += info->vpad + info->ipad + 1;
        info->padstr = c4m_gc_array_value_alloc(c4m_codepoint_t, info->pad_ix);
        for (i = 0; i < last_len; i++) {
            if (prev_pad[i] == info->tchar || prev_pad[i] == info->vchar) {
                info->padstr[i] = info->vchar;
            }
            else {
                if (prev_pad[i] == info->lchar) {
                    info->padstr[i] = info->vchar;
                }
                else {
                    info->padstr[i] = ' ';
                }
            }
        }

        if (last) {
            info->padstr[i++] = info->lchar;
        }
        else {
            info->padstr[i++] = info->tchar;
        }

        for (int j = 0; j < info->vpad; j++) {
            info->padstr[i++] = info->hchar;
        }

        for (int j = 0; j < info->ipad; j++) {
            info->padstr[i++] = ' ';
        }

        c4m_utf32_t *pad = c4m_new(c4m_type_utf32(),
                                   c4m_kw("length",
                                          c4m_ka(i),
                                          "codepoints",
                                          c4m_ka(info->padstr)));
        c4m_str_set_style(pad, info->style);
        line = c4m_str_concat(pad, line);
    }
    else {
        info->root = false;
    }

    c4m_style_gaps(line, info->style);
    c4m_renderable_t *item = c4m_to_str_renderable(line, info->tag);

    c4m_grid_add_row(info->grid, item);

    int64_t num_kids = c4m_tree_get_number_children(node);

    if (last) {
        assert(info->padstr[last_len] == info->lchar);
        info->padstr[last_len] = 'x';
    }

    int              my_pad_ix = info->pad_ix;
    c4m_codepoint_t *my_pad    = info->padstr;

    for (i = 0; i < num_kids; i++) {
        if (i + 1 == num_kids) {
            build_tree_output(c4m_tree_get_child(node, i), info, true);
        }
        else {
            build_tree_output(c4m_tree_get_child(node, i), info, false);
            info->pad_ix = my_pad_ix;
            info->padstr = my_pad;
        }
    }
}

void
c4m_set_column_props(c4m_grid_t *grid, int col, c4m_render_style_t *s)
{
    if (grid->col_props == NULL) {
        grid->col_props = c4m_new(c4m_type_dict(c4m_type_int(),
                                                c4m_type_ref()));
    }

    hatrack_dict_put(grid->col_props, (void *)(int64_t)col, s);
}

void
c4m_set_row_props(c4m_grid_t *grid, int row, c4m_render_style_t *s)
{
    if (grid->row_props == NULL) {
        grid->row_props = c4m_new(c4m_type_dict(c4m_type_int(),
                                                c4m_type_ref()));
    }

    hatrack_dict_put(grid->row_props, (void *)(int64_t)row, s);
}

void
c4m_set_column_style(c4m_grid_t *grid, int col, char *tag)
{
    c4m_render_style_t *style = c4m_lookup_cell_style(tag);

    if (!style) {
        C4M_CRAISE("Style not found.");
    }

    c4m_set_column_props(grid, col, style);
}

void
c4m_set_row_style(c4m_grid_t *grid, int row, char *tag)
{
    c4m_render_style_t *style = c4m_lookup_cell_style(tag);

    if (!style) {
        C4M_CRAISE("Style not found.");
    }

    c4m_set_row_props(grid, row, style);
}

// This currently expects a tree[utf8] or tree[utf32].  Eventually
// maybe would make it handle anything via it's repr.  However, it
// should also be restructured to be a single renderable item itself,
// so that it can be responsive when we want to add items, once we get
// more GUI-oriented.
//
// This is the quick-and-dirty implementation to replace the trees
// I currently have in NIM for c4m debugging, etc.

c4m_grid_t *
_c4m_grid_tree(c4m_tree_node_t *tree, ...)
{
    c4m_codepoint_t pad       = ' ';
    c4m_codepoint_t tchar     = 0x251c;
    c4m_codepoint_t lchar     = 0x2514;
    c4m_codepoint_t hchar     = 0x2500;
    c4m_codepoint_t vchar     = 0x2502;
    int32_t         vpad      = 2;
    int32_t         ipad      = 1;
    bool            no_nl     = true;
    char           *tag       = "tree_item";
    void           *converter = NULL;

    c4m_karg_only_init(tree);
    c4m_kw_codepoint("pad", pad);
    c4m_kw_codepoint("t_char", tchar);
    c4m_kw_codepoint("l_char", lchar);
    c4m_kw_codepoint("h_char", hchar);
    c4m_kw_codepoint("v_char", vchar);
    c4m_kw_int32("vpad", vpad);
    c4m_kw_int32("ipad", ipad);
    c4m_kw_bool("truncate_at_newline", no_nl);
    c4m_kw_ptr("style_tag", tag);
    c4m_kw_ptr("converter", converter);

    if (converter != NULL) {
        tree = c4m_tree_str_transform(tree,
                                      (c4m_str_t * (*)(void *)) converter);
    }

    if (vpad < 1) {
        vpad = 1;
    }
    if (ipad < 0) {
        ipad = 1;
    }

    c4m_grid_t *result = c4m_new(c4m_type_grid(),
                                 c4m_kw("container_tag",
                                        c4m_ka("flow"),
                                        "td_tag",
                                        c4m_ka(tag)));

    tree_fmt_t fmt_info = {
        .pad    = pad,
        .tchar  = tchar,
        .lchar  = lchar,
        .hchar  = hchar,
        .vchar  = vchar,
        .vpad   = vpad,
        .ipad   = ipad,
        .no_nl  = no_nl,
        .style  = c4m_str_style(c4m_lookup_cell_style(tag)),
        .tag    = tag,
        .pad_ix = 0,
        .grid   = result,
        .nl     = c4m_utf8_repeat('\n', 1),
        .root   = true,
    };

    build_tree_output(tree, &fmt_info, false);

    return result;
}

static void
c4m_grid_set_gc_bits(uint64_t *bitfield, int alloc_words)
{
    int ix;

    c4m_set_object_header_bits(bitfield, &ix);
    // First 6 bits of the grid are pointers.
    *bitfield |= (0x3f << ix);
}

static void
c4m_renderable_set_gc_bits(uint64_t *bitfield, int alloc_words)
{
    int ix;

    c4m_set_object_header_bits(bitfield, &ix);
    // First 4 words of the renderable are pointers.
    *bitfield |= (0x0f << ix);
}

const c4m_vtable_t c4m_grid_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)grid_init,
        [C4M_BI_TO_STR]      = (c4m_vtable_entry)c4m_grid_to_str,
        [C4M_BI_MARSHAL]     = (c4m_vtable_entry)c4m_grid_marshal,
        [C4M_BI_UNMARSHAL]   = (c4m_vtable_entry)c4m_grid_unmarshal,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)c4m_grid_set_gc_bits,
        // Explicit because some compilers don't seem to always properly
        // zero it (Was sometimes crashing on a `c4m_stream_t` on my mac).
        [C4M_BI_FINALIZER]   = NULL,
    },
};

const c4m_vtable_t c4m_renderable_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)renderable_init,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)c4m_renderable_set_gc_bits,
        [C4M_BI_MARSHAL]     = (c4m_vtable_entry)c4m_renderable_marshal,
        [C4M_BI_UNMARSHAL]   = (c4m_vtable_entry)c4m_renderable_unmarshal,
        [C4M_BI_FINALIZER]   = NULL,
    },
};

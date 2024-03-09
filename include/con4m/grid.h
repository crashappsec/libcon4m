#pragma once

/** Grid Layout objects.

 * For our initial implementation, we are going to keep it as simple
 * as possible, and then go from there. Each grid is a collection of n
 * x m cells that form a rectangle.
 *
 * The individual rows and columns in a single grid must all be of
 * uniform size.
 *
 * Each cell can contain:
 *
 * 1) Another grid.
 * 2) Simple text.
 * 3) A 'Flow' layout object.
 * 4) More later? (notcurses content?)
 *
 * Grids contained inside other grids can span cells, but they must
 * be rectangular.
 *
 * An individual grid has the following properties:
 *
 * - Rows and columns.
 * - Widths (per column) -- see below
 * - Heights (per row)
 * - L/R and top/bottom alignment of contents.
 * - outer pad (left, right, top, bottom) -- a measure of how much space to
 *   put around the outside of the grid.
 * - inner pad -- the *default* padding for any interior cells. This is applied
 *   automatically to text, and is applied to inner grids if they do not
 *   specify an explicit outer pad.
 * - Whether borders should be drawn outside the box.
 * - Whether interior lines that don't go through a sub-grid should be used.
 *   If not, they are not counted when making room for cells.
 * - The border style.
 * - FG and BG color for borders.
 * - Default FG and BG for contents (can be overridden by cells).
 * - Default alignment for inner contents.
 * - Some kind of "hide-if-not-fully-renderable" property.
 *
 * All widths and hights can be specified with:
 *
 * a) % of total render width. (like % in html)
 * b) Absolute number of characters (like px in html)
 * c) Flex-units (like fr in CSS Grid layout)
 * d) Auto (the system divides space between all auto-items evenly).
 *    for heights, the default for rows is to give an arbitrary amount
 *    of space for a row to render, but then will ask each cell to
 *    render to that one height.
 * e) Semi-automatic-- you can specify a minimum, maximum or both,
 *    _in absolute sizes only_.
 *
 * Note that all absolute widths are measured based on `characters`;
 * this assumes we are using a terminal with a fixed width font, where
 * the character is essentially an indivisible unit.
 *
 * When we consider the overall grid dimensions, we mean the amount of
 * space we have available for rendering the grid. Most commonly, if
 * we call 'print' on the grid, we generally will want to render with
 * a width equal to the current terminal width, and an unbounded
 * length (relying on the terminal or a pager to scroll up / down).
 *
 * However, we could also render into a plane with a scroll bar around
 * it, in which case we might not show the whole table at once.
 *
 * Still, the overall table dimensions are used when computing the
 * dimensions or its cells. Currently, each column and each row may be
 * independently sized, but not individual cells (e.g., you cannot
 * currently do a 'mason' layout (a la pintrest).
 *
 * Given the constraints provided for rows or columns, it can
 * certainly be the case where there is not enough available table
 * space, and rows / columns may be assigned absolute widths of 0, in
 * which case they will not be rendered.
 *
 * The grid is set up to facilitate re-rendering. Each item in the
 * grid is expected to cache a rendering in its current dimensions;
 * when the grid is asked to render, it asks each item that will get
 * rendered to hand it a pre-rendering, which will usually be cached.
 *
 * If a grid's size has changed (or if any other property that affects
 * rendering has changed), it will send a re-render event to cells.
 *
 * Items can, of course, changes what they contain in between grid
 * renderings.
 *
 * When the grid asks cells for their rendering, the expectations are:
 *
 * 1) The item provides an array of `str_t *` objects, one for each
 *    row of the requested height (with no newline type characters).
 *
 * 2) Each item in that array is padded (generally with spaces, but
 *    with whatever 'pad' is set) to the requested width.
 *
 * 3) All alignment and coloring rules are respected in what is passed
 *    back. That means the str_t *'s styling info will be set
 *    appropriately, and that padd will be added based on any
 *    alignment properties.
 *
 * 4) To be clear, the returned array of str_t's returned will contain
 *    the number of rows asked for, even if some rows are 100% pad.
 *
 * Of course, for any property beyond render dimensions, the cell's
 * contents can override.
 *
 * See the 'renderable' abstraction, which allows the programmer to
 * specify what SHOULD override. For instance, consider the behavior
 * we might want when putting pre-formatted text into a cell. If the
 * grid represents a table, we probably want the grid's notion of what
 * the background color should be to override any formatting within
 * the text. But, we might want things like italics, and possibly
 * foreground color, to be able to change.
 *
 * Currently, when the grid asks cells to render, it sends style
 * information to that cell. When rendering, the renderbox merges
 * styles, based on the override.
 *
 * This abstraction is built to supporting dynamic re-rendering, for
 * instance, as a window is resized.
 *
 * Currently, the render box's cache is expected to only be updated
 * when rendering occurs, though. The state accessed in doing the
 * rendering should be built in a way that it can be updated from
 * multiple threads if useful, but if so, that should be done in a way
 * so as not to interfere with the rendering.
 */
#include <con4m.h>

typedef struct grid_t grid_t;

typedef enum : int8_t {
    ALIGN_UNSPECED      = -1,
    ALIGN_INHERIT       = 0,
    ALIGN_TOP_LEFT      = 1,
    ALIGN_TOP_CENTER    = 2,
    ALIGN_TOP_RIGHT     = 3,
    ALIGN_MID_LEFT      = 4,
    ALIGN_MID_CENTER    = 5,
    ALIGN_MID_RIGHT     = 6,
    ALIGN_BOTTOM_LEFT   = 7,
    ALIGN_BOTTOM_CENTER = 8,
    ALIGN_BOTTOM_RIGHT  = 9
} alignment_t;

typedef struct {
    int8_t left;
    int8_t right;
    int8_t top;
    int8_t bottom;
} padspec_t;

typedef enum : uint8_t {
    DIM_AUTO,
    DIM_PERCENT_ROUND_DOWN,
    DIM_PERCENT_ROUND_UP,
    DIM_FLEX_UNITS,
    DIM_ABSOLUTE,
    DIM_ABSOLUTE_RANGE,
} dimspec_kind_t;

typedef struct {
    dimspec_kind_t kind;
    union {
	float    percent;   // for dim_percent*
	uint64_t units;     // Used for flex or absolute.
	int32_t  range[2];  // -1 for unspecified.
    } diminfo;
} dimspec_t;

typedef struct border_style_t {
    char                  *name;
    int32_t                horizontal_rule;
    int32_t                vertical_rule;
    int32_t                upper_left;
    int32_t                upper_right;
    int32_t                lower_left;
    int32_t                lower_right;
    int32_t                cross;
    int32_t                top_t;
    int32_t                bottom_t;
    int32_t                left_t;
    int32_t                right_t;
    struct border_style_t *next_style;
} border_style_t;

#define BORDER_TOP          0x01
#define BORDER_BOTTOM       0x02
#define BORDER_LEFT         0x04
#define BORDER_RIGHT        0x08
#define INTERIOR_HORIZONTAL 0x10
#define INTERIOR_VERTICAL   0x20

typedef uint8_t border_set_t;
// The outer grid draws borders assembling cells, and does not draw
// borders if they would go THROUGH a cell; that is left to the
// contained cell, if it's done at all.
//
// However, if per-row/col colors have been set, then the current row
// color will override interior vertical lines, and the current column
// color will override interior horizontal lines by default. This
// allows one to shade header rows or columns / etc.
//
// For the border color, we use a style_t, where we only use the
// color bits from style.h: FG_COLOR_ON and BG_COLOR_ON get used, and
// the bottom 48 bits are color.
//
// When used in the context of a column color, we might add a bit
// specific to this type to *not* to the above override, but for now
// it always happens if the row/column provides color info.
//
// The text_style_override field is used to indicate what a cell is
// ALLOWED to override. The bitfield is the only thing checked, and
// anything bit set is allowed to be overridden. By default, colors
// are disallowed, but no other text styling.
//
// This is what can be set on a per-row or per-column basis. Indivudal
// cells can either get their own, or inherit from an outer container.
// The exception is 'text_style_override' gets merged with any style.
// Note that when applying these across a table, they copy data in
// if it has
//
// 'Wrap' may seem like it applies to text; but wrap requires the
// notion of a container, just like alignment does. Here, the value
// should be -2 for 'inherit' (i.e., no specific override) -1 for NO
// wrap, meaning the text will be asked to be rendered for whatever
// the maximum width of a line is, and then will be CROPPED to the
// render width.
//
// Otherwise, the value is interpreted as how much 'hang' to leave
// on subsequent lines.

typedef struct {
    style_t         color;
    padspec_t       pad;
    alignment_t     alignment;
    dimspec_t       dimensions;
    int8_t          wrap;
    style_t         text_style_override;
} row_or_col_props_t;

typedef row_or_col_props_t row_props_t;
typedef row_or_col_props_t col_props_t;

typedef struct {
    row_props_t     *row_props;
    col_props_t     *col_props;
    object_t        *raw_item; // Currently, must be a grid_t * or str_t *.
    str_t          **render_cache;
    uint16_t         start_col;
    uint16_t         render_width;
    uint16_t         start_row;
    uint16_t         render_height;
    style_t          text_style_override;
} renderable_t;

struct grid_t {
    renderable_t     ***cells; // A 2d array of renderable_objects, by ref
    row_props_t        *default_row_properties;
    col_props_t        *default_col_properties;
    border_style_t     *border_style;
    style_t             border_color;
    padspec_t           outer_pad;
    uint16_t            num_rows;
    uint16_t            num_cols;
    uint16_t            spare_rows;
    border_set_t        enabled_borders;
};

// For some incomprehensible reason this needs fw referencing here.
// I think it's the build system's fault?
typedef struct flexarray_t flexarray_t;

void grid_set_all_contents(grid_t *, flexarray_t *);
void _grid_set_outer_pad(grid_t *, ...);
#define grid_set_outer_pad(g, ...) _grid_set_outer_pad(g, KFUNC(__VAR_ARGS__))
bool grid_set_border_style(grid_t *, str_t *);

static inline void
grid_set_row_defaults(grid_t *grid, row_props_t *defaults)
{
    grid->default_row_properties = defaults;
}

static inline void
grid_set_col_defaults(grid_t *grid, col_props_t *defaults)
{
    grid->default_col_properties = defaults;
}

static inline void
set_enabled_borders(grid_t *grid, border_set_t info)
{
    grid->enabled_borders = info;
}

static inline void
set_border_color(grid_t *grid, style_t color_info)
{
    grid->border_color = color_info;
}
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
 * - A hide-if-not-rendered property.
 *
 * All widths and hights can be specified with:
 *
 * a) % of total render width. (like % in html)
 * b) Absolute number of characters (like px in html)
 * c) Flex-units (like fr in CSS Grid layout)
 * d) Auto (the system divides space between all auto-items evenly).
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
 *    row of the requested width.
 *
 * 2) Each item in that array is padded (generally with spaces, but
 *    with whatever 'pad' is set) to the requested width.
 *
 * 3) The returned array contains the number of rows asked for, even
 *    if some rows are 100% pad.
 *
 * 4) All alignment and coloring rules are respected in what is passed
 *    back. That means the str_t *'s styling info will be set
 *    appropriately, and that padd will be added based on any
 *    alignment properties.
 *
 * Of course, for any property beyond render dimensions, the cell's
 * contents can override.
 *
 * See the 'renderbox' abstraction, which allows the programmer to
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
 * multiple threads if useful, but should be done in a way so as not
 * to interfere with the rendering.
 */
#include <con4m.h>


// Contents must be properly formatted, etc.
typedef struct {
    int32_t  render_cols;
    int32_t  render_rows;
    str_t   *rows;
} box_rendering_t;

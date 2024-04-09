#pragma once
#include "con4m.h"

static inline renderable_t **
c4m_cell_address(grid_t *g, int row, int col)
{
    return &g->cells[g->num_cols * row + col];
}

static inline char *
c4m_get_th_tag(grid_t *g)
{
    if (g->th_tag_name != NULL) {
        return g->th_tag_name;
    }
    return "th";
}

static inline char *
c4m_get_td_tag(grid_t *g)
{
    if (g->td_tag_name != NULL) {
        return g->td_tag_name;
    }
    return "td";
}
void c4m_grid_set_all_contents(grid_t *, flexarray_t *);

extern grid_t *c4m_grid_flow(uint64_t items, ...);
utf32_t       *c4m_grid_to_str(grid_t *, to_str_use_t);
extern grid_t *_c4m_ordered_list(flexarray_t *, ...);
extern grid_t *_c4m_unordered_list(flexarray_t *, ...);
extern grid_t *_c4m_grid_tree(tree_node_t *, ...);
xlist_t       *_c4m_grid_render(grid_t *, ...);

#define c4m_grid_render(g, ...)    _c4m_grid_render(g, KFUNC(__VA_ARGS__))
#define c4m_ordered_list(l, ...)   _c4m_ordered_list(l, KFUNC(__VA_ARGS__))
#define c4m_unordered_list(l, ...) _c4m_unordered_list(l, KFUNC(__VA_ARGS__))
#define c4m_grid_tree(t, ...)      _c4m_grid_tree(t, KFUNC(__VA_ARGS__))

void
c4m_grid_add_col_span(grid_t       *grid,
                      renderable_t *contents,
                      int64_t       row,
                      int64_t       start_col,
                      int64_t       num_cols);

static inline renderable_t *
c4m_to_str_renderable(any_str_t *s, char *tag)
{
    return c4m_new(tspec_renderable(), kw("obj", ka(s), "tag", ka(tag)));
}

static inline void
c4m_set_column_style(grid_t *grid, int col, char *tag)
{
    grid->col_props[col] = lookup_cell_style(tag);
}

static inline void
c4m_set_row_style(grid_t *grid, int row, char *tag)
{
    grid->row_props[row] = lookup_cell_style(tag);
}

static inline void
c4m_set_column_props(grid_t *grid, int col, render_style_t *s)
{
    grid->col_props[col] = s;
}

static inline void
c4m_set_row_props(grid_t *grid, int row, render_style_t *s)
{
    grid->row_props[row] = s;
}

static inline style_t
c4m_grid_blend_color(style_t style1, style_t style2)
{
    // We simply do a linear average of the colors.
    return ((style1 & ~FG_COLOR_MASK) + (style2 & ~FG_COLOR_MASK)) >> 1;
}

extern void    c4m_apply_container_style(renderable_t *, char *);
extern bool    c4m_install_renderable(grid_t *,
                                      renderable_t *,
                                      int,
                                      int,
                                      int,
                                      int);
extern void    c4m_apply_container_style(renderable_t *, char *);
extern void    c4m_grid_expand_columns(grid_t *, uint64_t);
extern void    c4m_grid_expand_rows(grid_t *, uint64_t);
extern void    c4m_grid_add_row(grid_t *, object_t);
extern grid_t *c4m_grid(int, int, char *, char *, char *, int, int, int);
extern grid_t *c4m_grid_horizontal_flow(xlist_t *,
                                        uint64_t,
                                        uint64_t,
                                        char *,
                                        char *);

static inline void
c4m_grid_set_cell_contents(grid_t *g, int row, int col, object_t item)
{
    renderable_t *cell;

    if (row >= g->num_rows) {
        c4m_grid_expand_rows(g, row - (g->num_rows - 1));
    }

    switch (get_base_type(item)) {
    case T_RENDERABLE:
        cell = (renderable_t *)item;
        break;
    case T_GRID: {
        grid_t *subobj = (grid_t *)item;
        int     tcells = subobj->num_rows * subobj->num_cols;
        cell           = subobj->self;

        for (int i = 0; i < tcells; i++) {
            renderable_t *item = subobj->cells[i];
            if (item == NULL) {
                continue;
            }
            object_t sub = item->raw_item;

            if (get_base_type(sub) == T_GRID) {
                layer_styles(g->self->current_style,
                             ((grid_t *)sub)->self->current_style);
            }
        }

        break;
    }
    case T_UTF8:
    case T_UTF32: {
        char *tag;
        if (row < g->header_rows || col < g->header_cols) {
            tag = c4m_get_th_tag(g);
        }
        else {
            tag = c4m_get_td_tag(g);
        }

        cell = c4m_new(tspec_renderable(),
                       kw("tag", ka(tag), "obj", ka(item)));
        break;
    }
    default:
        abort();
    }

    layer_styles(g->self->current_style, cell->current_style);
    c4m_install_renderable(g, cell, row, row + 1, col, col + 1);
    if (row >= g->row_cursor) {
        if (col + 1 == g->num_cols) {
            g->row_cursor = row + 1;
            g->col_cursor = 0;
        }
        else {
            g->row_cursor = row;
            g->col_cursor = col + 1;
        }
    }
}

static inline void
c4m_grid_add_cell(grid_t *grid, object_t container)
{
    c4m_grid_set_cell_contents(grid,
                               grid->row_cursor,
                               grid->col_cursor,
                               container);
}

static inline void
c4m_grid_stripe_rows(grid_t *grid)
{
    grid->stripe = 1;
}

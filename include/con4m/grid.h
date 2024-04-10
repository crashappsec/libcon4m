#pragma once
#include "con4m.h"

static inline c4m_renderable_t **
c4m_cell_address(c4m_grid_t *g, int row, int col)
{
    return &g->cells[g->num_cols * row + col];
}

static inline char *
c4m_get_th_tag(c4m_grid_t *g)
{
    if (g->th_tag_name != NULL) {
        return g->th_tag_name;
    }
    return "th";
}

static inline char *
c4m_get_td_tag(c4m_grid_t *g)
{
    if (g->td_tag_name != NULL) {
        return g->td_tag_name;
    }
    return "td";
}
void c4m_grid_set_all_contents(c4m_grid_t *, flexarray_t *);

extern c4m_grid_t *c4m_grid_flow(uint64_t items, ...);
c4m_utf32_t       *c4m_c4m_grid_to_str(c4m_grid_t *, to_str_use_t);
extern c4m_grid_t *_c4m_ordered_list(flexarray_t *, ...);
extern c4m_grid_t *_c4m_unordered_list(flexarray_t *, ...);
extern c4m_grid_t *_c4m_c4m_grid_tree(c4m_tree_node_t *, ...);
c4m_xlist_t       *_c4m_grid_render(c4m_grid_t *, ...);

#define c4m_grid_render(g, ...)    _c4m_grid_render(g, KFUNC(__VA_ARGS__))
#define c4m_ordered_list(l, ...)   _c4m_ordered_list(l, KFUNC(__VA_ARGS__))
#define c4m_unordered_list(l, ...) _c4m_unordered_list(l, KFUNC(__VA_ARGS__))
#define c4m_c4m_grid_tree(t, ...)  _c4m_c4m_grid_tree(t, KFUNC(__VA_ARGS__))

void
c4m_grid_add_col_span(c4m_grid_t       *grid,
                      c4m_renderable_t *contents,
                      int64_t           row,
                      int64_t           start_col,
                      int64_t           num_cols);

static inline c4m_renderable_t *
c4m_to_str_renderable(c4m_str_t *s, char *tag)
{
    return c4m_new(c4m_tspec_renderable(),
                   c4m_kw("obj", c4m_ka(s), "tag", c4m_ka(tag)));
}

static inline void
c4m_set_column_style(c4m_grid_t *grid, int col, char *tag)
{
    grid->col_props[col] = c4m_lookup_cell_style(tag);
}

static inline void
c4m_set_row_style(c4m_grid_t *grid, int row, char *tag)
{
    grid->row_props[row] = c4m_lookup_cell_style(tag);
}

static inline void
c4m_set_column_props(c4m_grid_t *grid, int col, c4m_render_style_t *s)
{
    grid->col_props[col] = s;
}

static inline void
c4m_set_row_props(c4m_grid_t *grid, int row, c4m_render_style_t *s)
{
    grid->row_props[row] = s;
}

static inline c4m_style_t
c4m_grid_blend_color(c4m_style_t style1, c4m_style_t style2)
{
    // We simply do a linear average of the colors.
    return ((style1 & ~C4M_STY_CLEAR_FG) + (style2 & ~C4M_STY_CLEAR_FG)) >> 1;
}

extern void        c4m_apply_container_style(c4m_renderable_t *, char *);
extern bool        c4m_install_renderable(c4m_grid_t *,
                                          c4m_renderable_t *,
                                          int,
                                          int,
                                          int,
                                          int);
extern void        c4m_apply_container_style(c4m_renderable_t *, char *);
extern void        c4m_grid_expand_columns(c4m_grid_t *, uint64_t);
extern void        c4m_grid_expand_rows(c4m_grid_t *, uint64_t);
extern void        c4m_grid_add_row(c4m_grid_t *, c4m_obj_t);
extern c4m_grid_t *c4m_grid(int, int, char *, char *, char *, int, int, int);
extern c4m_grid_t *c4m_grid_horizontal_flow(c4m_xlist_t *,
                                            uint64_t,
                                            uint64_t,
                                            char *,
                                            char *);

static inline void
c4m_grid_set_cell_contents(c4m_grid_t *g, int row, int col, c4m_obj_t item)
{
    c4m_renderable_t *cell;

    if (row >= g->num_rows) {
        c4m_grid_expand_rows(g, row - (g->num_rows - 1));
    }

    switch (c4m_base_type(item)) {
    case C4M_T_RENDERABLE:
        cell = (c4m_renderable_t *)item;
        break;
    case C4M_T_GRID: {
        c4m_grid_t *subobj = (c4m_grid_t *)item;
        int         tcells = subobj->num_rows * subobj->num_cols;
        cell               = subobj->self;

        for (int i = 0; i < tcells; i++) {
            c4m_renderable_t *item = subobj->cells[i];
            if (item == NULL) {
                continue;
            }
            c4m_obj_t sub = item->raw_item;

            if (c4m_base_type(sub) == C4M_T_GRID) {
                c4m_layer_styles(g->self->current_style,
                                 ((c4m_grid_t *)sub)->self->current_style);
            }
        }

        break;
    }
    case C4M_T_UTF8:
    case C4M_T_UTF32: {
        char *tag;
        if (row < g->header_rows || col < g->header_cols) {
            tag = c4m_get_th_tag(g);
        }
        else {
            tag = c4m_get_td_tag(g);
        }

        cell = c4m_new(c4m_tspec_renderable(),
                       c4m_kw("tag",
                              c4m_ka(tag),
                              "obj",
                              c4m_ka(item)));
        break;
    }
    default:
        abort();
    }

    c4m_layer_styles(g->self->current_style, cell->current_style);
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
c4m_grid_add_cell(c4m_grid_t *grid, c4m_obj_t container)
{
    c4m_grid_set_cell_contents(grid,
                               grid->row_cursor,
                               grid->col_cursor,
                               container);
}

static inline void
c4m_grid_stripe_rows(c4m_grid_t *grid)
{
    grid->stripe = 1;
}

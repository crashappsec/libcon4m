#pragma once
#include "con4m.h"

static inline c4m_renderable_t **
c4m_cell_address(c4m_grid_t *g, int row, int col)
{
    return &g->cells[g->num_cols * row + col];
}

static inline c4m_utf8_t *
c4m_get_th_tag(c4m_grid_t *g)
{
    if (g->th_tag_name != NULL) {
        return g->th_tag_name;
    }
    return c4m_new_utf8("th");
}

static inline c4m_utf8_t *
c4m_get_td_tag(c4m_grid_t *g)
{
    if (g->td_tag_name != NULL) {
        return g->td_tag_name;
    }
    return c4m_new_utf8("td");
}
void c4m_grid_set_all_contents(c4m_grid_t *, c4m_list_t *);

extern c4m_grid_t  *c4m_grid_flow(uint64_t items, ...);
extern c4m_grid_t  *c4m_grid_flow_from_list(c4m_list_t *);
extern c4m_grid_t  *c4m_callout(c4m_str_t *s);
extern c4m_utf32_t *c4m_grid_to_str(c4m_grid_t *);
extern c4m_grid_t  *_c4m_ordered_list(c4m_list_t *, ...);
extern c4m_grid_t  *_c4m_unordered_list(c4m_list_t *, ...);
extern c4m_grid_t  *_c4m_grid_tree(c4m_tree_node_t *, ...);
extern c4m_grid_t  *_c4m_grid_tree_new(c4m_tree_node_t *, ...);
extern c4m_list_t  *_c4m_grid_render(c4m_grid_t *, ...);
extern void         c4m_set_column_props(c4m_grid_t *,
                                         int,
                                         c4m_render_style_t *);
extern void         c4m_row_column_props(c4m_grid_t *,
                                         int,
                                         c4m_render_style_t *);
extern void         c4m_set_column_style(c4m_grid_t *, int, c4m_utf8_t *);
extern void         c4m_set_row_style(c4m_grid_t *, int, c4m_utf8_t *);

#define c4m_grid_render(g, ...)    _c4m_grid_render(g, C4M_VA(__VA_ARGS__))
#define c4m_ordered_list(l, ...)   _c4m_ordered_list(l, C4M_VA(__VA_ARGS__))
#define c4m_unordered_list(l, ...) _c4m_unordered_list(l, C4M_VA(__VA_ARGS__))
#define c4m_grid_tree(t, ...)      _c4m_grid_tree(t, C4M_VA(__VA_ARGS__))
#define c4m_grid_tree_new(t, ...)  _c4m_grid_tree_new(t, C4M_VA(__VA_ARGS__))

void
c4m_grid_add_col_span(c4m_grid_t       *grid,
                      c4m_renderable_t *contents,
                      int64_t           row,
                      int64_t           start_col,
                      int64_t           num_cols);

static inline c4m_renderable_t *
c4m_to_str_renderable(c4m_str_t *s, c4m_utf8_t *tag)
{
    return c4m_new(c4m_type_renderable(),
                   c4m_kw("obj", c4m_ka(s), "tag", c4m_ka(tag)));
}

static inline c4m_style_t
c4m_grid_blend_color(c4m_style_t style1, c4m_style_t style2)
{
    // We simply do a linear average of the colors.
    return ((style1 & ~C4M_STY_CLEAR_FG) + (style2 & ~C4M_STY_CLEAR_FG)) >> 1;
}

extern void        c4m_apply_container_style(c4m_renderable_t *, c4m_utf8_t *);
extern bool        c4m_install_renderable(c4m_grid_t *,
                                          c4m_renderable_t *,
                                          int,
                                          int,
                                          int,
                                          int);
extern void        c4m_apply_container_style(c4m_renderable_t *, c4m_utf8_t *);
extern void        c4m_grid_expand_columns(c4m_grid_t *, uint64_t);
extern void        c4m_grid_expand_rows(c4m_grid_t *, uint64_t);
extern void        c4m_grid_add_row(c4m_grid_t *, c4m_obj_t);
extern c4m_grid_t *c4m_grid(int,
                            int,
                            c4m_utf8_t *,
                            c4m_utf8_t *,
                            c4m_utf8_t *,
                            int,
                            int,
                            int);
extern c4m_grid_t *c4m_grid_horizontal_flow(c4m_list_t *,
                                            uint64_t,
                                            uint64_t,
                                            c4m_utf8_t *,
                                            c4m_utf8_t *);

extern c4m_grid_t *c4m_new_cell(c4m_str_t *, c4m_utf8_t *);

extern void c4m_grid_set_cell_contents(c4m_grid_t *, int, int, c4m_obj_t);

static inline void
c4m_grid_add_cell(c4m_grid_t *grid, c4m_obj_t container)
{
    c4m_grid_set_cell_contents(grid,
                               grid->row_cursor,
                               grid->col_cursor,
                               container);
}

static inline void
c4m_grid_add_rows(c4m_grid_t *grid, c4m_list_t *l)
{
    int n = c4m_list_len(l);

    for (int i = 0; i < n; i++) {
        c4m_list_t *row = c4m_list_get(l, i, NULL);

        if (row) {
            c4m_grid_add_row(grid, row);
        }
    }
}

static inline void
c4m_grid_stripe_rows(c4m_grid_t *grid)
{
    grid->stripe = 1;
}

static inline c4m_list_t *
c4m_new_table_row()
{
    return c4m_new(c4m_type_list(c4m_type_utf32()));
}

extern void _c4m_print(c4m_obj_t, ...);

static inline void
c4m_debug_callout(char *s)
{
    _c4m_print(c4m_callout(c4m_new_utf8(s)), 0);
}

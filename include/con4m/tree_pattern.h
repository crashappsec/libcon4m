#pragma once

#include "con4m.h"

// Would consider doing a str literal for these a la:
// "->x(a(b, c), b!, .(f, b), v*, ->identifier!)"
// .(f, b) and (f, b) would be the same.

c4m_tpat_node_t *_c4m_tpat_find(void *, int, ...);
c4m_tpat_node_t *_c4m_tpat_match(void *, int, ...);
c4m_tpat_node_t *_c4m_tpat_opt_match(void *, int, ...);
c4m_tpat_node_t *_c4m_tpat_n_m_match(void *, int16_t, int16_t, int, ...);
c4m_tpat_node_t *c4m_tpat_content_find(void *, int);
c4m_tpat_node_t *c4m_tpat_content_match(void *, int);
c4m_tpat_node_t *_c4m_tpat_opt_content_match(void *, int);
c4m_tpat_node_t *c4m_tpat_n_m_content_match(void *, int16_t, int16_t, int);

bool c4m_tree_match(c4m_tree_node_t *,
                    c4m_tpat_node_t *,
                    c4m_cmp_fn,
                    c4m_xlist_t **matches);

#define c4m_tpat_find(x, y, ...)  _c4m_tpat_find(x, y, KFUNC(__VA_ARGS__))
#define c4m_tpat_match(x, y, ...) _c4m_tpat_match(x, y, KFUNC(__VA_ARGS__))
#define c4m_tpat_opt_match(x, y, ...) \
    _c4m_tpat_opt_match(x, y, KFUNC(__VA_ARGS__))
#define c4m_tpat_n_m_match(a, b, c, d, ...) \
    _c4m_tpat_n_m_match(a, b, c, d, KFUNC(__VA_ARGS__))

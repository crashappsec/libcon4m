#pragma once

#include "con4m.h"

// Would consider doing a str literal for these a la:
// "->x(a(b, c), b!, .(f, b), v*, ->identifier!)"
// .(f, b) and (f, b) would be the same.

c4m_tpat_node_t *_c4m_tpat_find(void *, int64_t, ...);
c4m_tpat_node_t *_c4m_tpat_match(void *, int64_t, ...);
c4m_tpat_node_t *_c4m_tpat_opt_match(void *, int64_t, ...);
c4m_tpat_node_t *_c4m_tpat_n_m_match(void *, int64_t, int64_t, int64_t, ...);
c4m_tpat_node_t *c4m_tpat_content_find(void *, int64_t);
c4m_tpat_node_t *c4m_tpat_content_match(void *, int64_t);
c4m_tpat_node_t *_c4m_tpat_opt_content_match(void *, int64_t);
c4m_tpat_node_t *c4m_tpat_n_m_content_match(void *, int64_t, int64_t, int64_t);

bool c4m_tree_match(c4m_tree_node_t *,
                    c4m_tpat_node_t *,
                    c4m_cmp_fn,
                    c4m_list_t **matches);

#ifdef C4M_USE_INTERNAL_API
// We use the null value (error) in patterns to match any type node.
#define c4m_nt_any    (c4m_nt_error)
#define c4m_max_nodes 0x7fff

#define C4M_PAT_NO_KIDS 0ULL

// More consise aliases internally only.

#define c4m_tfind(x, y, ...)                           \
    _c4m_tpat_find(((void *)(int64_t)x),               \
                   ((int64_t)y),                       \
                   ((int64_t)C4M_PP_NARG(__VA_ARGS__)) \
                       __VA_OPT__(, ) __VA_ARGS__)

#define c4m_tfind_content(x, y) c4m_tpat_content_find((void *)(int64_t)x, y)

#define c4m_tmatch(x, y, ...)                           \
    _c4m_tpat_match(((void *)(int64_t)x),               \
                    ((int64_t)y),                       \
                    ((int64_t)C4M_PP_NARG(__VA_ARGS__)) \
                        __VA_OPT__(, ) __VA_ARGS__)

#define c4m_tcontent(x, y, ...)                                \
    c4m_tpat_content_match(((void *)(int64_t)x),               \
                           ((int64_t)C4M_PP_NARG(__VA_ARGS__)) \
                               __VA_OPT__(, ) __VA_ARGS__)

#define c4m_toptional(x, y, ...)                            \
    _c4m_tpat_opt_match(((void *)(int64_t)x),               \
                        ((int64_t)y),                       \
                        ((int64_t)C4M_PP_NARG(__VA_ARGS__)) \
                            __VA_OPT__(, ) __VA_ARGS__)

#define c4m_tcount(a, b, c, ...)                            \
    _c4m_tpat_n_m_match(((void *)(int64_t)a),               \
                        ((int64_t)b),                       \
                        ((int64_t)c),                       \
                        ((int64_t)C4M_PP_NARG(__VA_ARGS__)) \
                            __VA_OPT__(, ) __VA_ARGS__)

#define c4m_tcount_content(a, b, c, d)             \
    c4m_tpat_n_m_content_match((void *)(int64_t)a, \
                               ((int64_t)b),       \
                               ((int64_t)c),       \
                               ((int64_t)d))
#endif

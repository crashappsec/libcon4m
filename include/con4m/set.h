#pragma once

#include "con4m.h"

extern c4m_set_t   *c4m_set_shallow_copy(c4m_set_t *);
extern c4m_xlist_t *c4m_set_to_xlist(c4m_set_t *);

#define c4m_set_contains    hatrack_set_contains
#define c4m_set_put         hatrack_set_put
#define c4m_set_add         hatrack_set_add
#define c4m_set_remove      hatrack_set_remove
#define c4m_set_items       hatrack_set_items
#define c4m_set_items_sort  hatrack_set_items_sort
#define c4m_set_is_eq       hatrack_set_is_eq
#define c4m_set_is_superset hatrack_set_is_superset
#define c4m_set_is_subset   hatrack_set_is_subset
#define c4m_set_is_disjoint hatrack_set_is_disjoint

extern c4m_set_t *c4m_set_union(c4m_set_t *, c4m_set_t *);
extern c4m_set_t *c4m_set_intersection(c4m_set_t *, c4m_set_t *);
extern c4m_set_t *c4m_set_disjunction(c4m_set_t *, c4m_set_t *);

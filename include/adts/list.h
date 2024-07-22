#pragma once

#include "con4m.h"

typedef int (*c4m_sort_fn)(const void *, const void *);

extern void       *c4m_list_get(c4m_list_t *, int64_t, bool *);
extern void        c4m_list_append(c4m_list_t *list, void *item);
extern void        c4m_list_add_if_unique(c4m_list_t *list,
                                          void       *item,
                                          bool (*fn)(void *, void *));
extern void       *c4m_list_pop(c4m_list_t *list);
extern void        c4m_list_plus_eq(c4m_list_t *, c4m_list_t *);
extern c4m_list_t *c4m_list_plus(c4m_list_t *, c4m_list_t *);
extern bool        c4m_list_set(c4m_list_t *, int64_t, void *);
extern c4m_list_t *c4m_list(c4m_type_t *);
extern int64_t     c4m_list_len(const c4m_list_t *);
extern c4m_list_t *c4m_list_get_slice(c4m_list_t *, int64_t, int64_t);
extern void        c4m_list_set_slice(c4m_list_t *,
                                      int64_t,
                                      int64_t,
                                      c4m_list_t *);
extern bool        c4m_list_contains(c4m_list_t *, c4m_obj_t);
extern c4m_list_t *c4m_list_copy(c4m_list_t *);
extern c4m_list_t *c4m_list_shallow_copy(c4m_list_t *);
extern void        c4m_list_sort(c4m_list_t *, c4m_sort_fn);
extern void        c4m_list_resize(c4m_list_t *, size_t);

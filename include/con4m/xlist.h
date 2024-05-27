#pragma once

#include "con4m.h"

// The 'x' stands for exclusive; this is meant to be exclusive to
// a single thread.

static inline void *
c4m_xlist_get(const c4m_xlist_t *list, int64_t ix, bool *err)
{
    if (!list) {
        if (err) {
            *err = true;
        }

        return NULL;
    }

    if (ix < 0) {
        ix += list->append_ix;
    }

    if (err) {
        if (ix < 0 || ix >= list->append_ix) {
            if (err) {
                *err = true;
            }
        }
        else {
            if (err) {
                *err = false;
            }
        }
    }

    return (void *)list->data[ix];
}

extern void         c4m_xlist_append(c4m_xlist_t *list, void *item);
extern void        *c4m_xlist_pop(c4m_xlist_t *list);
extern void         c4m_xlist_plus_eq(c4m_xlist_t *, c4m_xlist_t *);
extern c4m_xlist_t *c4m_xlist_plus(c4m_xlist_t *, c4m_xlist_t *);
extern bool         c4m_xlist_set(c4m_xlist_t *, int64_t, void *);
extern c4m_xlist_t *c4m_xlist(c4m_type_t *);
extern int64_t      c4m_xlist_len(const c4m_xlist_t *);
extern c4m_xlist_t *c4m_xlist_get_slice(c4m_xlist_t *, int64_t, int64_t);
extern void         c4m_xlist_set_slice(c4m_xlist_t *,
                                        int64_t,
                                        int64_t,
                                        c4m_xlist_t *);
extern bool         c4m_xlist_contains(c4m_xlist_t *, c4m_obj_t);
extern c4m_xlist_t *c4m_xlist_copy(c4m_xlist_t *);
extern c4m_xlist_t *c4m_xlist_shallow_copy(c4m_xlist_t *);

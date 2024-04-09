#pragma once

#include "con4m.h"

// The 'x' stands for exclusive; this is meant to be exclusive to
// a single thread.

static inline void *
c4m_xlist_get(const xlist_t *list, int64_t ix, bool *err)
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

extern void     c4m_xlist_append(xlist_t *list, void *item);
extern void     c4m_xlist_plus_eq(xlist_t *, xlist_t *);
extern xlist_t *c4m_xlist_plus(xlist_t *, xlist_t *);
extern bool     c4m_xlist_set(xlist_t *, int64_t, void *);
extern xlist_t *c4m_xlist(type_spec_t *);
extern int64_t  c4m_xlist_len(const xlist_t *);
extern xlist_t *c4m_xlist_get_slice(xlist_t *, int64_t, int64_t);
extern void     c4m_xlist_set_slice(xlist_t *, int64_t, int64_t, xlist_t *);
extern bool     c4m_xlist_contains(xlist_t *, object_t);

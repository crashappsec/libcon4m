#pragma once

// The 'x' stands for exclusive; this is meant to be exclusive to
// a single thread.

typedef struct {
    alignas(8)
    // The actual length if treated properly. We should be
    // careful about it.
    int32_t   append_ix;
    int32_t   length;   // The allocated length.
    int64_t  **data;
} xlist_t;

extern const con4m_vtable xlist_vtable;

static inline void *
xlist_get(const xlist_t *list, int64_t ix, bool *err)
{
    if (ix < 0) {
	ix += list->append_ix;
    }

    if (err) {
	if (ix < 0 || ix >= list->append_ix) {
	    *err = true;
	}
	else {
	    *err = false;
	}
    }

    return (void *)list->data[ix];
}

static inline int64_t
xlist_len(const xlist_t *list)
{
    return (int64_t)list->append_ix;
}

extern void     xlist_append(xlist_t *list, void *item);
extern void     xlist_plus_eq(xlist_t *, xlist_t *);
extern xlist_t *xlist_plus(xlist_t *, xlist_t *);
extern bool     xlist_set(xlist_t *, int64_t, void *);

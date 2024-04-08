#pragma once

// The 'x' stands for exclusive; this is meant to be exclusive to
// a single thread.

static inline void *
xlist_get(const xlist_t *list, int64_t ix, bool *err)
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

extern void     xlist_append(xlist_t *list, void *item);
extern void     xlist_plus_eq(xlist_t *, xlist_t *);
extern xlist_t *xlist_plus(xlist_t *, xlist_t *);
extern bool     xlist_set(xlist_t *, int64_t, void *);
extern xlist_t *con4m_xlist(type_spec_t *);
extern int64_t  xlist_len(const xlist_t *);
extern xlist_t *xlist_get_slice(xlist_t *, int64_t, int64_t);
extern void     xlist_set_slice(xlist_t *, int64_t, int64_t , xlist_t *);
extern bool     xlist_contains(xlist_t *, object_t);


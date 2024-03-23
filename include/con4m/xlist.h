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

static inline int64_t
xlist_len(const xlist_t *list)
{
    if (list == NULL) {
	return 0;
    }
    return (int64_t)list->append_ix;
}

extern void     xlist_append(xlist_t *list, void *item);
extern void     xlist_plus_eq(xlist_t *, xlist_t *);
extern xlist_t *xlist_plus(xlist_t *, xlist_t *);
extern bool     xlist_set(xlist_t *, int64_t, void *);
extern xlist_t * con4m_xlist(type_spec_t *);

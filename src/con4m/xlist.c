// "Exclusive" array, meaning not shared across threads. It's dynamic,
// and supports resizing.
#include <con4m.h>

static void
xlist_init(xlist_t *list, va_list args)
{
    DECLARE_KARGS(
	uint64_t length = 16;
	);

    method_kargs(args, length);

    list->append_ix = 0;
    list->length    = length;
    list->data      = gc_array_alloc(uint64_t *, length);
}

static inline void
xlist_resize(xlist_t *list, size_t len)
{
    int64_t **old = list->data;
    int64_t **new = gc_array_alloc(uint64_t *, len);

    for (int i = 0; i < list->length; i++) {
	new[i] = old[i];
    }
    list->data     = new;
    list->length   = len;
}

static inline void
xlist_auto_resize(xlist_t *list)
{
    xlist_resize(list, list->length << 1);
}

bool
xlist_set(xlist_t *list, int64_t ix, void *item)
{
    if (ix < 0) {
	ix += list->append_ix;
    }

    if (ix < 0) {
	return false;
    }

    if (ix >= list->length) {
	xlist_resize(list, max(ix, list->length << 1));
    }

    if (ix >= list->append_ix) {
	list->append_ix = ix + 1;
    }

    list->data[ix] = (int64_t *)item;
    return true;
}

void
xlist_append(xlist_t *list, void *item)
{
    if (list->append_ix >= list->length) {
	xlist_auto_resize(list);
    }

    list->data[list->append_ix++] =  item;
    return;
}

void
xlist_plus_eq(xlist_t *l1, xlist_t *l2)
{
    int needed = l1->append_ix + l2->append_ix;

    if (needed > l1->length) {
	xlist_resize(l1, needed);
    }

    for (int i = 0; i < l2->append_ix; i++) {
	l1->data[l1->append_ix++] = l2->data[i];
    }
}

xlist_t *
xlist_plus(xlist_t *l1, xlist_t *l2)
{
    size_t   needed = l1->append_ix + l2->append_ix;
    xlist_t *result = con4m_new(T_XLIST, "length", needed);

    for (int i = 0; i < l1->append_ix; i++) {
	result->data[i] = l1->data[i];
    }

    result->append_ix = l1->append_ix;

    for (int i = 0; i < l2->append_ix; i++) {
	result->data[result->append_ix++] = l2->data[i];
    }

    return result;
}

const con4m_vtable xlist_vtable = {
    .num_entries = 1,
    .methods     = {
	(con4m_vtable_entry)xlist_init
    }
};

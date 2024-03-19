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

static void
con4m_xlist_marshal(xlist_t *r, FILE *f, dict_t *memos, int64_t *mid)
{
    marshal_i32(r->append_ix, f);
    marshal_i32(r->length, f);

    // WARNING!!!  For right now, we are only using this to hold
    // objects.  We should do this differently when we *aren't*
    // holding objects, but we can't do that until we port more of the
    // type system.
    for (int i = 0; i < r->append_ix; i++) {
	con4m_sub_marshal(r->data[i], f, memos, mid);
    }
}

static void
con4m_xlist_unmarshal(xlist_t *r, FILE *f, dict_t *memos)
{
    r->append_ix = unmarshal_i32(f);
    r->length    = unmarshal_i32(f);
    r->data      = gc_array_alloc(int64_t *, r->length);
    // Same warning here as in the marshal version.
    for (int i = 0; i < r->append_ix; i++) {
	r->data[i] = con4m_sub_unmarshal(f, memos);
    }
}

const con4m_vtable xlist_vtable = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	(con4m_vtable_entry)xlist_init,
	NULL,
	NULL,
	(con4m_vtable_entry)con4m_xlist_marshal,
	(con4m_vtable_entry)con4m_xlist_unmarshal,
    }
};

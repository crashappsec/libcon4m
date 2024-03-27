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
    list->length    = max(length, 16);
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
    // This assumes type checking already happened statically.
    // You can make mistakes manually.

    type_spec_t *t      = get_my_type(l1);
    size_t       needed = l1->append_ix + l2->append_ix;
    xlist_t     *result = con4m_new(t, "length", needed);

    for (int i = 0; i < l1->append_ix; i++) {
	result->data[i] = l1->data[i];
    }

    result->append_ix = l1->append_ix;

    for (int i = 0; i < l2->append_ix; i++) {
	result->data[result->append_ix++] = l2->data[i];
    }

    return result;
}

void
con4m_xlist_marshal(xlist_t *r, stream_t *s, dict_t *memos, int64_t *mid)
{
    type_spec_t *list_type   = get_my_type(r);
    xlist_t     *type_params = tspec_get_parameters(list_type);
    type_spec_t *item_type   = xlist_get(type_params, 0, NULL);
    dt_info     *item_info   = tspec_get_data_type_info(item_type);
    bool         by_val      = item_info->by_value;

    marshal_i32(r->append_ix, s);
    marshal_i32(r->length, s);

    if (by_val) {
	for (int i = 0; i < r->append_ix; i++) {
	    marshal_u64((uint64_t)r->data[i], s);
	}
    }
    else {
	for (int i = 0; i < r->append_ix; i++) {
	    con4m_sub_marshal(r->data[i], s, memos, mid);
	}
    }
}

void
con4m_xlist_unmarshal(xlist_t *r, stream_t *s, dict_t *memos)
{
    type_spec_t *list_type   = get_my_type(r);
    xlist_t     *type_params = tspec_get_parameters(list_type);
    type_spec_t *item_type   = xlist_get(type_params, 0, NULL);
    dt_info     *item_info   = item_type ? tspec_get_data_type_info(item_type) :
 	                       NULL;
    bool         by_val      = item_info ? item_info->by_value: false;

    r->append_ix = unmarshal_i32(s);
    r->length    = unmarshal_i32(s);
    r->data      = gc_array_alloc(int64_t *, r->length);

    if (by_val) {
	for (int i = 0; i < r->append_ix; i++) {
	    r->data[i] = (void *)unmarshal_u64(s);
	}
    }
    else {
	for (int i = 0; i < r->append_ix; i++) {
	    r->data[i] = con4m_sub_unmarshal(s, memos);
	}
    }
}

int64_t
xlist_len(const xlist_t *list)
{
    if (list == NULL) {
	return 0;
    }
    return (int64_t)list->append_ix;
}

xlist_t *
con4m_xlist(type_spec_t *x)
{
    return con4m_new(tspec_xlist(x));
}

static any_str_t *
xlist_repr(xlist_t *list, to_str_use_t how)
{
    type_spec_t *list_type   = get_my_type(list);
    xlist_t     *type_params = tspec_get_parameters(list_type);
    type_spec_t *item_type   = xlist_get(type_params, 0, NULL);
    int64_t      len         = xlist_len(list);
    xlist_t     *items       = con4m_new(tspec_xlist(tspec_utf32()));

    for (int i = 0; i < len; i++) {
	int   err  = 0;
	void *item = xlist_get(list, i, &err);
	if (err) {
	    continue;
	}
	any_str_t *s = con4m_repr(item, item_type, how);
	xlist_append(items, s);
    }

    any_str_t *sep     = get_comma_const();
    any_str_t *result  = string_join(items, sep);

    if (how == TO_STR_USE_QUOTED) {
	result = string_concat(get_lbrak_const(),
			       string_concat(result, get_rbrak_const()));
    }

    return result;
}


static object_t
xlist_coerce_to(xlist_t *list, type_spec_t *dst_type)
{
    base_t base                = type_spec_get_base(dst_type);
    type_spec_t *src_item_type = tspec_get_param(get_my_type(list), 0);
    type_spec_t *dst_item_type = tspec_get_param(dst_type, 0);
    int64_t      len           = xlist_len(list);

    if (base == T_BOOL) {
	return (object_t)(int64_t)(xlist_len(list) != 0);
    }

    if (base == T_XLIST) {
	xlist_t *res = con4m_new(dst_type, "length", len);

	for (int i = 0; i < len; i++) {
	    void *item = xlist_get(list, i, NULL);
	    xlist_set(res, i, con4m_coerce(item, src_item_type, dst_item_type));
	}

	return (object_t)res;
    }

    flexarray_t *res = con4m_new(dst_type, "length", len);

    for (int i = 0; i < len; i++) {
	void *item = xlist_get(list, i, NULL);
	flexarray_set(res, i, con4m_coerce(item, src_item_type, dst_item_type));
    }

    return (object_t)res;
}

static xlist_t *
xlist_copy(xlist_t *list)
{
    int64_t  len = xlist_len(list);
    xlist_t *res = con4m_new(get_my_type((object_t)list), "length", len);

    for (int i = 0; i < len; i++) {
	object_t item = xlist_get(list, i, NULL);
	xlist_set(res, i, con4m_copy_object(item));
    }

    return res;
}

static object_t
xlist_safe_get(xlist_t *list, int64_t ix)
{
    bool err = false;

    object_t result = xlist_get(list, ix, &err);

    if (err) {
	CRAISE("Index out of bounds error.");
    }

    return result;
}

static xlist_t *
xlist_get_slice(xlist_t *list, int64_t start, int64_t end)
{
    int64_t  len = xlist_len(list);
    xlist_t *res;

    if (start < 0) {
	start += len;
    }
    else {
	if (start >= len) {
	    return con4m_new(get_my_type(list), "length", 0);
	}
    }
    if (end < 0) {
	end += len + 1;
    }
    else {
	if (end > len) {
	    end = len;
	}
    }

    if ((start | end) < 0 || start >= end) {
	return con4m_new(get_my_type(list), "length", 0);
    }

    len = end - start;
    res = con4m_new(get_my_type(list), "length", len);

    for (int i = 0; i < len; i++) {
	void *item = xlist_get(list, start + i, NULL);
	xlist_set(res, i, item);
    }

    return res;
}

static void
xlist_set_slice(xlist_t *list, int64_t start, int64_t end, xlist_t *new)
{
    int64_t len1 = xlist_len(list);
    int64_t len2 = xlist_len(new);

    if (start < 0) {
	start += len1;
    }
    else {
	if (start >= len1) {
	    CRAISE("Out of bounds slice.");
	}
    }
    if (end < 0) {
	end += len1 + 1;
    }
    else {
	if (end > len1) {
	    end = len1;
	}
    }

    if ((start | end) < 0 || start >= end) {
	CRAISE("Out of bounds slice.");
    }

    int64_t slicelen = end - start;
    int64_t newlen   = len1 + len2 - slicelen;

    void **newdata = gc_array_alloc(void *, newlen);

    if (start > 0) {
	for (int i = 0; i < start; i++) {
	    void *item = xlist_get(list, i, NULL);
	    newdata[i] = item;
	}
    }

    for (int i = 0; i < len2; i++) {
	void *item = xlist_get(new, i, NULL);
	newdata[start++] = item;
    }

    for (int i = end; i < len1; i++) {
	void *item = xlist_get(list, i, NULL);
	newdata[start++] = item;
    }

    list->data = (int64_t **)newdata;
}

extern bool list_can_coerce_to(type_spec_t *, type_spec_t *);

const con4m_vtable xlist_vtable = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	(con4m_vtable_entry)xlist_init,
	(con4m_vtable_entry)xlist_repr,
	NULL,
	(con4m_vtable_entry)con4m_xlist_marshal,
	(con4m_vtable_entry)con4m_xlist_unmarshal,
	(con4m_vtable_entry)list_can_coerce_to,
	(con4m_vtable_entry)xlist_coerce_to,
	NULL,
	(con4m_vtable_entry)xlist_copy,
	(con4m_vtable_entry)xlist_plus,
	NULL, // Subtract
	NULL, // Mul
	NULL, // Div
	NULL, // MOD
	(con4m_vtable_entry)xlist_len,
	(con4m_vtable_entry)xlist_safe_get,
	(con4m_vtable_entry)xlist_set,
	(con4m_vtable_entry)xlist_get_slice,
	(con4m_vtable_entry)xlist_set_slice
    }
};

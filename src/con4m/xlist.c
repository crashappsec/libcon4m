// "Exclusive" array, meaning not shared across threads. It's dynamic,
// and supports resizing.
#include "con4m.h"

static void
xlist_init(xlist_t *list, va_list args)
{
    int64_t length = 16;

    c4m_karg_va_init(args);
    c4m_kw_int64("length", length);

    list->append_ix = 0;
    list->length    = max(length, 16);
    list->data      = c4m_gc_array_alloc(uint64_t *, length);
}

static inline void
xlist_resize(xlist_t *list, size_t len)
{
    int64_t **old = list->data;
    int64_t **new = c4m_gc_array_alloc(uint64_t *, len);

    for (int i = 0; i < list->length; i++) {
        new[i] = old[i];
    }

    list->data   = new;
    list->length = len;
}

static inline void
xlist_auto_resize(xlist_t *list)
{
    xlist_resize(list, list->length << 1);
}

bool
c4m_xlist_set(xlist_t *list, int64_t ix, void *item)
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
c4m_xlist_append(xlist_t *list, void *item)
{
    if (list->append_ix >= list->length) {
        xlist_auto_resize(list);
    }

    list->data[list->append_ix++] = item;

    return;
}

void
c4m_xlist_plus_eq(xlist_t *l1, xlist_t *l2)
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
c4m_xlist_plus(xlist_t *l1, xlist_t *l2)
{
    // This assumes type checking already happened statically.
    // You can make mistakes manually.

    type_spec_t *t      = c4m_get_my_type(l1);
    size_t       needed = l1->append_ix + l2->append_ix;
    xlist_t     *result = c4m_new(t, c4m_kw("length", c4m_ka(needed)));

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
c4m_xlist_marshal(xlist_t *r, stream_t *s, dict_t *memos, int64_t *mid)
{
    type_spec_t   *list_type   = c4m_get_my_type(r);
    xlist_t       *type_params = c4m_tspec_get_parameters(list_type);
    type_spec_t   *item_type   = c4m_xlist_get(type_params, 0, NULL);
    c4m_dt_info_t *item_info   = c4m_tspec_get_data_type_info(item_type);
    bool           by_val      = item_info->by_value;

    c4m_marshal_i32(r->append_ix, s);
    c4m_marshal_i32(r->length, s);

    if (by_val) {
        for (int i = 0; i < r->append_ix; i++) {
            c4m_marshal_u64((uint64_t)r->data[i], s);
        }
    }
    else {
        for (int i = 0; i < r->append_ix; i++) {
            c4m_sub_marshal(r->data[i], s, memos, mid);
        }
    }
}

void
c4m_xlist_unmarshal(xlist_t *r, stream_t *s, dict_t *memos)
{
    type_spec_t   *list_type   = c4m_get_my_type(r);
    xlist_t       *type_params = c4m_tspec_get_parameters(list_type);
    type_spec_t   *item_type   = c4m_xlist_get(type_params, 0, NULL);
    c4m_dt_info_t *item_info   = item_type ? c4m_tspec_get_data_type_info(item_type) : NULL;
    bool           by_val      = item_info ? item_info->by_value : false;

    r->append_ix = c4m_unmarshal_i32(s);
    r->length    = c4m_unmarshal_i32(s);
    r->data      = c4m_gc_array_alloc(int64_t *, r->length);

    if (by_val) {
        for (int i = 0; i < r->append_ix; i++) {
            r->data[i] = (void *)c4m_unmarshal_u64(s);
        }
    }
    else {
        for (int i = 0; i < r->append_ix; i++) {
            r->data[i] = c4m_sub_unmarshal(s, memos);
        }
    }
}

int64_t
c4m_xlist_len(const xlist_t *list)
{
    if (list == NULL) {
        return 0;
    }
    return (int64_t)list->append_ix;
}

xlist_t *
c4m_xlist(type_spec_t *x)
{
    return c4m_new(c4m_tspec_xlist(x));
}

static any_str_t *
xlist_repr(xlist_t *list, to_str_use_t how)
{
    type_spec_t *list_type   = c4m_get_my_type(list);
    xlist_t     *type_params = c4m_tspec_get_parameters(list_type);
    type_spec_t *item_type   = c4m_xlist_get(type_params, 0, NULL);
    int64_t      len         = c4m_xlist_len(list);
    xlist_t     *items       = c4m_new(c4m_tspec_xlist(c4m_tspec_utf32()));

    for (int i = 0; i < len; i++) {
        bool  err  = false;
        void *item = c4m_xlist_get(list, i, &err);
        if (err) {
            continue;
        }
        any_str_t *s = c4m_repr(item, item_type, how);
        c4m_xlist_append(items, s);
    }

    any_str_t *sep    = c4m_get_comma_const();
    any_str_t *result = c4m_str_join(items, sep);

    if (how == C4M_REPR_QUOTED) {
        result = c4m_str_concat(c4m_get_lbrak_const(),
                                c4m_str_concat(result, c4m_get_rbrak_const()));
    }

    return result;
}

static object_t
xlist_coerce_to(xlist_t *list, type_spec_t *dst_type)
{
    c4m_dt_kind_t base          = c4m_tspec_get_base(dst_type);
    type_spec_t  *src_item_type = c4m_tspec_get_param(c4m_get_my_type(list), 0);
    type_spec_t  *dst_item_type = c4m_tspec_get_param(dst_type, 0);
    int64_t       len           = c4m_xlist_len(list);

    if (base == (c4m_dt_kind_t)C4M_T_BOOL) {
        return (object_t)(int64_t)(c4m_xlist_len(list) != 0);
    }

    if (base == (c4m_dt_kind_t)C4M_T_XLIST) {
        xlist_t *res = c4m_new(dst_type, c4m_kw("length", c4m_ka(len)));

        for (int i = 0; i < len; i++) {
            void *item = c4m_xlist_get(list, i, NULL);
            c4m_xlist_set(res, i, c4m_coerce(item, src_item_type, dst_item_type));
        }

        return (object_t)res;
    }

    flexarray_t *res = c4m_new(dst_type, c4m_kw("length", c4m_ka(len)));

    for (int i = 0; i < len; i++) {
        void *item = c4m_xlist_get(list, i, NULL);
        flexarray_set(res, i, c4m_coerce(item, src_item_type, dst_item_type));
    }

    return (object_t)res;
}

static xlist_t *
xlist_copy(xlist_t *list)
{
    int64_t  len = c4m_xlist_len(list);
    xlist_t *res = c4m_new(c4m_get_my_type((object_t)list),
                           c4m_kw("length", c4m_ka(len)));

    for (int i = 0; i < len; i++) {
        object_t item = c4m_xlist_get(list, i, NULL);
        c4m_xlist_set(res, i, c4m_copy_object(item));
    }

    return res;
}

static object_t
xlist_safe_get(xlist_t *list, int64_t ix)
{
    bool err = false;

    object_t result = c4m_xlist_get(list, ix, &err);

    if (err) {
        C4M_CRAISE("Index out of bounds error.");
    }

    return result;
}

xlist_t *
c4m_xlist_get_slice(xlist_t *list, int64_t start, int64_t end)
{
    int64_t  len = c4m_xlist_len(list);
    xlist_t *res;

    if (start < 0) {
        start += len;
    }
    else {
        if (start >= len) {
            return c4m_new(c4m_get_my_type(list), c4m_kw("length", c4m_ka(0)));
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
        return c4m_new(c4m_get_my_type(list), c4m_kw("length", c4m_ka(0)));
    }

    len = end - start;
    res = c4m_new(c4m_get_my_type(list), c4m_kw("length", c4m_ka(len)));

    for (int i = 0; i < len; i++) {
        void *item = c4m_xlist_get(list, start + i, NULL);
        c4m_xlist_set(res, i, item);
    }

    return res;
}

void
c4m_xlist_set_slice(xlist_t *list, int64_t start, int64_t end, xlist_t *new)
{
    int64_t len1 = c4m_xlist_len(list);
    int64_t len2 = c4m_xlist_len(new);

    if (start < 0) {
        start += len1;
    }
    else {
        if (start >= len1) {
            C4M_CRAISE("Out of bounds slice.");
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
        C4M_CRAISE("Out of bounds slice.");
    }

    int64_t slicelen = end - start;
    int64_t newlen   = len1 + len2 - slicelen;

    void **newdata = c4m_gc_array_alloc(void *, newlen);

    if (start > 0) {
        for (int i = 0; i < start; i++) {
            void *item = c4m_xlist_get(list, i, NULL);
            newdata[i] = item;
        }
    }

    for (int i = 0; i < len2; i++) {
        void *item       = c4m_xlist_get(new, i, NULL);
        newdata[start++] = item;
    }

    for (int i = end; i < len1; i++) {
        void *item       = c4m_xlist_get(list, i, NULL);
        newdata[start++] = item;
    }

    list->data = (int64_t **)newdata;
}

bool
c4m_xlist_contains(xlist_t *list, object_t item)
{
    int64_t      len       = c4m_xlist_len(list);
    type_spec_t *item_type = c4m_get_my_type(item);

    for (int i = 0; i < len; i++) {
        if (c4m_eq(item_type, item, c4m_xlist_get(list, i, NULL))) {
            return true;
        }
    }

    return false;
}

extern bool list_can_coerce_to(type_spec_t *, type_spec_t *);

const c4m_vtable_t c4m_xlist_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        (c4m_vtable_entry)xlist_init,
        (c4m_vtable_entry)xlist_repr,
        NULL,
        (c4m_vtable_entry)c4m_xlist_marshal,
        (c4m_vtable_entry)c4m_xlist_unmarshal,
        (c4m_vtable_entry)list_can_coerce_to,
        (c4m_vtable_entry)xlist_coerce_to,
        NULL,
        (c4m_vtable_entry)xlist_copy,
        (c4m_vtable_entry)c4m_xlist_plus,
        NULL, // Subtract
        NULL, // Mul
        NULL, // Div
        NULL, // MOD
        NULL, // EQ
        NULL, // LT
        NULL, // GT
        (c4m_vtable_entry)c4m_xlist_len,
        (c4m_vtable_entry)xlist_safe_get,
        (c4m_vtable_entry)c4m_xlist_set,
        (c4m_vtable_entry)c4m_xlist_get_slice,
        (c4m_vtable_entry)c4m_xlist_set_slice,
    },
};

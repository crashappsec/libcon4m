// "Exclusive" array, meaning not shared across threads. It's dynamic,
// and supports resizing.
#include "con4m.h"

static void
xlist_init(c4m_xlist_t *list, va_list args)
{
    int64_t length = 16;

    c4m_karg_va_init(args);
    c4m_kw_int64("length", length);

    list->append_ix = 0;
    list->length    = max(length, 16);
    list->data      = c4m_gc_array_alloc(uint64_t *, length);
}

static inline void
xlist_resize(c4m_xlist_t *list, size_t len)
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
xlist_auto_resize(c4m_xlist_t *list)
{
    xlist_resize(list, list->length << 1);
}

bool
c4m_xlist_set(c4m_xlist_t *list, int64_t ix, void *item)
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

    assert(item != NULL);
    list->data[ix] = (int64_t *)item;
    return true;
}

void
c4m_xlist_append(c4m_xlist_t *list, void *item)
{
    if (list->append_ix >= list->length) {
        xlist_auto_resize(list);
    }

    list->data[list->append_ix++] = item;

    return;
}

void *
c4m_xlist_pop(c4m_xlist_t *list)
{
    if (list->append_ix == 0) {
        C4M_CRAISE("Pop called on empty xlist.");
    }

    return c4m_xlist_get(list, --list->append_ix, NULL);
}

void
c4m_xlist_plus_eq(c4m_xlist_t *l1, c4m_xlist_t *l2)
{
    if (l1 == NULL || l2 == NULL) {
        return;
    }

    int needed = l1->append_ix + l2->append_ix;

    if (needed > l1->length) {
        xlist_resize(l1, needed);
    }

    for (int i = 0; i < l2->append_ix; i++) {
        l1->data[l1->append_ix++] = l2->data[i];
    }
}

c4m_xlist_t *
c4m_xlist_plus(c4m_xlist_t *l1, c4m_xlist_t *l2)
{
    // This assumes type checking already happened statically.
    // You can make mistakes manually.

    if (l1 == NULL && l2 == NULL) {
        return NULL;
    }
    if (l1 == NULL) {
        return c4m_xlist_shallow_copy(l2);
    }
    if (l2 == NULL) {
        return c4m_xlist_shallow_copy(l1);
    }

    c4m_type_t  *t      = c4m_get_my_type(l1);
    size_t       needed = l1->append_ix + l2->append_ix;
    c4m_xlist_t *result = c4m_new(t, c4m_kw("length", c4m_ka(needed)));

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
c4m_xlist_marshal(c4m_xlist_t *r, c4m_stream_t *s, c4m_dict_t *memos, int64_t *mid)
{
    c4m_type_t    *list_type   = c4m_get_my_type(r);
    c4m_xlist_t   *type_params = c4m_tspec_get_parameters(list_type);
    c4m_type_t    *item_type   = c4m_xlist_get(type_params, 0, NULL);
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
c4m_xlist_unmarshal(c4m_xlist_t *r, c4m_stream_t *s, c4m_dict_t *memos)
{
    c4m_type_t    *list_type   = c4m_get_my_type(r);
    c4m_xlist_t   *type_params = c4m_tspec_get_parameters(list_type);
    c4m_type_t    *item_type   = c4m_xlist_get(type_params, 0, NULL);
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
c4m_xlist_len(const c4m_xlist_t *list)
{
    if (list == NULL) {
        return 0;
    }
    return (int64_t)list->append_ix;
}

c4m_xlist_t *
c4m_xlist(c4m_type_t *x)
{
    return c4m_new(c4m_tspec_xlist(x));
}

static c4m_str_t *
xlist_repr(c4m_xlist_t *list)
{
    c4m_type_t  *list_type   = c4m_get_my_type(list);
    c4m_xlist_t *type_params = c4m_tspec_get_parameters(list_type);
    c4m_type_t  *item_type   = c4m_xlist_get(type_params, 0, NULL);
    int64_t      len         = c4m_xlist_len(list);
    c4m_xlist_t *items       = c4m_new(c4m_tspec_xlist(c4m_tspec_utf32()));

    for (int i = 0; i < len; i++) {
        bool  err  = false;
        void *item = c4m_xlist_get(list, i, &err);
        if (err) {
            continue;
        }
        c4m_str_t *s = c4m_repr(item, item_type);
        c4m_xlist_append(items, s);
    }

    c4m_str_t *sep    = c4m_get_comma_const();
    c4m_str_t *result = c4m_str_join(items, sep);

    result = c4m_str_concat(c4m_get_lbrak_const(),
                            c4m_str_concat(result, c4m_get_rbrak_const()));

    return result;
}

static c4m_obj_t
xlist_coerce_to(c4m_xlist_t *list, c4m_type_t *dst_type)
{
    c4m_dt_kind_t base          = c4m_tspec_get_base(dst_type);
    c4m_type_t   *src_item_type = c4m_tspec_get_param(c4m_get_my_type(list), 0);
    c4m_type_t   *dst_item_type = c4m_tspec_get_param(dst_type, 0);
    int64_t       len           = c4m_xlist_len(list);

    if (base == (c4m_dt_kind_t)C4M_T_BOOL) {
        return (c4m_obj_t)(int64_t)(c4m_xlist_len(list) != 0);
    }

    if (base == (c4m_dt_kind_t)C4M_T_XLIST) {
        c4m_xlist_t *res = c4m_new(dst_type, c4m_kw("length", c4m_ka(len)));

        for (int i = 0; i < len; i++) {
            void *item = c4m_xlist_get(list, i, NULL);
            c4m_xlist_set(res, i, c4m_coerce(item, src_item_type, dst_item_type));
        }

        return (c4m_obj_t)res;
    }

    flexarray_t *res = c4m_new(dst_type, c4m_kw("length", c4m_ka(len)));

    for (int i = 0; i < len; i++) {
        void *item = c4m_xlist_get(list, i, NULL);
        flexarray_set(res, i, c4m_coerce(item, src_item_type, dst_item_type));
    }

    return (c4m_obj_t)res;
}

c4m_xlist_t *
xlist_copy(c4m_xlist_t *list)
{
    int64_t      len       = c4m_xlist_len(list);
    c4m_type_t  *my_type   = c4m_get_my_type((c4m_obj_t)list);
    c4m_type_t  *item_type = c4m_tspec_get_param(my_type, 0);
    c4m_xlist_t *res       = c4m_new(my_type, c4m_kw("length", c4m_ka(len)));

    for (int i = 0; i < len; i++) {
        c4m_obj_t item = c4m_xlist_get(list, i, NULL);
        c4m_xlist_set(res, i, c4m_copy_object_of_type(item, item_type));
    }

    return res;
}

c4m_xlist_t *
c4m_xlist_shallow_copy(c4m_xlist_t *list)
{
    int64_t      len     = c4m_xlist_len(list);
    c4m_type_t  *my_type = c4m_get_my_type((c4m_obj_t)list);
    c4m_xlist_t *res     = c4m_new(my_type, c4m_kw("length", c4m_ka(len)));

    for (int i = 0; i < len; i++) {
        c4m_xlist_set(res, i, c4m_xlist_get(list, i, NULL));
    }

    return res;
}

static c4m_obj_t
c4m_xlist_safe_get(c4m_xlist_t *list, int64_t ix)
{
    bool err = false;

    c4m_obj_t result = c4m_xlist_get(list, ix, &err);

    if (err) {
        C4M_CRAISE("Index out of bounds error.");
    }

    return result;
}

c4m_xlist_t *
c4m_xlist_get_slice(c4m_xlist_t *list, int64_t start, int64_t end)
{
    int64_t      len = c4m_xlist_len(list);
    c4m_xlist_t *res;

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
c4m_xlist_set_slice(c4m_xlist_t *list,
                    int64_t      start,
                    int64_t      end,
                    c4m_xlist_t *new)
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
c4m_xlist_contains(c4m_xlist_t *list, c4m_obj_t item)
{
    int64_t     len       = c4m_xlist_len(list);
    c4m_type_t *item_type = c4m_get_my_type(item);

    for (int i = 0; i < len; i++) {
        if (!item_type) {
            // Don't know why ref is giving me no item type yet,
            // so this is a tmp fix.
            return item == c4m_xlist_get(list, i, NULL);
        }

        if (c4m_eq(item_type, item, c4m_xlist_get(list, i, NULL))) {
            return true;
        }
    }

    return false;
}

static void *
xlist_view(c4m_xlist_t *list, uint64_t *n)
{
    c4m_xlist_t *copy = c4m_xlist_shallow_copy(list);
    *n                = c4m_xlist_len(copy);
    return copy->data;
}

static c4m_xlist_t *
to_xlist_lit(c4m_type_t *objtype, c4m_xlist_t *items, c4m_utf8_t *litmod)
{
    return items;
}

extern bool list_can_coerce_to(c4m_type_t *, c4m_type_t *);

const c4m_vtable_t c4m_xlist_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR]   = (c4m_vtable_entry)xlist_init,
        [C4M_BI_TO_STR]        = (c4m_vtable_entry)xlist_repr,
        [C4M_BI_MARSHAL]       = (c4m_vtable_entry)c4m_xlist_marshal,
        [C4M_BI_UNMARSHAL]     = (c4m_vtable_entry)c4m_xlist_unmarshal,
        [C4M_BI_COERCIBLE]     = (c4m_vtable_entry)list_can_coerce_to,
        [C4M_BI_COERCE]        = (c4m_vtable_entry)xlist_coerce_to,
        [C4M_BI_COPY]          = (c4m_vtable_entry)xlist_copy,
        [C4M_BI_ADD]           = (c4m_vtable_entry)c4m_xlist_plus,
        [C4M_BI_LEN]           = (c4m_vtable_entry)c4m_xlist_len,
        [C4M_BI_INDEX_GET]     = (c4m_vtable_entry)c4m_xlist_safe_get,
        [C4M_BI_INDEX_SET]     = (c4m_vtable_entry)c4m_xlist_set,
        [C4M_BI_SLICE_GET]     = (c4m_vtable_entry)c4m_xlist_get_slice,
        [C4M_BI_SLICE_SET]     = (c4m_vtable_entry)c4m_xlist_set_slice,
        [C4M_BI_VIEW]          = (c4m_vtable_entry)xlist_view,
        [C4M_BI_CONTAINER_LIT] = (c4m_vtable_entry)to_xlist_lit,
    },
};

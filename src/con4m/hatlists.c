#include "con4m.h"

// Different queue/list types.

static void
c4m_flexarray_init(flexarray_t *list, va_list args)
{
    int64_t length = 0;

    c4m_karg_va_init(args);
    c4m_kw_int64("length", length);

    flexarray_init(list, length);
}

static void
c4m_queue_init(queue_t *list, va_list args)
{
    int64_t length = 0;

    c4m_karg_va_init(args);
    c4m_kw_int64("length", length);

    queue_init_size(list, (char)(64 - __builtin_clzll(length)));
}

static void
c4m_ring_init(hatring_t *ring, va_list args)
{
    int64_t length = 0;

    c4m_karg_va_init(args);
    c4m_kw_int64("length", length);

    hatring_init(ring, length);
}

static void
c4m_logring_init(logring_t *ring, va_list args)
{
    int64_t num_entries  = 0;
    int64_t entry_length = 1024;

    c4m_karg_va_init(args);
    c4m_kw_int64("num_entries", num_entries);
    c4m_kw_int64("entry_length", entry_length);

    logring_init(ring, num_entries, entry_length);
}

static void
c4m_stack_init(hatstack_t *stack, va_list args)
{
    int64_t prealloc = 128;

    c4m_karg_va_init(args);
    c4m_kw_int64("prealloc", prealloc);

    hatstack_init(stack, prealloc);
}

static void
c4m_flexarray_marshal(flexarray_t  *r,
                      c4m_stream_t *s,
                      c4m_dict_t   *memos,
                      int64_t      *mid)
{
    c4m_type_t    *list_type   = c4m_get_my_type(r);
    c4m_list_t    *type_params = c4m_type_get_params(list_type);
    c4m_type_t    *item_type   = c4m_list_get(type_params, 0, NULL);
    c4m_dt_info_t *item_info   = c4m_type_get_data_type_info(item_type);
    bool           by_val      = item_info->by_value;
    flex_view_t   *view        = flexarray_view(r);
    uint64_t       len         = flexarray_view_len(view);

    c4m_marshal_u64(len, s);

    if (by_val) {
        for (uint64_t i = 0; i < len; i++) {
            uint64_t n = (uint64_t)flexarray_view_next(view, NULL);
            c4m_marshal_u64(n, s);
        }
    }
    else {
        for (uint64_t i = 0; i < len; i++) {
            c4m_sub_marshal(flexarray_view_next(view, NULL), s, memos, mid);
        }
    }
}

static void
c4m_flexarray_unmarshal(flexarray_t *r, c4m_stream_t *s, c4m_dict_t *memos)
{
    c4m_type_t    *list_type   = c4m_get_my_type(r);
    c4m_list_t    *type_params = c4m_type_get_params(list_type);
    c4m_type_t    *item_type   = c4m_list_get(type_params, 0, NULL);
    c4m_dt_info_t *item_info   = c4m_type_get_data_type_info(item_type);
    bool           by_val      = item_info->by_value;
    uint64_t       len         = c4m_unmarshal_u64(s);

    flexarray_init(r, len);

    if (by_val) {
        for (uint64_t i = 0; i < len; i++) {
            uint64_t n = c4m_unmarshal_u64(s);
            flexarray_set(r, i, (void *)n);
        }
    }
    else {
        for (uint64_t i = 0; i < len; i++) {
            flexarray_set(r, i, c4m_sub_unmarshal(s, memos));
        }
    }
}

bool
c4m_flexarray_can_coerce_to(c4m_type_t *my_type, c4m_type_t *dst_type)
{
    c4m_dt_kind_t base = c4m_type_get_base(dst_type);

    if (base == (c4m_dt_kind_t)C4M_T_BOOL) {
        return true;
    }

    // clang-format off
    if (base == (c4m_dt_kind_t)C4M_T_FLIST ||
	base == (c4m_dt_kind_t)C4M_T_XLIST) {
        // clang-format on
        c4m_type_t *my_item  = c4m_type_get_param(my_type, 0);
        c4m_type_t *dst_item = c4m_type_get_param(dst_type, 0);

        return c4m_can_coerce(my_item, dst_item);
    }

    return false;
}

static c4m_obj_t
c4m_flexarray_coerce_to(flexarray_t *list, c4m_type_t *dst_type)
{
    c4m_dt_kind_t base          = c4m_type_get_base(dst_type);
    flex_view_t  *view          = flexarray_view(list);
    int64_t       len           = flexarray_view_len(view);
    c4m_type_t   *src_item_type = c4m_type_get_param(c4m_get_my_type(list), 0);
    c4m_type_t   *dst_item_type = c4m_type_get_param(dst_type, 0);

    if (base == (c4m_dt_kind_t)C4M_T_BOOL) {
        return (c4m_obj_t)(int64_t)(flexarray_view_len(view) != 0);
    }

    if (base == (c4m_dt_kind_t)C4M_T_FLIST) {
        flexarray_t *res = c4m_new(dst_type, c4m_kw("length", c4m_ka(len)));

        for (int i = 0; i < len; i++) {
            void *item = flexarray_view_next(view, NULL);
            flexarray_set(res,
                          i,
                          c4m_coerce(item, src_item_type, dst_item_type));
        }

        return (c4m_obj_t)res;
    }

    c4m_list_t *res = c4m_new(dst_type, c4m_kw("length", c4m_ka(len)));

    for (int i = 0; i < len; i++) {
        void *item = flexarray_view_next(view, NULL);
        c4m_list_set(res, i, c4m_coerce(item, src_item_type, dst_item_type));
    }

    return (c4m_obj_t)res;
}

static flexarray_t *
c4m_flexarray_copy(flexarray_t *list)
{
    flex_view_t *view = flexarray_view(list);
    int64_t      len  = flexarray_view_len(view);
    c4m_type_t  *myty = c4m_get_my_type(list);

    flexarray_t *res    = c4m_new(myty, c4m_kw("length", c4m_ka(len)));
    c4m_type_t  *itemty = c4m_type_get_param(myty, 0);

    // itemty = c4m_resolve_type(itemty);

    for (int i = 0; i < len; i++) {
        c4m_obj_t item = flexarray_view_next(view, NULL);
        flexarray_set(res, i, c4m_copy_object_of_type(item, itemty));
    }

    return res;
}

static void *
c4m_flexarray_get(flexarray_t *list, int64_t index)
{
    c4m_obj_t result;
    int       status;

    if (index < 0) {
        // Thing might get resized, so we have to take a view.
        flex_view_t *view = flexarray_view(list);
        int64_t      len  = flexarray_view_len(view);

        index += len;

        if (index < 0) {
            c4m_utf8_t *err = c4m_cstr_format(
                "Array index out of bounds "
                "(ix = {}; size = {})",
                c4m_box_i64(index),
                c4m_box_i64(len));

            C4M_RAISE(err);
        }

        result = flexarray_view_get(view, index, &status);
    }
    else {
        result = flexarray_get(list, index, &status);
    }

    if (status == FLEX_OK) {
        return result;
    }

    if (status == FLEX_UNINITIALIZED) {
        C4M_CRAISE("Array access is for uninitialized value.");
    }
    else {
        flex_view_t *view = flexarray_view(list);
        int64_t      len  = flexarray_view_len(view);

        c4m_utf8_t *err = c4m_cstr_format(
            "Array index out of bounds "
            "(ix = {}; size = {})",
            c4m_box_i64(index),
            c4m_box_i64(len));

        C4M_RAISE(err);
    }
}

static void
c4m_flexarray_set(flexarray_t *list, int64_t ix, void *item)
{
    if (!flexarray_set(list, ix, item)) {
        flex_view_t *view = flexarray_view(list);
        int64_t      len  = flexarray_view_len(view);

        c4m_utf8_t *err = c4m_cstr_format(
            "Array index out of bounds (ix = {})",
            c4m_box_i64(ix),
            c4m_box_i64(len));
        C4M_RAISE(err);
    }
}

static flexarray_t *
c4m_flexarray_get_slice(flexarray_t *list, int64_t start, int64_t end)
{
    flex_view_t *view = flexarray_view(list);
    int64_t      len  = flexarray_view_len(view);
    flexarray_t *res;

    if (start < 0) {
        start += len;
    }
    else {
        if (start >= len) {
            return c4m_new(c4m_get_my_type(list), c4m_kw("length", c4m_ka(0)));
        }
    }
    if (end < 0) {
        end += len;
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
        void *item = flexarray_view_get(view, start + i, NULL);
        flexarray_set(res, i, item);
    }

    return res;
}

// Semantics are, take a moment-in time view of each list (not the same moment),
// Compute the result, then replace the list. Definitely not atomic.
static void
c4m_flexarray_set_slice(flexarray_t *list, int64_t start, int64_t end, flexarray_t *new)
{
    flex_view_t *view1 = flexarray_view(list);
    flex_view_t *view2 = flexarray_view(new);
    int64_t      len1  = flexarray_view_len(view1);
    int64_t      len2  = flexarray_view_len(view2);
    flexarray_t *tmp;

    if (start < 0) {
        start += len1;
    }
    else {
        if (start >= len1) {
            C4M_CRAISE("Out of bounds slice.");
        }
    }
    if (end < 0) {
        end += len1;
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

    tmp = c4m_new(c4m_get_my_type(list), c4m_kw("length", c4m_ka(newlen)));

    if (start > 0) {
        for (int i = 0; i < start; i++) {
            void *item = flexarray_view_get(view1, i, NULL);
            flexarray_set(tmp, i, item);
        }
    }

    for (int i = 0; i < len2; i++) {
        void *item = flexarray_view_get(view2, i, NULL);
        flexarray_set(tmp, start++, item);
    }

    for (int i = end; i < len1; i++) {
        void *item = flexarray_view_get(view1, i, NULL);
        flexarray_set(tmp, start++, item);
    }

    atomic_store(&list->store, atomic_load(&tmp->store));
}

static c4m_str_t *
c4m_flexarray_repr(flexarray_t *list)
{
    c4m_type_t  *list_type   = c4m_get_my_type(list);
    c4m_list_t  *type_params = c4m_type_get_params(list_type);
    c4m_type_t  *item_type   = c4m_list_get(type_params, 0, NULL);
    flex_view_t *view        = flexarray_view(list);
    int64_t      len         = flexarray_view_len(view);
    c4m_list_t  *items       = c4m_new(c4m_type_list(c4m_type_utf32()));

    for (int i = 0; i < len; i++) {
        int   err  = 0;
        void *item = flexarray_view_get(view, i, &err);
        if (err) {
            continue;
        }

        c4m_str_t *s = c4m_repr(item, item_type);
        c4m_list_append(items, s);
    }

    c4m_str_t *sep    = c4m_get_comma_const();
    c4m_str_t *result = c4m_str_join(items, sep);

    result = c4m_str_concat(c4m_get_lbrak_const(),
                            c4m_str_concat(result, c4m_get_rbrak_const()));

    return result;
}

static flexarray_t *
c4m_to_flexarray_lit(c4m_type_t *objtype, c4m_list_t *items, c4m_utf8_t *litmod)
{
    uint64_t     n      = c4m_list_len(items);
    flexarray_t *result = c4m_new(objtype, c4m_kw("length", c4m_ka(n)));

    for (unsigned int i = 0; i < n; i++) {
        c4m_flexarray_set(result, i, c4m_list_get(items, i, NULL));
    }

    return result;
}

const c4m_vtable_t c4m_flexarray_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR]   = (c4m_vtable_entry)c4m_flexarray_init,
        [C4M_BI_FINALIZER]     = (c4m_vtable_entry)flexarray_cleanup,
        [C4M_BI_TO_STR]        = (c4m_vtable_entry)c4m_flexarray_repr,
        [C4M_BI_MARSHAL]       = (c4m_vtable_entry)c4m_flexarray_marshal,
        [C4M_BI_UNMARSHAL]     = (c4m_vtable_entry)c4m_flexarray_unmarshal,
        [C4M_BI_COERCIBLE]     = (c4m_vtable_entry)c4m_flexarray_can_coerce_to,
        [C4M_BI_COERCE]        = (c4m_vtable_entry)c4m_flexarray_coerce_to,
        [C4M_BI_COPY]          = (c4m_vtable_entry)c4m_flexarray_copy,
        [C4M_BI_ADD]           = (c4m_vtable_entry)flexarray_add,
        [C4M_BI_LEN]           = (c4m_vtable_entry)flexarray_len,
        [C4M_BI_INDEX_GET]     = (c4m_vtable_entry)c4m_flexarray_get,
        [C4M_BI_INDEX_SET]     = (c4m_vtable_entry)c4m_flexarray_set,
        [C4M_BI_SLICE_GET]     = (c4m_vtable_entry)c4m_flexarray_get_slice,
        [C4M_BI_SLICE_SET]     = (c4m_vtable_entry)c4m_flexarray_set_slice,
        [C4M_BI_VIEW]          = (c4m_vtable_entry)flexarray_view,
        [C4M_BI_CONTAINER_LIT] = (c4m_vtable_entry)c4m_to_flexarray_lit,
        [C4M_BI_GC_MAP]        = (c4m_vtable_entry)C4M_GC_SCAN_ALL,
        NULL,
    },
};

const c4m_vtable_t c4m_queue_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)c4m_queue_init,
        [C4M_BI_FINALIZER]   = (c4m_vtable_entry)queue_cleanup,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)C4M_GC_SCAN_ALL,
        NULL,
    },
};

const c4m_vtable_t c4m_ring_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)c4m_ring_init,
        [C4M_BI_FINALIZER]   = (c4m_vtable_entry)hatring_cleanup,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)C4M_GC_SCAN_ALL,
        NULL,
    },
};

const c4m_vtable_t c4m_logring_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)c4m_logring_init,
        [C4M_BI_FINALIZER]   = (c4m_vtable_entry)logring_cleanup,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)C4M_GC_SCAN_ALL,
        NULL,
    },
};

const c4m_vtable_t c4m_stack_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)c4m_stack_init,
        [C4M_BI_FINALIZER]   = (c4m_vtable_entry)hatstack_cleanup,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)C4M_GC_SCAN_ALL,
        NULL,
    },
};

#include "con4m.h"

// Different queue/list types.

static void
con4m_list_init(flexarray_t *list, va_list args)
{
    int64_t length = 0;

    karg_va_init(args);
    kw_int64("length", length);

    flexarray_init(list, length);
}

static void
con4m_queue_init(queue_t *list, va_list args)
{
    int64_t length = 0;

    karg_va_init(args);
    kw_int64("length", length);

    queue_init_size(list, (char)(64 - __builtin_clzll(length)));
}

static void
con4m_ring_init(hatring_t *ring, va_list args)
{
    int64_t length = 0;

    karg_va_init(args);
    kw_int64("length", length);

    hatring_init(ring, length);
}

static void
con4m_logring_init(logring_t *ring, va_list args)
{
    int64_t num_entries  = 0;
    int64_t entry_length = 1024;

    karg_va_init(args);
    kw_int64("num_entries", num_entries);
    kw_int64("entry_length", entry_length);

    logring_init(ring, num_entries, entry_length);
}

static void
con4m_stack_init(hatstack_t *stack, va_list args)
{
    int64_t prealloc = 128;

    karg_va_init(args);
    kw_int64("prealloc", prealloc);

    hatstack_init(stack, prealloc);
}

static void
con4m_list_marshal(flexarray_t *r, stream_t *s, dict_t *memos, int64_t *mid)
{
    type_spec_t *list_type   = get_my_type(r);
    xlist_t     *type_params = tspec_get_parameters(list_type);
    type_spec_t *item_type   = xlist_get(type_params, 0, NULL);
    dt_info     *item_info   = tspec_get_data_type_info(item_type);
    bool         by_val      = item_info->by_value;
    flex_view_t *view        = flexarray_view(r);
    uint64_t     len         = flexarray_view_len(view);

    marshal_u64(len, s);

    if (by_val) {
        for (uint64_t i = 0; i < len; i++) {
            marshal_u64((uint64_t)flexarray_view_next(view, NULL), s);
        }
    }
    else {
        for (uint64_t i = 0; i < len; i++) {
            con4m_sub_marshal(flexarray_view_next(view, NULL), s, memos, mid);
        }
    }
}

static void
con4m_list_unmarshal(flexarray_t *r, stream_t *s, dict_t *memos)
{
    type_spec_t *list_type   = get_my_type(r);
    xlist_t     *type_params = tspec_get_parameters(list_type);
    type_spec_t *item_type   = xlist_get(type_params, 0, NULL);
    dt_info     *item_info   = tspec_get_data_type_info(item_type);
    bool         by_val      = item_info->by_value;
    uint64_t     len         = unmarshal_u64(s);

    flexarray_init(r, len);

    if (by_val) {
        for (uint64_t i = 0; i < len; i++) {
            flexarray_set(r, i, (void *)unmarshal_u64(s));
        }
    }
    else {
        for (uint64_t i = 0; i < len; i++) {
            flexarray_set(r, i, con4m_sub_unmarshal(s, memos));
        }
    }
}

bool
list_can_coerce_to(type_spec_t *my_type, type_spec_t *dst_type)
{
    base_t base = type_spec_get_base(dst_type);

    if (base == T_BOOL) {
        return true;
    }

    if (base == T_LIST || base == T_XLIST) {
        type_spec_t *my_item  = tspec_get_param(my_type, 0);
        type_spec_t *dst_item = tspec_get_param(dst_type, 0);

        return con4m_can_coerce(my_item, dst_item);
    }

    return false;
}

static object_t
list_coerce_to(flexarray_t *list, type_spec_t *dst_type)
{
    base_t       base          = type_spec_get_base(dst_type);
    flex_view_t *view          = flexarray_view(list);
    int64_t      len           = flexarray_view_len(view);
    type_spec_t *src_item_type = tspec_get_param(get_my_type(list), 0);
    type_spec_t *dst_item_type = tspec_get_param(dst_type, 0);

    if (base == T_BOOL) {
        return (object_t)(int64_t)(flexarray_view_len(view) != 0);
    }

    if (base == T_LIST) {
        flexarray_t *res = con4m_new(dst_type, kw("length", ka(len)));

        for (int i = 0; i < len; i++) {
            void *item = flexarray_view_next(view, NULL);
            flexarray_set(res, i, con4m_coerce(item, src_item_type, dst_item_type));
        }

        return (object_t)res;
    }

    xlist_t *res = con4m_new(dst_type, kw("length", ka(len)));

    for (int i = 0; i < len; i++) {
        void *item = flexarray_view_next(view, NULL);
        xlist_set(res, i, con4m_coerce(item, src_item_type, dst_item_type));
    }

    return (object_t)res;
}

static flexarray_t *
list_copy(flexarray_t *list)
{
    flex_view_t *view = flexarray_view(list);
    int64_t      len  = flexarray_view_len(view);
    flexarray_t *res  = con4m_new(get_my_type((object_t)list),
                                 kw("length", ka(len)));

    for (int i = 0; i < len; i++) {
        object_t item = flexarray_view_next(view, NULL);
        flexarray_set(res, i, con4m_copy_object(item));
    }

    return res;
}

static void *
list_get(flexarray_t *list, int64_t index)
{
    object_t result;
    int      status;

    if (index < 0) {
        // Thing might get resized, so we have to take a view.
        flex_view_t *view = flexarray_view(list);
        int64_t      len  = flexarray_view_len(view);

        index += len;

        if (index < 0) {
            C4M_CRAISE("Array index out of bounds.");
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
        C4M_CRAISE("Array index out of bounds.");
    }
}

static void
list_set(flexarray_t *list, int64_t ix, void *item)
{
    if (!flexarray_set(list, ix, item)) {
        C4M_CRAISE("Array index out of bounds.");
    }
}

static flexarray_t *
list_get_slice(flexarray_t *list, int64_t start, int64_t end)
{
    flex_view_t *view = flexarray_view(list);
    int64_t      len  = flexarray_view_len(view);
    flexarray_t *res;

    if (start < 0) {
        start += len;
    }
    else {
        if (start >= len) {
            return con4m_new(get_my_type(list), kw("length", ka(0)));
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
        return con4m_new(get_my_type(list), kw("length", ka(0)));
    }

    len = end - start;
    res = con4m_new(get_my_type(list), kw("length", ka(len)));

    for (int i = 0; i < len; i++) {
        void *item = flexarray_view_get(view, start + i, NULL);
        flexarray_set(res, i, item);
    }

    return res;
}

// Semantics are, take a moment-in time view of each list (not the same moment),
// Compute the result, then replace the list. Definitely not atomic.
static void
list_set_slice(flexarray_t *list, int64_t start, int64_t end, flexarray_t *new)
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

    tmp = con4m_new(get_my_type(list), kw("length", ka(newlen)));

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

static any_str_t *
list_repr(flexarray_t *list, to_str_use_t how)
{
    type_spec_t *list_type   = get_my_type(list);
    xlist_t     *type_params = tspec_get_parameters(list_type);
    type_spec_t *item_type   = xlist_get(type_params, 0, NULL);
    flex_view_t *view        = flexarray_view(list);
    int64_t      len         = flexarray_view_len(view);
    xlist_t     *items       = con4m_new(tspec_xlist(tspec_utf32()));

    for (int i = 0; i < len; i++) {
        int   err  = 0;
        void *item = flexarray_view_get(view, i, &err);
        if (err) {
            continue;
        }
        any_str_t *s = con4m_repr(item, item_type, how);
        xlist_append(items, s);
    }

    any_str_t *sep    = c4m_get_comma_const();
    any_str_t *result = string_join(items, sep);

    if (how == TO_STR_USE_QUOTED) {
        result = string_concat(c4m_get_lbrak_const(),
                               string_concat(result, c4m_get_rbrak_const()));
    }

    return result;
}

const con4m_vtable list_vtable = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
        (con4m_vtable_entry)con4m_list_init,
        (con4m_vtable_entry)list_repr,
        NULL, // no finalizer
        (con4m_vtable_entry)con4m_list_marshal,
        (con4m_vtable_entry)con4m_list_unmarshal,
        (con4m_vtable_entry)list_can_coerce_to,
        (con4m_vtable_entry)list_coerce_to,
        NULL, // From lit,
        (con4m_vtable_entry)list_copy,
        (con4m_vtable_entry)flexarray_add,
        NULL, // Subtract
        NULL, // Mul
        NULL, // Div
        NULL, // MOD
        NULL, // EQ
        NULL, // LT
        NULL, // GT
        (con4m_vtable_entry)flexarray_len,
        (con4m_vtable_entry)list_get,
        (con4m_vtable_entry)list_set,
        (con4m_vtable_entry)list_get_slice,
        (con4m_vtable_entry)list_set_slice,
    }};

const con4m_vtable queue_vtable = {
    .num_entries = 1,
    .methods     = {
        (con4m_vtable_entry)con4m_queue_init}};

const con4m_vtable ring_vtable = {
    .num_entries = 1,
    .methods     = {
        (con4m_vtable_entry)con4m_ring_init}};

const con4m_vtable logring_vtable = {
    .num_entries = 1,
    .methods     = {
        (con4m_vtable_entry)con4m_logring_init}};

const con4m_vtable stack_vtable = {
    .num_entries = 1,
    .methods     = {
        (con4m_vtable_entry)con4m_stack_init}};

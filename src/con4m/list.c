#include "con4m.h"

static void
c4m_list_init(c4m_list_t *list, va_list args)
{
    int64_t length = 16;

    c4m_karg_va_init(args);
    c4m_kw_int64("length", length);

    list->append_ix    = 0;
    list->length       = c4m_max(length, 16);
    list->dont_acquire = false;
    pthread_rwlock_init(&list->lock, NULL);

    if (c4m_obj_item_type_is_value(list)) {
        list->data = c4m_gc_array_value_alloc(uint64_t *, length);
    }
    else {
        list->data = c4m_gc_array_alloc(uint64_t *, length);
    }
}

static inline void
c4m_list_resize(c4m_list_t *list, size_t len)
{
    if (!list->dont_acquire) {
        pthread_rwlock_wrlock(&list->lock);
    }
    int64_t **old = list->data;
    int64_t **new = c4m_gc_array_alloc(uint64_t *, len);

    for (int i = 0; i < list->length; i++) {
        new[i] = old[i];
    }

    list->data   = new;
    list->length = len;
    if (!list->dont_acquire) {
        pthread_rwlock_unlock(&list->lock);
    }
}

static inline void
list_auto_resize(c4m_list_t *list)
{
    c4m_list_resize(list, list->length << 1);
}

#define lock_list(x)                 \
    pthread_rwlock_wrlock(&x->lock); \
    x->dont_acquire = true

#define unlock_list(x)       \
    x->dont_acquire = false; \
    pthread_rwlock_unlock(&x->lock)

#define read_start(x) pthread_rwlock_rdlock(&x->lock);
#define read_end(x)   pthread_rwlock_unlock(&x->lock)

bool
c4m_list_set(c4m_list_t *list, int64_t ix, void *item)
{
    if (ix < 0) {
        ix += list->append_ix;
    }

    if (ix < 0) {
        return false;
    }

    lock_list(list);

    if (ix >= list->length) {
        c4m_list_resize(list, c4m_max(ix, list->length << 1));
    }

    if (ix >= list->append_ix) {
        list->append_ix = ix + 1;
    }

    list->data[ix] = (int64_t *)item;
    unlock_list(list);
    return true;
}

void
c4m_list_append(c4m_list_t *list, void *item)
{
    lock_list(list);
    if (list->append_ix >= list->length) {
        list_auto_resize(list);
    }

    list->data[list->append_ix++] = item;

    unlock_list(list);
    return;
}

static inline void *
c4m_list_get_base(c4m_list_t *list, int64_t ix, bool *err)
{
    if (!list) {
        if (err) {
            *err = true;
        }

        return NULL;
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

void *
c4m_list_get(c4m_list_t *list, int64_t ix, bool *err)
{
    lock_list(list);

    if (ix < 0) {
        ix += list->append_ix;
    }

    void *result = c4m_list_get_base(list, ix, err);
    unlock_list(list);

    return result;
}

void
c4m_list_add_if_unique(c4m_list_t *list,
                       void       *item,
                       bool (*fn)(void *, void *))
{
    lock_list(list);
    // Really meant to be internal for debugging sets; use sets instead.
    for (int i = 0; i < c4m_list_len(list); i++) {
        void *x = c4m_list_get_base(list, i, NULL);

        if ((*fn)(x, item)) {
            unlock_list(list);
            return;
        }
    }

    if (list->append_ix >= list->length) {
        list_auto_resize(list);
    }

    list->data[list->append_ix++] = item;

    unlock_list(list);
}

void *
c4m_list_pop(c4m_list_t *list)
{
    if (list->append_ix == 0) {
        C4M_CRAISE("Pop called on empty list.");
    }

    lock_list(list);
    return c4m_list_get_base(list, --list->append_ix, NULL);
    unlock_list(list);
}

void
c4m_list_plus_eq(c4m_list_t *l1, c4m_list_t *l2)
{
    if (l1 == NULL || l2 == NULL) {
        return;
    }

    lock_list(l1);
    read_start(l2);

    int needed = l1->append_ix + l2->append_ix;

    if (needed > l1->length) {
        c4m_list_resize(l1, needed);
    }

    for (int i = 0; i < l2->append_ix; i++) {
        l1->data[l1->append_ix++] = l2->data[i];
    }

    read_end(l2);
    unlock_list(l1);
}

c4m_list_t *
c4m_list_plus(c4m_list_t *l1, c4m_list_t *l2)
{
    // This assumes type checking already happened statically.
    // You can make mistakes manually.
    c4m_list_t *result;

    if (l1 == NULL && l2 == NULL) {
        return NULL;
    }
    if (l1 == NULL) {
        result = c4m_list_shallow_copy(l2);
        return result;
    }
    if (l2 == NULL) {
        result = c4m_list_shallow_copy(l1);
        return result;
    }

    read_start(l1);
    read_start(l2);
    c4m_type_t *t      = c4m_get_my_type(l1);
    size_t      needed = l1->append_ix + l2->append_ix;
    result             = c4m_new(t, c4m_kw("length", c4m_ka(needed)));

    for (int i = 0; i < l1->append_ix; i++) {
        result->data[i] = l1->data[i];
    }

    result->append_ix = l1->append_ix;

    for (int i = 0; i < l2->append_ix; i++) {
        result->data[result->append_ix++] = l2->data[i];
    }
    read_end(l2);
    read_end(l1);

    return result;
}

void
c4m_list_marshal(c4m_list_t *r, c4m_stream_t *s, c4m_dict_t *memos, int64_t *mid)
{
    c4m_type_t    *list_type   = c4m_get_my_type(r);
    c4m_list_t    *type_params = c4m_type_get_params(list_type);
    c4m_type_t    *item_type   = c4m_list_get_base(type_params, 0, NULL);
    c4m_dt_info_t *item_info   = c4m_type_get_data_type_info(item_type);
    bool           by_val      = item_info->by_value;

    read_start(r);
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
    read_end(r);
}

void
c4m_list_unmarshal(c4m_list_t *r, c4m_stream_t *s, c4m_dict_t *memos)
{
    c4m_type_t    *list_type   = c4m_get_my_type(r);
    c4m_list_t    *type_params = c4m_type_get_params(list_type);
    c4m_type_t    *item_type   = c4m_list_get_base(type_params, 0, NULL);
    c4m_dt_info_t *item_info   = item_type ? c4m_type_get_data_type_info(item_type) : NULL;
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
c4m_list_len(const c4m_list_t *list)
{
    if (list == NULL) {
        return 0;
    }
    return (int64_t)list->append_ix;
}

c4m_list_t *
c4m_list(c4m_type_t *x)
{
    return c4m_new(c4m_type_list(x));
}

static c4m_str_t *
c4m_list_repr(c4m_list_t *list)
{
    read_start(list);

    c4m_type_t *list_type   = c4m_get_my_type(list);
    c4m_list_t *type_params = c4m_type_get_params(list_type);
    c4m_type_t *item_type   = c4m_list_get_base(type_params, 0, NULL);
    int64_t     len         = c4m_list_len(list);
    c4m_list_t *items       = c4m_new(c4m_type_list(c4m_type_utf32()));

    for (int i = 0; i < len; i++) {
        bool  err  = false;
        void *item = c4m_list_get_base(list, i, &err);
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

    read_end(list);

    return result;
}

static c4m_obj_t
c4m_list_coerce_to(c4m_list_t *list, c4m_type_t *dst_type)
{
    read_start(list);

    c4m_obj_t     result;
    c4m_dt_kind_t base          = c4m_type_get_base(dst_type);
    c4m_type_t   *src_item_type = c4m_type_get_param(c4m_get_my_type(list), 0);
    c4m_type_t   *dst_item_type = c4m_type_get_param(dst_type, 0);
    int64_t       len           = c4m_list_len(list);

    if (base == (c4m_dt_kind_t)C4M_T_BOOL) {
        result = (c4m_obj_t)(int64_t)(c4m_list_len(list) != 0);

        read_end(list);

        return result;
    }

    if (base == (c4m_dt_kind_t)C4M_T_LIST) {
        c4m_list_t *res = c4m_new(dst_type, c4m_kw("length", c4m_ka(len)));

        for (int i = 0; i < len; i++) {
            void *item = c4m_list_get_base(list, i, NULL);
            c4m_list_set(res,
                         i,
                         c4m_coerce(item, src_item_type, dst_item_type));
        }

        read_end(list);
        return (c4m_obj_t)res;
    }

    if (base == (c4m_dt_kind_t)C4M_T_FLIST) {
        flexarray_t *res = c4m_new(dst_type, c4m_kw("length", c4m_ka(len)));

        for (int i = 0; i < len; i++) {
            void *item = c4m_list_get_base(list, i, NULL);
            flexarray_set(res,
                          i,
                          c4m_coerce(item, src_item_type, dst_item_type));
        }

        return (c4m_obj_t)res;
    }
    c4m_unreachable();
}

c4m_list_t *
c4m_list_copy(c4m_list_t *list)
{
    read_start(list);

    int64_t     len       = c4m_list_len(list);
    c4m_type_t *my_type   = c4m_get_my_type((c4m_obj_t)list);
    c4m_type_t *item_type = c4m_type_get_param(my_type, 0);
    c4m_list_t *res       = c4m_new(my_type, c4m_kw("length", c4m_ka(len)));

    for (int i = 0; i < len; i++) {
        c4m_obj_t item = c4m_list_get_base(list, i, NULL);
        c4m_list_set(res, i, c4m_copy_object_of_type(item, item_type));
    }

    read_end(list);

    return res;
}

c4m_list_t *
c4m_list_shallow_copy(c4m_list_t *list)
{
    read_start(list);

    int64_t     len     = c4m_list_len(list);
    c4m_type_t *my_type = c4m_get_my_type((c4m_obj_t)list);
    c4m_list_t *res     = c4m_new(my_type, c4m_kw("length", c4m_ka(len)));

    for (int i = 0; i < len; i++) {
        c4m_list_set(res, i, c4m_list_get_base(list, i, NULL));
    }

    read_end(list);

    return res;
}

static c4m_obj_t
c4m_list_safe_get(c4m_list_t *list, int64_t ix)
{
    bool err = false;

    read_start(list);

    c4m_obj_t result = c4m_list_get_base(list, ix, &err);

    if (err) {
        c4m_utf8_t *msg = c4m_cstr_format(
            "Array index out of bounds "
            "(ix = {}; size = {})",
            c4m_box_i64(ix),
            c4m_box_i64(c4m_list_len(list)));

        read_end(list);
        C4M_RAISE(msg);
    }

    read_end(list);
    return result;
}

c4m_list_t *
c4m_list_get_slice(c4m_list_t *list, int64_t start, int64_t end)
{
    read_start(list);

    int64_t     len = c4m_list_len(list);
    c4m_list_t *res;

    if (start < 0) {
        start += len;
    }
    else {
        if (start >= len) {
            read_end(list);
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
        read_end(list);
        return c4m_new(c4m_get_my_type(list), c4m_kw("length", c4m_ka(0)));
    }

    len = end - start;
    res = c4m_new(c4m_get_my_type(list), c4m_kw("length", c4m_ka(len)));

    for (int i = 0; i < len; i++) {
        void *item = c4m_list_get_base(list, start + i, NULL);
        c4m_list_set(res, i, item);
    }

    read_end(list);
    return res;
}

void
c4m_list_set_slice(c4m_list_t *list,
                   int64_t     start,
                   int64_t     end,
                   c4m_list_t *new)
{
    lock_list(list);
    read_start(new);
    int64_t len1 = c4m_list_len(list);
    int64_t len2 = c4m_list_len(new);

    if (start < 0) {
        start += len1;
    }
    else {
        if (start >= len1) {
            read_end(new);
            unlock_list(list);
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
        read_end(new);
        unlock_list(list);
        C4M_CRAISE("Out of bounds slice.");
    }

    int64_t slicelen = end - start;
    int64_t newlen   = len1 + len2 - slicelen;

    void **newdata = c4m_gc_array_alloc(void *, newlen);

    if (start > 0) {
        for (int i = 0; i < start; i++) {
            void *item = c4m_list_get_base(list, i, NULL);
            newdata[i] = item;
        }
    }

    for (int i = 0; i < len2; i++) {
        void *item       = c4m_list_get_base(new, i, NULL);
        newdata[start++] = item;
    }

    for (int i = end; i < len1; i++) {
        void *item       = c4m_list_get_base(list, i, NULL);
        newdata[start++] = item;
    }

    list->data      = (int64_t **)newdata;
    list->append_ix = start;

    read_end(new);
    unlock_list(list);
}

bool
c4m_list_contains(c4m_list_t *list, c4m_obj_t item)
{
    read_start(list);

    int64_t     len       = c4m_list_len(list);
    c4m_type_t *item_type = c4m_get_my_type(item);

    for (int i = 0; i < len; i++) {
        if (!item_type) {
            // Don't know why ref is giving me no item type yet,
            // so this is a tmp fix.
            read_end(list);
            return item == c4m_list_get_base(list, i, NULL);
        }

        if (c4m_eq(item_type, item, c4m_list_get_base(list, i, NULL))) {
            read_end(list);
            return true;
        }
    }

    read_end(list);
    return false;
}

static void *
c4m_list_view(c4m_list_t *list, uint64_t *n)
{
    read_start(list);

    void **view;
    int    len = c4m_list_len(list);

    if (c4m_obj_item_type_is_value(list)) {
        view = c4m_gc_array_value_alloc(void *, len);
    }
    else {
        view = c4m_gc_array_alloc(void *, len);
    }

    for (int i = 0; i < len; i++) {
        view[i] = c4m_list_get_base(list, i, NULL);
    }

    read_end(list);

    *n = len;

    return view;
}
static c4m_list_t *
c4m_to_list_lit(c4m_type_t *objtype, c4m_list_t *items, c4m_utf8_t *litmod)
{
    c4m_base_obj_t *hdr = c4m_object_header((c4m_obj_t)items);
    hdr->concrete_type  = objtype;
    return items;
}

extern bool c4m_flexarray_can_coerce_to(c4m_type_t *, c4m_type_t *);

static void
c4m_list_set_gc_bits(uint64_t *bitfield, int alloc_words)
{
    int ix;
    c4m_set_object_header_bits(bitfield, &ix);
    c4m_set_bit(bitfield, ix);
}

const c4m_vtable_t c4m_list_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR]   = (c4m_vtable_entry)c4m_list_init,
        [C4M_BI_MARSHAL]       = (c4m_vtable_entry)c4m_list_marshal,
        [C4M_BI_UNMARSHAL]     = (c4m_vtable_entry)c4m_list_unmarshal,
        [C4M_BI_COERCIBLE]     = (c4m_vtable_entry)c4m_flexarray_can_coerce_to,
        [C4M_BI_COERCE]        = (c4m_vtable_entry)c4m_list_coerce_to,
        [C4M_BI_COPY]          = (c4m_vtable_entry)c4m_list_copy,
        [C4M_BI_ADD]           = (c4m_vtable_entry)c4m_list_plus,
        [C4M_BI_LEN]           = (c4m_vtable_entry)c4m_list_len,
        [C4M_BI_INDEX_GET]     = (c4m_vtable_entry)c4m_list_safe_get,
        [C4M_BI_INDEX_SET]     = (c4m_vtable_entry)c4m_list_set,
        [C4M_BI_SLICE_GET]     = (c4m_vtable_entry)c4m_list_get_slice,
        [C4M_BI_SLICE_SET]     = (c4m_vtable_entry)c4m_list_set_slice,
        [C4M_BI_VIEW]          = (c4m_vtable_entry)c4m_list_view,
        [C4M_BI_CONTAINER_LIT] = (c4m_vtable_entry)c4m_to_list_lit,
        [C4M_BI_REPR]          = (c4m_vtable_entry)c4m_list_repr,
        [C4M_BI_GC_MAP]        = (c4m_vtable_entry)c4m_list_set_gc_bits,
        // Explicit because some compilers don't seem to always properly
        // zero it (Was sometimes crashing on a `c4m_stream_t` on my mac).
        [C4M_BI_FINALIZER]     = NULL,
    },
};

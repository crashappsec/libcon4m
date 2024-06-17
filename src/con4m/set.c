#include "con4m.h"

static int
hatrack_set_epoch_sort_cmp(const void *b1, const void *b2)
{
    hatrack_set_view_t *item1;
    hatrack_set_view_t *item2;

    item1 = (hatrack_set_view_t *)b1;
    item2 = (hatrack_set_view_t *)b2;

    return item2->sort_epoch - item1->sort_epoch;
}

static int
hatrack_set_hv_sort_cmp(const void *b1, const void *b2)
{
    hatrack_set_view_t *item1;
    hatrack_set_view_t *item2;

    item1 = (hatrack_set_view_t *)b1;
    item2 = (hatrack_set_view_t *)b2;

    if (hatrack_hash_gt(item1->hv, item2->hv)) {
        return 1;
    }

    if (hatrack_hashes_eq(item1->hv, item2->hv)) {
        abort(); // Shouldn't happen; hash entries should be unique.
    }

    return -1;
}

static void
c4m_set_init(c4m_set_t *set, va_list args)
{
    size_t         hash_fn;
    c4m_type_t    *stype = c4m_get_my_type(set);
    c4m_dt_info_t *info;

    if (stype != NULL) {
        stype   = c4m_xlist_get(c4m_tspec_get_parameters(stype), 0, NULL);
        info    = c4m_tspec_get_data_type_info(stype);
        hash_fn = info->hash_fn;
    }
    else {
        hash_fn = (uint32_t)va_arg(args, size_t);
    }

    hatrack_set_init(set, hash_fn);

    switch (hash_fn) {
    case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
        hatrack_set_set_hash_offset(set, 2 * (int32_t)sizeof(uint64_t));
        /* fallthrough */
    case HATRACK_DICT_KEY_TYPE_OBJ_PTR:
    case HATRACK_DICT_KEY_TYPE_OBJ_INT:
    case HATRACK_DICT_KEY_TYPE_OBJ_REAL:
        hatrack_set_set_cache_offset(set, -2 * (int32_t)sizeof(uint64_t));
        break;
    default:
        // nada.
    }
}

// Same container challenge as with other types, for values anyway.
// For keys, we leverage the key_type field being CSTR or PTR.
// We don't use the OBJ_ options currently. We will use that
// for strings at some point soon though.

static void
c4m_set_marshal(c4m_set_t *d, c4m_stream_t *s, c4m_dict_t *memos, int64_t *mid)
{
    uint64_t length;
    uint8_t  kt   = (uint8_t)d->item_type;
    void   **view = c4m_set_items_sort(d, &length);

    c4m_marshal_u32((uint32_t)length, s);
    c4m_marshal_u8(kt, s);

    for (uint64_t i = 0; i < length; i++) {
        switch (kt) {
        case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
        case HATRACK_DICT_KEY_TYPE_OBJ_PTR:
            c4m_sub_marshal(view[i], s, memos, mid);
            break;
        case HATRACK_DICT_KEY_TYPE_CSTR:
            c4m_marshal_cstring(view[i], s);
            break;
        default:
            c4m_marshal_u64((uint64_t)view[i], s);
            break;
        }
    }
}

static void
c4m_set_unmarshal(c4m_set_t *d, c4m_stream_t *s, c4m_dict_t *memos)
{
    uint32_t length;
    uint8_t  kt;

    length = c4m_unmarshal_u32(s);
    kt     = c4m_unmarshal_u8(s);

    hatrack_set_init(d, (uint32_t)kt);

    switch (kt) {
    case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
        hatrack_set_set_hash_offset(d, sizeof(uint64_t) * 2);
        /* fallthrough */
    case HATRACK_DICT_KEY_TYPE_PTR:
        hatrack_set_set_cache_offset(d, (int32_t)(-sizeof(uint64_t) * 2));
        break;
    default:
        // nada.
    }

    for (uint32_t i = 0; i < length; i++) {
        void *key;

        switch (kt) {
        case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
        case HATRACK_DICT_KEY_TYPE_OBJ_PTR:
            key = c4m_sub_unmarshal(s, memos);
            break;
        case HATRACK_DICT_KEY_TYPE_CSTR:
            key = c4m_unmarshal_cstring(s);
            break;
        default:
            key = (void *)c4m_unmarshal_u64(s);
            break;
        }

        c4m_set_add(d, key);
    }
}

c4m_set_t *
c4m_set_shallow_copy(c4m_set_t *s)
{
    if (s == NULL) {
        return NULL;
    }

    c4m_set_t *result = c4m_new(c4m_get_my_type(s));
    uint64_t   count  = 0;
    void     **items  = (void **)c4m_set_items_sort(s, &count);

    for (uint64_t i = 0; i < count; i++) {
        c4m_set_add(result, items[i]);
    }

    return result;
}

c4m_xlist_t *
c4m_set_to_xlist(c4m_set_t *s)
{
    if (s == NULL) {
        return NULL;
    }

    c4m_type_t  *item_type = c4m_tspec_get_param(c4m_get_my_type(s), 0);
    c4m_xlist_t *result    = c4m_new(c4m_tspec_xlist(item_type));
    uint64_t     count     = 0;
    void       **items     = (void **)c4m_set_items_sort(s, &count);

    for (uint64_t i = 0; i < count; i++) {
        c4m_xlist_append(result, items[i]);
    }

    return result;
}

/* c4m_set_union(A, B)
 *
 * Returns a new set that consists of all the items from both sets, at
 * the moment in time of the call (as defined by the epoch).
 */

c4m_set_t *
c4m_set_union(c4m_set_t *set1, c4m_set_t *set2)
{
    c4m_set_t          *ret;
    uint64_t            epoch;
    uint64_t            num1;
    uint64_t            num2;
    hatrack_set_view_t *view1;
    hatrack_set_view_t *view2;
    uint64_t            i, j;

    mmm_thread_t *thread = mmm_thread_acquire();

    ret   = c4m_new(c4m_get_my_type(set1));
    epoch = mmm_start_linearized_op(thread);

    view1 = woolhat_view_epoch(&set1->woolhat_instance, &num1, epoch);
    view2 = woolhat_view_epoch(&set2->woolhat_instance, &num2, epoch);

    qsort(view1, num1, sizeof(hatrack_set_view_t), hatrack_set_epoch_sort_cmp);
    qsort(view2, num2, sizeof(hatrack_set_view_t), hatrack_set_epoch_sort_cmp);

    /* Here we're going to add from each array based on the insertion
     * epoch, to preserve insertion ordering.
     */
    i = 0;
    j = 0;

    while ((i < num1) && (j < num2)) {
        if (view1[i].sort_epoch < view2[j].sort_epoch) {
            if (woolhat_add_mmm(&ret->woolhat_instance, thread, view1[i].hv, view1[i].item)
                && set1->pre_return_hook) {
                (*set1->pre_return_hook)(set1, view1[i].item);
            }
            i++;
        }
        else {
            if (woolhat_add_mmm(&ret->woolhat_instance, thread, view2[j].hv, view2[j].item)
                && set2->pre_return_hook) {
                (*set2->pre_return_hook)(set2, view2[j].item);
            }

            j++;
        }
    }

    while (i < num1) {
        if (woolhat_add_mmm(&ret->woolhat_instance, thread, view1[i].hv, view1[i].item)
            && set1->pre_return_hook) {
            (*set1->pre_return_hook)(set1, view1[i].item);
        }
        i++;
    }

    while (j < num2) {
        if (woolhat_add_mmm(&ret->woolhat_instance, thread, view2[j].hv, view2[j].item)
            && set2->pre_return_hook) {
            (*set2->pre_return_hook)(set2, view2[j].item);
        }
        j++;
    }

    mmm_end_op(thread);

    return ret;
}

/* c4m_set_intersection(A, B)
 *
 * Returns a new set that consists of only the items that exist in
 * both sets at the time of the call (as defined by the epoch).
 *
 * This does NOT currently preserve insertion ordering the way that
 * hatrack_set_union() does. It could, if we first mark what gets
 * copied and what doesn't, then re-sort based on original epoch.
 *
 * But, meh.
 *
 * The basic algorithm is to sort both views by hash value, then
 * march through them in tandem.
 *
 * If we see two hash values are equal, add the item to the new set
 * and advance to the next item in both sets.
 *
 * Otherwise, the item with the lower hash value definitely is NOT
 * in the intersection (since the items are sorted by hash value).
 * Advance to the next item in that view.
 *
 * Once one view ends, there are no more items in the intersection.
 */
c4m_set_t *
c4m_set_intersection(c4m_set_t *set1, c4m_set_t *set2)
{
    c4m_set_t          *ret;
    uint64_t            epoch;
    uint64_t            num1;
    uint64_t            num2;
    hatrack_set_view_t *view1;
    hatrack_set_view_t *view2;
    uint64_t            i, j;

    mmm_thread_t *thread = mmm_thread_acquire();

    ret   = c4m_new(c4m_get_my_type(set1));
    epoch = mmm_start_linearized_op(thread);

    view1 = woolhat_view_epoch(&set1->woolhat_instance, &num1, epoch);
    view2 = woolhat_view_epoch(&set2->woolhat_instance, &num2, epoch);

    qsort(view1, num1, sizeof(hatrack_set_view_t), hatrack_set_hv_sort_cmp);
    qsort(view2, num2, sizeof(hatrack_set_view_t), hatrack_set_hv_sort_cmp);

    i = 0;
    j = 0;

    while ((i < num1) && (j < num2)) {
        if (hatrack_hashes_eq(view1[i].hv, view2[j].hv)) {
            if (set1->pre_return_hook) {
                (*set1->pre_return_hook)(set1, view1[i].item);
            }

            woolhat_add_mmm(&ret->woolhat_instance, thread, view1[i].hv, view1[i].item);
            i++;
            j++;
            continue;
        }

        if (hatrack_hash_gt(view1[i].hv, view2[j].hv)) {
            j++;
        }

        else {
            i++;
        }
    }

    mmm_end_op(thread);

    return ret;
}

/* hatrack_set_disjunction(A, B)
 *
 * Returns a new set that contains items in set A that did not exist
 * in set B, PLUS the items in set B that did not exist in set A.
 *
 * Like intersection, this does not currently preserve intersection
 * order.
 *
 * The algorithm here is to sort by hash value, then go through in
 * tandem. If the item at the current index in one view has a lower
 * hash value than the item in the current index of the other view,
 * then that item is part of the disjunction.
 */
c4m_set_t *
c4m_set_disjunction(c4m_set_t *set1, c4m_set_t *set2)
{
    c4m_set_t          *ret;
    uint64_t            epoch;
    uint64_t            num1;
    uint64_t            num2;
    hatrack_set_view_t *view1;
    hatrack_set_view_t *view2;
    uint64_t            i, j;

    mmm_thread_t *thread = mmm_thread_acquire();

    ret   = c4m_new(c4m_get_my_type(set1));
    epoch = mmm_start_linearized_op(thread);

    view1 = woolhat_view_epoch(&set1->woolhat_instance, &num1, epoch);
    view2 = woolhat_view_epoch(&set2->woolhat_instance, &num2, epoch);

    qsort(view1, num1, sizeof(hatrack_set_view_t), hatrack_set_hv_sort_cmp);
    qsort(view2, num2, sizeof(hatrack_set_view_t), hatrack_set_hv_sort_cmp);

    i = 0;
    j = 0;

    while ((i < num1) && (j < num2)) {
        if (hatrack_hashes_eq(view1[i].hv, view2[j].hv)) {
            i++;
            j++;
            continue;
        }

        if (hatrack_hash_gt(view1[i].hv, view2[j].hv)) {
            if (set2->pre_return_hook) {
                (*set2->pre_return_hook)(set2, view2[j].item);
            }

            woolhat_add_mmm(&ret->woolhat_instance, thread, view2[j].hv, view2[j].item);
            j++;
        }

        else {
            if (set1->pre_return_hook) {
                (*set1->pre_return_hook)(set1, view1[i].item);
            }

            woolhat_add_mmm(&ret->woolhat_instance, thread, view1[i].hv, view1[i].item);
            i++;
        }
    }

    mmm_end_op(thread);

    return ret;
}

static c4m_set_t *
to_set_lit(c4m_type_t *objtype, c4m_xlist_t *items, c4m_utf8_t *litmod)
{
    c4m_set_t *result = c4m_new(objtype);
    int        n      = c4m_xlist_len(items);

    for (int i = 0; i < n; i++) {
        hatrack_set_add(result, c4m_xlist_get(items, i, NULL));
    }

    return result;
}

const c4m_vtable_t c4m_set_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR]   = (c4m_vtable_entry)c4m_set_init,
        [C4M_BI_MARSHAL]       = (c4m_vtable_entry)c4m_set_marshal,
        [C4M_BI_UNMARSHAL]     = (c4m_vtable_entry)c4m_set_unmarshal,
        [C4M_BI_VIEW]          = (c4m_vtable_entry)c4m_set_items_sort,
        [C4M_BI_CONTAINER_LIT] = (c4m_vtable_entry)to_set_lit,
    },
};

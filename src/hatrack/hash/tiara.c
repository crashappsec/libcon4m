/*
 * Copyright © 2021-2022 John Viega
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *  Name:           tiara.c
 *
 *  Description:    This Is A Rediculous Acronym.
 *
 *                  This is roughly in the hihat family, but uses
 *                  64-bit hash values, which we do not generally
 *                  recommend. However, it allows us to show off an
 *                  algorithm that requires only a single
 *                  compare-and-swap per core operation.
 *
 *                  Note that this is not full-featured relative to
 *                  some of our other hash algorithms. For instance,
 *                  we don't use a bool * to pass information about
 *                  whether an item is present or not.
 *
 *                  Similarly, as we've moved to an atomic OR for
 *                  setting migration flags, we've not updated that
 *                  here.
 *
 *  Author:         John Viega, john@zork.org
 *
 */

#include "hatrack/tiara.h"
#include "hatrack/malloc.h"
#include "hatrack/hatomic.h"
#include "../hatrack-internal.h"

#include <stdlib.h>

// clang-format off
static tiara_store_t *tiara_store_new    (uint64_t);
static void          *tiara_store_get    (tiara_store_t *, uint64_t);
static void          *tiara_store_put    (tiara_store_t *, mmm_thread_t *, tiara_t *, uint64_t, void *);
static void          *tiara_store_replace(tiara_store_t *, mmm_thread_t *, tiara_t *, uint64_t, void *);
static bool           tiara_store_add    (tiara_store_t *, mmm_thread_t *, tiara_t *, uint64_t, void *);
static void          *tiara_store_remove (tiara_store_t *, mmm_thread_t *, tiara_t *, uint64_t);
static tiara_store_t *tiara_store_migrate(tiara_store_t *, mmm_thread_t *, tiara_t *);
// clang-format on

enum64(tiara_flag_t,
       TIARA_F_MOVING = 0x0000000000000001,
       TIARA_F_MOVED  = 0x0000000000000002,
       TIARA_F_USED   = 0x0000000000000004,
       TIARA_F_ALL    = TIARA_F_MOVING | TIARA_F_MOVED | TIARA_F_USED);

tiara_t *
tiara_new(void)
{
    tiara_t *ret;

    ret = (tiara_t *)hatrack_malloc(sizeof(tiara_t));

    tiara_init(ret);

    return ret;
}

tiara_t *
tiara_new_size(char size)
{
    tiara_t *ret;

    ret = (tiara_t *)hatrack_malloc(sizeof(tiara_t));

    tiara_init_size(ret, size);

    return ret;
}

void
tiara_init(tiara_t *self)
{
    tiara_init_size(self, HATRACK_MIN_SIZE_LOG);

    return;
}

void
tiara_init_size(tiara_t *self, char size)
{
    tiara_store_t *store;
    uint64_t       len;

    if (((size_t)size) > (sizeof(intptr_t) * 8)) {
        hatrack_panic("invalid size in tiara_init_size");
    }

    if (size < HATRACK_MIN_SIZE_LOG) {
        hatrack_panic("invalid size in tiara_init_size");
    }

    len   = 1 << size;
    store = tiara_store_new(len);

    atomic_store(&self->store_current, store);
    atomic_store(&self->item_count, 0);

    return;
}

void
tiara_cleanup(tiara_t *self)
{
    mmm_retire_unused(atomic_load(&self->store_current));

    return;
}

void
tiara_delete(tiara_t *self)
{
    tiara_cleanup(self);
    hatrack_free(self, sizeof(tiara_t));

    return;
}

void *
tiara_get_mmm(tiara_t *self, mmm_thread_t *thread, uint64_t hv)
{
    void          *ret;
    tiara_store_t *store;

    mmm_start_basic_op(thread);

    store = atomic_read(&self->store_current);
    ret   = tiara_store_get(store, hv);

    mmm_end_op(thread);

    return ret;
}

void *
tiara_get(tiara_t *self, uint64_t hv)
{
    return tiara_get_mmm(self, mmm_thread_acquire(), hv);
}

void *
tiara_put_mmm(tiara_t *self, mmm_thread_t *thread, uint64_t hv, void *item)
{
    void          *ret;
    tiara_store_t *store;

    mmm_start_basic_op(thread);

    store = atomic_read(&self->store_current);
    ret   = tiara_store_put(store, thread, self, hv, item);

    mmm_end_op(thread);

    return ret;
}

void *
tiara_put(tiara_t *self, uint64_t hv, void *item)
{
    return tiara_put_mmm(self, mmm_thread_acquire(), hv, item);
}

void *
tiara_replace_mmm(tiara_t *self, mmm_thread_t *thread, uint64_t hv, void *item)
{
    void          *ret;
    tiara_store_t *store;

    mmm_start_basic_op(thread);

    store = atomic_read(&self->store_current);
    ret   = tiara_store_replace(store, thread, self, hv, item);

    mmm_end_op(thread);

    return ret;
}

void *
tiara_replace(tiara_t *self, uint64_t hv, void *item)
{
    return tiara_replace_mmm(self, mmm_thread_acquire(), hv, item);
}

bool
tiara_add_mmm(tiara_t *self, mmm_thread_t *thread, uint64_t hv, void *item)
{
    bool           ret;
    tiara_store_t *store;

    mmm_start_basic_op(thread);

    store = atomic_read(&self->store_current);
    ret   = tiara_store_add(store, thread, self, hv, item);

    mmm_end_op(thread);

    return ret;
}

bool
tiara_add(tiara_t *self, uint64_t hv, void *item)
{
    return tiara_add_mmm(self, mmm_thread_acquire(), hv, item);
}

void *
tiara_remove_mmm(tiara_t *self, mmm_thread_t *thread, uint64_t hv)
{
    void          *ret;
    tiara_store_t *store;

    mmm_start_basic_op(thread);

    store = atomic_read(&self->store_current);
    ret   = tiara_store_remove(store, thread, self, hv);

    mmm_end_op(thread);

    return ret;
}

void *
tiara_remove(tiara_t *self, uint64_t hv)
{
    return tiara_remove_mmm(self, mmm_thread_acquire(), hv);
}

uint64_t
tiara_len(tiara_t *self)
{
    return atomic_read(&self->item_count);
}

uint64_t
tiara_len_mmm(tiara_t *self, mmm_thread_t *thread)
{
    return tiara_len(self);
}

hatrack_view_t *
tiara_view_mmm(tiara_t *self, mmm_thread_t *thread, uint64_t *num, bool ignored)
{
    hatrack_view_t *view;
    hatrack_view_t *p;
    tiara_bucket_t *cur;
    tiara_bucket_t *end;
    tiara_record_t  record;
    uint64_t        num_items;
    uint64_t        alloc_len;
    tiara_store_t  *store;

    mmm_start_basic_op(thread);

    store     = atomic_read(&self->store_current);
    alloc_len = sizeof(hatrack_view_t) * (store->last_slot + 1);
    view      = (hatrack_view_t *)hatrack_malloc(alloc_len);
    p         = view;
    cur       = store->buckets;
    end       = cur + (store->last_slot + 1);

    while (cur < end) {
        record = atomic_read(cur);

        if (!hatrack_pflag_test(record.item, TIARA_F_USED)) {
            cur++;
            continue;
        }

        p->sort_epoch = 0;
        p->item       = record.item;

        p++;
        cur++;
    }

    num_items = p - view;
    *num      = num_items;

    if (!num_items) {
        hatrack_free(view, alloc_len);
        mmm_end_op(thread);

        return NULL;
    }

    view = hatrack_realloc(view, alloc_len, num_items * sizeof(hatrack_view_t));

    mmm_end_op(thread);

    return view;
}

hatrack_view_t *
tiara_view(tiara_t *self, uint64_t *num, bool ignored)
{
    return tiara_view_mmm(self, mmm_thread_acquire(), num, ignored);
}

static tiara_store_t *
tiara_store_new(uint64_t size)
{
    tiara_store_t *store;
    uint64_t       alloc_len;

    alloc_len        = sizeof(tiara_store_t) + sizeof(tiara_bucket_t) * size;
    store            = (tiara_store_t *)mmm_alloc_committed(alloc_len);
    store->last_slot = size - 1;
    store->threshold = hatrack_compute_table_threshold(size);

    return store;
}

static void *
tiara_store_get(tiara_store_t *self, uint64_t hv)
{
    uint64_t       bix;
    uint64_t       i;
    tiara_record_t record;

    bix = hv & self->last_slot;

    for (i = 0; i <= self->last_slot; i++) {
        record = atomic_read(&self->buckets[bix]);

        if (!record.hv) {
            return NULL;
        }

        if (record.hv != hv) {
            bix = (bix + 1) & self->last_slot;
            continue;
        }

        // If it's deleted, the clear will return null.
        return hatrack_pflag_clear(record.item, TIARA_F_ALL);
    }

    return NULL;
}

static void *
tiara_store_put(tiara_store_t *self, mmm_thread_t *thread, tiara_t *top, uint64_t hv, void *item)
{
    uint64_t       bix;
    uint64_t       i;
    tiara_record_t record;
    tiara_record_t candidate;

    bix = hv & self->last_slot;

    for (i = 0; i <= self->last_slot; i++) {
        record         = atomic_load(&self->buckets[bix]);
        candidate.hv   = hv;
        candidate.item = hatrack_pflag_set(item, TIARA_F_USED);

        if (!record.hv) {
            if (CAS(&self->buckets[bix], &record, candidate)) {
                if (atomic_fetch_add(&self->used_count, 1) >= self->threshold) {
                    tiara_store_migrate(self, thread, top);
                    return NULL;
                }
                return NULL;
            }
            // Someone beat us to the record. Fall through to see if it's
            // got the same hash value as us.
        }

        if (!(record.hv == hv)) {
            bix = (bix + 1) & self->last_slot;
            continue;
        }

        if (hatrack_pflag_test(record.item, TIARA_F_MOVING)) {
            break;
        }

        if (CAS(&self->buckets[bix], &record, candidate)) {
            return hatrack_pflag_clear(record.item, TIARA_F_ALL);
        }

        if (hatrack_pflag_test(record.item, TIARA_F_MOVING)) {
            break;
        }

        return NULL;
    }

    self = tiara_store_migrate(self, thread, top);
    return tiara_store_put(self, thread, top, hv, item);
}

static void *
tiara_store_replace(tiara_store_t *self, mmm_thread_t *thread, tiara_t *top, uint64_t hv, void *item)
{
    uint64_t       bix;
    uint64_t       i;
    tiara_record_t record;
    tiara_record_t candidate;

    bix = hv & self->last_slot;

    for (i = 0; i <= self->last_slot; i++) {
        record = atomic_load(&self->buckets[bix]);

        if (!record.hv) {
            return NULL;
        }

        if (!(record.hv == hv)) {
            bix = (bix + 1) & self->last_slot;
            continue;
        }

        if (hatrack_pflag_test(record.item, TIARA_F_MOVING)) {
migrate_and_retry:
            self = tiara_store_migrate(self, thread, top);
            return tiara_store_replace(self, thread, top, hv, item);
        }

        candidate.hv   = hv;
        candidate.item = hatrack_pflag_set(item, TIARA_F_USED);

        if (CAS(&self->buckets[bix], &record, candidate)) {
            return hatrack_pflag_clear(record.item, TIARA_F_ALL);
        }

        if (hatrack_pflag_test(record.item, TIARA_F_MOVING)) {
            goto migrate_and_retry;
        }

        return NULL;
    }

    return NULL;
}

static bool
tiara_store_add(tiara_store_t *self, mmm_thread_t *thread, tiara_t *top, uint64_t hv, void *item)
{
    uint64_t       bix;
    uint64_t       i;
    tiara_record_t record;
    tiara_record_t candidate;

    bix = hv & self->last_slot;

    for (i = 0; i <= self->last_slot; i++) {
        record = atomic_load(&self->buckets[bix]);

        if (!record.hv) {
            candidate.hv   = hv;
            candidate.item = hatrack_pflag_set(item, TIARA_F_USED);

            if (CAS(&self->buckets[bix], &record, candidate)) {
                if (atomic_fetch_add(&self->used_count, 1) >= self->threshold) {
                    tiara_store_migrate(self, thread, top);
                }

                return true;
            }
            // Someone beat us to the record. Fall through to see if it's
            // got the same hash value as us.
        }

        if (!(record.hv == hv)) {
            bix = (bix + 1) & self->last_slot;
            continue;
        }

        return false;
    }

    self = tiara_store_migrate(self, thread, top);
    return tiara_store_add(self, thread, top, hv, item);
}

static void *
tiara_store_remove(tiara_store_t *self, mmm_thread_t *thread, tiara_t *top, uint64_t hv)
{
    uint64_t       bix;
    uint64_t       i;
    tiara_record_t record;
    tiara_record_t candidate;

    bix = hv & self->last_slot;

    for (i = 0; i <= self->last_slot; i++) {
        record = atomic_load(&self->buckets[bix]);

        if (!record.hv) {
            return NULL;
        }

        if (!(record.hv == hv)) {
            bix = (bix + 1) & self->last_slot;
            continue;
        }

        if (hatrack_pflag_test(record.item, TIARA_F_MOVING)) {
migrate_and_retry:
            self = tiara_store_migrate(self, thread, top);
            return tiara_store_remove(self, thread, top, hv);
        }

        candidate.hv   = hv;
        candidate.item = hatrack_pflag_clear(record.item, TIARA_F_USED);

        if (CAS(&self->buckets[bix], &record, candidate)) {
            return hatrack_pflag_clear(record.item, TIARA_F_ALL);
        }

        if (hatrack_pflag_test(record.item, TIARA_F_MOVING)) {
            goto migrate_and_retry;
        }

        break;
    }

    return NULL;
}

static tiara_store_t *
tiara_store_migrate(tiara_store_t *self, mmm_thread_t *thread, tiara_t *top)
{
    tiara_store_t  *new_store;
    tiara_store_t  *candidate_store;
    uint64_t        new_size;
    tiara_bucket_t *bucket;
    tiara_bucket_t *new_bucket;
    tiara_record_t  record;
    tiara_record_t  new_record;
    tiara_record_t  candidate_record;
    uint64_t        i, j;
    uint64_t        bix;
    uint64_t        new_used;
    uint64_t        expected_used;

    new_store = atomic_read(&top->store_current);

    if (new_store != self) {
        return new_store;
    }

    new_used = 0;

    for (i = 0; i <= self->last_slot; i++) {
        bucket              = &self->buckets[i];
        record              = atomic_read(bucket);
        candidate_record.hv = record.hv;

        do {
            if (hatrack_pflag_test(record.item, TIARA_F_MOVING)) {
                break;
            }

            if (hatrack_pflag_test(record.item, TIARA_F_USED)) {
                candidate_record.item = hatrack_pflag_set(record.item,
                                                          TIARA_F_MOVING);
            }
            else {
                candidate_record.item = (void *)(TIARA_F_MOVING | TIARA_F_MOVED);
            }
        } while (!CAS(bucket, &record, candidate_record));

        if (hatrack_pflag_test(record.item, TIARA_F_USED)) {
            new_used++;
        }
    }

    new_store = atomic_read(&self->store_next);

    if (!new_store) {
        new_size        = hatrack_new_size(self->last_slot, new_used);
        candidate_store = tiara_store_new(new_size);

        if (!CAS(&self->store_next, &new_store, candidate_store)) {
            mmm_retire_unused(candidate_store);
        }
        else {
            new_store = candidate_store;
        }
    }

    for (i = 0; i <= self->last_slot; i++) {
        bucket = &self->buckets[i];
        record = atomic_read(bucket);

        if (hatrack_pflag_test(record.item, TIARA_F_MOVED)) {
            continue;
        }

        bix = record.hv & new_store->last_slot;

        for (j = 0; j < new_store->last_slot; j++) {
            new_bucket = &new_store->buckets[bix];
            new_record = atomic_read(new_bucket);

            if (!new_record.hv) {
                candidate_record.hv   = record.hv;
                candidate_record.item = hatrack_pflag_clear(record.item,
                                                            TIARA_F_MOVING);
                if (CAS(new_bucket, &new_record, candidate_record)) {
                    break;
                }
            }

            if (!(new_record.hv == record.hv)) {
                bix = (bix + 1) & new_store->last_slot;
                continue;
            }

            break;
        }

        candidate_record.item = hatrack_pflag_set(record.item, TIARA_F_MOVED);
        CAS(bucket, &record, candidate_record);
    }

    expected_used = 0;

    CAS(&new_store->used_count, &expected_used, new_used);

    if (CAS(&top->store_current, &self, new_store)) {
        mmm_retire(thread, self);
    }

    return top->store_current;
}

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
 *  Name:           swimcap.c
 *  Description:    Single Writer, Independent Multiple-readers,
 *                  Crappy, Albeit Parallel.
 *
 *                  This uses a per-data structure lock that writers hold
 *                  for their entire operation.
 *
 *                  It's similar to duncecap, but in this version,
 *                  readers do NOT use the lock; in fact, they are
 *                  fully wait free.
 *
 *                  Instead, we use an epoch-based memory management
 *                  scheme on our current data store, to make sure that
 *                  a store cannot be deleted while we are reading it,
 *                  even if a resize has completed.
 *
 *  Author:         John Viega, john@zork.org
 *
 */

#include "hatrack/swimcap.h"
#include "hatrack/malloc.h"
#include "hatrack/hatomic.h"
#include "../hatrack-internal.h"

#include <stdlib.h>

#ifndef HATRACK_NO_PTHREAD

// clang-format off
static swimcap_store_t *swimcap_store_new    (uint64_t);
static void            *swimcap_store_get    (swimcap_store_t *, hatrack_hash_t, bool *);
static void            *swimcap_store_put    (swimcap_store_t *, mmm_thread_t *, swimcap_t *, hatrack_hash_t, void *, bool *);
static void            *swimcap_store_replace(swimcap_store_t *, mmm_thread_t *, hatrack_hash_t, void *, bool *);
static bool             swimcap_store_add    (swimcap_store_t *, mmm_thread_t *, swimcap_t *, hatrack_hash_t, void *);
static void            *swimcap_store_remove (swimcap_store_t *, mmm_thread_t *, swimcap_t *, hatrack_hash_t, bool *);
static void             swimcap_migrate      (swimcap_t *, mmm_thread_t *);
// clang-format on

/* swimcap_new()
 *
 * Allocates a new swimcap object with the system malloc, and
 * initializes it.
 */
swimcap_t *
swimcap_new(void)
{
    swimcap_t *ret;

    ret = (swimcap_t *)hatrack_malloc(sizeof(swimcap_t));

    swimcap_init(ret);

    return ret;
}

swimcap_t *
swimcap_new_size(char size)
{
    swimcap_t *ret;

    ret = (swimcap_t *)hatrack_malloc(sizeof(swimcap_t));

    swimcap_init_size(ret, size);

    return ret;
}

/* swimcap_init()
 *
 * This is identical to duncecap_init().
 *
 * It's expected that swimcap instances will be created via the
 * default malloc.  This function cannot rely on zero-initialization
 * of its own object.
 */
void
swimcap_init(swimcap_t *self)
{
    swimcap_init_size(self, HATRACK_MIN_SIZE_LOG);

    return;
}

void
swimcap_init_size(swimcap_t *self, char size)
{
    swimcap_store_t *store;
    uint64_t         len;

    if (((size_t)size) > (sizeof(intptr_t) * 8)) {
        hatrack_panic("invalid size in swimcap_init_size");
    }

    if (size < HATRACK_MIN_SIZE_LOG) {
        hatrack_panic("invalid size in swimcap_init_size");
    }

    len                 = 1 << size;
    store               = swimcap_store_new(len);
    self->item_count    = 0;
    self->next_epoch    = 1; // 0 is reserved for empty buckets.
    self->store_current = store;

    pthread_mutex_init(&self->write_mutex, NULL);

    return;
}

/* swimcap_cleanup()
 *
 * This function is meant to be called when the table should clean up
 * its own internal state before deallocation. When you do so, it's
 * your responsibility to make sure that no threads are going to use
 * the object anymore.
 *
 * the delete function below is similar, except that it also calls
 * free() on the actual top-level object as well, under the assumption
 * it was created with the default malloc implementation.
 */
void
swimcap_cleanup(swimcap_t *self)
{
    pthread_mutex_destroy(&self->write_mutex);
    mmm_retire_unused(self->store_current);

    return;
}

/*
 * swimcap_delete()
 *
 * This implementation is identical to swimcap_delete().
 *
 * Deletes a swimcap object. Generally, you should be confident that
 * all threads except the one from which you're calling this have
 * stopped using the table (generally meaning they no longer hold a
 * reference to the store).
 *
 * Note that this function assumes the swimcap object was allocated
 * via the default malloc. If it wasn't, don't call this directly, but
 * do note that the stores were created via the system malloc, and the
 * most recent store will need to be freed (and the mutex destroyed).
 *
 * This is particularly important, not just because you might use
 * memory after freeing it (a reliability and security concern), but
 * also because using a mutex after it's destroyed is undefined. In
 * practice, there's a good chance that any thread waiting on this
 * mutex when it's destroyed will hang indefinitely.
 */
void
swimcap_delete(swimcap_t *self)
{
    swimcap_cleanup(self);
    hatrack_free(self, sizeof(swimcap_t));

    return;
}

/* swimcap_get()
 *
 * This function needs to safely acquire a reference to the current store,
 * before looking for the hash value in the store. We do so, by using our
 * memory management implementation, mmm.
 *
 * Essentially, mmm keeps a global, atomically updated counter of
 * memory "epochs". Each write operation starts a new epoch. Each
 * memory object records its "write" epoch, as well as its "retire"
 * epoch, meaning the epoch in which mmm_retire() was called.
 *
 * The way mmm protects from freeing data that might be in use by
 * parallel threads, is as follows:
 *
 * 1) All threads "register" by writing the current epoch into a
 *    special array, when they start an operation.  This is done via
 *    mmm_start_basic_op(), which is inlined and defined in mmm.h.
 *    Essentially, the algorithm will ensure that, if a thread has
 *    registered for an epoch, no values from that epoch onward will
 *    be deleted.
 *
 * 2) When the operation is done, they "unregister", via mmm_end_op().
 *
 * 3) When mmm_retire() is called on a pointer, the "retire" epoch is
 *    stored (in a hidden header). The cell is placed on a thread
 *    specific list, and is never immediately freed.
 *
 * 4) Periodically, each thread goes through its retirement list,
 *    looking at the retirement epoch.  If there are no threads that
 *    have registered an epoch requiring the pointer to be alive, then
 *    the value can be safely freed.
 *
 * There are more options with mmm, that we don't use in swimcap. See
 * mmm.c for more details on the algorithm, and options.
 *
 * Once the reference is required, we delegate to newshat_store_get()
 * to do the work. Note that the API (including use of the found
 * parameter) works as with every other hash table; see refhat.c or
 * swimcap.c for more details if needed.
 */
void *
swimcap_get_mmm(swimcap_t *self, mmm_thread_t *thread, hatrack_hash_t hv, bool *found)
{
    void *ret;

    mmm_start_basic_op(thread);

    ret = swimcap_store_get(self->store_current, hv, found);

    mmm_end_op(thread);

    return ret;
}

void *
swimcap_get(swimcap_t *self, hatrack_hash_t hv, bool *found)
{
    return swimcap_get_mmm(self, mmm_thread_acquire(), hv, found);
}

/* swimcap_put()
 *
 * Note that, since this implementation does not have competing
 * writers, the current thread is the only thread that can possibly do
 * a delete operation. Therefore, this thread does not need to
 * "register" an epoch with mmm to prevent deletions.
 *
 * We do need to acquire the write mutex, to make sure we don't
 * have simultaneous writers, though.
 *
 * And, we need to make sure to use mmm_retire() on an old store, when
 * migrating to a new one, so that we don't accidentally free it out
 * from under a reader.
 *
 * 'Put' inserts into the table, whether or not the associated hash
 * value already has a stored item. If it does have a stored item, the
 * old value will be returned (so that it can be deleted, if
 * necessary; the table does not do memory management for the actual
 * contents). Also, if you pass a memory address in the found
 * parameter, the associated memory location will get the value true
 * if the item was already in the table, and false otherwise.
 *
 * Note that, if you're using a key and a value, pass them together
 * in a single object in the item parameter.
 */
void *
swimcap_put_mmm(swimcap_t *self, mmm_thread_t *thread, hatrack_hash_t hv, void *item, bool *found)
{
    void *ret;

    hatrack_mutex_lock(&self->write_mutex);

    ret = swimcap_store_put(self->store_current, thread, self, hv, item, found);

    hatrack_mutex_unlock(&self->write_mutex);

    return ret;
}

void *
swimcap_put(swimcap_t *self, hatrack_hash_t hv, void *item, bool *found)
{
    return swimcap_put_mmm(self, mmm_thread_acquire(), hv, item, found);
}

/* swimcap_replace()
 *
 * As with swimcap_put(), we need to acquire the write lock, but do not
 * need to register an mmm epoch. This function will never result in
 * a table migration.
 *
 * provided, returning the old value, for purposes of the caller doing
 * any necessary memory allocation.  If there was not already an
 * associated item with the correct hash in the table, then NULL will
 * be returned, and the memory location referred to in the found
 * parameter will, if not NULL, be set to false.
 *
 * If you want the value to be set, whether or not the item was in the
 * table, then use swimcap_put().
 *
 * Note that, if you're using a key and a value, pass them together
 * in a single object in the item parameter.
 */
void *
swimcap_replace_mmm(swimcap_t *self, mmm_thread_t *thread, hatrack_hash_t hv, void *item, bool *found)
{
    void *ret;

    hatrack_mutex_lock(&self->write_mutex);

    ret = swimcap_store_replace(self->store_current, thread, hv, item, found);

    hatrack_mutex_unlock(&self->write_mutex);

    return ret;
}

void *
swimcap_replace(swimcap_t *self, hatrack_hash_t hv, void *item, bool *found)
{
    return swimcap_replace_mmm(self, mmm_thread_acquire(), hv, item, found);
}

/* swimcap_add()
 *
 * As with swimcap_put(), we need to acquire the write lock, but do not
 * need to register an mmm epoch.
 *
 * The 'add' operation adds an item to the hash table, but only if
 * there isn't currently an item stored with the associated hash
 * value.  If the item would lead to 75% of the buckets being in use,
 * then a table migration will occur (via swimhat_migrate())
 *
 * If an item previously existed, but has since been deleted, the
 * add operation will still succeed.
 *
 * Returns true if the insertion is succesful, and false otherwise.
 */
bool
swimcap_add_mmm(swimcap_t *self, mmm_thread_t *thread, hatrack_hash_t hv, void *item)
{
    bool ret;

    hatrack_mutex_lock(&self->write_mutex);

    ret = swimcap_store_add(self->store_current, thread, self, hv, item);

    hatrack_mutex_unlock(&self->write_mutex);

    return ret;
}

bool
swimcap_add(swimcap_t *self, hatrack_hash_t hv, void *item)
{
    return swimcap_add_mmm(self, mmm_thread_acquire(), hv, item);
}

/*
 * As with swimcap_put(), we need to acquire the write lock, but do not
 * need to register an mmm epoch. This function can never result in a
 * table migration.
 *
 * The 'remove' operation removes an item to the hash table, if it is
 * already present (i.e., if there is currently an item stored with
 * the associated hash value, at the time of the operation).  If the
 * item would lead to 75% of the buckets being in use, then a table
 * migration will occur (via swimhat_migrate())
 *
 * If an item was successfully removed, the old item will be returned
 * (for purposes of memory management), and the value true will be
 * written to the memory address provided in the 'found' parameter, if
 * appropriate.  If the item wasn't in the table at the time of the
 * operation, then NULL gets returned, and 'found' gets set to false,
 * when a non-NULL address is provided.
 *
 * If an item previously existed, but has since been deleted, the
 * behavior is the same as if the item was never in the table.
 */
void *
swimcap_remove_mmm(swimcap_t *self, mmm_thread_t *thread, hatrack_hash_t hv, bool *found)
{
    void *ret;

    hatrack_mutex_lock(&self->write_mutex);

    ret = swimcap_store_remove(self->store_current, thread, self, hv, found);

    hatrack_mutex_unlock(&self->write_mutex);

    return ret;
}

void *
swimcap_remove(swimcap_t *self, hatrack_hash_t hv, bool *found)
{
    return swimcap_remove_mmm(self, mmm_thread_acquire(), hv, found);
}

/* swimcap_len()
 *
 * This implementation is identical to swimcap_len().
 *
 * Returns the approximate number of items currently in the
 * table. Note that we strongly discourage using this call, since it
 * is close to meaningless in multi-threaded programs, as the value at
 * the time of check could be dramatically different by the time of
 * use.
 */

uint64_t
swimcap_len(swimcap_t *self)
{
    return self->item_count;
}

uint64_t
swimcap_len_mmm(swimcap_t *self, mmm_thread_t *thread)
{
    return swimcap_len(self);
}

/* swimcap_view()
 *
 * This returns an array of hatrack_view_t items, representing all of
 * the items in the hash table, for the purposes of iterating over the
 * items, for any reason. The number of items in the view will be
 * stored in the memory address pointed to by the second parameter,
 * num. If the third parameter (sort) is set to true, then quicksort
 * will be used to sort the items in the view, based on the insertion
 * order.
 *
 * This call is mostly the same as with swimcap, except that, if we
 * are okay with inconsistent views, we use mmm_start_basic_op() to
 * register as a reader. If we want consistent views, we use a full
 * write lock, just as we did with swimcap.
 */
hatrack_view_t *
swimcap_view_mmm(swimcap_t *self, mmm_thread_t *thread, uint64_t *num, bool sort)
{
    hatrack_view_t   *view;
    swimcap_store_t  *store;
    swimcap_record_t  record;
    hatrack_view_t   *p;
    swimcap_bucket_t *cur;
    swimcap_bucket_t *end;
    uint64_t          count;
    uint64_t          last_slot;
    uint64_t          alloc_len;

#ifdef SWIMCAP_CONSISTENT_VIEWS
    hatrack_mutex_lock(&self->write_mutex);
#else
    mmm_start_basic_op(thread);
#endif

    store     = self->store_current;
    last_slot = store->last_slot;
    alloc_len = sizeof(hatrack_view_t) * (last_slot + 1);
    view      = (hatrack_view_t *)hatrack_malloc(alloc_len);
    p         = view;
    cur       = store->buckets;
    end       = cur + (last_slot + 1);
    count     = 0;

    while (cur < end) {
        record = atomic_read(&cur->record);

        if (!record.epoch) {
            cur++;
            continue;
        }

        p->item       = record.item;
        p->sort_epoch = record.epoch;

        count++;
        p++;
        cur++;
    }

    *num = count;

    if (!count) {
        hatrack_free(view, alloc_len);

#ifdef SWIMCAP_CONSISTENT_VIEWS
        hatrack_mutex_unlock(&self->write_mutex);
#else
        mmm_end_op(thread);
#endif

        return NULL;
    }

    view = (hatrack_view_t *)hatrack_realloc(view, alloc_len, sizeof(hatrack_view_t) * count);

    if (sort) {
        qsort(view, count, sizeof(hatrack_view_t), hatrack_quicksort_cmp);
    }

#ifdef SWIMCAP_CONSISTENT_VIEWS
    hatrack_mutex_unlock(&self->write_mutex);
#else
    mmm_end_op(thread);
#endif

    return view;
}

hatrack_view_t *
swimcap_view(swimcap_t *self, uint64_t *num, bool sort)
{
    return swimcap_view_mmm(self, mmm_thread_acquire(), num, sort);
}

/*
 * Whenever we create a new store, we use mmm_alloc_committed(), which
 * records the epoch in which we allocated the memory. This is not
 * strictly necessary for our use of MMM here; we really only care
 * about the epoch in which we were retired.
 */

static swimcap_store_t *
swimcap_store_new(uint64_t size)
{
    swimcap_store_t *ret;
    uint64_t         alloc_len;

    alloc_len = sizeof(swimcap_store_t);
    alloc_len += size * sizeof(swimcap_bucket_t);
    ret            = (swimcap_store_t *)mmm_alloc_committed(alloc_len);
    ret->last_slot = size - 1;
    ret->threshold = hatrack_compute_table_threshold(size);

    return ret;
}

static void *
swimcap_store_get(swimcap_store_t *self, hatrack_hash_t hv, bool *found)
{
    uint64_t          bix;
    uint64_t          last_slot;
    uint64_t          i;
    swimcap_bucket_t *cur;
    swimcap_record_t  record;

    last_slot = self->last_slot;
    bix       = hatrack_bucket_index(hv, last_slot);

    for (i = 0; i <= last_slot; i++) {
        cur = &self->buckets[bix];

        if (hatrack_hashes_eq(hv, cur->hv)) {
            /* Since readers can run concurrently to writers, it is
             * possible the hash has been written, but no item has
             * been written yet. So we need to load atomically, then
             * make sure there's something to return.
             */
            record = atomic_read(&cur->record);

            if (record.epoch) {
                if (found) {
                    *found = true;
                }

                return record.item;
            }
            else {
                if (found) {
                    *found = false;
                }

                return NULL;
            }
        }

        if (hatrack_bucket_unreserved(cur->hv)) {
            if (found) {
                *found = false;
            }

            return NULL;
        }

        bix = (bix + 1) & last_slot;
    }
    __builtin_unreachable();
}

static void *
swimcap_store_put(swimcap_store_t *self,
                  mmm_thread_t    *thread,
                  swimcap_t       *top,
                  hatrack_hash_t   hv,
                  void            *item,
                  bool            *found)
{
    uint64_t          bix;
    uint64_t          i;
    uint64_t          last_slot;
    swimcap_bucket_t *cur;
    swimcap_record_t  record;
    void             *ret;

    last_slot = self->last_slot;
    bix       = hatrack_bucket_index(hv, last_slot);

    for (i = 0; i <= last_slot; i++) {
        cur = &self->buckets[bix];

        if (hatrack_hashes_eq(hv, cur->hv)) {
            record = atomic_load(&cur->record);
            /* If no epoch is set, it's either deleted or has never
             * been stored.  Either way, no worries.
             */
            if (!record.epoch) {
                if (found) {
                    *found = false;
                }

                record.epoch = top->next_epoch++;
                ret          = NULL;

                top->item_count++;
                // The bucket has already been used, so we do NOT bump
                // used_count in this case.
            }
            else {
                if (found) {
                    *found = true;
                }

                ret = record.item;
            }

            record.item = item;
            atomic_store(&cur->record, record);

            return ret;
        }

        if (hatrack_bucket_unreserved(cur->hv)) {
            if (self->used_count + 1 == self->threshold) {
                swimcap_migrate(top, thread);

                return swimcap_store_put(top->store_current,
                                         thread,
                                         top,
                                         hv,
                                         item,
                                         found);
            }

            self->used_count++;
            top->item_count++;

            cur->hv      = hv;
            record.item  = item;
            record.epoch = top->next_epoch++;

            atomic_store(&cur->record, record);

            if (found) {
                *found = false;
            }

            return NULL;
        }

        bix = (bix + 1) & last_slot;
    }
    __builtin_unreachable();
}

static void *
swimcap_store_replace(swimcap_store_t *self,
                      mmm_thread_t    *thread,
                      hatrack_hash_t   hv,
                      void            *item,
                      bool            *found)
{
    uint64_t          bix;
    uint64_t          i;
    uint64_t          last_slot;
    swimcap_bucket_t *cur;
    swimcap_record_t  record;
    void             *ret;

    last_slot = self->last_slot;
    bix       = hatrack_bucket_index(hv, last_slot);

    for (i = 0; i <= last_slot; i++) {
        cur = &self->buckets[bix];

        if (hatrack_hashes_eq(hv, cur->hv)) {
            record = atomic_read(&cur->record);

            if (!record.epoch) {
                if (found) {
                    *found = false;
                }

                return NULL;
            }

            ret         = record.item;
            record.item = item;

            atomic_store(&cur->record, record);

            if (found) {
                *found = true;
            }

            return ret;
        }

        if (hatrack_bucket_unreserved(cur->hv)) {
            if (found) {
                *found = false;
            }

            return NULL;
        }

        bix = (bix + 1) & last_slot;
    }
    __builtin_unreachable();
}

static bool
swimcap_store_add(swimcap_store_t *self,
                  mmm_thread_t    *thread,
                  swimcap_t       *top,
                  hatrack_hash_t   hv,
                  void            *item)
{
    uint64_t          bix;
    uint64_t          i;
    uint64_t          last_slot;
    swimcap_bucket_t *cur;
    swimcap_record_t  record;

    last_slot = self->last_slot;
    bix       = hatrack_bucket_index(hv, last_slot);

    for (i = 0; i <= last_slot; i++) {
        cur = &self->buckets[bix];

        if (hatrack_hashes_eq(hv, cur->hv)) {
            record = atomic_read(&cur->record);

            if (record.epoch) {
                return false;
            }

            record.item  = item;
            record.epoch = top->next_epoch++;

            top->item_count++;

            atomic_store(&cur->record, record);

            return true;
        }

        // In this branch, there's definitely nothing there at the
        // time of the operation, and we should add.
        if (hatrack_bucket_unreserved(cur->hv)) {
            if (self->used_count + 1 == self->threshold) {
                swimcap_migrate(top, thread);

                return swimcap_store_add(top->store_current, thread, top, hv, item);
            }

            self->used_count++;
            top->item_count++;

            cur->hv      = hv;
            record.item  = item;
            record.epoch = top->next_epoch++;

            atomic_store(&cur->record, record);

            return true;
        }
        bix = (bix + 1) & last_slot;
    }
    __builtin_unreachable();
}

void *
swimcap_store_remove(swimcap_store_t *self,
                     mmm_thread_t    *thread,
                     swimcap_t       *top,
                     hatrack_hash_t   hv,
                     bool            *found)
{
    uint64_t          bix;
    uint64_t          i;
    uint64_t          last_slot;
    swimcap_bucket_t *cur;
    swimcap_record_t  record;
    void             *ret;

    last_slot = self->last_slot;
    bix       = hatrack_bucket_index(hv, last_slot);

    for (i = 0; i <= last_slot; i++) {
        cur = &self->buckets[bix];

        if (hatrack_hashes_eq(hv, cur->hv)) {
            record = atomic_read(&cur->record);

            if (!record.epoch) {
                if (found) {
                    *found = false;
                }

                return NULL;
            }

            ret          = record.item;
            record.epoch = 0;

            atomic_store(&cur->record, record);
            --top->item_count;

            if (found) {
                *found = true;
            }

            return ret;
        }

        if (hatrack_bucket_unreserved(cur->hv)) {
            if (found) {
                *found = false;
            }

            return NULL;
        }

        bix = (bix + 1) & last_slot;
    }
    __builtin_unreachable();
}

static void
swimcap_migrate(swimcap_t *self, mmm_thread_t *thread)
{
    swimcap_store_t  *cur_store;
    swimcap_store_t  *new_store;
    swimcap_bucket_t *cur;
    swimcap_bucket_t *target;
    swimcap_record_t  record;
    uint64_t          new_size;
    uint64_t          cur_last_slot;
    uint64_t          new_last_slot;
    uint64_t          i, n, bix;

    cur_store     = self->store_current;
    cur_last_slot = cur_store->last_slot;
    new_size      = hatrack_new_size(cur_last_slot, swimcap_len_mmm(self, thread) + 1);
    new_last_slot = new_size - 1;
    new_store     = swimcap_store_new(new_size);

    for (n = 0; n <= cur_last_slot; n++) {
        cur    = &cur_store->buckets[n];
        record = atomic_read(&cur->record);

        if (!record.epoch) {
            continue;
        }

        bix = hatrack_bucket_index(cur->hv, new_last_slot);

        for (i = 0; i < new_size; i++) {
            target = &new_store->buckets[bix];

            if (hatrack_bucket_unreserved(target->hv)) {
                target->hv = cur->hv;

                atomic_store(&target->record, record);
                break;
            }

            bix = (bix + 1) & new_last_slot;
        }
    }

    new_store->used_count = self->item_count;
    self->store_current   = new_store;

    /* This is effectively a "deferred" free. It might end up calling
     * mmm_empty() (in mmm.c), but even if it does, mmm_empty() won't
     * free the store, unless there are no readers still active that
     * cane in before or during the epoch associated with this retire
     * operation.
     *
     * Note that it's very critical that the retire operation happen
     * at some time after the new store is installed. If this
     * operation were to come first, if some external force bumps the
     * epoch, then we might remove the store before there's a new one
     * installed, meaning readers might get a reference in an epoch
     * after the retirement epoch, which would constitute a
     * use-after-free bug.
     */
    mmm_retire(thread, cur_store);

    return;
}

#endif

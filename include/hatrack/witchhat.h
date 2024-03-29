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
 *  Name:           witchhat.h
 *  Description:    Waiting I Trully Cannot Handle
 *
 *                  This is a lock-free, and wait freehash table,
 *                  without consistency / full ordering.
 *
 *                  Note that witchhat is based on hihat1, with a
 *                  helping mechanism in place to ensure wait freedom.
 *                  There are only a few places in hihat1 where we
 *                  need such a mechanism, so we will only comment on
 *                  those places.
 *
 *                  Refer to hihat.h and hihat.c for more detail on
 *                  the core algorithm, as here, we only comment on
 *                  the things that are different about witchhat.
 *
 *  Author: John Viega, john@zork.org
 */

#pragma once

#include <hatrack.h>

typedef struct {
    void    *item;
    uint64_t info;
} witchhat_record_t;

enum64(witchhat_flag_t,
       WITCHHAT_F_MOVING   = 0x8000000000000000,
       WITCHHAT_F_MOVED    = 040000000000000000,
       WITCHHAT_F_INITED   = 0x2000000000000000,
       WITCHHAT_EPOCH_MASK = 0x1fffffffffffffff);

typedef struct {
    _Atomic hatrack_hash_t    hv;
    _Atomic witchhat_record_t record;
} witchhat_bucket_t;

typedef struct witchhat_store_st witchhat_store_t;

// clang-format off
struct witchhat_store_st {
    alignas(8)
    uint64_t                    last_slot;
    uint64_t                    threshold;
    _Atomic uint64_t            used_count;
    _Atomic(witchhat_store_t *) store_next;
    alignas(16)
    witchhat_bucket_t           buckets[];
};

typedef struct {
    alignas(8)
    _Atomic(witchhat_store_t *) store_current;
    _Atomic uint64_t            item_count;
    _Atomic uint64_t            help_needed;
            uint64_t            next_epoch;

} witchhat_t;


witchhat_t     *witchhat_new        (void);
witchhat_t     *witchhat_new_size   (char);
void            witchhat_init       (witchhat_t *);
void            witchhat_init_size  (witchhat_t *, char);
void            witchhat_cleanup    (witchhat_t *);
void            witchhat_delete     (witchhat_t *);
void           *witchhat_get        (witchhat_t *, hatrack_hash_t, bool *);
void           *witchhat_put        (witchhat_t *, hatrack_hash_t, void *,
				     bool *);
void           *witchhat_replace    (witchhat_t *, hatrack_hash_t, void *,
				     bool *);
bool            witchhat_add        (witchhat_t *, hatrack_hash_t, void *);
void           *witchhat_remove     (witchhat_t *, hatrack_hash_t, bool *);
uint64_t        witchhat_len        (witchhat_t *);
hatrack_view_t *witchhat_view       (witchhat_t *, uint64_t *, bool);
hatrack_view_t *witchhat_view_no_mmm(witchhat_t *, uint64_t *, bool);

/* These need to be non-static because tophat and hatrack_dict both
 * need them, so that they can call in without a second call to
 * MMM. But, they should be considered "friend" functions, and not
 * part of the public API.
 *
 * Actually, hatrack_dict no longer uses Witchhat, it uses Crown, but
 * I'm going to explicitly leave these here, instead of going back to
 * making them static.
 */
witchhat_store_t *witchhat_store_new    (uint64_t);
void             *witchhat_store_get    (witchhat_store_t *, hatrack_hash_t,
					 bool *);
void             *witchhat_store_put    (witchhat_store_t *, witchhat_t *,
					 hatrack_hash_t, void *, bool *,
					 uint64_t);
void             *witchhat_store_replace(witchhat_store_t *, witchhat_t *,
					 hatrack_hash_t, void *, bool *,
					 uint64_t);
bool              witchhat_store_add    (witchhat_store_t *, witchhat_t *,
					 hatrack_hash_t, void *, uint64_t);
void             *witchhat_store_remove (witchhat_store_t *, witchhat_t *,
					 hatrack_hash_t, bool *, uint64_t);

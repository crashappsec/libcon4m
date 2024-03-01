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
 *  Name:           woolhat.h
 *  Description:    Wait-free Operations, Orderable, Linearizable HAsh Table
 *                  This version keeps unordered buckets, and sorts
 *                  by epoch when needed. Views are fully consistent.
 *
 *  Author:         John Viega, john@zork.org
 */

#pragma once

#include <hatrack/hatrack_common.h>

typedef struct woolhat_record_st woolhat_record_t;

struct woolhat_record_st {
    woolhat_record_t *next;
    void             *item;
    bool              deleted;
};

enum {
    WOOLHAT_F_MOVING      = 0x0000000000000001,
    WOOLHAT_F_MOVED       = 0x0000000000000002,
    WOOLHAT_F_DELETE_HELP = 0x0000000000000004
};

// clang-format off

typedef struct {
    woolhat_record_t  *head;
    uint64_t           flags;
} woolhat_state_t;

typedef struct {
    alignas(16)
    _Atomic hatrack_hash_t  hv;
    _Atomic woolhat_state_t state;
} woolhat_history_t;

typedef struct woolhat_store_st woolhat_store_t;

struct woolhat_store_st {
    alignas(8)
    uint64_t                   last_slot;
    uint64_t                   threshold;
    _Atomic uint64_t           used_count;
    _Atomic(woolhat_store_t *) store_next;
    woolhat_history_t          hist_buckets[];
};

typedef struct woolhat_st {
    alignas(8)
    _Atomic(woolhat_store_t *) store_current;
    _Atomic uint64_t           item_count;
    _Atomic uint64_t           help_needed;
    mmm_cleanup_func           cleanup_func;
    void                      *cleanup_aux;
} woolhat_t;


/* This is a special type of view result that includes the hash
 * value, intended for set operations. Currently, it is only in use
 * by woolhat (and by hatrack_set, which is built on woolhat).
 */

typedef struct {
    hatrack_hash_t hv;
    void          *item;
    int64_t        sort_epoch;
} hatrack_set_view_t;

woolhat_t      *woolhat_new             (void);
woolhat_t      *woolhat_new_size        (char);
void            woolhat_init            (woolhat_t *);
void            woolhat_init_size       (woolhat_t *, char);
void            woolhat_cleanup         (woolhat_t *);
void            woolhat_delete          (woolhat_t *);
void            woolhat_set_cleanup_func(woolhat_t *, mmm_cleanup_func, void *);
void           *woolhat_get             (woolhat_t *, hatrack_hash_t, bool *);
void           *woolhat_put             (woolhat_t *, hatrack_hash_t, void *,
					 bool *);
void           *woolhat_replace         (woolhat_t *, hatrack_hash_t, void *,
					 bool *);
bool            woolhat_add             (woolhat_t *, hatrack_hash_t, void *);
void           *woolhat_remove          (woolhat_t *, hatrack_hash_t, bool *);
uint64_t        woolhat_len             (woolhat_t *);

hatrack_view_t     *woolhat_view        (woolhat_t *, uint64_t *, bool);
hatrack_set_view_t *woolhat_view_epoch  (woolhat_t *, uint64_t *, uint64_t);

/*
 * Copyright © 2024 John Viega
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License atn
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *  Name:           arr64.h
 *  Description:    A fast, wait-free flex array using only a 64-bit CAS.
 *
 *                  This ONLY allows indexing and resizing the array.
 *
 *  Author:         John Viega, john@zork.org
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <hatrack/hatrack_config.h>


#define ARR64_MIN_STORE_SZ_LOG 4

// clang-format off
typedef void (*arr64_callback_t)(void *);

typedef uint64_t arr64_item_t;

typedef _Atomic arr64_item_t arr64_cell_t;

typedef struct arr64_store_t arr64_store_t;

typedef struct {
    uint64_t         next_ix;
    arr64_store_t   *contents;
    arr64_callback_t eject_callback;
} arr64_view_t;

struct arr64_store_t {
    alignas(8)
    uint64_t                 store_size;
    _Atomic uint64_t         array_size;
    _Atomic (arr64_store_t *)next;
    _Atomic bool             claimed;
    arr64_cell_t             cells[];
};

typedef struct {
    arr64_callback_t          ret_callback;
    arr64_callback_t          eject_callback;
    _Atomic (arr64_store_t  *)store;
} arr64_t;

arr64_t      *arr64_new               (uint64_t);
void          arr64_init              (arr64_t *, uint64_t);
void          arr64_set_ret_callback  (arr64_t *, arr64_callback_t);
void          arr64_set_eject_callback(arr64_t *, arr64_callback_t);
void          arr64_cleanup           (arr64_t *);
void          arr64_delete            (arr64_t *);
void         *arr64_get               (arr64_t *, uint64_t, int *);
bool          arr64_set               (arr64_t *, uint64_t, void *);
void          arr64_grow              (arr64_t *, uint64_t);
void          arr64_shrink            (arr64_t *, uint64_t);
uint32_t      arr64_len               (arr64_t *);
arr64_view_t *arr64_view              (arr64_t *);
void         *arr64_view_next         (arr64_view_t *, bool *);
void          arr64_view_delete       (arr64_view_t *);

enum64(arr64_enum_t,
       ARR64_USED   = 0x00000000000000001,
       ARR64_MOVED  = 0x00000000000000002,
       ARR64_MOVING = 0x00000000000000004
       );

enum {
    ARR64_OK,
    ARR64_OOB,
    ARR64_UNINITIALIZED
};

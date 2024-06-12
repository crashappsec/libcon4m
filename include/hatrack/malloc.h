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
 *  Name:           malloc.h
 *  Description:    Memory allocation support
 *
 *  Author:         John Viega, john@zork.org
 */

#pragma once

#include "base.h"
#include <stddef.h>

// hatrack_malloc_t is the signature of a function that is used to allocate
// memory. The size is a minimum size; the function may return a larger size.
// The function must return a pointer that is 16-byte aligned. The returned
// pointer may be NULL if the requested allocation cannot be satisfied.
typedef void *(*hatrack_malloc_t)(size_t size, void *arg);

// hatrack_realloc_t is the signature of a function that is used to resize an
// existing allocation. The old pointer and size are always provided (oldptr
// will never be NULL). The new size functions the same as with the memory
// allocation function. Likewise, the returned pointer must be 16-byte aligned.
// The returned pointer may be the same as the old pointer as long as it
// satisfies the new size requirement. The caller will assume that the original
// pointer is no longer valid. It is this function's responsibility to copy the
// memory from the old pointer to the new pointer, as appropriate. The returned
// pointer may be NULL if the requested allocation cannot be satisfied. In this
// case the original pointer remains valid.
typedef void *(*hatrack_realloc_t)(void  *oldptr,
                                   size_t oldsize,
                                   size_t newsize,
                                   void  *arg);

// hatrack_free_t is the signature of a function that is used to deallocate
// previously allocated memory. The pointer to free is always provided (it will
// never be NULL).
typedef void (*hatrack_free_t)(void *ptr, size_t size, void *arg);

// hatrack_setmallocfns sets the function pointers to use for malloc, zalloc,
// realloc, and free operations. The zalloc function is the same as malloc,
// except that the contents of the allocation is expected to be filled with 0
// bytes before return. The defaults are to use the libc equivalents of malloc,
// calloc, realloc, and free.
extern void
hatrack_setmallocfns(hatrack_malloc_t  mallocfn,
                     hatrack_malloc_t  zallocfn,
                     hatrack_realloc_t reallocfn,
                     hatrack_free_t    freefn,
                     void             *arg);

// hatrack_malloc calls the configured memory allocation function to allocate
// the requested amount of memory. The returned pointer may be NULL if the
// allocation cannot be satisfied; otherwise, it is guaranteed to be 16-byte
// aligned.
extern void *
hatrack_malloc(size_t size);

// hatrack_zalloc calls the configured memory allocation function to allocate
// the requested amount of zeroed memory. The returned pointer may be NULL if
// the allocation cannot be satisfied; otherwise, it is guaranteed to be 16-byte
// aligned and filled with 0 bytes.
extern void *
hatrack_zalloc(size_t size);

// hatrack_realloc calls the configured memory reallocation function to resize
// an existing allocation. The returned pointer may be NULL if the allocation
// cannot be satisfied; otherwise, it is guaranteed to be 16-byte aligned. The
// original pointer (if different from the returned pointer) is deallocated as
// long as the returned poitner is not NULL. The old pointer to reallocate may
// be NULL, in which case this function behaves the same as hatrack_malloc.
// The allocated size of the original pointer to reallocate is required, but
// ignored if oldptr is NULL.
extern void *
hatrack_realloc(void *oldptr, size_t oldsize, size_t newsize);

// hatrack_free calls the configured memory deallocation function to deallocate
// an existing allocation. The pointer to deallocate may be NULL, in which case
// nothing is done. The allocated size of the pointer to free is required.
extern void
hatrack_free(void *oldptr, size_t oldsize);

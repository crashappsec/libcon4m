#pragma once
#define C4M_GC_SHOW_COLLECT_STACK_TRACES
// Home of anything remotely configurable. Don't change this file;
// update the meson config.
//
// If it's not in the meson options, then I'd discourage you from even
// considering changing the value.
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define __c4m_have_asan__
#endif // address_sanitizer
#elif defined(__SANITIZE_ADDRESS__)
#define __c4m_have_asan__
#endif // __has_feature

#define C4M_FORCED_ALIGNMENT 16

#ifdef C4M_DEBUG_PATTERNS
#undef C4M_DEBUG_PATTERNS
#endif

#ifdef C4M_PARSE_DEBUG
#undef C4M_PARSE_DEBUG
#endif

#ifdef C4M_ADD_ALLOC_LOC_INFO
#undef C4M_ADD_ALLOC_LOC_INFO
#endif

#ifdef C4M_DEBUG_KARGS
#undef C4M_DEBUG_KARGS
#endif

#ifdef C4M_TYPE_LOG
#undef C4M_TYPE_LOG
#endif

#ifdef C4M_SB_DEBUG
#undef C4M_SB_DEBUG
#endif

#ifdef C4M_SB_TEST
#undef C4M_SB_TEST
#endif

#if !defined(C4M_DEV)
#ifdef C4M_DEBUG
#error "C4M_DEBUG requires C4M_DEV"
#endif
#ifdef C4M_GC_STATS
#error "C4M_GC_STATS requires C4M_DEV"
#endif
#ifdef C4M_FULL_MEMCHECK
#error "C4M_FULL_MEMCHECK requires C4M_DEV"
#endif
#ifdef C4M_VM_DEBUG
#error "C4M_VM_DEBUG requires C4M_DEV"
#endif
#ifdef C4M_VM_WARN_ON_ZERO_ALLOCS
#error "C4M_VM_WARN_ON_ZERO_ALLOCS requires C4M_DEV"
#endif

#endif

#if !defined(HATRACK_PER_INSTANCE_AUX)
#error "HATRACK_PER_INSTANCE_AUX must be defined for con4m to compile."
#endif

#if defined(C4M_GC_FULL_TRACE) && !defined(C4M_GC_STATS)
#define C4M_GC_STATS
#endif

#if defined(C4M_FULL_MEMCHECK) && !defined(C4M_GC_STATS)
#define C4M_GC_STATS
#endif

#if defined(HATRACK_ALLOC_PASS_LOCATION)
#define C4M_ADD_ALLOC_LOC_INFO
#else
#if defined(C4M_GC_STATS)
#error "C4M_GC_STATS cannot be enabled without HATRACK_ALLOC_PASS_LOCATION"
#endif // C4M_GC_STATS

#endif // HATRACK_ALLOC_PASS_LOCATION

#if defined(C4M_VM_DEBUG) && !defined(C4M_VM_DEBUG_DEFAULT)
#define C4M_VM_DEBUG_DEFAULT false
#endif

#if defined(C4M_GC_FULL_TRACE) && !defined(C4M_GC_FULL_TRACE_DEFAULT)
#define C4M_GC_FULL_TRACE_DEFAULT 1
#endif

#ifndef C4M_MIN_RENDER_WIDTH
#define C4M_MIN_RENDER_WIDTH 80
#endif

#ifndef C4M_DEBUG
#if defined(C4M_WATCH_SLOTS) || defined(C4M_WATCH_LOG_SZ)
#warning "Watchpoint compile parameters set, but watchpoints are disabled"
#endif // either set
#else
#ifndef C4M_WATCH_SLOTS
#define C4M_WATCH_SLOTS 30
#endif

#ifndef C4M_WATCH_LOG_SZ
#define C4M_WATCH_LOG_SZ (1 << 14)
#endif
#endif // C4M_DEBUG

#ifndef C4M_MAX_KARGS_NESTING_DEPTH
// Must be a power of two, and probably shouldn't be lower.
#define C4M_MAX_KARGS_NESTING_DEPTH 32
#endif

// More accurately, max # of supported keywords.
#ifndef C4M_MAX_KEYWORD_SIZE
#define C4M_MAX_KEYWORD_SIZE 32
#endif

#if defined(C4M_FULL_MEMCHECK)
// #define C4M_SHOW_NEXT_ALLOCS 1000
#ifndef C4M_MEMCHECK_RING_SZ
// Must be a power of 2.
#define C4M_MEMCHECK_RING_SZ 128
#endif // RING_SZ

#if C4M_MEMCHECK_RING_SZ != 0
#define C4M_USE_RING
#else
#undef C4M_USE_RING
#endif // C4M_MEMCHECK_RING_SZ
#endif // C4M_FULL_MEMCHECK

#ifndef C4M_EMPTY_BUFFER_ALLOC
#define C4M_EMPTY_BUFFER_ALLOC 128
#endif

#ifndef C4M_DEFAULT_ARENA_SIZE
// This is the size any test case that prints a thing grows to awfully fast.
#define C4M_DEFAULT_ARENA_SIZE (1 << 30)
#endif

#ifndef C4M_STACK_SIZE
#define C4M_STACK_SIZE (1 << 17)
#endif

#ifndef C4M_MAX_CALL_DEPTH
#define C4M_MAX_CALL_DEPTH 100
#endif

#if defined(C4M_GC_STATS) && !defined(C4M_SHOW_GC_DEFAULT)
#define C4M_SHOW_GC_DEFAULT 0
#endif

#ifdef C4M_GC_ALL_ON
#define C4M_GC_DEFAULT_ON  1
#define C4M_GC_DEFAULT_OFF 1
#elif defined(C4M_GC_ALL_OFF)
#define C4M_GC_DEFAULT_ON  0
#define C4M_GC_DEFAULT_OFF 0
#else
#define C4M_GC_DEFAULT_ON  1
#define C4M_GC_DEFAULT_OFF 0
#endif

#ifndef C4M_GCT_INIT
#define C4M_GCT_INIT C4M_GC_DEFAULT_ON
#endif
#ifndef C4M_GCT_MMAP
#define C4M_GCT_MMAP C4M_GC_DEFAULT_ON
#endif
#ifndef C4M_GCT_MUNMAP
#define C4M_GCT_MUNMAP C4M_GC_DEFAULT_ON
#endif
#ifndef C4M_GCT_SCAN
#define C4M_GCT_SCAN C4M_GC_DEFAULT_ON
#endif
#ifndef C4M_GCT_OBJ
#define C4M_GCT_OBJ C4M_GC_DEFAULT_OFF
#endif
#ifndef C4M_GCT_SCAN_PTR
#define C4M_GCT_SCAN_PTR C4M_GC_DEFAULT_OFF
#endif
#ifndef C4M_GCT_PTR_TEST
#define C4M_GCT_PTR_TEST C4M_GC_DEFAULT_OFF
#endif
#ifndef C4M_GCT_PTR_TO_MOVE
#define C4M_GCT_PTR_TO_MOVE C4M_GC_DEFAULT_OFF
#endif
#ifndef C4M_GCT_MOVE
#define C4M_GCT_MOVE C4M_GC_DEFAULT_OFF
#endif
#ifndef C4M_GCT_ALLOC_FOUND
#define C4M_GCT_ALLOC_FOUND C4M_GC_DEFAULT_OFF
#endif
#ifndef C4M_GCT_PTR_THREAD
#define C4M_GCT_PTR_THREAD C4M_GC_DEFAULT_OFF
#endif
#ifndef C4M_GCT_WORKLIST
#define C4M_GCT_WORKLIST C4M_GC_DEFAULT_ON
#endif
#ifndef C4M_GCT_COLLECT
#define C4M_GCT_COLLECT C4M_GC_DEFAULT_ON
#endif
#ifndef C4M_GCT_REGISTER
#define C4M_GCT_REGISTER C4M_GC_DEFAULT_ON
#endif
#ifndef C4M_GCT_ALLOC
#define C4M_GCT_ALLOC C4M_GC_DEFAULT_OFF
#endif
#ifndef C4M_MAX_GC_ROOTS
#define C4M_MAX_GC_ROOTS (1 << 15)
#endif

#ifndef C4M_TEST_SUITE_TIMEOUT_SEC
#define C4M_TEST_SUITE_TIMEOUT_SEC 1
#endif
#ifndef C4M_TEST_SUITE_TIMEOUT_USEC
#define C4M_TEST_SUITE_TIMEOUT_USEC 0
#endif

#ifdef C4M_PACKAGE_INIT_MODULE
#undef C4M_PACKAGE_INIT_MODULE
#endif
#define C4M_PACKAGE_INIT_MODULE "__init"

// Current Con4m version info.
#define C4M_VERS_MAJOR   0x00
#define C4M_VERS_MINOR   0x02
#define C4M_VERS_PATCH   0x08
#define C4M_VERS_PREVIEW 0x00

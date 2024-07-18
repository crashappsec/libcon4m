#include "con4m.h"

#ifdef C4M_SHOW_PREPROC_CONFIG

#define STR_EXPAND(x) #x
#define STR(x)        STR_EXPAND(x)

#ifdef C4M_DEV
#pragma message "C4M_DEV is ON (development mode)"
#else
#pragma message "C4M_DEV is OFF (no development mode; required for all tests)"
#endif

#ifdef C4M_DEBUG
#pragma message "C4M_DEBUG is ON (C stack traces on thrown exceptions, watchpoints, etc)"
#else
#pragma message "C4M_DEBUG is OFF (no C stack traces on thrown exceptions)"
#ifndef C4M_DEV
#pragma message "enabling requires C4M_DEV"
#endif
#endif

// Adds c4m_end_guard field.
#ifdef C4M_FULL_MEMCHECK
#pragma message "C4M_FULL_MEMCHECK is ON (memory guards around each allocation checked at GC collect)"

#ifdef C4M_STRICT_MEMCHECK
#pragma message "C4M_STRICT_MEMCHECK is ON (abort on any memcheck error)"
#else
#pragma message "C4M_STRICT_MEMCHECK is OFF (don't abort on any memcheck error)"
#endif

#ifdef C4M_SHOW_NEXT_ALLOCS
#pragma message "C4M_SHOW_NEXT_ALLOCS is: " STR(C4M_SHOW_NEXT_ALLOCS)
#else
#pragma message "C4M_SHOW_NEXT_ALLOCS is not set (show next allocs on memcheck error)"
#endif
#else
#pragma message "C4M_FULL_MEMCHECK is OFF (no memory guards around each allocation)"
#ifndef C4M_DEV
#pragma message "enabling requires C4M_DEV"
#endif
#endif

#ifdef C4M_TRACE_GC
#pragma message "C4M_TRACE_GC is ON (garbage collection tracing is enabled)"
#else
#pragma message "C4M_TRACE_GC is OFF (garbage collection tracing is disabled)"
#ifndef C4M_DEV
#pragma message "enabling requires C4M_DEV"
#endif
#endif

#ifdef C4M_USE_FRAME_INTRINSIC
#pragma message "C4M_USE_FRAME_INSTRINSIC is ON"
#else
#pragma message "C4M_USE_FRAME_INSTRINSIC is OFF"
#endif

#ifdef C4M_VM_DEBUG
#pragma message "C4M_VM_DEBUG is ON (virtual machine debugging is enabled)"

#if defined(C4M_VM_DEBUG_DEFAULT)
#if C4M_VM_DEBUG_DEFAULT == true
#define __vm_debug_default
#endif
#endif

#ifdef __vm_debug_default
#pragma message "C4M_VM_DEBUG_DEFAULT is true (VM debugging on by default)"
#else
#pragma message "VM debugging is off unless a debug instruction turns it on."
#endif

#else
#pragma message "C4M_VM_DEBUG is ON (virtual machine debugging is disabled)"
#ifndef C4M_DEV
#pragma message "enabling requires C4M_DEV"
#endif
#endif

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define __c4m_have_asan__
#endif
#else
#ifdef HAS_ADDRESS_SANITIZER
#define __c4m_have_asan__
#endif
#endif

#ifdef __c4m_have_asan__
#pragma message "ADDRESS_SANTITIZER is ON (llvm memory checking enabled)"
#else
#pragma message "ADDRESS_SANTITIZER is OFF (llvm memory checking disabled)"
#ifndef C4M_DEV
#pragma message "enabling requires C4M_DEV"
#endif
#endif

#ifdef C4M_OMIT_UNDERFLOW_CHECKS
#pragma message "C4M_OMIT_UNDERFLOW_CHECKS is ON (Avoid some UBSAN falses)"
#else
#pragma message "C4M_OMIT_UNDERFLOW_CHECKS is OFF (Will see some UBSAN falses)"
#endif

#ifdef C4M_WARN_ON_ZERO_ALLOCS
#pragma message "C4M_WARN_ON_ZERO_ALLOCS is ON (Identify zero-length allocations)"
#else
#pragma message "C4M_WARN_ON_ZERO_ALLOCS is OFF (No spam about 0-length allocs)"
#ifndef C4M_DEV
#pragma message "enabling requires C4M_DEV"
#endif
#endif

#ifdef C4M_GC_SHOW_COLLECT_STACK_TRACES
#pragma message "C4M_GC_SHOW_COLLECT_STACK_TRACES is ON (Show C stack traces at every garbage collection invocation."
#else
#pragma message "C4M_GC_SHOW_COLLECT_STACK_TRACES is OFF (no C stack traces for GC collections)"
#ifndef C4M_DEV
#pragma message "enabling requires C4M_DEV"
#endif
#endif

#ifdef C4M_PARANOID_STACK_SCAN
#pragma message "C4M_PARANOID_STACK_SCAN is ON (Find slow, unaligned pointers)"
#else
#pragma message "C4M_PARANOID_STACK_SCAN is OFF (Does not look for unaligned pointers; this is slow and meant for debugging)"
#endif

#ifdef C4M_MIN_RENDER_WIDTH
#pragma message "C4M_MIN_RENDER_WIDTH is: " STR(C4M_MIN_RENDER_WIDTH)
#else
#pragma message "C4M_MIN_RENDER_WIDTH is not set."
#endif

#ifdef C4M_TEST_WITHOUT_FORK
#pragma message "C4M_TEST_WITHOUT_FORK is: ON (No forking during testing.)"
#else
#pragma message "C4M_TEST_WITHOUT_FORK is: OFF (Tests fork.)"
#endif

#ifdef _GNU_SOURCE
#pragma message "_GNU_SOURCE is ON (gnu stdlib)"
#else
#pragma message "_GNU_SOURCE is OFF (not a gnu stdlib)."
#endif

#ifdef HAVE_PTY_H
#pragma message "HAVE_PTY_H is ON (forkpty is available)"
#else
#pragma message "HAVE_PTY_H is OFF (forkpty is NOT available)"
#endif

#endif

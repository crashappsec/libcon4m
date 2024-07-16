#include "con4m.h"

#ifdef C4M_SHOW_PREPROC_CONFIG

#ifdef C4M_DEV
#warning "C4M_DEV is ON (development mode)"
#else
#warning "C4M_DEV is OFF (no development mode; required for all tests)"
#endif

#ifdef C4M_DEBUG
#warning "C4M_DEBUG is ON (C stack traces on thrown exceptions, watchpoints, etc)"
#else
#warning "C4M_DEBUG is OFF (no C stack traces on thrown exceptions)"
#ifndef C4M_DEV
#warning "enabling requires C4M_DEV"
#endif
#endif

// Adds c4m_end_guard field.
#ifdef C4M_FULL_MEMCHECK
#warning "C4M_FULL_MEMCHECK is ON (memory guards around each allocation checked at GC collect)"

#ifdef C4M_STRICT_MEMCHECK
#warning "C4M_STRICT_MEMCHECK is ON (abort on any memcheck error)"
#else
#warning "C4M_STRICT_MEMCHECK is OFF (don't abort on any memcheck error)"
#endif
#else
#warning "C4M_FULL_MEMCHECK is OFF (no memory guards around each allocation)"
#ifndef C4M_DEV
#warning "enabling requires C4M_DEV"
#endif
#endif

#ifdef C4M_TRACE_GC
#warning "C4M_TRACE_GC is ON (garbage collection tracing is enabled)"
#else
#warning "C4M_TRACE_GC is OFF (garbage collection tracing is disabled)"
#ifndef C4M_DEV
#warning "enabling requires C4M_DEV"
#endif
#endif

#ifdef C4M_USE_FRAME_INTRINSIC
#warning "C4M_USE_FRAME_INSTRINSIC is ON"
#else
#warning "C4M_USE_FRAME_INSTRINSIC is OFF"
#endif

#ifdef C4M_VM_DEBUG
#warning "C4M_VM_DEBUG is ON (virtual machine debugging is enabled)"

#if defined(C4M_VM_DEBUG_DEFAULT)
#if C4M_VM_DEBUG_DEFAULT == true
#define __vm_debug_default
#endif
#endif

#ifdef __vm_debug_default
#warning "C4M_VM_DEBUG_DEFAULT is true (VM debugging on by default)"
#else
#warning "VM debugging is off unless a debug instruction turns it on."
#endif

#else
#warning "C4M_VM_DEBUG is ON (virtual machine debugging is disabled)"
#ifndef C4M_DEV
#warning "enabling requires C4M_DEV"
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
#warning "ADDRESS_SANTITIZER is ON (llvm memory checking enabled)"
#else
#warning "ADDRESS_SANTITIZER is OFF (llvm memory checking disabled)"
#ifndef C4M_DEV
#warning "enabling requires C4M_DEV"
#endif
#endif

#ifdef C4M_OMIT_UNDERFLOW_CHECKS
#warning "C4M_OMIT_UNDERFLOW_CHECKS is ON (Avoid some UBSAN falses)"
#else
#warning "C4M_OMIT_UNDERFLOW_CHECKS is OFF (Will see some UBSAN falses)"
#endif

#ifdef C4M_WARN_ON_ZERO_ALLOCS
#warning "C4M_WARN_ON_ZERO_ALLOCS is ON (Identify zero-length allocations)"
#else
#warning "C4M_WARN_ON_ZERO_ALLOCS is OFF (No spam about 0-length allocs)"
#ifndef C4M_DEV
#warning "enabling requires C4M_DEV"
#endif
#endif

#ifdef C4M_GC_SHOW_COLLECT_STACK_TRACES
#warning "C4M_GC_SHOW_COLLECT_STACK_TRACES is ON (Show C stack traces at every garbage collection invocation."
#else
#warning "C4M_GC_SHOW_COLLECT_STACK_TRACES is OFF (no C stack traces for GC collections)"
#ifndef C4M_DEV
#warning "enabling requires C4M_DEV"
#endif
#endif

#ifdef C4M_PARANOID_STACK_SCAN
#warning "C4M_PARANOID_STACK_SCAN is ON (Find slow, unaligned pointers)"
#else
#warning "C4M_PARANOID_STACK_SCAN is OFF (Does not look for unaligned pointers; this is slow and meant for debugging)"
#endif

#ifdef C4M_MIN_RENDER_WIDTH
#warning "C4M_MIN_RENDER_WIDTH is: " #C4M_MIN_RENDER_WIDTH
#else
#warning "C4M_MIN_RENDER_WIDTH is not set.
#endif

#ifdef _GNU_SOURCE
#warning "_GNU_SOURCE is ON (gnu stdlib)"
#else
#warning "_GNU_SOURCE is OFF (not a gnu stdlib)."
#endif

#ifdef HAVE_PTY_H
#warning "HAVE_PTY_H is ON (forkpty is available")
#else
#warning "HAVE_PTY_H is OFF (forkpty is NOT available")
#endif

#endif

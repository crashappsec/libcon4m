#pragma once

#define C4M_DEBUG // Get backtrace on exceptions.

// #define C4M_FULL_MEMCHECK
// #define C4M_STRICT_MEMCHECK
// #define C4M_SHOW_NEXT_ALLOCS 1000
// #define C4M_TRACE_GC

// #define C4M_GC_SHOW_COLLECT_STACK_TRACES
#define C4M_USE_FRAME_INTRINSIC
// #define C4M_GCT_MOVE        1
// #define C4M_GCT_PTR_TO_MOVE 1
// #define C4M_GC_ALL_OFF
// #define C4M_GC_ALL_ON
// #define C4M_TYPE_LOG

// When this is on, the `debug` instruction will run.
// Note that the debug instruction is not even generated unless
// C4M_DEV is on.

// #define C4M_VM_DEBUG
// #define C4M_VM_DEBUG_DEFAULT true

// This won't work on systems that require aligned pointers.
// #define C4M_PARANOID_STACK_SCAN
// #define C4M_PARSE_DEBUG

// UBSan hates our underflow check.
// #define C4M_OMIT_UNDERFLOW_CHECKS
//
// If you want to identify zero-length allocs while FULL_MEMCHECK is on...
// But
// #define C4M_WARN_ON_ZERO_ALLOCS

// #define C4M_DEBUG_PATTERNS

#ifdef C4M_NO_DEV_MODE
#undef C4M_DEV
#undef C4M_PARSE_DEBUG
#else
#ifdef C4M_PARSE_DEBUG
#define C4M_DEV
#endif
#define C4M_DEV
#endif

#if defined(C4M_VM_DEBUG)
#if !defined(C4M_DEV)
#error "Cannot debug VM when C4M_DEV_MODE is set."
#endif
#if !defined(C4M_VM_DEBUG_DEFAULT)
#define C4M_VM_DEBUG_DEFAULT false
#endif
#endif

#if !defined(HATRACK_PER_INSTANCE_AUX)
#error "HATRACK_PER_INSTANCE_AUX must be defined for con4m to compile."
#endif

#if defined(C4M_TRACE_GC) || defined(HATRACK_ALLOC_PASS_LOCATION)
#ifndef C4M_GC_STATS
#define C4M_GC_STATS
#endif
#endif

#if defined(C4M_TRACE_GC)
#ifndef C4M_GC_FULL_TRACE
#define C4M_GC_FULL_TRACE
#endif

#else // C4M_TRACE_GC
#undef C4M_GC_FULL_TRACE
#undef C4M_TRACE_GC
#endif

#ifndef C4M_MIN_RENDER_WIDTH
#define C4M_MIN_RENDER_WIDTH 80
#endif

// Useful options (mainly for dev) are commented out here.
// The logic below (and into the relevent header files) sets up defaults.
//
// Everything includes this; the ordering here is somewhat important
// due to interdependencies, though they can always be solved via
// prototyping.
#include "con4m/base.h"
#include "core/init.h"
#include "util/macros.h" // Helper macros
#include "core/kargs.h"  // Keyword arguments.
#include "util/random.h"

// Memory management
#include "core/refcount.h"
#include "core/gc.h"

// Core object.
#include "core/object.h"

#include "util/color.h"

// Basic "exclusive" (i.e., single threaded) list.
#include "adts/list.h"

// Type system API.
#include "core/typestore.h"
#include "core/type.h"

#include "adts/box.h"

// Extra data structure stuff.
#include "adts/tree.h"
#include "util/tree_pattern.h"
#include "adts/buffer.h"
#include "adts/tuple.h"

// Basic string handling.
#include "adts/codepoint.h"
#include "adts/string.h"
#include "util/breaks.h"
#include "io/ansi.h"
#include "util/hex.h"

#include "util/style.h"
#include "util/styledb.h"

// Our grid API.
#include "adts/grid.h"

// IO primitives.
#include "io/term.h"
#include "io/switchboard.h"
#include "io/subproc.h"

// Basic exception handling support.
#include "core/exception.h"

// Stream IO API.
#include "adts/stream.h"

// Helper functions for object marshal implementations to
// marshal primitive values.
#include "core/marshal.h"

// Mixed data type API.
#include "adts/mixed.h"

// Basic internal API to cache and access common string constants.
#include "util/conststr.h"

// Boxes for ordinal types
#include "adts/box.h"

// A few prototypes for literal handling.
#include "core/literal.h"

// Format string API
#include "util/format.h"
#include "util/fp.h"

// Path handling utilities.
#include "util/path.h"

// Yes we use cryptographic hashes internally for type IDing.
#include "crypto/sha.h"

// Virtual machine for running con4m code
#include "core/vm.h"

// Bitfields.
#include "adts/flags.h"

// Really int log2 only right now.
#include "util/math.h"

// For functions we need to wrap to use through the FFI.
#include "util/wrappers.h"

#include "util/cbacktrace.h"

// The compiler.
#include "compiler/ast_utils.h"
#include "compiler/compile.h"
#include "compiler/errors.h"
#include "compiler/lex.h"
#include "compiler/parse.h"
#include "compiler/scope.h"
#include "compiler/spec.h"
#include "compiler/cfgs.h"
#include "compiler/codegen.h"

#include "adts/dict.h"
#include "adts/set.h"

#include "core/ffi.h"
#include "util/watch.h"

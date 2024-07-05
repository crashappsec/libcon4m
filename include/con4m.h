#pragma once

// #define C4M_FULL_MEMCHECK
// #define C4M_DEBUG
// #define C4M_GC_STATS
// #define C4M_TRACE_GC

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

#ifdef C4M_TRACE_GC
#ifndef C4M_GC_STATS
#define C4M_GC_STATS
#endif

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
#include "con4m/init.h"
#include "con4m/macros.h" // Helper macros, mostly 3rd party stuff.
#include "con4m/kargs.h"  // Keyword arguments.
#include "con4m/random.h"

// Memory management
#include "con4m/refcount.h"
#include "con4m/gc.h"
#include "con4m/object.h"
#include "con4m/color.h"

// Basic "exclusive" (i.e., single threaded) list.
#include "con4m/list.h"

// Type system API.
#include "con4m/type.h"

#include "con4m/box.h"

// Extra data structure stuff.
#include "con4m/tree.h"
#include "con4m/tree_pattern.h"
#include "con4m/buffer.h"
#include "con4m/tuple.h"

// Basic string handling.
#include "con4m/codepoint.h"
#include "con4m/string.h"
#include "con4m/breaks.h"
#include "con4m/ansi.h"
#include "con4m/hex.h"

#include "con4m/style.h"
#include "con4m/styledb.h"

// Our grid API.
#include "con4m/grid.h"

// IO primitives.
#include "con4m/term.h"
#include "con4m/switchboard.h"
#include "con4m/subproc.h"

// Basic exception handling support.
#include "con4m/exception.h"

// Stream IO API.
#include "con4m/stream.h"

// Helper functions for object marshal implementations to
// marshal primitive values.
#include "con4m/marshal.h"

// Mixed data type API.
#include "con4m/mixed.h"

// Basic internal API to cache and access common string constants.
#include "con4m/conststr.h"

// Boxes for ordinal types
#include "con4m/box.h"

// A few prototypes for literal handling.
#include "con4m/literal.h"

// Format string API
#include "con4m/format.h"

#include "con4m/fp.h"

// Path handling utilities.
#include "con4m/path.h"

// Yes we use cryptographic hashes internally for type IDing.
#include "crypto/sha.h"

// Virtual machine for running con4m code
#include "con4m/vm.h"

// Bitfields.
#include "con4m/flags.h"

// Really int log2 only right now.
#include "con4m/math.h"

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

#include "con4m/dict.h"
#include "con4m/set.h"

#include "con4m/ffi.h"
#include "con4m/watch.h"

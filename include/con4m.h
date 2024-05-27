#pragma once
#define C4M_TYPE_LOG

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
#include "con4m/xlist.h"

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

// The front end.
#include "frontend/treematch.h"
#include "frontend/compile.h"
#include "frontend/errors.h"
#include "frontend/lex.h"
#include "frontend/parse.h"
#include "frontend/scope.h"
#include "frontend/spec.h"
#include "frontend/partial.h"
#include "frontend/cfgs.h"

#include "con4m/set.h"

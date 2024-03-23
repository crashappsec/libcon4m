#pragma once

typedef void *object_t;

// Everything includes this; the ordering here is somewhat important
// due to interdependencies, though they can always be solved via
// prototyping.
#include <con4m/base.h>

#include <con4m/macros.h>  // Helper macros, mostly 3rd party stuff.
#include <con4m/kargs.h>   // Keyword arguments.
#include <con4m/random.h>

// Memory management
#include <con4m/refcount.h>
#include <con4m/gc.h>
#include <con4m/object.h>

#include <con4m/color.h>

// Basic "exclusive" (i.e., single threaded) list.
#include <con4m/xlist.h>

// Type system API.
#include <con4m/type.h>

// Extra data structure stuff.
#include <con4m/set.h>
#include <con4m/tree.h>


// Basic string handling.
#include <con4m/codepoint.h>
#include <con4m/string.h>
#include <con4m/breaks.h>
#include <con4m/ansi.h>
#include <con4m/hex.h>

#include <con4m/style.h>
#include <con4m/styledb.h>

// Our grid API.
#include <con4m/grid.h>

// IO primitives.
#include <con4m/term.h>
#include <con4m/switchboard.h>
#include <con4m/subproc.h>

// Basic exception handling support.
#include <con4m/exception.h>

// Helper functions for object marshal implementations to
// marshal primitive values.
#include <con4m/marshal.h>

// Yes we use cryptographic hashes internally for type IDing.
#include <crypto/sha.h>

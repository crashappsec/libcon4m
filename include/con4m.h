#pragma once

typedef void *object_t;

// Everything includes this; the ordering here is somewhat important
// due to interdependencies, though they can always be solved via
// prototyping.
#include <con4m/base.h>


#include <con4m/macros.h>  // Helper macros, mostly 3rd party stuff.
#include <con4m/kargs.h>   // Keyword arguments.
#include <con4m/random.h>

#include <con4m/objectbase.h>
#include <con4m/c4strbase.h>

// Memory management
#include <con4m/refcount.h>
#include <con4m/gc.h>
#include <con4m/object.h>

// Single-threaded data structures for internal use.
#include <con4m/xlist.h>

// Type system.
#include <con4m/types.h>

// Basic string handling.
#include <con4m/codepoint.h>
#include <con4m/colors.h>
#include <con4m/c4str.h>
#include <con4m/style.h>
#include <con4m/breaks.h>
#include <con4m/ansi.h>
#include <con4m/hex.h>

#include <con4m/styledb.h>

// Other core data structures.
#include <con4m/dict.h>
#include <con4m/lists.h>

// Our grid abstraction.
#include <con4m/grid.h>

#include <con4m/buffer.h>

// IO primitives.
#include <con4m/term.h>
#include <con4m/switchboard.h>
#include <con4m/subproc.h>

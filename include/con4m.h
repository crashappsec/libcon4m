#pragma once

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

// Basic string handling.
#include <con4m/colors.h>
#include <con4m/style.h>
#include <con4m/str.h>
#include <con4m/breaks.h>
#include <con4m/ansi.h>
#include <con4m/hex.h>

// Our grid abstraction.
#include <con4m/grid.h>

// IO primitives.
#include <con4m/term.h>
#include <con4m/switchboard.h>
#include <con4m/subproc.h>

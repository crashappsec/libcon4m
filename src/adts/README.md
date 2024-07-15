# ADTs-- Abstract Data Types

This directory contains C implementations for data types provided by
Con4m. A few things primarily live in Hatrack-- the set implementation
and the dictionary implementation. What's in this directory is minor
con4m-specific stuff, like integration with the garbage collector.

Current status of data types:

1. Strings. Con4m strings are meant to be non-mutable. The API
supports both UTF-8 and UTF-32 encodings (I'm considering having
strings hang on to both representations if the string is used in a way
that requires a conversion). Additionally, strings carry style
information out-of-band, that is applied when rendering (generally via
the ascii module in the io directory). These are available through
Con4m itself.

The core of the implementation is in `strings.c`, but a bunch of
related code lives in the `util` subdirectory, like code to support
'rich' text literals (available using a variety of lit modifiers).

2. Grids. Grids build on top of strings to make it easy to do things
like tables, flows, etc. These are meant to be first class citizens in
Con4m, and currently have some functionaility exposed.

3. Supporting data structures for strings and grids, that may never be
directly exposed as first class data types, like styles, colors, line
breaks, etc.

4. A buffer type, available from Con4m.

5. A stream type, not yet exposed (but will be).

6. A `callback` type, which probably doesn't work yet, but should be
exposed.

7. A tuple type, exposed to the language.

8. A tree type, not yet exposed.

9. Standard numeric types. Currently, we lack 16 bit and 128 bit
types. `char` in Con4m is a 32 bit int.

10. A bitfield, not yet exposed.

11. An internal `box` type. Eventually, the `mixed` type will be
exposed, but it's not close to done.

12. Some light hatrack wrappers.  `set` and `dict` are well exposed,
but things like `rings` are not.

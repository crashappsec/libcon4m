#include <con4m.h>

// Hashing a tree-based data structure down to a single integer can
// save a lot of work if the user does want to do any dynamic type
// checking at all.
//
// Generally, at run-time, objects mostly have instantiated types, but
// we still have to check against generic function parameters.
//
// And, I'd like to have a repl where we're incrementally typing,
// which also requires generic typing.
//
// The biggest challenge is that at time unit T, you might have two
// types that are (`x, `y) -> `z; sometimes those will be the same
// type, and sometimes they will be the same representation of
// different types. We can't have a hash value that say's they're
// always equal.
//
// The first con4m hash has been working pretty well, and I'm going to
// continue to use it for the time being. But at some point, I am
// going to revisit it, as I know how to do a lot better. The
// fundamental problen is that the hashes are almost fully one-way, so
// you still have to keep around the full graph for anything with
// generics in it. Not that big a deal, but eventually I wouldn't mind
// not having to keep full trees around at run-time.
//
//
// Here is the high-level on the current state of the type system,
// btw, covering the current, planned and possible "kinds" of types in
// con4m.


// SOLID type kinds that will never go away (tho might evolve a bit).
// -----------------------------------------------------------------
// 1. Primitive Types. These are built-in, non-structured types with
//    no directly exposed fields, like int, string, etc. Currently, a
//    lot of our config-related data types all fall in this bucket. It
//    also can include things like the 'void' type, although I currently
//    treat 'type' 'error' and 'void' as their own thing.
//
// 2. List Types. These are container types that take a single type
//    parameter (generally their their contents). Items must all have
//    a unified type, but in exchange you can have an arbitrary number
//    of items.  Con4m is close to supporting multiple built-in list
//    types, including queues, stacks, and rings, with single-threaded
//    and multi-threaded variants. All lists use traditional [] syntax
//    for literals.
//
// 3. Dictionary Types. These are container types that take two, or
//    sometimes one type parameter (e.g., Sets), and use { } for syntax.
//
// 4. Tuple Types. These are container types that take an arbitrary
//    but fixed number of type parameters. Importantly, the parameters
//    are positional, and currently anonymous (so they're structurally
//    typed).
//
// 5. Function types. I'm going to want to extend these to handle
//    keyword arguments soon, so the representation here is probably
//    going to change. Right now, conceptually there are the
//    parameters, which are effectively a tuple, and the return value,
//    which gets special treatment. When I add keyword parameters,
//    those parameters will essentially be a third 'part' of the
//    signature, and will work similar to how objects will end up
//    working (see below).
//
// 6. Type variables. These stand in for any information we haven't
//    resolved yet. These are key to how type checking works
//    statically, but we need them when running too.
//
//    This is particularly the case because I am taking a different
//    implementation approach to most languages, which generate
//    implementations for each instance type. I currently rely on
//    run-time boxing, and don't plan to change that unless it's
//    someday incredibly obviously important.
//
// Basically, type variables are the only 'unknowns', but can easily
// live as components of anything other than a primitive type.
//
//
// Stuff that's there that I'm thinking about ditching.
// -----------------------------------------------------------------
// 1. Currently, TypeSpecs are their own kind. Types can basically be
//    run-time values that people can assign, print, etc. The reason
//    they're not a primitive type is that they are actually
//    parameterized currently. Essentially, you want people to be able
//    to specify their own type to go along with some data field, you
//    can parameterize it to narrow what is allowed. For instance, you
//    can declare a field to be typespec[list[`x]], and values assigned
//    to it MUST be in that form.
//
// 2. I also have "OneOf", which is basically an algebraic sum
//    type. It's mainly meant for making typespec constraints more
//    useful. I find unrestructed types like this in a language have a
//    lot of practical issues that don't lend themselves well to the
//    type of language con4m is becoming.
//
// Basically, since we have explicit post-execution validation
// checking of fields, #1 isn't really necessary, which makes #2
// unnecessary. I haven't made a firm decision yet, though.
//
//
// Stuff that I'm planning that I haven't gotten to yet.
// -----------------------------------------------------------------
// 1. Object types. Once Chalk doesn't need anything specific from
// con4m, this will probably end up my top priority, and it isn't too
// much work. From a type system perspective, this will be kind of a
// dual of tuples... they will be structs of data fields, but order
// will be unimportant, and will have name equivolence. Functions will
// probably *not* be considered data objects, since I have no plans
// for inheritance.
//
// 2. Reference types. Every type will have an associated reference
//    type. Go, aliasing.
//
// 3. "Maybe" types. Basically, this is an highly constrained
// algabraic type that 'extends' a type by adding null (the `void`
// type) as an optional value. This one should be easy relative to
// more generic algabraic types.
//
// Stuff that I'm considering, but am in no hurry to decide.
// -----------------------------------------------------------------
// Most of the stuff here is possible to do well statically, depending
// on constraints, but I really want to wait and see if I can nail the
// usability.
//
// 1. Interfaces; especially if they basically give the feeling of
//    allowing statically checkable duck typing.  These are
//    essentially constraints on generic types. But, it does have some
//    implications because all types, including primitive types
//    suddenly, if we think of types as sets, primitive types can all
//    the sudden be in multiple sets. I think I need to get objects to
//    the point where I'm happy before I can play around and decide if
//    this will work.
//
// 2. 'Either' types. While I'd like to do this for return values instead of
//    exception handling, given we're doing garbage collection, exception
//    handling is pretty easy, whereas this is pretty hard to do well.
//    I think I'm going to start w/ exceptions, and then once Maybe is in,
//    I'll consider this in more detail.

extern type_t type_hash(type_node_t *);

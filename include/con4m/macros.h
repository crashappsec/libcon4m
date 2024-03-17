#pragma once

/*
 * This file is mostly third party macros, with some additions making
 * use of them of my own, down below.
 *
 * I am using these mainly to help provide easier interfaces to
 * variable argument functions.  For instance, we automatically add
 * sentinel NULLs at the ends of wrapped calls. And, we can expand a
 * single argument into multiple arguments-- for instance, with format
 * string arguments, we take one argument and pass both type
 * information and the argument, automatically applying the
 * transformation to all arguments.
 *
 */

/*=============================================================================
    Copyright (c) 2015 Paul Fultz II
    cloak.h
    Distributed under the Boost Software License, Version 1.0. (See
    http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#define CAT(a, ...)           PRIMITIVE_CAT(a, __VA_ARGS__)
#define PRIMITIVE_CAT(a, ...) a##__VA_ARGS__

#define COMPL(b) PRIMITIVE_CAT(COMPL_, b)
#define COMPL_0  1
#define COMPL_1  0

#define BITAND(x)   PRIMITIVE_CAT(BITAND_, x)
#define BITAND_0(y) 0
#define BITAND_1(y) y

#define INC(x) PRIMITIVE_CAT(INC_, x)
#define INC_0  1
#define INC_1  2
#define INC_2  3
#define INC_3  4
#define INC_4  5
#define INC_5  6
#define INC_6  7
#define INC_7  8
#define INC_8  9
#define INC_9  9

#define DEC(x) PRIMITIVE_CAT(DEC_, x)
#define DEC_0  0
#define DEC_1  0
#define DEC_2  1
#define DEC_3  2
#define DEC_4  3
#define DEC_5  4
#define DEC_6  5
#define DEC_7  6
#define DEC_8  7
#define DEC_9  8

#define CHECK_N(x, n, ...) n
#define CHECK(...)         CHECK_N(__VA_ARGS__, 0, )
#define PROBE(x)           x, 1,

#define IS_PAREN(x)         CHECK(IS_PAREN_PROBE x)
#define IS_PAREN_PROBE(...) PROBE(~)

#define NOT(x) CHECK(PRIMITIVE_CAT(NOT_, x))
#define NOT_0  PROBE(~)

#define COMPL(b) PRIMITIVE_CAT(COMPL_, b)
#define COMPL_0  1
#define COMPL_1  0

#define BOOL(x) COMPL(NOT(x))

#define IIF(c)        PRIMITIVE_CAT(IIF_, c)
#define IIF_0(t, ...) __VA_ARGS__
#define IIF_1(t, ...) t

#define IF(c) IIF(BOOL(c))

#define EAT(...)
#define EXPAND(...) __VA_ARGS__
#define WHEN(c)     IF(c)(EXPAND, EAT)

#define EMPTY()
#define DEFER(id)    id EMPTY()
#define OBSTRUCT(id) id DEFER(EMPTY)()

#define REPEAT(count, macro, ...)                                              \
    WHEN(count)                                                                \
    (OBSTRUCT(REPEAT_INDIRECT)()(DEC(count), macro, __VA_ARGS__)OBSTRUCT(      \
        macro)(DEC(count), __VA_ARGS__))
#define REPEAT_INDIRECT() REPEAT

#define WHILE(pred, op, ...)                                                   \
    IF(pred(__VA_ARGS__))                                                      \
    (OBSTRUCT(WHILE_INDIRECT)()(pred, op, op(__VA_ARGS__)), __VA_ARGS__)
#define WHILE_INDIRECT() WHILE

#define PRIMITIVE_COMPARE(x, y) IS_PAREN(COMPARE_##x(COMPARE_##y)(()))

#define IS_COMPARABLE(x) IS_PAREN(CAT(COMPARE_, x)(()))

#define NOT_EQUAL(x, y)                                                        \
    IIF(BITAND(IS_COMPARABLE(x))(IS_COMPARABLE(y)))                            \
    (PRIMITIVE_COMPARE, 1 EAT)(x, y)

#define EQUAL(x, y) COMPL(NOT_EQUAL(x, y))

#define COMMA() ,

#define COMMA_IF(n) IF(n)(COMMA, EAT)()

/*
 * Slightly modified public domain code. It's based on some of the
 * thoughts and macros from the person above.
 *
 * Original source: https://github.com/swansontec/map-macro
 *
 *
 * Name:          map.h
 * Description:   Macro to apply one macro to a __VA_ARGS__ list.
 * Author:        William Swanson (Original)
 *
 */

#define EVAL0(...) __VA_ARGS__
#define EVAL1(...) EVAL0(EVAL0(EVAL0(__VA_ARGS__)))
#define EVAL2(...) EVAL1(EVAL1(EVAL1(__VA_ARGS__)))
#define EVAL3(...) EVAL2(EVAL2(EVAL2(__VA_ARGS__)))
#define EVAL4(...) EVAL3(EVAL3(EVAL3(__VA_ARGS__)))
#define EVAL(...)  EVAL4(EVAL4(EVAL4(__VA_ARGS__)))

#define MAP_END(...)
#define MAP_OUT
#define MAP_COMMA ,

#define MAP_GET_END2()             0, MAP_END
#define MAP_GET_END1(...)          MAP_GET_END2
#define MAP_GET_END(...)           MAP_GET_END1
#define MAP_NEXT0(test, next, ...) next MAP_OUT
#define MAP_NEXT1(test, next)      MAP_NEXT0(test, next, 0)
#define MAP_NEXT(test, next)       MAP_NEXT1(MAP_GET_END test, next)

#define MAP0(f, x, peek, ...) f(x) MAP_NEXT(peek, MAP1)(f, peek, __VA_ARGS__)
#define MAP1(f, x, peek, ...) f(x) MAP_NEXT(peek, MAP0)(f, peek, __VA_ARGS__)

#define MAP_LIST_NEXT1(test, next) MAP_NEXT0(test, MAP_COMMA next, 0)
#define MAP_LIST_NEXT(test, next)  MAP_LIST_NEXT1(MAP_GET_END test, next)

#define MAP_LIST0(f, x, peek, ...)                                             \
    f(x) MAP_LIST_NEXT(peek, MAP_LIST1)(f, peek, __VA_ARGS__)
#define MAP_LIST1(f, x, peek, ...)                                             \
    f(x) MAP_LIST_NEXT(peek, MAP_LIST0)(f, peek, __VA_ARGS__)

/**
 * Applies the function macro `f` to each of the remaining parameters.
 */
#define MAP(f, ...) EVAL(MAP1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

/**
 * Applies the function macro `f` to each of the remaining parameters and
 * inserts commas between the results.
 */
#define MAP_LIST(f, ...) EVAL(MAP_LIST1(                                       \
    f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

// These macros are from a blog post:
// https://gustedt.wordpress.com/2010/06/08/detect-empty-macro-arguments/

#define _ARG16(_0,                                                             \
               _1,                                                             \
               _2,                                                             \
               _3,                                                             \
               _4,                                                             \
               _5,                                                             \
               _6,                                                             \
               _7,                                                             \
               _8,                                                             \
               _9,                                                             \
               _10,                                                            \
               _11,                                                            \
               _12,                                                            \
               _13,                                                            \
               _14,                                                            \
               _15,                                                            \
               ...)                                                            \
    _15

#define HAS_COMMA(...) _ARG16(__VA_ARGS__, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
			      1, 1, 0)
#define _TRIGGER_PARENTHESIS_(...) ,

#define ISEMPTY(...)                                                           \
    _ISEMPTY(/* test if there is just one argument, eventually an empty        \
                one */                                                         \
             HAS_COMMA(__VA_ARGS__), /* test if _TRIGGER_PARENTHESIS_ together \
                                        with the argument adds a comma */      \
             HAS_COMMA(_TRIGGER_PARENTHESIS_                                   \
                           __VA_ARGS__),        /* test if the argument w/     \
					   a parenthesis adds a comma */       \
             HAS_COMMA(__VA_ARGS__(/*empty*/)), /* test if placing it between  \
                                                 _TRIGGER_PARENTHESIS_ and the \
                                                   parenthesis adds a comma */ \
             HAS_COMMA(_TRIGGER_PARENTHESIS_ __VA_ARGS__(/*empty*/)))

#define PASTE5(_0, _1, _2, _3, _4) _0##_1##_2##_3##_4
#define _ISEMPTY(_0, _1, _2, _3)   HAS_COMMA(PASTE5(_IS_EMPTY_CASE_, _0, _1, _2, _3))
#define _IS_EMPTY_CASE_0001        ,

// Below is our stuff, mostly helper macros.

// Use int to hold booleans, so that we can use them in varargs functions.
// Use _Bool if you don't want that.

#undef bool
#define bool int

// This packages up a format string parameter into two pieces, an
// integer representing the type to use, and the parameter.  Oddly, if
// I set the default value to FA_OBJECT (at least for the final
// parameter in the case where this came up), the value pops out as
// MIN_INT, at least on my laptop (haven't tried anywhere else).  Need
// to figure out why?  For the moment, changing it to FA_STR, since
// format does NOT currently treat STRs and generic objects
// differently.
#define FORMAT_PARAMETER(x)                                                    \
    _Generic(x, any_str_t * : FA_STR, char : FA_CHAR, char * : FA_CSTR,        \
bool : FA_BOOL, int64_t : FA_INT, uint64_t : FA_UINT, double : FA_DOUBLE,      \
default : FA_STR), x

// If you try to pass a 32-bit value into a format parameter, you're
// going to have it treated like a bool.  This is easy to notice,
// because your value should usually get cast to 1 ("True").  You can
// use these macros if you don't want to change the size of your data
// types.
//
// This will most often happen when you try to pass in a numeric
// literal, instead of a variable.
#define I(x) ((int64_t)x)
#define U(x) ((uint64_t)x)
#define R(x) ((double)x)

#define KARG_END 0
// These are helper functions to make declaring format or kargs functions
// easier.
#define FMT(...)                                                               \
    IF(ISEMPTY(__VA_ARGS__))                                                   \
    (EMPTY(), MAP_LIST(FORMAT_PARAMETER, __VA_ARGS__) MAP_COMMA)0
#define KFUNC(...) IF(ISEMPTY(__VA_ARGS__))(EMPTY(), __VA_ARGS__ MAP_COMMA) 0

#define mem_copy(d, s, l) (l ? memcpy(d, s, l) : d)

#define thread_add_destructor(var, f)                                          \
    {                                                                          \
        static __thread pthread_key_t __destructor_key = 0;                    \
        if (!__destructor_key) {                                               \
            pthread_key_create(&__destructor_key, f);                          \
            pthread_setspecific(__destructor_key, var);                        \
        }                                                                      \
    }

#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#define foreach(x, y) while ((x = iterator_next(y)) != NULL)

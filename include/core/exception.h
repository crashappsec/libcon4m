#pragma once
#include "con4m.h"

// This exception handling mechanism is not meant to fully simulate a
// high-level language mechanism by itself.  Particularly, the EXCEPT
// macro exports an c4m_exception_t * object you get with the
// current_exception() macro, and you are excepted to switch on the
// object's type field (instead of having multiple EXCEPT blocks).
//
// You are responsible for either handling every possible exception
// that could be thrown (including system exceptions), or calling the
// RERAISE() macro.  So always make sure to have a default label.
//
// You MUST have an EXCEPT block,  a FINALLY block after the
// EXCEPT block and a TRY_END last (which we recommend adding
// a trailing semicolon to).
//
// If you don't do these things, you'll get compile errors.
//
// You shouldn't have any code after your switch statement in the
// EXCEPT block, and before the FINALLY or TRY_END.
//
// You may NOT use return, or any other control mechanism to escape
// any block, unless you use one of the two macros we provide:
//
// 1) JUMP_TO_FINALLY(), which must only be used in an EXCEPT block
// 2) JUMP_TO_TRY_END(), which can be used in a FINALLY block, but
//                       can only be used in an EXCEPT block if you
//                       have no FINALLY block.
//
// After your finally block runs(if any), you can do whatever you
// want.  So if you needed to, in concept, return a value from the
// middle of an except clause (for instance), then you can do
// something like:
//
// 1) Set a flag indicating you want to return after the finally code runs.
// 2) Use JUMP_TO_FINALLY()
// 3) After the TRY_END macro, test your flag and return whatever value when
//    appropriate.
//
//
// If the exception is NOT handled properly, you must either use
// RAISE() or RERAISE(), which will set the state to
// EXCEPTION_NOT_HANDLED, and then move to the next stage (either
// FINALLY or TRY_END, as appropriate), before unwinding the stack.
//
// Note that if you need two TRY blocks in one function you would end
// up with name collisions with the macros. We provide a second set of
// macros that allow you to label your exception handling code, to avoid
// this problem:
// 1) LFINALLY(label)
// 2) LTRY_END(label)
// 3) LJUMP_TO_FINALLY(label)
// 4) LJUMP_TO_TRY_END(label)
//
// Also, it is okay to add your own braces.
//
// TRY pushes a new exception frame onto the thread's exception
// stack.  It then does a setjmp and starts executing the block (if
// setjmp returns 0).  If longjmp is called, then the EXCEPT macro
// notices, and passes control to its block, so you can handle.
//
// The TRY_END call cleans up the exception stack and closes out
// hidden blocks created by the TRY and EXCEPT macros.
//
// You MUST NOT put the try/except block code in curly braces!
//

#define C4M_TRY                                                          \
    jmp_buf                   _c4x_jmpbuf;                               \
    int                       _c4x_setjmp_val;                           \
    c4m_exception_stack_t    *_c4x_stack;                                \
    c4m_exception_frame_t    *_c4x_frame;                                \
    volatile int              _c4x_exception_state   = C4M_EXCEPTION_OK; \
    volatile c4m_exception_t *_c4x_current_exception = NULL;             \
                                                                         \
    _c4x_stack      = c4m_exception_push_frame(&_c4x_jmpbuf);            \
    _c4x_frame      = _c4x_stack->top;                                   \
    _c4x_setjmp_val = setjmp(_c4x_jmpbuf);                               \
                                                                         \
    if (!_c4x_setjmp_val) {
#define C4M_EXCEPT                                           \
    }                                                        \
    else                                                     \
    {                                                        \
        _c4x_current_exception = _c4x_stack->top->exception; \
        _c4x_setjmp_val        = setjmp(_c4x_jmpbuf);        \
        if (!_c4x_setjmp_val) {
#define C4M_LFINALLY(user_label)                                          \
    }                                                                     \
    else                                                                  \
    {                                                                     \
        _c4x_exception_state            = C4M_EXCEPTION_IN_HANDLER;       \
        _c4x_frame->exception->previous = (void *)_c4x_current_exception; \
        _c4x_current_exception          = _c4x_stack->top->exception;     \
    }                                                                     \
    }                                                                     \
    _c4x_finally_##user_label:                                            \
    {                                                                     \
        if (!setjmp(_c4x_jmpbuf)) {
// The goto here avoids strict checking for unused labels.
#define C4M_LTRY_END(user_label)                                          \
    goto _c4x_try_end_##user_label;                                       \
    }                                                                     \
    else                                                                  \
    {                                                                     \
        _c4x_exception_state            = C4M_EXCEPTION_IN_HANDLER;       \
        _c4x_frame->exception->previous = (void *)_c4x_current_exception; \
        _c4x_current_exception          = _c4x_stack->top->exception;     \
    }                                                                     \
    }                                                                     \
    _c4x_try_end_##user_label : _c4x_stack->top = _c4x_frame->next;       \
    if (_c4x_exception_state == C4M_EXCEPTION_IN_HANDLER                  \
        || _c4x_exception_state == C4M_EXCEPTION_NOT_HANDLED) {           \
        if (!_c4x_frame->next) {                                          \
            c4m_exception_uncaught((void *)_c4x_current_exception);       \
        }                                                                 \
        c4m_exception_free_frame(_c4x_frame, _c4x_stack);                 \
        _c4x_stack->top->exception = (void *)_c4x_current_exception;      \
        _c4x_frame                 = _c4x_stack->top;                     \
        _c4x_frame->exception      = (void *)_c4x_current_exception;      \
        longjmp(*(_c4x_frame->buf), 1);                                   \
    }                                                                     \
    c4m_exception_free_frame(_c4x_frame, _c4x_stack);

#define C4M_LJUMP_TO_FINALLY(user_label) goto _c4x_finally_##user_label
#define C4M_LJUMP_TO_TRY_END(user_label) goto _c4x_try_end_##user_label

#define C4M_JUMP_TO_FINALLY() C4M_LJUMP_TO_FINALLY(default_label)
#define C4M_JUMP_TO_TRY_END() C4M_LJUMP_TO_TRY_END(default_label)
#define C4M_FINALLY           C4M_LFINALLY(default_label)
#define C4M_TRY_END           C4M_LTRY_END(default_label)

#if defined(C4M_DEBUG) && defined(C4M_BACKTRACE_SUPPORTED)
extern c4m_grid_t                        *c4m_get_c_backtrace(int);
extern thread_local c4m_exception_stack_t __exception_stack;

#define c4m_trace() c4m_get_c_backtrace(1)
#else
#define c4m_trace() NULL
#endif

#define C4M_CRAISE(s, ...)                                  \
    c4m_exception_raise(                                    \
        c4m_alloc_exception(s, __VA_OPT__(, ) __VA_ARGS__), \
        c4m_trace(),                                        \
        __FILE__,                                           \
        __LINE__)

#define C4M_RAISE(s, ...)                                      \
    c4m_exception_raise(                                       \
        c4m_alloc_str_exception(s __VA_OPT__(, ) __VA_ARGS__), \
        c4m_trace(),                                           \
        __FILE__,                                              \
        __LINE__)

#define C4M_RERAISE()                                 \
    _c4x_exception_state = C4M_EXCEPTION_NOT_HANDLED; \
    longjmp(*(_c4x_current_frame->buf), 1)

#define C4M_X_CUR() ((void *)_c4x_current_exception)

c4m_exception_t *_c4m_alloc_exception(const char *s, ...);
#define c4m_alloc_exception(s, ...) _c4m_alloc_exception(s, C4M_VA(__VA_ARGS__))

c4m_exception_t *_c4m_alloc_str_exception(c4m_utf8_t *s, ...);
#define c4m_alloc_str_exception(s, ...) \
    _c4m_alloc_str_exception(s, C4M_VA(__VA_ARGS__))

enum : int64_t {
    C4M_EXCEPTION_OK,
    C4M_EXCEPTION_IN_HANDLER,
    C4M_EXCEPTION_NOT_HANDLED
};

c4m_exception_stack_t *c4m_exception_push_frame(jmp_buf *);
void                   c4m_exception_free_frame(c4m_exception_frame_t *,
                                                c4m_exception_stack_t *);
void                   c4m_exception_uncaught(c4m_exception_t *);
void                   c4m_exception_raise(c4m_exception_t *,
                                           c4m_grid_t *,
                                           char *,
                                           int) __attribute((__noreturn__));
c4m_utf8_t            *c4m_repr_exception_stack_no_vm(c4m_utf8_t *);

static inline c4m_utf8_t *
c4m_exception_get_file(c4m_exception_t *exception)
{
    return c4m_new(c4m_type_utf8(), c4m_kw("cstring", c4m_ka(exception->file)));
}

static inline uint64_t
c4m_exception_get_line(c4m_exception_t *exception)
{
    return exception->line;
}

static inline c4m_utf8_t *
c4m_exception_get_message(c4m_exception_t *exception)
{
    return exception->msg;
}

void c4m_exception_register_uncaught_handler(void (*)(c4m_exception_t *));

#define C4M_RAISE_SYS()                                                     \
    {                                                                       \
        char buf[BUFSIZ];                                                   \
        strerror_r(errno, buf, BUFSIZ);                                     \
        C4M_RAISE(c4m_new(c4m_type_utf8(), c4m_kw("cstring", c4m_ka(buf))), \
                  c4m_kw("error_code", c4m_ka(errno)));                     \
    }

extern thread_local c4m_exception_stack_t __exception_stack;

static inline void
c4m_raise_errcode(int code)
{
    char msg[2048] = {
        0,
    };

    if (strerror_r(code, msg, 2048)) {}
    C4M_RAISE(c4m_new(c4m_type_utf8(), c4m_kw("cstring", c4m_ka(msg))));
}

static inline void
c4m_raise_errno()
{
    c4m_raise_errcode(errno);
}

#define c4m_unreachable()                                   \
    {                                                       \
        c4m_utf8_t *s = c4m_cstr_format(                    \
            "Reached code that the developer "              \
            "(wrongly) believed was unreachable, at {}:{}", \
            c4m_new_utf8(__FILE__),                         \
            c4m_box_i32(__LINE__));                         \
        C4M_RAISE(s);                                       \
    }

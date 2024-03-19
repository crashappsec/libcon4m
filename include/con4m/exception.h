#pragma once
#include <con4m.h>

// This exception handling mechanism is not meant to fully simulate a
// high-level language mechanism by itself.  Particularly, the EXCEPT
// macro exports an exception_t * object you get with the
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

#define TRY                                                                    \
    jmp_buf            __xh_jmpbuf;                                            \
    int                __xh_setjmp_val;                                        \
    exception_stack_t *__xh_stack;                                             \
    exception_frame_t *__xh_frame;                                             \
    int                __xh_exception_state   = EXCEPTION_OK;                  \
    exception_t       *__xh_current_exception = NULL;                          \
                                                                               \
    __xh_stack      = exception_push_frame(&__xh_jmpbuf);                      \
    __xh_frame      = __xh_stack->top;                                         \
    __xh_setjmp_val = setjmp(__xh_jmpbuf);                                     \
                                                                               \
    if (!__xh_setjmp_val) {


#define EXCEPT                                                                 \
    }									       \
    else                                                                       \
    {                                                                          \
        __xh_current_exception = __xh_stack->top->exception;                   \
        __xh_setjmp_val        = setjmp(__xh_jmpbuf);                          \
        if (!__xh_setjmp_val) {


#define LFINALLY(user_label)                                                   \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        __xh_exception_state            = EXCEPTION_IN_HANDLER;                \
        __xh_frame->exception->previous = __xh_current_exception;              \
        __xh_current_exception          = __xh_stack->top->exception;          \
    }                                                                          \
    }                                                                          \
    __xh_finally_##user_label:                                                 \
    {                                                                          \
        if (!setjmp(__xh_jmpbuf)) {


// The goto here avoids strict checking for unused labels.
#define LTRY_END(user_label)                                                   \
    goto __xh_try_end_##user_label;                                            \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        __xh_exception_state            = EXCEPTION_IN_HANDLER;                \
        __xh_frame->exception->previous = __xh_current_exception;              \
        __xh_current_exception          = __xh_stack->top->exception;          \
    }                                                                          \
    }                                                                          \
    __xh_try_end_##user_label : __xh_stack->top = __xh_frame->next;            \
    if (__xh_exception_state == EXCEPTION_IN_HANDLER                           \
        || __xh_exception_state == EXCEPTION_NOT_HANDLED) {                    \
        if (!__xh_frame->next) {                                               \
            exception_uncaught(__xh_current_exception);                        \
        }                                                                      \
        exception_free_frame(__xh_frame, __xh_stack);                          \
        __xh_stack->top->exception = __xh_current_exception;                   \
        __xh_frame                 = __xh_stack->top;                          \
        __xh_frame->exception      = __xh_current_exception;                   \
        longjmp(*(__xh_frame->buf), 1);                                        \
    }                                                                          \
    exception_free_frame(__xh_frame, __xh_stack);

#define LJUMP_TO_FINALLY(user_label) goto __xh_finally_##user_label
#define LJUMP_TO_TRY_END(user_label) goto __xh_try_end_##user_label

#define JUMP_TO_FINALLY() LJUMP_TO_FINALLY(default_label)
#define JUMP_TO_TRY_END() LJUMP_TO_TRY_END(default_label)
#define FINALLY           LFINALLY(default_label)
#define TRY_END           LTRY_END(default_label)

#define CRAISE(s, ...) exception_raise(alloc_exception(s,                      \
        IF(ISEMPTY(__VA_ARGS__))(EMPTY(), __VA_ARGS__)) , __FILE__, __LINE__)

#define RAISE(s, ...) exception_raise(alloc_str_exception(s,                   \
        IF(ISEMPTY(__VA_ARGS__))(EMPTY(), __VA_ARGS__)) , __FILE__, __LINE__)

#define RERAISE()							       \
    __xh_exception_state = EXCEPTION_NOT_HANDLED;                              \
    longjmp(*(__xh_current_frame->buf), 1)

#define X_CUR() __xh_current_exception

exception_t *_alloc_exception(char *s, ...);
#define alloc_exception(s, ...) _alloc_exception(s, KFUNC(__VA_ARGS__))

exception_t *_alloc_str_exception(utf8_t *s, ...);
#define alloc_str_exception(s, ...) _alloc_str_exception(s, KFUNC(__VA_ARGS__))

enum : int64_t
{
    EXCEPTION_OK,
    EXCEPTION_IN_HANDLER,
    EXCEPTION_NOT_HANDLED
};

exception_stack_t *exception_push_frame(jmp_buf *);
void               exception_free_frame(exception_frame_t *,
					exception_stack_t *);
void               exception_uncaught(exception_t *);
void               exception_raise(exception_t *, char *,
				   int) __attribute((__noreturn__));

static inline utf8_t *
exception_get_file(exception_t *exception)
{
    return con4m_new(T_UTF8, "cstring", exception->file);
}

static inline uint64_t
exception_get_line(exception_t *exception)
{
    return exception->line;
}

static inline utf8_t *
exception_get_message(exception_t *exception)
{
    return exception->msg;
}

void exception_register_uncaught_handler(void (*)(exception_t *));

#define RAISE_SYS()                                                            \
    {                                                                          \
        char buf[BUFSIZ];                                                      \
        strerror_r(errno, buf, BUFSIZ);                                        \
        RAISE(con4m_new(T_UTF8, "cstring", buf), "error_code", errno);	       \
    }

extern __thread exception_stack_t __exception_stack;

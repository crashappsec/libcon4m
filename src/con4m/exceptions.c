#include "con4m.h"

static void (*uncaught_handler)(c4m_exception_t *) = c4m_exception_uncaught;

__thread c4m_exception_stack_t __exception_stack = {
    0,
};
static pthread_once_t exceptions_inited = PTHREAD_ONCE_INIT;

// Skip the 4 GC header words, then the first 3 words are heap pointers.
const uint64_t c4m_exception_pmap[2] = {1, 0x0e00000000000000};

static void
exception_init(c4m_exception_t *exception, va_list args)
{
    c4m_utf8_t *message    = NULL;
    c4m_obj_t   context    = NULL;
    int64_t     error_code = -1;

    c4m_karg_va_init(args);
    c4m_kw_ptr("message", message);
    c4m_kw_ptr("context", context);
    c4m_kw_int64("error_code", error_code);

    exception->msg     = message;
    exception->context = context;
    exception->code    = error_code;
}

c4m_exception_t *
_c4m_alloc_exception(const char *msg, ...)
{
    c4m_exception_t *ret = c4m_gc_alloc(c4m_exception_t);
    ret->msg             = c4m_new(c4m_tspec_utf8(),
                       c4m_kw("cstring", c4m_ka(msg)));

    return ret;
}

c4m_exception_t *
_c4m_alloc_str_exception(c4m_utf8_t *msg, ...)
{
    c4m_exception_t *ret = c4m_gc_alloc(c4m_exception_t);
    ret->msg             = msg;

    return ret;
}

void
c4m_exception_register_uncaught_handler(void (*handler)(c4m_exception_t *))
{
    uncaught_handler = handler;
}

static void
c4m_exception_thread_start(void)
{
    c4m_gc_register_root(&__exception_stack, sizeof(__exception_stack) / 8);
}

c4m_exception_stack_t *
c4m_exception_push_frame(jmp_buf *jbuf)
{
    c4m_exception_frame_t *frame;

    pthread_once(&exceptions_inited, c4m_exception_thread_start);

    if (__exception_stack.free_frames) {
        frame                         = __exception_stack.free_frames;
        __exception_stack.free_frames = frame->next;
    }
    else {
        frame = c4m_gc_alloc(c4m_exception_frame_t);
    }
    frame->buf            = jbuf;
    frame->next           = __exception_stack.top;
    __exception_stack.top = frame;

    return &__exception_stack;
}

void
c4m_exception_free_frame(c4m_exception_frame_t *frame,
                         c4m_exception_stack_t *stack)
{
    if (frame == stack->top) {
        stack->top = NULL;
    }
    memset(frame,
           0,
           sizeof(c4m_exception_frame_t) - sizeof(c4m_exception_frame_t *));
    frame->next        = stack->free_frames;
    stack->free_frames = frame;
}

c4m_utf8_t *
c4m_repr_exception_stack_no_vm(c4m_utf8_t *title)
{
    c4m_exception_frame_t *frame     = __exception_stack.top;
    c4m_exception_t       *exception = frame->exception;

    c4m_utf8_t *result;

    if (title == NULL) {
        title = c4m_new_utf8("");
    }

    result = c4m_cstr_format("[red]{}[/] {}\n", title, exception->msg);

    while (frame != NULL) {
        exception        = frame->exception;
        c4m_utf8_t *frep = c4m_cstr_format("[i]Raised from:[/] [em]{}:{}[/]\n",
                                           c4m_new_utf8(exception->file),
                                           c4m_box_u64(exception->line));
        result           = c4m_str_concat(result, frep);
        frame            = frame->next;
    }

    return result;
}

void
c4m_exception_uncaught(c4m_exception_t *exception)
{
    c4m_print(c4m_repr_exception_stack_no_vm(c4m_new_utf8("FATAL ERROR:")));
}

void
c4m_exception_raise(c4m_exception_t *exception, char *filename, int line)
{
    c4m_exception_frame_t *frame = __exception_stack.top;

    frame->exception = exception;
    exception->file  = filename;
    exception->line  = line;

    if (!frame) {
        (*uncaught_handler)(exception);
        abort();
    }

    longjmp(*(frame->buf), 1);
}

const c4m_vtable_t c4m_exception_vtable = {
    .num_entries = 1,
    .methods     = {
        (c4m_vtable_entry)exception_init,
    },
};

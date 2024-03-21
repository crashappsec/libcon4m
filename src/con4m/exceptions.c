#include <con4m.h>

static void (*uncaught_handler)(exception_t *) = exception_uncaught;

__thread exception_stack_t __exception_stack = {0,};
static pthread_once_t exceptions_inited = PTHREAD_ONCE_INIT;

// Skip the 4 GC header words, then the first 3 words are heap pointers.
const uint64_t exception_pmap[2] = {1, 0x0e00000000000000};

static void
exception_init(exception_t *exception, va_list args)
{
    DECLARE_KARGS(
	utf8_t  *message    = NULL;
	object_t context    = NULL;
	int64_t  error_code = -1;
	);

    method_kargs(args, message, context, error_code);

    if (message == NULL) {
    }

    exception->msg     = message;
    exception->context = context;
    exception->code    = error_code;
}

exception_t *
_alloc_exception(char *msg, ...)
{
    exception_t *ret = gc_alloc(sizeof(exception_t));
    ret->msg = con4m_new(tspec_utf8(), "cstring", msg);

    return ret;
}

exception_t *
_alloc_str_exception(utf8_t *msg, ...)
{
    exception_t *ret = gc_alloc(sizeof(exception_t));
    ret->msg = msg;

    return ret;
}

void
exception_register_uncaught_handler(void (*handler)(exception_t *))
{
    uncaught_handler = handler;
}

static void
exception_thread_start(void)
{
    con4m_gc_register_root(&__exception_stack, sizeof(__exception_stack)/8);
}

exception_stack_t *
exception_push_frame(jmp_buf *jbuf)
{
    exception_frame_t *frame;

    pthread_once(&exceptions_inited, exception_thread_start);

    if (__exception_stack.free_frames) {
        frame                         = __exception_stack.free_frames;
        __exception_stack.free_frames = frame->next;
    }
    else {
        frame = gc_alloc(exception_frame_t);
    }
    frame->buf             = jbuf;
    frame->next            = __exception_stack.top;
    __exception_stack.top = frame;

    return &__exception_stack;
}

void
exception_free_frame(exception_frame_t *frame, exception_stack_t *stack)
{
    if (frame == stack->top) {
        stack->top = NULL;
    }
    memset(frame, 0, sizeof(exception_frame_t) - sizeof(exception_frame_t *));
    frame->next        = stack->free_frames;
    stack->free_frames = frame;
}

void
exception_uncaught(exception_t *exception)
{
    // Basic for now.
    fprintf(stderr, "%s:%lld: Uncaught exception: ", exception->file,
	    exception->line);


    ansi_render(exception->msg, stderr);
    fputc('\n', stderr);
}

void
exception_raise(exception_t *exception, char *filename, int line)
{
    exception_frame_t *frame = __exception_stack.top;

    frame->exception = exception;
    exception->file  = filename;
    exception->line  = line;

    if (!frame) {
        (*uncaught_handler)(exception);
        abort();
    }


    longjmp(*(frame->buf), 1);
}

const con4m_vtable exception_vtable = {
    .num_entries = 1,
    .methods     = {
	(con4m_vtable_entry)exception_init
    }
};

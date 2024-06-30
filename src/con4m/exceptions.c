#include "con4m.h"

static void (*uncaught_handler)(c4m_exception_t *) = c4m_exception_uncaught;

thread_local c4m_exception_stack_t __exception_stack = {
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
    ret->msg             = c4m_new(c4m_type_utf8(),
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

static void
c4m_default_uncaught_handler(c4m_exception_t *exception)
{
    c4m_list_t       *empty = c4m_list(c4m_type_utf8());
    c4m_renderable_t *hdr   = c4m_to_str_renderable(c4m_new_utf8("UNCAUGHT EXCEPTION"), "h1");
    c4m_renderable_t *row1  = c4m_to_str_renderable(exception->msg, NULL);
    c4m_list_t       *row2  = c4m_list(c4m_type_utf8());
    c4m_stream_t     *errf  = c4m_get_stderr();

    c4m_list_append(empty, c4m_new_utf8(""));
    c4m_list_append(empty, c4m_new_utf8(""));

    c4m_list_append(row2, c4m_rich_lit("[h2]Raised from:[/]"));
    c4m_list_append(row2,
                    c4m_cstr_format("{}:{}",
                                    c4m_new_utf8(exception->file),
                                    c4m_box_u64(exception->line)));

    c4m_grid_t *tbl = c4m_new(c4m_type_grid(),
                              c4m_kw("header_rows",
                                     c4m_ka(0),
                                     "start_rows",
                                     c4m_ka(4),
                                     "start_cols",
                                     c4m_ka(2),
                                     "container_tag",
                                     c4m_ka("flow"),
                                     "stripe",
                                     c4m_ka(true)));

    c4m_set_column_style(tbl, 0, "snap");
    // For the moment, we need to write an empty row then install the span over it.
    c4m_grid_add_row(tbl, empty);
    c4m_grid_add_col_span(tbl, hdr, 0, 0, 2);
    c4m_grid_add_row(tbl, empty);
    c4m_grid_add_row(tbl, empty);
    c4m_grid_add_col_span(tbl, row1, 1, 0, 2);
    c4m_grid_add_row(tbl, row2);
    c4m_stream_write_object(errf, tbl, true);
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
    uncaught_handler = c4m_default_uncaught_handler;
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
        frame = calloc(1, sizeof(c4m_exception_frame_t));
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

    exception->file = filename;
    exception->line = line;

    if (!frame) {
        (*uncaught_handler)(exception);
        abort();
    }

    frame->exception = exception;

    longjmp(*(frame->buf), 1);
}

const c4m_vtable_t c4m_exception_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)exception_init,
        // Explicit because some compilers don't seem to always properly
        // zero it (Was sometimes crashing on a `c4m_stream_t` on my mac).
        [C4M_BI_FINALIZER]   = NULL,

        NULL,
    },
};

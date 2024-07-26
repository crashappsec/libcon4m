#include "con4m.h"

#if defined(C4M_DEBUG) && !defined(C4M_BACKTRACE_SUPPORTED)
#warning "Cannot add debug stack traces to exceptions: libbacktrace not supported"
#endif

#ifdef C4M_BACKTRACE_SUPPORTED

static void
c4m_bt_err(void *data, const char *msg, int errnum)
{
    fprintf(stderr, "ERROR: %s (%d)", msg, errnum);
}

static thread_local c4m_grid_t *c4m_trace_grid;

struct backtrace_state *btstate;

static int
c4m_bt_create_backtrace(void       *data,
                        uintptr_t   pc,
                        const char *pathname,
                        int         line_number,
                        const char *function)
{
    c4m_utf8_t *file;
    c4m_utf8_t *fn;

    if (pathname == NULL) {
        file = c4m_rich_lit("[em]??[/] (unavailable)");
    }
    else {
        char *filename = rindex(pathname, '/');
        if (filename) {
            filename++;
        }
        else {
            filename = (char *)pathname;
        }

        file = c4m_cstr_format("{}:{}",
                               c4m_new_utf8(filename),
                               c4m_box_u64(line_number));
    }

    if (function == NULL) {
        fn = c4m_rich_lit("[em]??[/] (unavailable)");
    }
    else {
        fn = c4m_new_utf8(function);
    }

    c4m_list_t *x = c4m_list(c4m_type_utf8());

    c4m_list_append(x, c4m_cstr_format("{:18x}", c4m_box_u64(pc)));
    c4m_list_append(x, file);
    c4m_list_append(x, fn);
    c4m_grid_add_row(c4m_trace_grid, x);

    return 0;
};

static int
c4m_bt_static_backtrace(void       *data,
                        uintptr_t   pc,
                        const char *pathname,
                        int         n,
                        const char *function)
{
    static const char *unavailable = "?? (unavailable)";

    char *path;
    char *fn = (char *)function;
    if (pathname == NULL) {
        path = (char *)unavailable;
    }
    else {
        path = rindex(pathname, '/');
        if (path) {
            path++;
        }
        else {
            path = (char *)pathname;
        }
    }

    if (fn == NULL) {
        fn = (char *)unavailable;
    }

    fprintf(stderr, "pc: %p: file: %s:%d; func: %s\n", (void *)pc, path, n, fn);

    return 0;
}

#define backtrace_core(nframes)                       \
    c4m_trace_grid = c4m_new(c4m_type_grid(),         \
                             c4m_kw("start_cols",     \
                                    c4m_ka(3),        \
                                    "start_rows",     \
                                    c4m_ka(2),        \
                                    "header_rows",    \
                                    c4m_ka(1),        \
                                    "container_tag",  \
                                    c4m_ka("table2"), \
                                    "stripe",         \
                                    c4m_ka(true)));   \
                                                      \
    c4m_list_t *x = c4m_list(c4m_type_utf8());        \
    c4m_list_append(x, c4m_new_utf8("PC"));           \
    c4m_list_append(x, c4m_new_utf8("Location"));     \
    c4m_list_append(x, c4m_new_utf8("Function"));     \
                                                      \
    c4m_grid_add_row(c4m_trace_grid, x);              \
                                                      \
    c4m_snap_column(c4m_trace_grid, 0);               \
    c4m_snap_column(c4m_trace_grid, 1);               \
    c4m_snap_column(c4m_trace_grid, 2);               \
                                                      \
    backtrace_full(btstate, nframes, c4m_bt_create_backtrace, c4m_bt_err, NULL);

void
c4m_print_c_backtrace()
{
    backtrace_core(1);
    c4m_printf("[h6]C Stack Trace:");
    c4m_print(c4m_trace_grid);
}

void
c4m_backtrace_init(char *fname)
{
    btstate = backtrace_create_state(fname, 1, c4m_bt_err, NULL);
}

c4m_grid_t *
c4m_get_c_backtrace(int skips)
{
    backtrace_core(skips + 1);
    return c4m_trace_grid;
}

void
c4m_static_c_backtrace()
{
    backtrace_full(btstate, 0, c4m_bt_static_backtrace, c4m_bt_err, NULL);
}

#else
c4m_grid_t *
c4m_get_c_backtrace(int skips)
{
    return c4m_callout(c4m_new_utf8("Stack traces not enabled."));
}

void
c4m_static_c_backtrace()
{
    printf("Stack traces not enabled.\n");
}

#endif

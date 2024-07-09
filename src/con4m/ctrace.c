#include "con4m.h"

#ifdef BACKTRACE_SUPPORTED

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

    c4m_list_append(x, c4m_cstr_format("{:x}", c4m_box_u64(pc)));
    c4m_list_append(x, file);
    c4m_list_append(x, fn);
    c4m_grid_add_row(c4m_trace_grid, x);

    return 0;
};

#define backtrace_core()                             \
    c4m_trace_grid = c4m_new(c4m_type_grid(),        \
                             c4m_kw("start_cols",    \
                                    c4m_ka(3),       \
                                    "start_rows",    \
                                    c4m_ka(2),       \
                                    "header_rows",   \
                                    c4m_ka(1),       \
                                    "container_tag", \
                                    c4m_ka("table"), \
                                    "striped",       \
                                    c4m_ka(true)));  \
                                                     \
    c4m_list_t *x = c4m_list(c4m_type_utf8());       \
    c4m_list_append(x, c4m_new_utf8("PC"));          \
    c4m_list_append(x, c4m_new_utf8("Location"));    \
    c4m_list_append(x, c4m_new_utf8("Function"));    \
                                                     \
    c4m_grid_add_row(c4m_trace_grid, x);             \
                                                     \
    c4m_snap_column(c4m_trace_grid, 0);              \
    c4m_snap_column(c4m_trace_grid, 1);              \
    c4m_snap_column(c4m_trace_grid, 2);              \
                                                     \
    backtrace_full(btstate, 0, c4m_bt_create_backtrace, c4m_bt_err, NULL);

void
c4m_print_c_backtrace()
{
    backtrace_core();
    c4m_printf("[h6]C Stack Trace:");
    c4m_print(c4m_trace_grid);
}

static void
c4m_crash_handler(int n)
{
    c4m_print(c4m_callout(c4m_new_utf8("PROGRAM CRASHED.")));
    backtrace_core();
    c4m_printf("[h6]C Stack Trace:");
    c4m_print(c4m_trace_grid);
    _exit(-4);
}

void
c4m_backtrace_init(char *fname)
{
    signal(SIGSEGV, c4m_crash_handler);
    btstate = backtrace_create_state(fname, 1, c4m_bt_err, NULL);
}

c4m_grid_t *
c4m_get_c_backtrace()
{
    backtrace_core();
    return c4m_trace_grid;
}
#else
c4m_grid_t *
c4m_get_c_backtrace()
{
    return c4m_callout(c4m_new_utf8("Stack traces not enabled."));
}
#endif

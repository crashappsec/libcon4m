#include "con4m.h"

extern c4m_vmthread_t *c4m_thread_runtime_acquire();
extern c4m_grid_t     *c4m_get_backtrace(c4m_vmthread_t *);
extern int16_t        *c4m_calculate_col_widths(c4m_grid_t *, int16_t, int16_t *);

static void (*c4m_crash_callback)()   = NULL;
static bool c4m_show_c_trace_on_crash = true;

void
c4m_set_crash_callback(void (*cb)())
{
    c4m_crash_callback = cb;
}

void
c4m_set_show_c_trace_on_crash(bool n)
{
    c4m_show_c_trace_on_crash = n;
}

static void
crash_print_trace(c4m_stream_t *f, char *title, c4m_grid_t *g)
{
    c4m_utf8_t *t = c4m_rich_lit(title);
    c4m_print(c4m_kw("stream", c4m_ka(f), "sep", c4m_ka('\n')), 2, t, g);
}

static void
c4m_crash_handler(int n)
{
    c4m_vmthread_t *runtime   = c4m_thread_runtime_acquire();
    c4m_grid_t     *c4m_trace = NULL;
    c4m_grid_t     *ct        = NULL;
    c4m_stream_t   *f         = c4m_get_stderr();

    if (runtime && runtime->running) {
        c4m_trace = c4m_get_backtrace(runtime);
    }

    c4m_utf8_t *s = c4m_rich_lit("[h5]Program crashed due to SIGSEGV.");
    c4m_print(c4m_kw("stream", c4m_ka(f)), 1, s);

#if defined(C4M_BACKTRACE_SUPPORTED)
    if (c4m_show_c_trace_on_crash) {
        ct = c4m_get_c_backtrace(1);

        if (runtime && runtime->running) {
            int16_t  tmp;
            int16_t *widths1 = c4m_calculate_col_widths(ct,
                                                        C4M_GRID_TERMINAL_DIM,
                                                        &tmp);
            int16_t *widths2 = c4m_calculate_col_widths(c4m_trace,
                                                        C4M_GRID_TERMINAL_DIM,
                                                        &tmp);

            for (int i = 0; i < 3; i++) {
                int w = c4m_max(widths1[i], widths2[i]);

                c4m_render_style_t *s = c4m_new(c4m_type_render_style(),
                                                c4m_kw("min_size",
                                                       c4m_ka(w),
                                                       "max_size",
                                                       c4m_ka(w),
                                                       "left_pad",
                                                       c4m_ka(1),
                                                       "right_pad",
                                                       c4m_ka(1)));
                c4m_set_column_props(ct, i, s);
                c4m_set_column_props(c4m_trace, i, s);
            }
        }
    }

#endif
    if (runtime && runtime->running) {
        crash_print_trace(f, "[h6]Con4m stack trace:", c4m_trace);
    }

#if defined(C4M_BACKTRACE_SUPPORTED)
    if (c4m_show_c_trace_on_crash) {
        crash_print_trace(f, "[h6]C stack trace:", ct);
    }
#endif

    if (c4m_crash_callback) {
        (*c4m_crash_callback)();
    }

    _exit(139); // Standard for sigsegv.
}

void
c4m_crash_init()
{
    signal(SIGSEGV, c4m_crash_handler);
}

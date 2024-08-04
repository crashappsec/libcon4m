#include "con4m.h"

c4m_duration_t *
c4m_now(void)
{
    c4m_duration_t *result = c4m_new(c4m_type_duration());

    clock_gettime(CLOCK_REALTIME, result);

    return result;
}

c4m_duration_t *
c4m_timestamp(void)
{
    c4m_duration_t *result = c4m_new(c4m_type_duration());

    clock_gettime(CLOCK_MONOTONIC, result);

    return result;
}

c4m_duration_t *
c4m_process_cpu(void)
{
    c4m_duration_t *result = c4m_new(c4m_type_duration());

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, result);

    return result;
}

c4m_duration_t *
c4m_thread_cpu(void)
{
    c4m_duration_t *result = c4m_new(c4m_type_duration());

    clock_gettime(CLOCK_THREAD_CPUTIME_ID, result);

    return result;
}

#if defined(__MACH__)
c4m_duration_t *
c4m_uptime(void)
{
    c4m_duration_t *result = c4m_new(c4m_type_duration());

    clock_gettime(CLOCK_UPTIME_RAW, result);

    return result;
}
#elif defined(__linux__)
// In Posix, the MONOTONIC clock's reference frame is arbitrary.
// In Linux, it's uptime, and this is higher precisssion than using
// sysinfo(), which only has second resolution.
c4m_duration_t *
c4m_uptime(void)
{
    c4m_duration_t *result = c4m_new(c4m_type_duration());

    clock_gettime(CLOCK_MONOTONIC, result);

    return result;
}
#else
#error "Unsupported system."
#endif

static c4m_duration_t *monotonic_start;

void
c4m_init_program_timestamp(void)
{
    c4m_gc_register_root(&monotonic_start, 1);
    monotonic_start = c4m_timestamp();
}

c4m_duration_t *
c4m_program_clock(void)
{
    c4m_duration_t *now = c4m_timestamp();

    return c4m_duration_diff(now, monotonic_start);
}

static c4m_list_t *
duration_atomize(c4m_utf8_t *s)
{
    c4m_utf8_t *one;
    c4m_list_t *result = c4m_list(c4m_type_utf8());
    int         l      = c4m_str_byte_len(s);
    char       *p      = s->data;
    char       *end    = p + l;
    int         start  = 0;
    int         cur    = 0;

    while (p < end) {
        while (p < end && isdigit(*p)) {
            cur++;
            p++;
        }
        if (start == cur) {
            return NULL;
        }
        one = c4m_str_slice(s, start, cur);
        c4m_list_append(result, one);

        while (p < end && (isspace(*p) || *p == ',')) {
            cur++;
            p++;
        }

        start = cur;

        while (p < end && !isdigit(*p) && *p != ' ' && *p != ',') {
            cur++;
            p++;
        }
        if (start == cur) {
            return NULL;
        }

        one = c4m_str_slice(s, start, cur);
        c4m_list_append(result, one);

        while (p < end && (isspace(*p) || *p == ',')) {
            cur++;
            p++;
        }
        start = cur;
    }

    return result;
}

#define SEC_PER_MIN  60
#define SEC_PER_HR   (SEC_PER_MIN * 60)
#define SEC_PER_DAY  (SEC_PER_HR * 24)
#define SEC_PER_WEEK (SEC_PER_DAY * 7)
#define SEC_PER_YEAR (SEC_PER_DAY * 365)
#define NS_PER_MS    1000000
#define NS_PER_US    1000
#define C4M_MAX_UINT (~0ULL)

static inline int64_t
tv_sec_multiple(c4m_utf8_t *s)
{
    // clang-format off
    switch (s->data[0]) {
    case 's':
	if (!strcmp(s->data, "s") ||
	    !strcmp(s->data, "sec") ||
	    !strcmp(s->data, "secs") ||
	    !strcmp(s->data, "seconds")) {
	    return 1;
	}
	return 0;
    case 'm':
	if (!strcmp(s->data, "m") ||
	    !strcmp(s->data, "min") ||
	    !strcmp(s->data, "mins") ||
	    !strcmp(s->data, "minutes")) {
	    return SEC_PER_MIN;
	}
	return 0;
    case 'h':
	if (!strcmp(s->data, "h") ||
	    !strcmp(s->data, "hr") ||
	    !strcmp(s->data, "hrs") ||
	    !strcmp(s->data, "hours")) {
	    return SEC_PER_HR;
	}
	return 0;
    case 'd':
	if (!strcmp(s->data, "d") ||
	    !strcmp(s->data, "day") ||
	    !strcmp(s->data, "days")) {
	    return SEC_PER_DAY;
	}
	return 0;
    case 'w':
	if (!strcmp(s->data, "w") ||
	    !strcmp(s->data, "wk") ||
	    !strcmp(s->data, "wks") ||
	    !strcmp(s->data, "week") ||
	    !strcmp(s->data, "weeks")) {
	    return SEC_PER_WEEK;
	}
	return 0;
    case 'y':
	if (!strcmp(s->data, "y") ||
	    !strcmp(s->data, "yr") ||
	    !strcmp(s->data, "yrs") ||
	    !strcmp(s->data, "year") ||
	    !strcmp(s->data, "years")) {
	    return SEC_PER_YEAR;
	}
	return 0;
    default:
	return 0;
    }
}

static inline int64_t
tv_nano_multiple(c4m_utf8_t *s)
{
    // For sub-seconds, we convert to nanoseconds.
    switch (s->data[0]) {
    case 'n':
	if (!strcmp(s->data, "n") ||
	    !strcmp(s->data, "ns") ||
	    !strcmp(s->data, "nsec") ||
	    !strcmp(s->data, "nsecs") ||
	    !strcmp(s->data, "nanosec") ||
	    !strcmp(s->data, "nanosecs") ||
	    !strcmp(s->data, "nanosecond") ||
	    !strcmp(s->data, "nanoseconds")) {
	    return 1;
	}
	return 0;
    case 'm':
	if (!strcmp(s->data, "m") ||
	    !strcmp(s->data, "ms") ||
	    !strcmp(s->data, "msec") ||
	    !strcmp(s->data, "msecs") ||
	    !strcmp(s->data, "millisec") ||
	    !strcmp(s->data, "millisecs") ||
	    !strcmp(s->data, "millisecond") ||
	    !strcmp(s->data, "milliseconds")) {
	    return NS_PER_MS;
	}
	if (!strcmp(s->data, "microsec") ||
	    !strcmp(s->data, "microsecs") ||
	    !strcmp(s->data, "microsecond") ||
	    !strcmp(s->data, "microseconds")) {
	    return NS_PER_US;
	}
	return 0;
    case 'u':
	if (!strcmp(s->data, "u") ||
	    !strcmp(s->data, "us") ||
	    !strcmp(s->data, "usec") ||
	    !strcmp(s->data, "usecs") ||
	    !strcmp(s->data, "usecond") ||
	    strcmp(s->data, "useconds")) {
	    return NS_PER_US;
	}
	return 0;
    default:
	return 0;
    }
    // clang-format on
}

static bool
str_to_duration(c4m_utf8_t          *s,
                struct timespec     *ts,
                c4m_compile_error_t *err)
{
    c4m_list_t *atoms = duration_atomize(s);

    if (!atoms) {
        *err = c4m_err_invalid_duration_lit;
        return false;
    }

    int         i   = 0;
    int         n   = c4m_list_len(atoms);
    __uint128_t sec = 0;
    __uint128_t sub = 0;
    __uint128_t tmp;
    int64_t     multiple;
    bool        neg;
    c4m_utf8_t *tmpstr;

    while (i < n) {
        tmp = c4m_raw_int_parse(c4m_list_get(atoms, i++, NULL), err, &neg);

        if (neg) {
            *err = c4m_err_invalid_duration_lit;
            return false;
        }

        if (*err != c4m_err_no_error) {
            *err = c4m_err_invalid_duration_lit;
            return false;
        }

        if (tmp > C4M_MAX_UINT) {
            *err = c4m_err_parse_lit_overflow;
            return false;
        }

        tmpstr   = c4m_list_get(atoms, i++, NULL);
        multiple = tv_sec_multiple(tmpstr);

        if (multiple) {
            sec += (multiple * tmp);
            if (sec > C4M_MAX_UINT) {
                *err = c4m_err_parse_lit_overflow;
                return false;
            }
            continue;
        }

        multiple = tv_nano_multiple(tmpstr);
        if (!multiple) {
            *err = c4m_err_invalid_duration_lit;
            return false;
        }

        sub += (multiple * tmp);
        if (sub > C4M_MAX_UINT) {
            *err = c4m_err_parse_lit_overflow;
            return false;
        }
    }

    ts->tv_sec  = sec;
    ts->tv_nsec = sub;

    return true;
}

static void
duration_init(struct timespec *ts, va_list args)
{
    c4m_utf8_t         *to_parse = NULL;
    int64_t             sec      = -1;
    int64_t             nanosec  = -1;
    c4m_compile_error_t err;

    c4m_karg_va_init(args);
    c4m_kw_ptr("to_parse", args);
    c4m_kw_uint64("sec", args);
    c4m_kw_uint64("nanosec", args);

    if (to_parse) {
        if (!str_to_duration(to_parse, ts, &err)) {
            C4M_RAISE(c4m_err_code_to_str(err));
        }
        return;
    }

    if (sec < 0) {
        sec = 0;
    }
    if (nanosec < 0) {
        nanosec = 0;
    }

    ts->tv_sec  = sec;
    ts->tv_nsec = nanosec;
    return;
}

static c4m_utf8_t *
repr_sec(int64_t n)
{
    c4m_list_t *l = c4m_list(c4m_type_utf8());
    c4m_utf8_t *s;
    int64_t     tmp;

    if (n >= SEC_PER_YEAR) {
        tmp = n / SEC_PER_YEAR;
        if (tmp > 1) {
            s = c4m_cstr_format("{} years", c4m_box_u64(tmp));
        }
        else {
            s = c4m_new_utf8("1 year");
        }

        c4m_list_append(l, s);
        n -= (tmp * SEC_PER_YEAR);
    }

    if (n >= SEC_PER_WEEK) {
        tmp = n / SEC_PER_WEEK;
        if (tmp > 1) {
            s = c4m_cstr_format("{} weeks", c4m_box_u64(tmp));
        }
        else {
            s = c4m_new_utf8("1 week");
        }

        c4m_list_append(l, s);
        n -= (tmp * SEC_PER_WEEK);
    }

    if (n >= SEC_PER_DAY) {
        tmp = n / SEC_PER_DAY;
        if (tmp > 1) {
            s = c4m_cstr_format("{} days", c4m_box_u64(tmp));
        }
        else {
            s = c4m_new_utf8("1 day");
        }

        c4m_list_append(l, s);
        n -= (tmp * SEC_PER_DAY);
    }

    if (n >= SEC_PER_HR) {
        tmp = n / SEC_PER_HR;
        if (tmp > 1) {
            s = c4m_cstr_format("{} hours", c4m_box_u64(tmp));
        }
        else {
            s = c4m_new_utf8("1 hour");
        }

        c4m_list_append(l, s);
        n -= (tmp * SEC_PER_HR);
    }

    if (n >= SEC_PER_MIN) {
        tmp = n / SEC_PER_MIN;
        if (tmp > 1) {
            s = c4m_cstr_format("{} minutes", c4m_box_u64(tmp));
        }
        else {
            s = c4m_new_utf8("1 minute");
        }

        c4m_list_append(l, s);
        n -= (tmp * SEC_PER_MIN);
    }

    if (n) {
        if (n == 1) {
            s = c4m_new_utf8("1 second");
        }
        else {
            s = c4m_cstr_format("{} seconds", c4m_box_u64(n));
        }
        c4m_list_append(l, s);
    }

    return c4m_to_utf8(c4m_str_join(l, c4m_new_utf8(", ")));
}

static c4m_utf8_t *
repr_ns(int64_t n)
{
    int ms = n / NS_PER_MS;
    int ns = n - (ms * NS_PER_MS);
    int us = ns / NS_PER_US;

    ns = ns - (us * NS_PER_US);

    c4m_list_t *parts = c4m_list(c4m_type_utf8());

    if (ms) {
        c4m_list_append(parts, c4m_cstr_format("{} msec", c4m_box_u64(ms)));
    }
    if (us) {
        c4m_list_append(parts, c4m_cstr_format("{} usec", c4m_box_u64(us)));
    }
    if (ns) {
        c4m_list_append(parts, c4m_cstr_format("{} nsec", c4m_box_u64(ns)));
    }

    return c4m_str_join(parts, c4m_new_utf8(", "));
}

static c4m_utf8_t *
duration_repr(c4m_duration_t *ts)
{
    // TODO: Do better.

    if (!ts->tv_sec && !ts->tv_nsec) {
        return c4m_new_utf8("0 seconds");
    }

    if (ts->tv_sec && ts->tv_nsec) {
        return c4m_cstr_format("{} {}",
                               repr_sec(ts->tv_sec),
                               repr_ns(ts->tv_nsec));
    }

    if (ts->tv_sec) {
        return repr_sec(ts->tv_sec);
    }
    return repr_ns(ts->tv_nsec);
}

static bool
duration_eq(c4m_duration_t *t1, c4m_duration_t *t2)
{
    return (t1->tv_sec == t2->tv_sec && t1->tv_nsec == t2->tv_nsec);
}

static bool
duration_gt(c4m_duration_t *t1, c4m_duration_t *t2)
{
    if (t1->tv_sec > t2->tv_sec) {
        return true;
    }
    if (t1->tv_sec < t2->tv_sec) {
        return false;
    }

    return t1->tv_nsec > t2->tv_nsec;
}

static bool
duration_lt(c4m_duration_t *t1, c4m_duration_t *t2)
{
    if (t1->tv_sec < t2->tv_sec) {
        return true;
    }
    if (t1->tv_sec > t2->tv_sec) {
        return false;
    }

    return t1->tv_nsec < t2->tv_nsec;
}

c4m_duration_t *
c4m_duration_diff(c4m_duration_t *t1, c4m_duration_t *t2)
{
    c4m_duration_t *result = c4m_new(c4m_type_duration());
    c4m_duration_t *b, *l;

    if (duration_gt(t1, t2)) {
        b = t1;
        l = t2;
    }
    else {
        b = t2;
        l = t1;
    }
    result->tv_nsec = b->tv_nsec - l->tv_nsec;
    result->tv_sec  = b->tv_sec - l->tv_sec;

    if (result->tv_nsec < 0) {
        result->tv_nsec += 1000000000;
        result->tv_sec -= 1;
    }

    return result;
}

static bool
duration_add(c4m_duration_t *t1, c4m_duration_t *t2)
{
    c4m_duration_t *result = c4m_new(c4m_type_duration());

    result->tv_nsec = t1->tv_nsec + t2->tv_nsec;
    result->tv_sec  = t1->tv_sec + t2->tv_sec;

    if (result->tv_nsec >= 1000000000) {
        result->tv_sec += 1;
        result->tv_nsec -= 1000000000;
    }

    return result;
}

static c4m_duration_t *
duration_lit(c4m_utf8_t          *s,
             c4m_lit_syntax_t     st,
             c4m_utf8_t          *mod,
             c4m_compile_error_t *err)
{
    c4m_duration_t *result = c4m_new(c4m_type_duration());

    if (str_to_duration(s, result, err)) {
        return result;
    }

    return NULL;
}

const c4m_vtable_t c4m_duration_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR]  = (c4m_vtable_entry)duration_init,
        [C4M_BI_REPR]         = (c4m_vtable_entry)duration_repr,
        [C4M_BI_FROM_LITERAL] = (c4m_vtable_entry)duration_lit,
        [C4M_BI_EQ]           = (c4m_vtable_entry)duration_eq,
        [C4M_BI_LT]           = (c4m_vtable_entry)duration_lt,
        [C4M_BI_GT]           = (c4m_vtable_entry)duration_gt,
        [C4M_BI_GC_MAP]       = (c4m_vtable_entry)c4m_header_gc_bits,
        [C4M_BI_ADD]          = (c4m_vtable_entry)duration_add,
        [C4M_BI_SUB]          = (c4m_vtable_entry)c4m_duration_diff,
        [C4M_BI_FINALIZER]    = NULL,
    },
};

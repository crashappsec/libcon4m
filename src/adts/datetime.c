#include "con4m.h"

static inline bool
us_written_date(c4m_utf8_t *input, c4m_date_time_t *result)
{
    c4m_utf8_t *month_part = c4m_new_utf8("");
    int         ix         = 0;
    int         l          = c4m_str_byte_len(input);
    int         day        = 0;
    int         year       = 0;
    int         daylen;
    int         yearlen;
    int         start_ix;
    char       *s;

    if (!input || c4m_str_byte_len(input) == 0) {
        return false;
    }

    s = input->data;

    while (ix < l) {
        char c = *s;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            ++ix;
            ++s;
        }
        else {
            break;
        }
    }

    if (ix == 0) {
        return false;
    }

    month_part = c4m_to_utf8(c4m_str_lower(c4m_str_slice(input, 0, ix)));

    while (ix < l) {
        if (*s == ' ') {
            ++ix;
            ++s;
        }
        else {
            break;
        }
    }

    start_ix = ix;

    while (ix < l) {
        switch (*s) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            day *= 10;
            day += *s - '0';
            ++ix;
            ++s;
            continue;
        case ' ':
        case ',':
            break;
        default:
            return false;
        }
        break;
    }

    daylen = ix - start_ix;

    if (ix < l && *s == ',') {
        s++;
        ix++;
    }

    while (ix < l) {
        if (*s != ' ') {
            break;
        }
        s++;
        ix++;
    }

    start_ix = ix;
    while (ix < l) {
        if (*s < '0' || *s > '9') {
            break;
        }
        year *= 10;
        year += *s - '0';
        ++ix;
        ++s;
    }

    if (year < 100) {
        year += 2000;
    }

    if (ix < l) {
        return false;
    }

    yearlen = ix - start_ix;

    if (daylen == 4 && yearlen == 0) {
        year = day;
        day  = 0;
    }

#define month_str_is(x) !strcmp(month_part->data, x)
    do {
        if (month_str_is("jan") || month_str_is("january")) {
            result->dt.tm_mon = 0;
            break;
        }
        if (month_str_is("feb") || month_str_is("february")) {
            result->dt.tm_mon = 1;
            break;
        }
        if (month_str_is("mar") || month_str_is("march")) {
            result->dt.tm_mon = 2;
            break;
        }
        if (month_str_is("apr") || month_str_is("april")) {
            result->dt.tm_mon = 3;
            break;
        }
        if (month_str_is("may")) {
            result->dt.tm_mon = 4;
            break;
        }
        if (month_str_is("jun") || month_str_is("june")) {
            result->dt.tm_mon = 5;
            break;
        }
        if (month_str_is("jul") || month_str_is("july")) {
            result->dt.tm_mon = 6;
            break;
        }
        if (month_str_is("aug") || month_str_is("august")) {
            result->dt.tm_mon = 7;
            break;
        }
        // clang-format off
        if (month_str_is("sep") || month_str_is("sept") ||
	    month_str_is("september")) {
            // clang-format on
            result->dt.tm_mon = 8;
            break;
        }
        if (month_str_is("oct") || month_str_is("october")) {
            result->dt.tm_mon = 9;
            break;
        }
        if (month_str_is("nov") || month_str_is("november")) {
            result->dt.tm_mon = 10;
            break;
        }
        if (month_str_is("dec") || month_str_is("december")) {
            result->dt.tm_mon = 11;
            break;
        }
        return false;
    } while (true);

    result->have_month = 1;

    if (day != 0) {
        if (day > 31) {
            return false;
        }
        result->have_day   = 1;
        result->dt.tm_mday = day;
    }
    if (year != 0) {
        result->have_year  = 1;
        result->dt.tm_year = year - 1900;
    }

    if (day > 31) {
        return false;
    }

    if (result->dt.tm_mon == 1 && day == 30) {
        return false;
    }

    if (day == 31) {
        switch (result->dt.tm_mon) {
        case 1:
        case 3:
        case 5:
        case 8:
        case 10:
            return false;
        default:
            break;
        }
    }

    return true;
}

static inline bool
other_written_date(c4m_utf8_t *input, c4m_date_time_t *result)
{
    int         l = c4m_str_byte_len(input);
    char       *s = input->data;
    char       *e = s + l;
    c4m_utf8_t *day;

    while (s < e) {
        char c = *s;

        if (c < '0' || c > '9') {
            break;
        }
        s++;
    }

    if (s == input->data) {
        return false;
    }

    day = c4m_to_utf8(c4m_str_slice(input, 0, s - input->data));

    while (s < e) {
        if (*s != ' ') {
            break;
        }
        s++;
    }

    char *month_part = s;

    while (s < e) {
        char c = *s;

        if (c < 'A' || c > 'z' || (c > 'Z' && c < 'a')) {
            break;
        }
        s++;
    }
    if (s == month_part) {
        return false;
    }

    int mstart = month_part - input->data;
    int mend   = s - input->data;

    c4m_utf8_t *mo           = c4m_to_utf8(c4m_str_slice(input, mstart, mend));
    c4m_utf8_t *year         = c4m_to_utf8(c4m_str_slice(input, mend, l));
    c4m_utf8_t *americanized = c4m_cstr_format("{} {}{}", mo, day, year);

    return us_written_date(americanized, result);
}

#define WAS_DIGIT(x)      \
    if (x < 0 || x > 9) { \
        return false;     \
    }

#define ONE_DIGIT(n)  \
    if (s == e) {     \
        return false; \
    }                 \
    n = *s++ - '0';   \
    WAS_DIGIT(n)

#define PARSE_MONTH()           \
    ONE_DIGIT(m);               \
    ONE_DIGIT(tmp);             \
    m *= 10;                    \
    m += tmp;                   \
                                \
    if (m > 12) {               \
        return false;           \
    }                           \
                                \
    result->dt.tm_mon  = m - 1; \
    result->have_month = true

#define PARSE_DAY()         \
    ONE_DIGIT(d);           \
    if (d > 3) {            \
        return false;       \
    }                       \
    d *= 10;                \
    ONE_DIGIT(tmp);         \
    d += tmp;               \
    switch (m) {            \
    case 2:                 \
        if (d > 29) {       \
            return false;   \
        }                   \
        break;              \
    case 4:                 \
    case 6:                 \
    case 9:                 \
    case 11:                \
        if (d > 30) {       \
            return false;   \
        }                   \
        break;              \
    default:                \
        if (d > 31) {       \
            return false;   \
        }                   \
        break;              \
    }                       \
                            \
    result->dt.tm_mday = d; \
    result->have_day   = true

#define PARSE_YEAR4()              \
    ONE_DIGIT(y);                  \
    y *= 10;                       \
    ONE_DIGIT(tmp);                \
    y += tmp;                      \
    y *= 10;                       \
    ONE_DIGIT(tmp);                \
    y += tmp;                      \
    y *= 10;                       \
    ONE_DIGIT(tmp);                \
    y += tmp;                      \
                                   \
    result->dt.tm_year = y - 1900; \
    result->have_year  = true

#define REQUIRE_DASH() \
    if (*s++ != '-') { \
        return false;  \
    }

static inline bool
iso_date(c4m_utf8_t *input, c4m_date_time_t *result)
{
    int   l             = c4m_str_byte_len(input);
    char *s             = input->data;
    char *e             = s + l;
    int   m             = 0;
    int   d             = 0;
    int   y             = 0;
    bool  elided_dashes = false;
    int   tmp;

    switch (l) {
    case 4:
        REQUIRE_DASH();
        REQUIRE_DASH();
        PARSE_MONTH();
        return true;

    case 7:
        REQUIRE_DASH();
        REQUIRE_DASH();
        PARSE_MONTH();
        REQUIRE_DASH();
        PARSE_DAY();
        return true;
    case 8:
        elided_dashes = true;
        // fallthrough
    case 10:
        PARSE_YEAR4();

        if (!elided_dashes) {
            REQUIRE_DASH();
        }
        PARSE_MONTH();
        if (!elided_dashes) {
            REQUIRE_DASH();
        }
        PARSE_DAY();
        return true;

    default:
        return false;
    }
}

static bool
to_native_date(c4m_utf8_t *i, c4m_date_time_t *r)
{
    if (iso_date(i, r) || other_written_date(i, r) || us_written_date(i, r)) {
        return true;
    }

    return false;
}

#define END_OR_TZ()   \
    if (s == e) {     \
        return true;  \
    }                 \
    switch (*s) {     \
    case 'Z':         \
    case 'z':         \
    case '+':         \
    case '-':         \
        break;        \
    default:          \
        return false; \
    }

static bool
to_native_time(c4m_utf8_t *input, c4m_date_time_t *result)
{
    int   hr  = 0;
    int   min = 0;
    int   sec = 0;
    int   l   = c4m_str_byte_len(input);
    char *s   = input->data;
    char *e   = s + l;
    char  tmp;

    ONE_DIGIT(hr);
    if (*s != ':') {
        hr *= 10;
        ONE_DIGIT(tmp);
        hr += tmp;
    }
    if (*s++ != ':') {
        return false;
    }
    if (hr > 23) {
        return false;
    }
    result->dt.tm_hour = hr;

    ONE_DIGIT(min);
    min *= 10;
    ONE_DIGIT(tmp);
    min += tmp;

    if (min > 59) {
        return false;
    }

    result->dt.tm_min = min;
    result->have_time = true;

    if (s == e) {
        return true;
    }
    if (*s == ':') {
        s++;
        ONE_DIGIT(sec);
        sec *= 10;
        ONE_DIGIT(tmp);
        sec += tmp;
        if (sec > 60) {
            return false;
        }
        result->have_sec  = true;
        result->dt.tm_sec = sec;

        if (s == e) {
            return true;
        }
        if (*s == '.') {
            result->fracsec       = 0;
            result->have_frac_sec = true;

            ONE_DIGIT(result->fracsec);
            while (*s >= '0' && *s <= '9') {
                result->fracsec *= 10;
                ONE_DIGIT(tmp);
                result->fracsec += tmp;
            }
        }
    }

    while (s < e && *s == ' ') {
        s++;
    }

    if (s == e) {
        return true;
    }

    switch (*s) {
    case 'a':
        if (*++s != 'm') {
            return false;
        }
        ++s;
        END_OR_TZ();
        break;
    case 'A':
        ++s;
        if (*s != 'm' && *s != 'M') {
            return false;
        }
        ++s;
        END_OR_TZ();
        break;
    case 'p':
        if (*++s != 'm') {
            return false;
        }
        ++s;
        if (result->dt.tm_hour <= 11) {
            result->dt.tm_hour += 12;
        }
        END_OR_TZ();
        break;
    case 'P':
        ++s;
        if (*s != 'm' && *s != 'M') {
            return false;
        }
        ++s;
        if (result->dt.tm_hour <= 11) {
            result->dt.tm_hour += 12;
        }
        END_OR_TZ();
        break;
    case 'Z':
    case 'z':
    case '+':
    case '-':
        break;
    default:
        return false;
    }

    result->have_offset = true;

    if (*s == 'Z' || *s == 'z') {
        s++;
    }

    if (s == e) {
        return true;
    }

    int mul    = 1;
    int offset = 0;

    if (*s == '-') {
        mul = -1;
        s++;
    }
    else {
        if (*s == '+') {
            s++;
        }
    }

    if (s == e) {
        return false;
    }

    ONE_DIGIT(offset);
    offset *= mul;

    if (s != e) {
        ONE_DIGIT(tmp);
        offset *= 10;
        offset += tmp;
    }

    // This range covers it;
    // the true *historic range is -15:56:00 - 15:13:42
    // and in practice should generally be -12 to +14 now.

    if (offset < -15 || offset > 15) {
        return false;
    }

    result->dt.tm_gmtoff = offset * 60 * 60;

    if (s == e) {
        return true;
    }

    if (*s == ':') {
        s++;
    }

    offset = 0;
    ONE_DIGIT(offset);
    ONE_DIGIT(tmp);
    offset *= 10;
    offset += tmp;
    if (offset > 59) {
        return false;
    }

    result->dt.tm_gmtoff += offset * 60;

    if (s == e) {
        return true;
    }

    if (*s++ != ':') {
        return false;
    }

    offset = 0;
    ONE_DIGIT(offset);
    ONE_DIGIT(tmp);
    offset *= 10;
    offset += tmp;
    if (offset > 60) {
        return false;
    }

    result->dt.tm_gmtoff += offset;

    return s == e;
}

static bool
to_native_date_and_or_time(c4m_utf8_t *input, c4m_date_time_t *result)
{
    int  ix           = c4m_str_find(input, c4m_new_utf8("T"));
    bool im_exhausted = false;

    if (ix == -1) {
        ix = c4m_str_find(input, c4m_new_utf8("t"));
    }

try_a_slice:
    if (ix != -1) {
        int         l    = c4m_str_codepoint_len(input);
        c4m_utf8_t *date = c4m_to_utf8(c4m_str_slice(input, 0, ix));
        c4m_utf8_t *time = c4m_to_utf8(c4m_str_slice(input, ix + 1, l));

        if (iso_date(date, result) && to_native_time(time, result)) {
            return true;
        }

        if (to_native_date(date, result) && to_native_time(time, result)) {
            return true;
        }

        if (im_exhausted) {
            // We've been up here twice, why loop forever when it isn't
            // going to work out?
            return false;
        }
    }

    // Otherwise, first look for the first colon after a space.
    ix = c4m_str_find(input, c4m_new_utf8(":"));

    if (ix != -1) {
        int              last_space = -1;
        c4m_utf32_t     *as_32      = c4m_to_utf32(input);
        c4m_codepoint_t *cptr       = (c4m_codepoint_t *)as_32->data;

        for (int i = 0; i < ix; i++) {
            if (*cptr++ == ' ') {
                last_space = i;
            }
        }

        if (last_space != -1) {
            ix           = last_space;
            im_exhausted = true;
            goto try_a_slice;
        }
    }

    if (to_native_date(input, result)) {
        return true;
    }

    memset(result, 0, sizeof(c4m_date_time_t));

    if (to_native_time(input, result)) {
        return true;
    }

    return false;
}

#define YEAR_NOT_SET 0x7fffffff

static void
datetime_init(c4m_date_time_t *self, va_list args)
{
    c4m_utf8_t *to_parse   = NULL;
    int32_t     hr         = -1;
    int32_t     min        = -1;
    int32_t     sec        = -1;
    int32_t     month      = -1;
    int32_t     day        = -1;
    int32_t     year       = YEAR_NOT_SET;
    int32_t     offset_hr  = -100;
    int32_t     offset_min = 0;
    int32_t     offset_sec = 0;
    int64_t     fracsec    = -1;

    c4m_karg_va_init(args);
    c4m_kw_ptr("to_parse", args);
    c4m_kw_int32("hr", hr);
    c4m_kw_int32("min", min);
    c4m_kw_int32("sec", sec);
    c4m_kw_int32("month", month);
    c4m_kw_int32("day", day);
    c4m_kw_int32("year", year);
    c4m_kw_int32("offset_hr", offset_hr);
    c4m_kw_int32("offset_min", offset_min);
    c4m_kw_int32("offset_sec", offset_sec);
    c4m_kw_int64("fracsec", fracsec);

    if (to_parse != NULL) {
        to_parse = c4m_to_utf8(to_parse);
        if (!to_native_date_and_or_time(to_parse, self)) {
            C4M_CRAISE("Invalid date-time literal.");
        }
        return;
    }

    self->dt.tm_isdst = -1;

    if (hr != -1) {
        if (hr < 0 || hr > 23) {
            C4M_CRAISE("Invalid hour (must be 0 - 23)");
        }
        self->dt.tm_hour = hr;
        self->have_time  = true;

        if (min != -1) {
            if (min < 0 || min > 59) {
                C4M_CRAISE("Invalid minute (must be 0 - 59)");
            }
            self->dt.tm_min = min;
        }

        if (sec != -1) {
            if (sec < 0 || sec > 61) {
                C4M_CRAISE("Invalid second (must be 0 - 60)");
            }
            self->dt.tm_sec = sec;
        }

        if (fracsec > 0) {
            self->fracsec = fracsec;
        }
    }

    if (year != YEAR_NOT_SET) {
        self->have_year  = true;
        self->dt.tm_year = year;
    }

    if (month != -1) {
        if (month < 1 || month > 12) {
            C4M_CRAISE("Invalid month (must be 1 - 12)");
        }
        self->dt.tm_mon  = month - 1;
        self->have_month = true;
    }

    if (day != -1) {
        if (day < 1 || day > 31) {
            C4M_CRAISE("Invalid day of month");
        }
        self->dt.tm_mday = day;
        self->have_day   = true;
    }

    int offset = 0;

    if (offset_hr >= -15 && offset_hr <= 15) {
        offset            = offset_hr * 60 * 60;
        self->have_offset = true;
    }

    if (offset_min > 0 && offset_min < 60) {
        offset += offset_min * 60;
        self->have_offset = true;
    }

    if (offset_sec > 0 && offset_sec <= 60) {
        offset += offset_min;
    }

    self->dt.tm_gmtoff = offset;
}

#define DT_HAVE_TIME 1
#define DT_HAVE_SEC  2
#define DT_HAVE_FRAC 4
#define DT_HAVE_MO   8
#define DT_HAVE_Y    16
#define DT_HAVE_D    32
#define DT_HAVE_OFF  64

static c4m_str_t *
datetime_repr(c4m_date_time_t *self)
{
    // TODO: this could use a lot more logic to make it more sane
    // when bits aren't fully filled out.
    //
    // Also, for now we are just omitting the fractional second.

    char *fmt = NULL;

    if (self->have_time) {
        if (self->have_day || self->have_month || self->have_year) {
            if (self->have_offset) {
                fmt = "%Y-%m-%dT%H:%M:%S%z";
            }
            else {
                fmt = "%Y-%m-%dT%H:%M:%S";
            }
        }
        else {
            fmt = "%H:%M:%S";
        }
    }
    else {
        fmt = "%Y-%m-%d";
    }

    char buf[1024];

    if (!strftime(buf, 1024, fmt, &self->dt)) {
        return c4m_new_utf8("<<invalid time>>");
    }

    return c4m_new_utf8(buf);
}

static c4m_date_time_t *
datetime_lit(c4m_utf8_t          *s,
             c4m_lit_syntax_t     st,
             c4m_utf8_t          *mod,
             c4m_compile_error_t *err)
{
    c4m_date_time_t *result = c4m_new(c4m_type_datetime());

    if (!to_native_date_and_or_time(s, result)) {
        *err = c4m_err_invalid_dt_spec;
        return NULL;
    }

    return result;
}

static bool
datetime_can_coerce_to(c4m_type_t *my_type, c4m_type_t *target_type)
{
    switch (target_type->details->base_type->typeid) {
    case C4M_T_DATETIME:
    case C4M_T_DATE:
    case C4M_T_TIME:
        return true;
    default:
        return false;
    }
}

static c4m_date_time_t *
datetime_coerce_to(c4m_date_time_t *dt)
{
    return dt;
}

static c4m_date_time_t *
datetime_copy(c4m_date_time_t *dt)
{
    c4m_date_time_t *result = c4m_new(c4m_get_my_type(dt));

    memcpy(result, dt, sizeof(c4m_date_time_t));

    return result;
}

static c4m_utf8_t *
datetime_format(c4m_date_time_t *dt, c4m_fmt_spec_t *spec)
{
    char fmt_str[3] = {'%', 'F', 0};
    char buf[1024];

    switch (spec->type) {
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'F':
    case 'G':
    case 'H':
    case 'I':
    case 'M':
    case 'R':
    case 'S':
    case 'T':
    case 'U':
    case 'V':
    case 'W':
    case 'X':
    case 'Y':
    case 'Z':
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'g':
    case 'j':
    case 'k':
    case 'l':
    case 'm':
    case 'n':
    case 'p':
    case 'r':
    case 's':
    case 't':
    case 'u':
    case 'v':
    case 'w':
    case 'x':
    case 'y':
    case 'z':
    case '+':
        fmt_str[1] = (char)spec->type;
        if (!strftime(buf, 1024, fmt_str, &dt->dt)) {
            C4M_CRAISE("Internal error (when calling strftime)");
        }

        return c4m_new_utf8(buf);

    default:
        C4M_CRAISE("Invalid format specifier for Datetime object");
    }
}

static c4m_utf8_t *
date_format(c4m_date_time_t *dt, c4m_fmt_spec_t *spec)
{
    char fmt_str[3] = {'%', 'F', 0};
    char buf[1024];

    switch (spec->type) {
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'F':
    case 'G':
    case 'U':
    case 'V':
    case 'W':
    case 'Y':
    case 'a':
    case 'b':
    case 'd':
    case 'e':
    case 'g':
    case 'j':
    case 'm':
    case 'n':
    case 't':
    case 'u':
    case 'v':
    case 'w':
    case 'x':
    case 'y':
        fmt_str[1] = (char)spec->type;
        if (!strftime(buf, 1024, fmt_str, &dt->dt)) {
            C4M_CRAISE("Internal error (when calling strftime)");
        }

        return c4m_new_utf8(buf);

    default:
        C4M_CRAISE("Invalid format specifier for Date object");
    }
}

static c4m_utf8_t *
time_format(c4m_date_time_t *dt, c4m_fmt_spec_t *spec)
{
    char fmt_str[3] = {'%', 'F', 0};
    char buf[1024];

    switch (spec->type) {
    case 'H':
    case 'I':
    case 'M':
    case 'R':
    case 'S':
    case 'T':
    case 'X':
    case 'Z':
    case 'k':
    case 'l':
    case 'n':
    case 'p':
    case 'r':
    case 's':
    case 't':
    case 'z':
        fmt_str[1] = (char)spec->type;
        if (!strftime(buf, 1024, fmt_str, &dt->dt)) {
            C4M_CRAISE("Internal error (when calling strftime)");
        }

        return c4m_new_utf8(buf);

    default:
        C4M_CRAISE("Invalid format specifier for Time object");
    }
}

static c4m_date_time_t *
date_lit(c4m_utf8_t          *s,
         c4m_lit_syntax_t     st,
         c4m_utf8_t          *mod,
         c4m_compile_error_t *err)
{
    c4m_date_time_t *result = c4m_new(c4m_type_date());

    if (!to_native_date(s, result)) {
        *err = c4m_err_invalid_date_spec;
        return NULL;
    }

    return result;
}

static c4m_date_time_t *
time_lit(c4m_utf8_t          *s,
         c4m_lit_syntax_t     st,
         c4m_utf8_t          *mod,
         c4m_compile_error_t *err)
{
    c4m_date_time_t *result = c4m_new(c4m_type_time());

    if (!to_native_time(s, result)) {
        *err = c4m_err_invalid_time_spec;
        return NULL;
    }

    return result;
}

const c4m_vtable_t c4m_datetime_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR]  = (c4m_vtable_entry)datetime_init,
        [C4M_BI_REPR]         = (c4m_vtable_entry)datetime_repr,
        [C4M_BI_FORMAT]       = (c4m_vtable_entry)datetime_format,
        [C4M_BI_COERCIBLE]    = (c4m_vtable_entry)datetime_can_coerce_to,
        [C4M_BI_COERCE]       = (c4m_vtable_entry)datetime_coerce_to,
        [C4M_BI_FROM_LITERAL] = (c4m_vtable_entry)datetime_lit,
        [C4M_BI_COPY]         = (c4m_vtable_entry)datetime_copy,
        [C4M_BI_GC_MAP]       = (c4m_vtable_entry)c4m_header_gc_bits,
        [C4M_BI_FINALIZER]    = NULL,
    },
};

const c4m_vtable_t c4m_date_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR]  = (c4m_vtable_entry)datetime_init,
        [C4M_BI_REPR]         = (c4m_vtable_entry)datetime_repr,
        [C4M_BI_FORMAT]       = (c4m_vtable_entry)date_format,
        [C4M_BI_COERCIBLE]    = (c4m_vtable_entry)datetime_can_coerce_to,
        [C4M_BI_COERCE]       = (c4m_vtable_entry)datetime_coerce_to,
        [C4M_BI_FROM_LITERAL] = (c4m_vtable_entry)date_lit,
        [C4M_BI_COPY]         = (c4m_vtable_entry)datetime_copy,
        [C4M_BI_GC_MAP]       = (c4m_vtable_entry)c4m_header_gc_bits,
        [C4M_BI_FINALIZER]    = NULL,
    },
};

const c4m_vtable_t c4m_time_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR]  = (c4m_vtable_entry)datetime_init,
        [C4M_BI_REPR]         = (c4m_vtable_entry)datetime_repr,
        [C4M_BI_FORMAT]       = (c4m_vtable_entry)time_format,
        [C4M_BI_COERCIBLE]    = (c4m_vtable_entry)datetime_can_coerce_to,
        [C4M_BI_COERCE]       = (c4m_vtable_entry)datetime_coerce_to,
        [C4M_BI_FROM_LITERAL] = (c4m_vtable_entry)time_lit,
        [C4M_BI_COPY]         = (c4m_vtable_entry)datetime_copy,
        [C4M_BI_GC_MAP]       = (c4m_vtable_entry)c4m_header_gc_bits,
        [C4M_BI_FINALIZER]    = NULL,
    },
};

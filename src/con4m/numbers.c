#include "con4m.h"

inline int
clz_u128(__uint128_t u)
{
    uint64_t n;

    if ((n = u >> 64) != 0) {
        return __builtin_clzll(n);
    }
    else {
        if ((n = u & ~0ULL) != 0) {
            return 64 + __builtin_clzll(n);
        }
    }

    return 128;
}

static c4m_str_t *
signed_repr(int64_t item)
{
    // TODO, add hex as an option in how.
    char buf[21] = {
        0,
    };
    bool negative = false;

    if (item < 0) {
        negative = true;
        item *= -1;
    }

    if (item == 0) {
        return c4m_new(c4m_type_utf8(), c4m_kw("cstring", c4m_ka("0")));
    }

    int i = 20;

    while (item) {
        buf[--i] = (item % 10) + '0';
        item /= 10;
    }

    if (negative) {
        buf[--i] = '-';
    }

    return c4m_new(c4m_type_utf8(), c4m_kw("cstring", c4m_ka(&buf[i])));
}

static c4m_str_t *
unsigned_repr(int64_t item)
{
    // TODO, add hex as an option in how.
    char buf[21] = {
        0,
    };

    if (item == 0) {
        return c4m_new(c4m_type_utf8(), c4m_kw("cstring", c4m_ka("0")));
    }

    int i = 20;

    while (item) {
        buf[--i] = (item % 10) + '0';
        item /= 10;
    }

    return c4m_new(c4m_type_utf8(), c4m_kw("cstring", c4m_ka(&buf[i])));
}

__uint128_t
raw_int_parse(c4m_utf8_t *u8, c4m_compile_error_t *err, bool *neg)
{
    __uint128_t cur  = 0;
    __uint128_t last = 0;
    char       *s    = u8->data;
    char       *p    = s;

    if (*p == '-') {
        *neg = true;
        p++;
    }
    else {
        *neg = false;
    }

    *err = c4m_err_no_error;

    int32_t c;
    while ((c = *p++) != 0) {
        c -= '0';
        last = cur;
        cur *= 10;
        if (c < 0 || c > 9) {
            if (err) {
                *err = c4m_err_parse_invalid_lit_char;
                // err->loc  = p - s - 1;
            }
            return ~0;
        }
        if (cur < last) {
            if (err) {
                *err = c4m_err_parse_lit_overflow;
            }
        }
        cur += c;
    }
    return cur;
}

__uint128_t
raw_hex_parse(c4m_utf8_t *u8, c4m_compile_error_t *err)
{
    // Here we expect *s to point to the first
    // character after any leading '0x'.
    __uint128_t cur = 0;
    char       *s   = u8->data + 2;
    char        c;
    bool        even = true;

    *err = c4m_err_no_error;

    while ((c = *s++) != 0) {
        if (cur & (((__uint128_t)0x0f) << 124)) {
            *err = c4m_err_parse_lit_overflow;
            return ~0;
        }
        even = !even;
        cur <<= 4;

        switch (c) {
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
            cur |= (c - '0');
            continue;
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
            cur |= (c + 10 - 'a');
            continue;
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
            cur |= (c + 10 - 'A');
            continue;
        default:
            *err = c4m_err_parse_invalid_lit_char;
            return ~0;
        }
    }

    if (!even) {
        *err = c4m_err_parse_lit_odd_hex;
        return ~0;
    }

    return cur;
}

#define BASE_INT_PARSE()                                      \
    bool        neg = false;                                  \
    __uint128_t val;                                          \
                                                              \
    switch (st) {                                             \
    case ST_Base10:                                           \
        val = raw_int_parse(s, code, &neg);                   \
        break;                                                \
    case ST_1Quote:                                           \
        C4M_CRAISE("Single quoted not reimplemented yet.\n"); \
    default:                                                  \
        val = raw_hex_parse(s, code);                         \
        break;                                                \
    }                                                         \
                                                              \
    if (*code != c4m_err_no_error) {                          \
        return NULL;                                          \
    }

#define SIGNED_PARSE(underlow_val, overflow_val, magic_type) \
    BASE_INT_PARSE()                                         \
    if (neg) {                                               \
        if (val > overflow_val) {                            \
            *code = c4m_err_parse_lit_underflow;             \
            return NULL;                                     \
        }                                                    \
        return c4m_box_##magic_type(-1 * val);               \
    }                                                        \
    else {                                                   \
        if (st == ST_Base10 && val > overflow_val) {         \
            *code = c4m_err_parse_lit_overflow;              \
            return NULL;                                     \
        }                                                    \
        return c4m_box_##magic_type(val);                    \
    }

#define UNSIGNED_PARSE(overflow_val, magic_type) \
    BASE_INT_PARSE()                             \
    if (neg) {                                   \
        *code = c4m_err_parse_lit_invalid_neg;   \
        return NULL;                             \
    }                                            \
                                                 \
    if (val > overflow_val) {                    \
        *code = c4m_err_parse_lit_overflow;      \
        return NULL;                             \
    }                                            \
    return c4m_box_##magic_type(val)

static c4m_obj_t
i8_parse(c4m_utf8_t          *s,
         c4m_lit_syntax_t     st,
         c4m_utf8_t          *litmod,
         c4m_compile_error_t *code)
{
    SIGNED_PARSE(0x80, 0x7f, i8);
}

static c4m_obj_t
u8_parse(c4m_utf8_t          *s,
         c4m_lit_syntax_t     st,
         c4m_utf8_t          *litmod,
         c4m_compile_error_t *code)
{
    UNSIGNED_PARSE(0xff, u8);
}

c4m_obj_t
i32_parse(c4m_utf8_t          *s,
          c4m_lit_syntax_t     st,
          c4m_utf8_t          *litmod,
          c4m_compile_error_t *code)
{
    SIGNED_PARSE(0x80000000, 0x7fffffff, i32);
}

c4m_obj_t
u32_parse(c4m_utf8_t          *s,
          c4m_lit_syntax_t     st,
          c4m_utf8_t          *litmod,
          c4m_compile_error_t *code)
{
    UNSIGNED_PARSE(0xffffffff, u32);
}

c4m_obj_t
i64_parse(c4m_utf8_t          *s,
          c4m_lit_syntax_t     st,
          c4m_utf8_t          *litmod,
          c4m_compile_error_t *code)
{
    SIGNED_PARSE(0x8000000000000000, 0x7fffffffffffffff, i64);
}

c4m_obj_t
u64_parse(c4m_utf8_t          *s,
          c4m_lit_syntax_t     st,
          c4m_utf8_t          *litmod,
          c4m_compile_error_t *code)
{
    UNSIGNED_PARSE(0xffffffffffffffff, u64);
}

static c4m_obj_t false_lit = NULL;
static c4m_obj_t true_lit  = NULL;

c4m_obj_t
bool_parse(c4m_utf8_t          *u8,
           c4m_lit_syntax_t     st,
           c4m_utf8_t          *litmod,
           c4m_compile_error_t *code)
{
    char *s = u8->data;
    *code   = c4m_err_no_error;

    switch (*s++) {
    case 't':
    case 'T':
        if (!strcmp(s, "rue")) {
            if (true_lit == NULL) {
                true_lit = c4m_box_bool(true);
                c4m_gc_register_root(&true_lit, 1);
            }
            return true_lit;
        }
        break;
    case 'f':
    case 'F':
        if (!strcmp(s, "alse")) {
            if (false_lit == NULL) {
                false_lit = c4m_box_bool(false);
                c4m_gc_register_root(&false_lit, 1);
            }
            return false_lit;
        }
        break;
    default:
        break;
    }

    *code = c4m_err_parse_invalid_lit_char;

    return NULL;
}

c4m_obj_t
f64_parse(c4m_utf8_t          *s,
          c4m_lit_syntax_t     st,
          c4m_utf8_t          *litmod,
          c4m_compile_error_t *code)
{
    *code = c4m_err_no_error;
    char  *end;
    double d = strtod(s->data, &end);

    if (end == s->data || !*end) {
        *code = c4m_err_parse_invalid_lit_char;
        return NULL;
    }

    if (errno == ERANGE) {
        if (d == HUGE_VAL) {
            *code = c4m_err_parse_lit_overflow;
            return NULL;
        }
        *code = c4m_err_parse_lit_underflow;
        return NULL;
    }
    return c4m_box_double(d);
}

static c4m_str_t *true_repr  = NULL;
static c4m_str_t *false_repr = NULL;

static c4m_str_t *
bool_repr(bool b)
{
    if (b == false) {
        if (false_repr == NULL) {
            false_repr = c4m_new(c4m_type_utf8(),
                                 c4m_kw("cstring", c4m_ka("false")));
            c4m_gc_register_root(&false_repr, 1);
        }
        return false_repr;
    }
    if (true_repr == NULL) {
        true_repr = c4m_new(c4m_type_utf8(),
                            c4m_kw("cstring", c4m_ka("true")));
        c4m_gc_register_root(&true_repr, 1);
    }

    return true_repr;
}

static bool
any_number_can_coerce_to(c4m_type_t *my_type, c4m_type_t *target_type)
{
    switch (c4m_type_get_data_type_info(target_type)->typeid) {
    case C4M_T_BOOL:
    case C4M_T_I8:
    case C4M_T_BYTE:
    case C4M_T_I32:
    case C4M_T_CHAR:
    case C4M_T_U32:
    case C4M_T_INT:
    case C4M_T_UINT:
    case C4M_T_F32:
    case C4M_T_F64:
        return true;
    default:
        return false;
    }
}

static void *
any_int_coerce_to(const int64_t data, c4m_type_t *target_type)
{
    double d;

    switch (c4m_type_get_data_type_info(target_type)->typeid) {
    case C4M_T_BOOL:
    case C4M_T_I8:
    case C4M_T_BYTE:
    case C4M_T_I32:
    case C4M_T_CHAR:
    case C4M_T_U32:
    case C4M_T_INT:
    case C4M_T_UINT:
        return (void *)data;
    case C4M_T_F32:
    case C4M_T_F64:
        d = (double)(data);
        return c4m_double_to_ptr(d);
    default:
        C4M_CRAISE("Invalid type conversion.");
    }
}

static void *
bool_coerce_to(const int64_t data, c4m_type_t *target_type)
{
    switch (c4m_type_get_data_type_info(target_type)->typeid) {
    case C4M_T_BOOL:
    case C4M_T_I8:
    case C4M_T_BYTE:
    case C4M_T_I32:
    case C4M_T_CHAR:
    case C4M_T_U32:
    case C4M_T_INT:
    case C4M_T_UINT:
        if (data) {
            return (void *)NULL;
        }
        else {
            return NULL;
        }
    case C4M_T_F32:
    case C4M_T_F64:
        if (data) {
            return c4m_double_to_ptr(1.0);
        }
        else {
            return c4m_double_to_ptr(0.0);
        }
    default:
        C4M_CRAISE("Invalid type conversion.");
    }
}

static c4m_str_t *
float_repr(const double d)
{
    char buf[20] = {
        0,
    };

    // snprintf includes null terminator in its count.
    snprintf(buf, 20, "%g", d);

    return c4m_new(c4m_type_utf8(), c4m_kw("cstring", c4m_ka(buf)));
}

static void *
float_coerce_to(const double d, c4m_type_t *target_type)
{
    int64_t i;

    switch (c4m_type_get_data_type_info(target_type)->typeid) {
    case C4M_T_BOOL:
    case C4M_T_I8:
    case C4M_T_BYTE:
    case C4M_T_I32:
    case C4M_T_CHAR:
    case C4M_T_U32:
    case C4M_T_INT:
    case C4M_T_UINT:
        i = (int64_t)d;

        return (void *)i;
    case C4M_T_F32:
    case C4M_T_F64:
        return c4m_double_to_ptr(d);
    default:
        C4M_CRAISE("Invalid type conversion.");
    }
}

static char lowercase_map[] = "0123456789abcdef";
static char uppercase_map[] = "0123456789ABCDEF";

#define MAX_INT_LEN (100)
static c4m_str_t *
base_int_fmt(__int128_t v, c4m_fmt_spec_t *spec, c4m_codepoint_t default_type)
{
    int processed_digits = 0;
    int prefix_option    = 0; // 1 will be 0x, 2 will be U+

    char repr[MAX_INT_LEN] = {
        0,
    };

    int      n = MAX_INT_LEN - 1;
    uint64_t val;

    if (v < 0) {
        val = (uint64_t)(v * -1);
    }
    else {
        val = (uint64_t)v;
    }

    c4m_codepoint_t t = spec->type;

    if (t == 0) {
        t = default_type;
    }

    switch (t) {
    // Print as a unicode codepoint. Zero-filling is never performed.
    case 'c':
        if (val > 0x10ffff || v < 0) {
            return c4m_utf8_repeat(0xfffd, 1); // Replacement character.
        }
        return c4m_utf8_repeat((int32_t)val, 1);
    case 'u': // Print as unicode CP with U+ at the beginning.
        if (val > 0x10ffff || v < 0) {
            val = 0xfffd;
            v   = 0;
        }
        prefix_option++; // fallthrough.
    case 'x':            // Print as hex with "0x" at the beginning.
        prefix_option++; // fallthrough.
    case 'h':            // Print as hex with no prefix.
        if (val == 0) {
            repr[--n] = '0';
        }
        else {
            while (val != 0) {
                repr[--n] = lowercase_map[val & 0xf];
                val >>= 4;
            }
        }
        break;
    case 'U':
        if (val > 0x10ffff || v < 0) {
            val = 0xfffd;
            v   = 0;
        }
        prefix_option++; // fallthrough.
    case 'X':
        prefix_option++; // fallthrough.
    case 'H':            // Print as hex with no prefix, using capital letters.
        if (n == 0) {
            repr[--n] = '0';
        }
        else {
            while (val != 0) {
                repr[--n] = uppercase_map[val & 0xf];
                val >>= 4;
            }
        }
        break;
    case 'n':
    case 'i':
    case 'd': // Normal ordinal.
        if (v == 0) {
            repr[--n] = '0';
        }
        else {
            while (val != 0) {
                if (processed_digits && processed_digits % 3 == 0) {
                    switch (spec->sep) {
                    case C4M_FMT_SEP_COMMA:
                        repr[--n] = ',';
                        break;
                    case C4M_FMT_SEP_USCORE:
                        repr[--n] = '_';
                        break;
                    default:
                        break;
                    }
                }

                repr[--n] = (val % 10) + '0';
                val /= 10;
                processed_digits++;
            }
        }
        break;
    default:
        C4M_CRAISE("Invalid specifier for integer.");
    }

    c4m_utf8_t *s = c4m_new(c4m_type_utf8(),
                            c4m_kw("cstring", c4m_ka(repr + n)));

    // Figure out if we're going to use the sign before doing any padding.

    if (spec->width != 0) {
        bool use_sign = true;

        if (v < 0 && spec->sign == C4M_FMT_SIGN_DEFAULT) {
            use_sign = false;
        }

        // Now, zero-pad if requested.
        int l = MAX_INT_LEN - n - 1;

        if (use_sign) {
            l -= 1;
        }

        if (prefix_option) {
            l -= 2;
        }

        if (l < spec->width) {
            if (!spec->fill || spec->fill == '0') {
                s = c4m_str_concat(s, c4m_utf8_repeat('0', spec->width - l));
            }
        }
    }

    switch (prefix_option) {
    case 1:
        s = c4m_str_concat(
            c4m_new(c4m_type_utf8(),
                    c4m_kw("cstring", c4m_ka("0x"))),
            s);
        break;
    case 2:
        s = c4m_str_concat(
            c4m_new(c4m_type_utf8(),
                    c4m_kw("cstring", c4m_ka("U+"))),
            s);
        break;
    default:
        break;
    }

    // Finally, add the sign if appropriate.
    if (v < 0) {
        s = c4m_str_concat(c4m_utf8_repeat('-', 1), s);
    }
    else {
        switch (spec->sign) {
        case C4M_FMT_SIGN_ALWAYS:
            s = c4m_str_concat(c4m_utf8_repeat('+', 1), s);
            break;
        case C4M_FMT_SIGN_POS_SPACE:
            s = c4m_str_concat(c4m_utf8_repeat(' ', 1), s);
            break;
        default:
            break;
        }
    }

    return s;
}

static c4m_str_t *
u8_fmt(uint8_t *repr, c4m_fmt_spec_t *spec)
{
    uint8_t n = *repr;

    return base_int_fmt((__int128_t)n, spec, 'c');
}

static c4m_str_t *
i8_fmt(int8_t *repr, c4m_fmt_spec_t *spec)
{
    int8_t n = *repr;

    return base_int_fmt((__int128_t)n, spec, 'n');
}

static c4m_str_t *
u32_fmt(uint32_t *repr, c4m_fmt_spec_t *spec)
{
    uint32_t n = *repr;

    return base_int_fmt((__int128_t)n, spec, 'c');
}

static c4m_str_t *
i32_fmt(int32_t *repr, c4m_fmt_spec_t *spec)
{
    int32_t n = *repr;

    return base_int_fmt((__int128_t)n, spec, 'n');
}

static c4m_str_t *
u64_fmt(uint64_t *repr, c4m_fmt_spec_t *spec)
{
    uint64_t n = *repr;

    return base_int_fmt((__int128_t)n, spec, 'n');
}

static c4m_str_t *
i64_fmt(int64_t *repr, c4m_fmt_spec_t *spec)
{
    int64_t n = *repr;

    return base_int_fmt((__int128_t)n, spec, 'n');
}

static c4m_str_t *
bool_fmt(bool *repr, c4m_fmt_spec_t *spec)
{
    switch (spec->type) {
    case 0:
        if (*repr) {
            return c4m_new(c4m_type_utf8(),
                           c4m_kw("cstring", c4m_ka("True")));
        }
        else {
            return c4m_new(c4m_type_utf8(),
                           c4m_kw("cstring", c4m_ka("False")));
        }
    case 'b': // Boolean
        if (*repr) {
            return c4m_new(c4m_type_utf8(),
                           c4m_kw("cstring", c4m_ka("true")));
        }
        else {
            return c4m_new(c4m_type_utf8(),
                           c4m_kw("cstring", c4m_ka("false")));
        }
    case 'B':
        if (*repr) {
            return c4m_new(c4m_type_utf8(),
                           c4m_kw("cstring", c4m_ka("TRUE")));
        }
        else {
            return c4m_new(c4m_type_utf8(),
                           c4m_kw("cstring", c4m_ka("FALSE")));
        }
    case 't':
        if (*repr) {
            return c4m_utf8_repeat('t', 1);
        }
        else {
            return c4m_utf8_repeat('f', 1);
        }
    case 'T':
        if (*repr) {
            return c4m_utf8_repeat('T', 1);
        }
        else {
            return c4m_utf8_repeat('F', 1);
        }
    case 'Q': // Question
        if (*repr) {
            return c4m_new(c4m_type_utf8(),
                           c4m_kw("cstring", c4m_ka("YES")));
        }
        else {
            return c4m_new(c4m_type_utf8(),
                           c4m_kw("cstring", c4m_ka("NO")));
        }
    case 'q':
        if (*repr) {
            return c4m_new(c4m_type_utf8(),
                           c4m_kw("cstring", c4m_ka("yes")));
        }
        else {
            return c4m_new(c4m_type_utf8(),
                           c4m_kw("cstring", c4m_ka("no")));
        }
    case 'Y':
        if (*repr) {
            return c4m_utf8_repeat('Y', 1);
        }
        else {
            return c4m_utf8_repeat('N', 1);
        }
    case 'y':
        if (*repr) {
            return c4m_utf8_repeat('y', 1);
        }
        else {
            return c4m_utf8_repeat('n', 1);
        }
    default:
        C4M_CRAISE("Invalid boolean output type specifier");
    }
}

#define FP_TO_STR_BUFSZ    (24)
#define FP_MAX_INTERNAL_SZ (100)
#define FP_OFFSET          (FP_MAX_INTERNAL_SZ - FP_TO_STR_BUFSZ)

static c4m_str_t *
float_fmt(double *repr, c4m_fmt_spec_t *spec)
{
    switch (spec->type) {
    case 0:
    case 'f':
    case 'g':
    case 'd':
        break;
    default:
        C4M_CRAISE("Invalid specifier for floating point format string.");
    }

    // Currently not using the precision field at all.
    char fprepr[FP_MAX_INTERNAL_SZ] = {
        0,
    };

    double value      = *repr;
    int    strlen     = c4m_internal_fptostr(value, fprepr + FP_OFFSET);
    int    n          = FP_OFFSET;
    bool   using_sign = true;

    if (value > 0) {
        switch (spec->sign) {
        case C4M_FMT_SIGN_ALWAYS:
            fprepr[--n] = '+';
            strlen++;
            using_sign = true;
            break;
        case C4M_FMT_SIGN_POS_SPACE:
            fprepr[--n] = ' ';
            strlen++;
            using_sign = true;
            break;
        default:
            using_sign = false;
        }
    }

    if (spec->width != 0 && strlen < spec->width) {
        int tofill = spec->width - strlen;

        if (spec->fill == 0 || spec->fill == '0') {
            // Zero-pad.
            c4m_utf8_t *pad = c4m_utf8_repeat('0', tofill);
            c4m_utf8_t *rest;
            c4m_utf8_t *sign;

            if (using_sign) {
                sign = c4m_utf8_repeat(fprepr[n++], 1);
            }

            rest = c4m_new(c4m_type_utf8(),
                           c4m_kw("cstring", c4m_ka(fprepr + n)));

            if (using_sign) {
                pad = c4m_str_concat(sign, pad);
            }

            // Zero this out to indicate we already padded it.
            spec->width = 0;

            return c4m_str_concat(pad, rest);
        }
    }

    return c4m_new(c4m_type_utf8(), c4m_kw("cstring", c4m_ka(fprepr + n)));
}

const c4m_vtable_t c4m_u8_type = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_REPR]         = (c4m_vtable_entry)unsigned_repr,
        [C4M_BI_FORMAT]       = (c4m_vtable_entry)u8_fmt,
        [C4M_BI_COERCIBLE]    = (c4m_vtable_entry)any_number_can_coerce_to,
        [C4M_BI_COERCE]       = (c4m_vtable_entry)any_int_coerce_to,
        [C4M_BI_FROM_LITERAL] = (c4m_vtable_entry)u8_parse,
        // Explicit because some compilers don't seem to always properly
        // zero it (Was sometimes crashing on a `c4m_stream_t` on my mac).
        [C4M_BI_FINALIZER]    = NULL,
        NULL,
    },
};

const c4m_vtable_t c4m_i8_type = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_REPR]         = (c4m_vtable_entry)signed_repr,
        [C4M_BI_FORMAT]       = (c4m_vtable_entry)i8_fmt,
        [C4M_BI_COERCIBLE]    = (c4m_vtable_entry)any_number_can_coerce_to,
        [C4M_BI_COERCE]       = (c4m_vtable_entry)any_int_coerce_to,
        [C4M_BI_FROM_LITERAL] = (c4m_vtable_entry)i8_parse,
        [C4M_BI_FINALIZER]    = NULL,
        NULL,
    },
};

const c4m_vtable_t c4m_u32_type = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_REPR]         = (c4m_vtable_entry)unsigned_repr,
        [C4M_BI_FORMAT]       = (c4m_vtable_entry)u32_fmt,
        [C4M_BI_COERCIBLE]    = (c4m_vtable_entry)any_number_can_coerce_to,
        [C4M_BI_COERCE]       = (c4m_vtable_entry)any_int_coerce_to,
        [C4M_BI_FROM_LITERAL] = (c4m_vtable_entry)u32_parse,
        [C4M_BI_FINALIZER]    = NULL,
        NULL,
    },
};

const c4m_vtable_t c4m_i32_type = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_REPR]         = (c4m_vtable_entry)signed_repr,
        [C4M_BI_FORMAT]       = (c4m_vtable_entry)i32_fmt,
        [C4M_BI_COERCIBLE]    = (c4m_vtable_entry)any_number_can_coerce_to,
        [C4M_BI_COERCE]       = (c4m_vtable_entry)any_int_coerce_to,
        [C4M_BI_FROM_LITERAL] = (c4m_vtable_entry)i32_parse,
        [C4M_BI_FINALIZER]    = NULL,
        NULL,
    },
};

const c4m_vtable_t c4m_u64_type = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_REPR]         = (c4m_vtable_entry)unsigned_repr,
        [C4M_BI_FORMAT]       = (c4m_vtable_entry)u64_fmt,
        [C4M_BI_COERCIBLE]    = (c4m_vtable_entry)any_number_can_coerce_to,
        [C4M_BI_COERCE]       = (c4m_vtable_entry)any_int_coerce_to,
        [C4M_BI_FROM_LITERAL] = (c4m_vtable_entry)u64_parse,
        [C4M_BI_FINALIZER]    = NULL,
        NULL,
    },
};

const c4m_vtable_t c4m_i64_type = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_REPR]         = (c4m_vtable_entry)signed_repr,
        [C4M_BI_FORMAT]       = (c4m_vtable_entry)i64_fmt,
        [C4M_BI_COERCIBLE]    = (c4m_vtable_entry)any_number_can_coerce_to,
        [C4M_BI_COERCE]       = (c4m_vtable_entry)any_int_coerce_to,
        [C4M_BI_FROM_LITERAL] = (c4m_vtable_entry)i64_parse,
        [C4M_BI_FINALIZER]    = NULL,
        NULL,
    },
};

const c4m_vtable_t c4m_bool_type = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_REPR]         = (c4m_vtable_entry)bool_repr,
        [C4M_BI_FORMAT]       = (c4m_vtable_entry)bool_fmt,
        [C4M_BI_COERCIBLE]    = (c4m_vtable_entry)any_number_can_coerce_to,
        [C4M_BI_COERCE]       = (c4m_vtable_entry)bool_coerce_to,
        [C4M_BI_FROM_LITERAL] = (c4m_vtable_entry)bool_parse,
        [C4M_BI_FINALIZER]    = NULL,
        NULL,
    },
};

const c4m_vtable_t c4m_float_type = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_REPR]         = (c4m_vtable_entry)float_repr,
        [C4M_BI_FORMAT]       = (c4m_vtable_entry)float_fmt,
        [C4M_BI_COERCIBLE]    = (c4m_vtable_entry)any_number_can_coerce_to,
        [C4M_BI_COERCE]       = (c4m_vtable_entry)float_coerce_to,
        [C4M_BI_FROM_LITERAL] = (c4m_vtable_entry)f64_parse,
        [C4M_BI_FINALIZER]    = NULL,
        NULL,
    },
};

#include "con4m.h"

#define C4M_SZ_KB    1000ULL
#define C4M_SZ_KI    (1ULL << 10)
#define C4M_SZ_MB    1000000ULL
#define C4M_SZ_MI    (1ULL << 20)
#define C4M_SZ_GB    1000000000ULL
#define C4M_SZ_GI    (1ULL << 30)
#define C4M_SZ_TB    1000000000000ULL
#define C4M_SZ_TI    (1ULL << 40)
#define C4M_MAX_UINT (~0ULL)

static bool
parse_size_lit(c4m_utf8_t *to_parse, c4m_size_t *result, bool *oflow)
{
    __int128_t n_bytes = 0;
    __int128_t cur;

    if (oflow) {
        *oflow = false;
    }

    to_parse = c4m_to_utf8(c4m_str_lower(to_parse));

    int      l = c4m_str_byte_len(to_parse);
    char    *p = to_parse->data;
    char    *e = p + l;
    char     c;
    uint64_t multiplier;

    if (!l) {
        return false;
    }

    while (p < e) {
        c = *p;

        if (!isdigit(c)) {
            return false;
        }

        cur = 0;

        while (isdigit(c)) {
            cur *= 10;
            cur += (c - '0');

            if (cur > (__int128_t)C4M_MAX_UINT) {
                if (oflow) {
                    *oflow = true;
                }
                return false;
            }
            if (p == e) {
                return false;
            }
            c = *++p;
        }

        while (c == ' ' || c == ',') {
            if (p == e) {
                return false;
            }
            c = *++p;
        }

        switch (c) {
        case 'b':
            p++;
            if (p != e) {
                c = *p;
                if (c == 'y') {
                    p++;
                    if (p != e) {
                        c = *p;
                        if (c == 't') {
                            p++;
                            if (p != e) {
                                c = *p;
                                if (c == 'e') {
                                    p++;
                                    if (p != e) {
                                        c = *p;
                                        if (c == 's') {
                                            p++;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            multiplier = 1;
            break;
        case 'k':
            p++;
            if (p != e) {
                c = *p;
                if (c == 'i') {
                    p++;
                    multiplier = C4M_SZ_KI;
                }
                else {
                    multiplier = C4M_SZ_KB;
                }

                if (p != e) {
                    c = *p;
                    if (c == 'b') {
                        p++;
                    }
                }
            }
            break;
        case 'm':
            p++;
            if (p != e) {
                c = *p;
                if (c == 'i') {
                    p++;
                    multiplier = C4M_SZ_MI;
                }
                else {
                    multiplier = C4M_SZ_MB;
                }

                if (p != e) {
                    c = *p;
                    if (c == 'b') {
                        p++;
                    }
                }
            }
            break;
        case 'g':
            p++;
            if (p != e) {
                c = *p;
                if (c == 'i') {
                    p++;
                    multiplier = C4M_SZ_GI;
                }
                else {
                    multiplier = C4M_SZ_GB;
                }

                if (p != e) {
                    c = *p;
                    if (c == 'b') {
                        p++;
                    }
                }
            }
            break;
        case 't':
            p++;
            if (p != e) {
                c = *p;
                if (c == 'i') {
                    p++;
                    multiplier = C4M_SZ_TI;
                }
                else {
                    multiplier = C4M_SZ_TB;
                }

                if (p != e) {
                    c = *p;
                    if (c == 'b') {
                        p++;
                    }
                }
            }
            break;
        default:
            return false;
        }
        n_bytes += multiplier * cur;

        if (n_bytes > (__int128_t)C4M_MAX_UINT) {
            if (oflow) {
                *oflow = true;
            }
            return false;
        }
        while (p < e) {
            c = *p;
            if (c != ' ' && c != ',') {
                break;
            }
            p++;
        }
    }

    uint64_t cast = (uint64_t)n_bytes;
    *result       = cast;

    return true;
}

static void
size_init(c4m_size_t *self, va_list args)
{
    c4m_utf8_t *to_parse = NULL;
    bool        oflow    = false;
    uint64_t    bytes    = 0;

    c4m_karg_va_init(args);
    c4m_kw_ptr("to_parse", to_parse);
    c4m_kw_uint64("bytes", bytes);

    if (to_parse != NULL) {
        to_parse = c4m_to_utf8(to_parse);

        if (!parse_size_lit(to_parse, self, &oflow)) {
            if (oflow) {
                C4M_CRAISE("Size literal value is too large.");
            }
            else {
                C4M_CRAISE("Invalid size literal.");
            }
        }
    }
    *self = bytes;
}

static c4m_str_t *
size_repr(c4m_size_t *self)
{
    // We produce both power of 2 and power of 10, and then return
    // the shorter of the 2.

    uint64_t    n   = *self;
    c4m_utf8_t *p10 = c4m_new_utf8("");
    c4m_utf8_t *p2;
    uint64_t    tmp;

    if (!n) {
        return c4m_new_utf8("0 Bytes");
    }

    if (n >= C4M_SZ_TB) {
        tmp = n / C4M_SZ_TB;
        p10 = c4m_cstr_format("{} Tb ", c4m_box_u64(tmp));
        tmp *= C4M_SZ_TB;
        n -= tmp;
    }
    if (n >= C4M_SZ_GB) {
        tmp = n / C4M_SZ_GB;
        p10 = c4m_cstr_format("{}{} Gb ", p10, c4m_box_u64(tmp));
        tmp *= C4M_SZ_GB;
        n -= tmp;
    }
    if (n >= C4M_SZ_MB) {
        tmp = n / C4M_SZ_MB;
        p10 = c4m_cstr_format("{}{} Mb ", p10, c4m_box_u64(tmp));
        tmp *= C4M_SZ_MB;
        n -= tmp;
    }
    if (n >= C4M_SZ_KB) {
        tmp = n / C4M_SZ_KB;
        p10 = c4m_cstr_format("{}{} Kb ", p10, c4m_box_u64(tmp));
        tmp *= C4M_SZ_KB;
        n -= tmp;
    }

    if (n != 0) {
        p10 = c4m_cstr_format("{}{} Bytes", p10, c4m_box_u64(n));
    }
    else {
        p10 = c4m_to_utf8(c4m_str_strip(p10));
    }

    n = *self;

    if (n < 1024) {
        return p10;
    }

    p2 = c4m_new_utf8("");

    if (n >= C4M_SZ_TI) {
        tmp = n / C4M_SZ_TI;
        p2  = c4m_cstr_format("{} TiB ", c4m_box_u64(tmp));
        tmp *= C4M_SZ_TI;
        n -= tmp;
    }
    if (n >= C4M_SZ_GI) {
        tmp = n / C4M_SZ_GI;
        p2  = c4m_cstr_format("{}{} GiB ", p2, c4m_box_u64(tmp));
        tmp *= C4M_SZ_GI;
        n -= tmp;
    }
    if (n >= C4M_SZ_MI) {
        tmp = n / C4M_SZ_MI;
        p2  = c4m_cstr_format("{}{} MiB ", p2, c4m_box_u64(tmp));
        tmp *= C4M_SZ_MI;
        n -= tmp;
    }
    if (n >= C4M_SZ_KI) {
        tmp = n / C4M_SZ_KI;
        p2  = c4m_cstr_format("{}{} KiB ", p2, c4m_box_u64(tmp));
        tmp *= C4M_SZ_KI;
        n -= tmp;
    }

    if (n != 0) {
        p2 = c4m_cstr_format("{}{} Bytes", p2, c4m_box_u64(n));
    }
    else {
        p2 = c4m_to_utf8(c4m_str_strip(p2));
    }

    if (c4m_str_codepoint_len(p10) < c4m_str_codepoint_len(p2)) {
        return p10;
    }

    return p2;
}

static c4m_size_t *
size_lit(c4m_utf8_t          *s,
         c4m_lit_syntax_t     st,
         c4m_utf8_t          *mod,
         c4m_compile_error_t *err)
{
    c4m_size_t *result   = c4m_new(c4m_type_size());
    bool        overflow = false;

    if (st == ST_Base10) {
        __uint128_t v;
        bool        neg;
        v = c4m_raw_int_parse(s, err, &neg);

        if (neg) {
            *err = c4m_err_invalid_size_lit;
            return NULL;
        }

        if (*err != c4m_err_no_error) {
            return NULL;
        }

        if (v > (__uint128_t)C4M_MAX_UINT) {
            *err = c4m_err_parse_lit_overflow;
            return NULL;
        }
        *result = (uint64_t)v;

        return result;
    }

    if (!parse_size_lit(s, result, &overflow)) {
        if (overflow) {
            *err = c4m_err_parse_lit_overflow;
            return NULL;
        }
        else {
            *err = c4m_err_invalid_size_lit;
            return NULL;
        }
    }

    return result;
}

static bool
size_eq(c4m_size_t *r1, c4m_size_t *r2)
{
    return *r1 == *r2;
}

static bool
size_lt(c4m_size_t *r1, c4m_size_t *r2)
{
    return *r1 < *r2;
}

static bool
size_gt(c4m_size_t *r1, c4m_size_t *r2)
{
    return *r1 > *r2;
}

static c4m_size_t *
size_add(c4m_size_t *s1, c4m_size_t *s2)
{
    c4m_size_t *result = c4m_new(c4m_type_size());

    *result = *s1 + *s2;

    return result;
}

static c4m_size_t *
size_diff(c4m_size_t *s1, c4m_size_t *s2)
{
    c4m_size_t *result = c4m_new(c4m_type_size());

    if (*s1 > *s2) {
        *result = *s1 - *s2;
    }
    else {
        *result = *s2 - *s1;
    }

    return result;
}

static bool
size_can_coerce_to(c4m_type_t *me, c4m_type_t *them)
{
    switch (c4m_type_get_data_type_info(them)->typeid) {
    case C4M_T_INT:
    case C4M_T_UINT:
    case C4M_T_SIZE:
        return true;
    default:
        return false;
    }
}

static void *
size_coerce_to(c4m_size_t *self, c4m_type_t *target_type)
{
    switch (c4m_type_get_data_type_info(target_type)->typeid) {
    case C4M_T_INT:
    case C4M_T_UINT:
        return (void *)*self;
    default:
        return self;
    }
}

const c4m_vtable_t c4m_size_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR]  = (c4m_vtable_entry)size_init,
        [C4M_BI_REPR]         = (c4m_vtable_entry)size_repr,
        [C4M_BI_FROM_LITERAL] = (c4m_vtable_entry)size_lit,
        [C4M_BI_EQ]           = (c4m_vtable_entry)size_eq,
        [C4M_BI_LT]           = (c4m_vtable_entry)size_lt,
        [C4M_BI_GT]           = (c4m_vtable_entry)size_gt,
        [C4M_BI_GC_MAP]       = (c4m_vtable_entry)C4M_GC_SCAN_NONE,
        [C4M_BI_ADD]          = (c4m_vtable_entry)size_add,
        [C4M_BI_SUB]          = (c4m_vtable_entry)size_diff,
        [C4M_BI_COERCIBLE]    = (c4m_vtable_entry)size_can_coerce_to,
        [C4M_BI_COERCE]       = (c4m_vtable_entry)size_coerce_to,
        [C4M_BI_FINALIZER]    = NULL,
    },
};

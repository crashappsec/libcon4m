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
signed_repr(int64_t item, to_str_use_t how)
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
        return c4m_new(c4m_tspec_utf8(), c4m_kw("cstring", c4m_ka("0")));
    }

    int i = 20;

    while (item) {
        buf[--i] = (item % 10) + '0';
        item /= 10;
    }

    if (negative) {
        buf[--i] = '-';
    }

    return c4m_new(c4m_tspec_utf8(), c4m_kw("cstring", c4m_ka(&buf[i])));
}

static c4m_str_t *
unsigned_repr(int64_t item, to_str_use_t how)
{
    // TODO, add hex as an option in how.
    char buf[21] = {
        0,
    };

    if (item == 0) {
        return c4m_new(c4m_tspec_utf8(), c4m_kw("cstring", c4m_ka("0")));
    }

    int i = 20;

    while (item) {
        buf[--i] = (item % 10) + '0';
        item /= 10;
    }

    return c4m_new(c4m_tspec_utf8(), c4m_kw("cstring", c4m_ka(&buf[i])));
}

__uint128_t
raw_int_parse(char *s, c4m_lit_error_t *err, bool *neg)
{
    __uint128_t cur  = 0;
    __uint128_t last = 0;
    char       *p    = s;

    if (*p == '-') {
        *neg = true;
        p++;
    }
    else {
        *neg = false;
    }

    char c;
    while ((c = *p++) != 0) {
        c -= '0';
        last = cur;
        cur *= 10;
        if (c < 0 || c > 9) {
            if (err) {
                err->code = LE_InvalidChar;
                err->loc  = p - s - 1;
            }
            return ~0;
        }
        if (cur < last) {
            err->code = LE_Overflow;
        }
        cur += c;
    }
    return cur;
}

__uint128_t
raw_hex_parse(char *s, c4m_lit_error_t *err)
{
    // Here we expect *s to point to the first
    // character after any leading '0x'.
    __uint128_t cur = 0;
    char        c;
    bool        even = true;

    while ((c = *s++) != 0) {
        if (cur & (((__uint128_t)0x0f) << 124)) {
            err->code = LE_Overflow;
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
            err->code = LE_InvalidChar;
            return ~0;
        }
    }

    if (!even) {
        err->code = LE_OddHex;
        return ~0;
    }

    return cur;
}

static c4m_obj_t
i8_parse(char *s, c4m_lit_syntax_t st, char *litmod, c4m_lit_error_code_t *code)
{
    char           *result = c4m_new(c4m_tspec_i8());
    c4m_lit_error_t err    = {0, LE_NoError};
    bool            neg;
    __uint128_t     val;

    if (st == ST_Base10) {
        val = raw_int_parse(s, &err, &neg);
    }
    else {
        val = raw_hex_parse(s, &err);
        neg = false;
    }

    if (err.code != LE_NoError) {
        *code = err.code;
        return NULL;
    }

    if (neg) {
        if (val > 0x80) {
            *code = LE_Underflow;
            return NULL;
        }

        *result = -1 * (char)val;
    }
    else {
        if (val > 0x7f) {
            *code = LE_Overflow;
            return NULL;
        }
        *result = (char)val;
    }

    return (c4m_obj_t)result;
}

c4m_obj_t
u8_parse(char                 *s,
         c4m_lit_syntax_t      st,
         char                 *litmod,
         c4m_lit_error_code_t *code)
{
    uint8_t        *result = c4m_new(c4m_tspec_byte());
    c4m_lit_error_t err    = {0, LE_NoError};
    bool            neg;
    __uint128_t     val;

    if (st == ST_Base10) {
        val = raw_int_parse(s, &err, &neg);
    }
    else {
        val = raw_hex_parse(s, &err);
    }

    if (err.code != LE_NoError) {
        *code = err.code;
        return NULL;
    }

    if (neg) {
        *code = LE_InvalidNeg;
        return NULL;
    }

    if (val > 0xff) {
        *code = LE_Overflow;
        return NULL;
    }
    *result = (uint8_t)val;

    return (c4m_obj_t)result;
}

c4m_obj_t
i32_parse(char                 *s,
          c4m_lit_syntax_t      st,
          char                 *litmod,
          c4m_lit_error_code_t *code)
{
    int32_t        *result = c4m_new(c4m_tspec_i32());
    c4m_lit_error_t err    = {0, LE_NoError};
    bool            neg;
    __uint128_t     val;

    if (st == ST_Base10) {
        val = raw_int_parse(s, &err, &neg);
    }
    else {
        val = raw_hex_parse(s, &err);
        neg = false;
    }

    if (err.code != LE_NoError) {
        *code = err.code;
        return NULL;
    }

    if (neg) {
        if (val > 0x80000000) {
            *code = LE_Underflow;
            return NULL;
        }

        *result = -1 * (int32_t)val;
    }
    else {
        if (val > 0x7fffffff) {
            *code = LE_Overflow;
            return NULL;
        }

        *result = (int32_t)val;
    }

    return (c4m_obj_t)result;
}

c4m_obj_t
u32_parse(char                 *s,
          c4m_lit_syntax_t      st,
          char                 *litmod,
          c4m_lit_error_code_t *code)
{
    uint32_t       *result = c4m_new(c4m_tspec_u32());
    c4m_lit_error_t err    = {0, LE_NoError};
    bool            neg;
    __uint128_t     val;

    if (st == ST_Base10) {
        val = raw_int_parse(s, &err, &neg);
    }
    else {
        val = raw_hex_parse(s, &err);
    }

    if (err.code != LE_NoError) {
        *code = err.code;
        return NULL;
    }

    if (neg) {
        *code = LE_InvalidNeg;
        return NULL;
    }

    if (val > 0xffffffff) {
        *code = LE_Overflow;
        return NULL;
    }

    *result = (uint32_t)val;

    return (c4m_obj_t)result;
}

static c4m_obj_t false_lit = NULL;
static c4m_obj_t true_lit  = NULL;

c4m_obj_t
bool_parse(char                 *s,
           c4m_lit_syntax_t      st,
           char                 *litmod,
           c4m_lit_error_code_t *code)
{
    switch (*s++) {
    case 't':
    case 'T':
        if (!strcmp(s, "rue")) {
            if (true_lit == NULL) {
                int32_t *lit = c4m_new(c4m_tspec_bool());
                *lit         = 1;
                true_lit     = (c4m_obj_t)lit;
                c4m_gc_register_root(&true_lit, 1);
            }
            return true_lit;
        }
        break;
    case 'f':
    case 'F':
        if (!strcmp(s, "alse")) {
            if (false_lit == NULL) {
                int32_t *lit = c4m_new(c4m_tspec_bool());
                *lit         = 0;
                false_lit    = (c4m_obj_t)lit;
                c4m_gc_register_root(&false_lit, 1);
            }
            return false_lit;
        }
        break;
    default:
        break;
    }
    *code = LE_InvalidChar;

    return NULL;
}

c4m_obj_t
i64_parse(char                 *s,
          c4m_lit_syntax_t      st,
          char                 *litmod,
          c4m_lit_error_code_t *code)
{
    int64_t        *result = c4m_new(c4m_tspec_int());
    c4m_lit_error_t err    = {0, LE_NoError};
    bool            neg;
    __uint128_t     val;

    if (st == ST_Base10) {
        val = raw_int_parse(s, &err, &neg);
    }
    else {
        val = raw_hex_parse(s, &err);
        neg = false;
    }

    if (err.code != LE_NoError) {
        *code = err.code;
        return NULL;
    }

    if (neg) {
        if (val > 0x8000000000000000) {
            *code = LE_Underflow;
            return NULL;
        }

        *result = -1 * (int64_t)val;
    }
    else {
        if (val > 0x7fffffffffffffff) {
            *code = LE_Overflow;
            return NULL;
        }

        *result = (int64_t)val;
    }

    return (c4m_obj_t)result;
}

c4m_obj_t
u64_parse(char                 *s,
          c4m_lit_syntax_t      st,
          char                 *litmod,
          c4m_lit_error_code_t *code)
{
    uint64_t       *result = c4m_new(c4m_tspec_uint());
    c4m_lit_error_t err    = {0, LE_NoError};
    bool            neg;
    __uint128_t     val;

    if (st == ST_Base10) {
        val = raw_int_parse(s, &err, &neg);
    }
    else {
        val = raw_hex_parse(s, &err);
    }

    if (err.code != LE_NoError) {
        *code = err.code;
        return NULL;
    }

    if (neg) {
        *code = LE_InvalidNeg;
        return NULL;
    }

    if (val > 0xffffffffffffffff) {
        *code = LE_Overflow;
        return NULL;
    }

    *result = (uint64_t)val;

    return (c4m_obj_t)result;
}

c4m_obj_t
f64_parse(char                 *s,
          c4m_lit_syntax_t      st,
          char                 *litmod,
          c4m_lit_error_code_t *code)
{
    char   *end;
    double *lit = c4m_new(c4m_tspec_f64());
    double  d   = strtod(s, &end);

    if (end == s || !*end) {
        *code = LE_InvalidChar;
        return NULL;
    }

    if (errno == ERANGE) {
        if (d == HUGE_VAL) {
            *code = LE_Overflow;
            return NULL;
        }
        *code == LE_Underflow;
        return NULL;
    }
    *lit = d;
    return lit;
}

static c4m_str_t *
i8_repr(i8_box *i8, to_str_use_t how)
{
    int64_t n = *i8;
    return signed_repr(n, how);
}

static c4m_str_t *
u8_repr(u8_box *u8, to_str_use_t how)
{
    uint64_t n = *u8;
    return unsigned_repr(n, how);
}

static c4m_str_t *
i32_repr(i32_box *i32, to_str_use_t how)
{
    int64_t n = *i32;
    return signed_repr(n, how);
}

static c4m_str_t *
u32_repr(u32_box *u32, to_str_use_t how)
{
    uint64_t n = *u32;
    return unsigned_repr(n, how);
}

static c4m_str_t *
i64_repr(i64_box *i64, to_str_use_t how)
{
    return signed_repr(*i64, how);
}

static c4m_str_t *
u64_repr(u64_box *u64, to_str_use_t how)
{
    return unsigned_repr(*u64, how);
}

static c4m_str_t *true_repr  = NULL;
static c4m_str_t *false_repr = NULL;

static c4m_str_t *
bool_repr(bool *b, to_str_use_t how)
{
    if (*b == false) {
        if (false_repr == NULL) {
            false_repr = c4m_new(c4m_tspec_utf8(),
                                 c4m_kw("cstring", c4m_ka("false")));
            c4m_gc_register_root(&false_repr, 1);
        }
        return false_repr;
    }
    if (true_repr == NULL) {
        true_repr = c4m_new(c4m_tspec_utf8(), c4m_kw("cstring", c4m_ka("true")));
        c4m_gc_register_root(&true_repr, 1);
    }

    return true_repr;
}

bool
any_number_can_coerce_to(c4m_type_t *my_type, c4m_type_t *target_type)
{
    switch (c4m_tspec_get_data_type_info(target_type)->typeid) {
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

void *
any_int_coerce_to(const int64_t data, c4m_type_t *target_type)
{
    double d;

    switch (c4m_tspec_get_data_type_info(target_type)->typeid) {
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

void *
bool_coerce_to(const int64_t data, c4m_type_t *target_type)
{
    switch (c4m_tspec_get_data_type_info(target_type)->typeid) {
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

c4m_str_t *
float_repr(const double *dp, to_str_use_t how)
{
    double d       = *dp;
    char   buf[20] = {
        0,
    };

    // snprintf includes null terminator in its count.
    snprintf(buf, 20, "%g", d);

    return c4m_new(c4m_tspec_utf8(), c4m_kw("cstring", c4m_ka(buf)));
}

void *
float_coerce_to(const double d, c4m_type_t *target_type)
{
    int64_t i;

    switch (c4m_tspec_get_data_type_info(target_type)->typeid) {
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

const c4m_vtable_t c4m_u8_type = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        NULL, // You have to get it through a reference or mixed.
        (c4m_vtable_entry)u8_repr,
        NULL, // finalizer
        NULL, // Not used for ints.
        NULL, // Not used for ints.
        (c4m_vtable_entry)any_number_can_coerce_to,
        (c4m_vtable_entry)any_int_coerce_to,
        (c4m_vtable_entry)u8_parse,
        NULL, // The rest are not implemented for value types.
    },
};

const c4m_vtable_t c4m_i8_type = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        NULL, // You have to get it through a reference or mixed.
        (c4m_vtable_entry)i8_repr,
        NULL, // finalizer
        NULL, // Not used for ints.
        NULL, // Not used for ints.
        (c4m_vtable_entry)any_number_can_coerce_to,
        (c4m_vtable_entry)any_int_coerce_to,
        (c4m_vtable_entry)i8_parse,
        NULL, // The rest are not implemented for value types.
    },
};

const c4m_vtable_t c4m_u32_type = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        NULL, // You have to get it through a reference or mixed.
        (c4m_vtable_entry)u32_repr,
        NULL, // finalizer
        NULL, // Not used for ints.
        NULL, // Not used for ints.
        (c4m_vtable_entry)any_number_can_coerce_to,
        (c4m_vtable_entry)any_int_coerce_to,
        (c4m_vtable_entry)u32_parse,
        NULL, // The rest are not implemented for value types.
    },
};

const c4m_vtable_t c4m_i32_type = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        NULL, // You have to get it through a reference or mixed.
        (c4m_vtable_entry)i32_repr,
        NULL, // finalizer
        NULL, // Not used for ints.
        NULL, // Not used for ints.
        (c4m_vtable_entry)any_number_can_coerce_to,
        (c4m_vtable_entry)any_int_coerce_to,
        (c4m_vtable_entry)i32_parse,
        NULL, // The rest are not implemented for value types.
    },
};

const c4m_vtable_t c4m_u64_type = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        NULL, // You have to get it through a reference or mixed.
        (c4m_vtable_entry)u64_repr,
        NULL, // finalizer
        NULL, // Not used for ints.
        NULL, // Not used for ints.
        (c4m_vtable_entry)any_number_can_coerce_to,
        (c4m_vtable_entry)any_int_coerce_to,
        (c4m_vtable_entry)u64_parse,
        NULL, // The rest are not implemented for value types.
    },
};

const c4m_vtable_t c4m_i64_type = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        NULL, // You have to get it through a reference or mixed.
        (c4m_vtable_entry)i64_repr,
        NULL, // finalizer
        NULL, // Not used for ints.
        NULL, // Not used for ints.
        (c4m_vtable_entry)any_number_can_coerce_to,
        (c4m_vtable_entry)any_int_coerce_to,
        (c4m_vtable_entry)i64_parse,
        NULL, // The rest are not implemented for value types.
    },
};

const c4m_vtable_t c4m_bool_type = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        NULL, // You have to get it through a reference or mixed.
        (c4m_vtable_entry)bool_repr,
        NULL, // finalizer
        NULL, // Not used for ints.
        NULL, // Not used for ints.
        (c4m_vtable_entry)any_number_can_coerce_to,
        (c4m_vtable_entry)bool_coerce_to,
        (c4m_vtable_entry)bool_parse,
        NULL, // The rest are not implemented for value types.
    },
};

const c4m_vtable_t c4m_float_type = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        NULL, // You have to get it through a reference or mixed.
        (c4m_vtable_entry)float_repr,
        NULL, // finalizer
        NULL, // Not used for ints.
        NULL, // Not used for ints.
        (c4m_vtable_entry)any_number_can_coerce_to,
        (c4m_vtable_entry)float_coerce_to,
        (c4m_vtable_entry)f64_parse,
        NULL, // The rest are not implemented for value types.
    },
};

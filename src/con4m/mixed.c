#include "con4m.h"

static const char *err1 =
    "Cannot set mixed object contents with a "
    "concrete type.";
static const char *err2 =
    "Expected type does not match actual type when "
    "extracting the value from a mixed-type object";

void
c4m_mixed_set_value(mixed_t *m, type_spec_t *type, void **ptr)
{
    if (type == NULL) {
        if (ptr != NULL) {
            C4M_CRAISE((char *)err1);
        }

        m->held_type  = NULL;
        m->held_value = NULL;
        return;
    }

    if (!type_spec_is_concrete(type)) {
        C4M_CRAISE((char *)err1);
    }

    m->held_type = type;

    switch (tspec_get_data_type_info(type)->typeid) {
    case T_BOOL: {
        bool   *p = (bool *)ptr;
        bool    b = *p;
        int64_t n = (int64_t)b;

        m->held_value = (void *)n;
        return;
    }
    case T_I8: {
        char   *p = (char *)ptr;
        char    c = *p;
        int64_t n = (int64_t)c;

        m->held_value = (void *)n;
        return;
    }
    case T_BYTE: {
        uint8_t *p = (uint8_t *)ptr;
        uint8_t  c = *p;
        int64_t  n = (int64_t)c;

        m->held_value = (void *)n;
        return;
    }
    case T_I32: {
        int32_t *p = (int32_t *)ptr;
        int32_t  v = *p;
        int64_t  n = (int64_t)v;

        m->held_value = (void *)n;
        return;
    }
    case T_CHAR:
    case T_U32: {
        uint32_t *p = (uint32_t *)ptr;
        uint32_t  v = *p;
        int64_t   n = (int64_t)v;

        m->held_value = (void *)n;
    }
    case T_INT:
    case T_UINT:
        m->held_value = *ptr;
        return;
    case T_F32: {
        float *p      = (float *)ptr;
        float  f      = *p;
        double d      = (double)f;
        m->held_value = c4m_double_to_ptr(d);
        return;
    }
    case T_F64: {
        double *p     = (double *)ptr;
        double  d     = *p;
        m->held_value = c4m_double_to_ptr(d);
        return;
    }
    default:
        m->held_value = *(void **)ptr;
        return;
    }
}

static void
mixed_init(mixed_t *m, va_list args)
{
    type_spec_t *type = NULL;
    void        *ptr  = NULL;

    karg_va_init(args);

    kw_ptr("type", type);
    kw_ptr("value", ptr);

    c4m_mixed_set_value(m, type, ptr);
}

void
c4m_unbox_mixed(mixed_t *m, type_spec_t *expected_type, void **ptr)
{
    if (!tspecs_are_compat(m->held_type, expected_type)) {
        C4M_CRAISE((char *)err2);
    }

    switch (tspec_get_data_type_info(m->held_type)->typeid) {
    case T_BOOL: {
        if (m->held_value) {
            *(bool *)ptr = true;
        }
        else {
            *(bool *)ptr = false;
        }
        return;
    }
    case T_I8: {
        int64_t n    = (int64_t)m->held_value;
        char    c    = n & 0xff;
        *(char *)ptr = c;
        return;
    }
    case T_BYTE: {
        int64_t n       = (int64_t)m->held_value;
        uint8_t c       = n & 0xff;
        *(uint8_t *)ptr = c;
        return;
    }
    case T_I32: {
        int64_t n       = (int64_t)m->held_value;
        int32_t v       = n & 0xffffffff;
        *(int32_t *)ptr = v;
        return;
    }
    case T_CHAR:
    case T_U32: {
        int64_t  n       = (int64_t)m->held_value;
        uint32_t v       = n & 0xffffffff;
        *(uint32_t *)ptr = v;
        return;
    }
    case T_INT:
        *(int64_t *)ptr = (int64_t)m->held_value;
        return;
    case T_UINT:
        *(uint64_t *)ptr = (uint64_t)m->held_value;
        return;
    case T_F32: {
        double d = c4m_ptr_to_double(m->held_value);

        *(float *)ptr = (float)d;
        return;
    }
    case T_F64: {
        *(double *)ptr = c4m_ptr_to_double(m->held_value);
        return;
    }
    default:
        *(void **)m->held_value = m->held_value;
        return;
    }
}

static int64_t
mixed_as_word(mixed_t *m)
{
    switch (tspec_get_data_type_info(m->held_type)->typeid) {
    case T_BOOL:
        if (m->held_value == NULL) {
            return 0;
        }
        else {
            return 1;
        }
        break;
    case T_I8: {
        char b;

        c4m_unbox_mixed(m, m->held_type, (void **)&b);
        return (int64_t)b;
    }
    case T_BYTE: {
        uint8_t b;

        c4m_unbox_mixed(m, m->held_type, (void **)&b);
        return (int64_t)b;
    }
    case T_I32: {
        int32_t n;

        c4m_unbox_mixed(m, m->held_type, (void **)&n);
        return (int64_t)n;
    }
    case T_CHAR:
    case T_U32: {
        uint32_t n;
        c4m_unbox_mixed(m, m->held_type, (void **)&n);
        return (int64_t)n;
    }
    default:
        return (int64_t)m->held_value;
    }
}

static any_str_t *
mixed_repr(mixed_t *mixed, to_str_use_t how)
{
    // For the value types, we need to convert them to a 64-bit equiv
    // to send to the appropriate repr.
    return c4m_repr((void *)mixed_as_word(mixed), mixed->held_type, how);
}

static void
mixed_marshal_arts(mixed_t *m, stream_t *s, dict_t *memos, int64_t *mid)
{
    c4m_sub_marshal(m->held_type, s, memos, mid);

    if (tspec_get_data_type_info(m->held_type)->by_value) {
        marshal_i64((int64_t)m->held_value, s);
    }
    else {
        c4m_sub_marshal(m->held_value, s, memos, mid);
    }
}

static void
mixed_unmarshal_arts(mixed_t *m, stream_t *s, dict_t *memos)
{
    m->held_type = c4m_sub_unmarshal(s, memos);

    if (tspec_get_data_type_info(m->held_type)->by_value) {
        m->held_value = (void *)unmarshal_i64(s);
    }
    else {
        m->held_value = c4m_sub_unmarshal(s, memos);
    }
}

static mixed_t *
mixed_copy(mixed_t *m)
{
    mixed_t *result = c4m_new(tspec_mixed());

    // Types are concrete whenever there is a value, so we don't need to
    // call copy, but we do it anyway.

    result->held_type = global_copy(m->held_type);

    if (tspec_get_data_type_info(m->held_type)->by_value) {
        result->held_value = m->held_value;
    }
    else {
        result->held_value = c4m_copy_object(m->held_value);
    }

    return result;
}

const c4m_vtable mixed_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        (c4m_vtable_entry)mixed_init,
        (c4m_vtable_entry)mixed_repr,
        NULL, // finalizer
        (c4m_vtable_entry)mixed_marshal_arts,
        (c4m_vtable_entry)mixed_unmarshal_arts,
        NULL, // Mixed is not directly coercible statically.
        NULL, // NO! Call unbox_mixed() at runtime.
        NULL, // From lit,
        (c4m_vtable_entry)mixed_copy,
        NULL, // Nothing else supported w/o unboxing.
    },
};

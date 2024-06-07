#pragma once

#include "con4m.h"

static inline c4m_obj_t
c4m_box_obj(c4m_box_t item, c4m_type_t *type)
{
    c4m_box_t *result = c4m_new(c4m_global_resolve_type(type));

    switch (c4m_get_alloc_len(type)) {
    case 1:
        result->u8 = item.u8;
        break;
    case 2:
        result->u16 = item.u16;
        break;
    case 4:
        result->u32 = item.u32;
        break;
    case 8:
        result->u64 = item.u64;
        break;
    default:
        abort();
    }

    return result;
}

static inline c4m_box_t
c4m_unbox_obj(c4m_box_t *box)
{
    c4m_box_t result = {
        .u64 = 0,
    };

    switch (c4m_get_alloc_len(c4m_get_my_type(box))) {
    case 1:
        result.u8 = box->u8;
        break;
    case 2:
        result.u16 = box->u16;
        break;
    case 4:
        result.u32 = box->u32;
        break;
    case 8:
        result.u64 = box->u64;
        break;
    default:
        abort();
    }

    return result;
}

static inline uint64_t
c4m_unbox(c4m_obj_t value)
{
    return c4m_unbox_obj(value).u64;
}

static inline bool *
c4m_box_bool(bool val)
{
    c4m_box_t v = {
        .b = val,
    };
    return c4m_box_obj(v, c4m_tspec_bool());
}

static inline int8_t *
c4m_box_i8(int8_t val)
{
    c4m_box_t v = {
        .i8 = val,
    };
    return c4m_box_obj(v, c4m_tspec_i8());
}

static inline uint8_t *
c4m_box_u8(uint8_t val)
{
    c4m_box_t v = {
        .u8 = val,
    };
    return c4m_box_obj(v, c4m_tspec_u8());
}

static inline int32_t *
c4m_box_i32(int32_t val)
{
    c4m_box_t v = {
        .i32 = val,
    };
    return c4m_box_obj(v, c4m_tspec_i32());
}

static inline uint32_t *
c4m_box_u32(uint32_t val)
{
    c4m_box_t v = {
        .u32 = val,
    };
    return c4m_box_obj(v, c4m_tspec_u32());
}

static inline int64_t *
c4m_box_i64(int64_t val)
{
    c4m_box_t v = {
        .i64 = val,
    };
    return c4m_box_obj(v, c4m_tspec_i64());
}

static inline uint64_t *
c4m_box_u64(uint64_t val)
{
    c4m_box_t v = {
        .u64 = val,
    };
    return c4m_box_obj(v, c4m_tspec_u64());
}

static inline double *
c4m_box_double(double val)
{
    c4m_box_t v = {
        .dbl = val,
    };
    return c4m_box_obj(v, c4m_tspec_f64());
}

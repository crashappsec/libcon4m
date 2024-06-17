#pragma once

#include "con4m.h"

static inline c4m_obj_t
c4m_box_obj(c4m_box_t value, c4m_type_t *type)
{
    return c4m_new(c4m_tspec_box(type), value);
}

// Safely dereference a boxed item, thus removing the box.
// Since we're internally reserving 64 bits for values, we
// return it as a 64 bit item.
//
// However, the allocated item allocated the actual item's size, so we
// have to make sure to get it right on both ends; we can't just
// dereference a uint64_t, for instance.
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
    default:
        result.u64 = box->u64;
        break;
    }

    return result;
}

// This just drops the union, which is not needed after the above.
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

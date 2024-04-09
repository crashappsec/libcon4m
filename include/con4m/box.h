#include "con4m.h"

static inline i64_box *
c4m_box_i64(int64_t n)
{
    int64_t *result = con4m_new(tspec_i64());
    *result         = n;

    return result;
}

static inline int64_t
c4m_unbox_i64(i64_box *b)
{
    return *b;
}

static inline u64_box *
c4m_box_u64(uint64_t n)
{
    uint64_t *result = con4m_new(tspec_u64());
    *result          = n;

    return result;
}

static inline uint64_t
c4m_unbox_u64(u64_box *b)
{
    return *b;
}

static inline i32_box *
c4m_box_i32(int32_t n)
{
    int32_t *result = con4m_new(tspec_i32());
    *result         = n;

    return result;
}

static inline int32_t
c4m_unbox_i32(i32_box *b)
{
    return *b;
}

static inline u32_box *
c4m_box_u32(uint32_t n)
{
    uint32_t *result = con4m_new(tspec_u32());
    *result          = n;

    return result;
}

static inline uint32_t
c4m_unbox_u32(u32_box *b)
{
    return *b;
}

#if 0 // I somehow have missed u16

static inline i16_box *
c4m_box_i16(int16_t n)
{
    int16_t *result = con4m_new(tspec_i16());
    *result = n;

    return result;
}

static inline int16_t
c4m_unbox_i16(i16_box *b)
{
    return *b;
}

static inline u16_box *
c4m_box_u16(uint16_t n)
{
    uint16_t *result = con4m_new(tspec_u16());
    *result = n;

    return result;
}

static inline uint16_t
c4m_unbox_u16(u16_box *b)
{
    return *b;
}

#endif

static inline i8_box *
c4m_box_i8(int8_t n)
{
    int8_t *result = con4m_new(tspec_i8());
    *result        = n;

    return result;
}

static inline int8_t
c4m_unbox_i8(i8_box *b)
{
    return *b;
}

static inline u8_box *
c4m_box_u8(uint8_t n)
{
    uint8_t *result = con4m_new(tspec_u8());
    *result         = n;

    return result;
}

static inline uint8_t
c4m_unbox_u8(u8_box *b)
{
    return *b;
}

static inline double_box *
c4m_box_double(double d)
{
    double *result = con4m_new(tspec_f64());
    *result        = d;

    return result;
}

static inline double
c4m_unbox_double(double_box *b)
{
    return *b;
}

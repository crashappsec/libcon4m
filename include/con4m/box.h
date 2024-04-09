#include <con4m.h>

static inline i64_box *
box_i64(int64_t n)
{
    int64_t *result = con4m_new(tspec_i64());
    *result         = n;

    return result;
}

static inline int64_t
unbox_i64(i64_box *b)
{
    return *b;
}

static inline u64_box *
box_u64(uint64_t n)
{
    uint64_t *result = con4m_new(tspec_u64());
    *result          = n;

    return result;
}

static inline uint64_t
unbox_u64(u64_box *b)
{
    return *b;
}

static inline i32_box *
box_i32(int32_t n)
{
    int32_t *result = con4m_new(tspec_i32());
    *result         = n;

    return result;
}

static inline int32_t
unbox_i32(i32_box *b)
{
    return *b;
}

static inline u32_box *
box_u32(uint32_t n)
{
    uint32_t *result = con4m_new(tspec_u32());
    *result          = n;

    return result;
}

static inline uint32_t
unbox_u32(u32_box *b)
{
    return *b;
}

#if 0 // I somehow have missed u16

static inline i16_box *
box_i16(int16_t n)
{
    int16_t *result = con4m_new(tspec_i16());
    *result = n;

    return result;
}

static inline int16_t
unbox_i16(i16_box *b)
{
    return *b;
}

static inline u16_box *
box_u16(uint16_t n)
{
    uint16_t *result = con4m_new(tspec_u16());
    *result = n;

    return result;
}

static inline uint16_t
unbox_u16(u16_box *b)
{
    return *b;
}

#endif

static inline i8_box *
box_i8(int8_t n)
{
    int8_t *result = con4m_new(tspec_i8());
    *result        = n;

    return result;
}

static inline int8_t
unbox_i8(i8_box *b)
{
    return *b;
}

static inline u8_box *
box_u8(uint8_t n)
{
    uint8_t *result = con4m_new(tspec_u8());
    *result         = n;

    return result;
}

static inline uint8_t
unbox_u8(u8_box *b)
{
    return *b;
}

static inline double_box *
box_double(double d)
{
    double *result = con4m_new(tspec_f64());
    *result        = d;

    return result;
}

static inline double
unbox_double(double_box *b)
{
    return *b;
}

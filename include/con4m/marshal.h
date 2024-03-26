#pragma once

void    marshal_cstring(char *, stream_t *);
char   *unmarshal_cstring(stream_t *);
void    marshal_i64(int64_t, stream_t *);
int64_t unmarshal_i64(stream_t *);
void    marshal_i32(int32_t, stream_t *);
int32_t unmarshal_i32(stream_t *);
void    marshal_i16(int16_t, stream_t *);
int16_t unmarshal_i16(stream_t *);

void     con4m_sub_marshal(object_t, stream_t *, struct dict_t *, int64_t *);
object_t con4m_sub_unmarshal(stream_t *, struct dict_t *);
void     con4m_marshal(object_t, stream_t *);
object_t con4m_unmarshal(stream_t *);

static inline void
marshal_i8(int8_t c, stream_t *s)
{
    stream_raw_write(s, 1, (char *)&c);
}

static inline int8_t
unmarshal_i8(stream_t *s)
{
    int8_t ret;

    stream_raw_read(s, 1, (char *)&ret);

    return ret;
}

static inline void
marshal_bool(int value, stream_t *s)
{
    marshal_i8(value ? 1 : 0, s);
}

static inline bool
unmarshal_bool(stream_t *s)
{
    return (bool)unmarshal_i8(s);
}

static inline void
marshal_u64(uint64_t n, stream_t *s)
{
    marshal_i64((int64_t)n, s);
}

static inline uint64_t
unmarshal_u64(stream_t *s)
{
    return (uint64_t)unmarshal_i64(s);
}

static inline void
marshal_u32(uint32_t n, stream_t *s)
{
    marshal_i32((int32_t)n, s);
}

static inline uint32_t
unmarshal_u32(stream_t *s)
{
    return (uint32_t)unmarshal_i32(s);
}

static inline void
marshal_u16(uint16_t n, stream_t *s)
{
    marshal_i16((int16_t)n, s);
}

static inline uint16_t
unmarshal_u16(stream_t *s)
{
    return (uint16_t)unmarshal_i16(s);
}

static inline void
marshal_u8(uint8_t n, stream_t *s)
{
    marshal_i8((int8_t)n, s);
}

static inline uint8_t
unmarshal_u8(stream_t *s)
{
    return (uint8_t)unmarshal_i64(s);
}

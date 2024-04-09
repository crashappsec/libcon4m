#pragma once

#include "con4m.h"

extern void    marshal_cstring(char *, stream_t *);
extern char   *unmarshal_cstring(stream_t *);
extern void    marshal_i64(int64_t, stream_t *);
extern int64_t unmarshal_i64(stream_t *);
extern void    marshal_i32(int32_t, stream_t *);
extern int32_t unmarshal_i32(stream_t *);
extern void    marshal_i16(int16_t, stream_t *);
extern int16_t unmarshal_i16(stream_t *);

extern void     c4m_sub_marshal(object_t,
                                stream_t *,
                                struct dict_t *,
                                int64_t *);
extern object_t c4m_sub_unmarshal(stream_t *, struct dict_t *);
extern void     c4m_marshal(object_t, stream_t *);
extern object_t c4m_unmarshal(stream_t *);
extern void     marshal_unmanaged_object(void *,
                                         stream_t *,
                                         struct dict_t *,
                                         int64_t *,
                                         marshal_fn);
extern void    *unmarshal_unmanaged_object(size_t,
                                           stream_t *,
                                           struct dict_t *,
                                           unmarshal_fn);
extern void     dump_c_static_instance_code(object_t, char *, utf8_t *);
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
marshal_bool(bool value, stream_t *s)
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

static inline buffer_t *
c4m_marshal_to_buf(object_t obj)
{
    buffer_t *b = c4m_new(tspec_buffer(), kw("length", ka(16)));
    stream_t *s = c4m_new(tspec_stream(), kw("buffer", ka(b), "write", ka(1), "read", ka(0)));

    c4m_marshal(obj, s);
    stream_close(s);

    return b;
}

static inline object_t
c4m_mem_unmarshal(char *mem, int64_t len)
{
    buffer_t *b = c4m_new(tspec_buffer(), kw("length", ka(len), "ptr", mem));
    stream_t *s = c4m_new(tspec_stream(), kw("buffer", ka(b)));

    object_t result = c4m_unmarshal(s);

    stream_close(s);
    return result;
}

static inline struct dict_t *
alloc_marshal_memos()
{
    return c4m_new(tspec_dict(tspec_ref(), tspec_u64()));
}

static inline struct dict_t *
alloc_unmarshal_memos()
{
    return c4m_new(tspec_dict(tspec_u64(), tspec_ref()));
}

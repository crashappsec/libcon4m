#pragma once

#include "con4m.h"

extern void    c4m_marshal_cstring(char *, c4m_stream_t *);
extern char   *c4m_unmarshal_cstring(c4m_stream_t *);
extern void    c4m_marshal_i64(int64_t, c4m_stream_t *);
extern int64_t c4m_unmarshal_i64(c4m_stream_t *);
extern void    c4m_marshal_i32(int32_t, c4m_stream_t *);
extern int32_t c4m_unmarshal_i32(c4m_stream_t *);
extern void    c4m_marshal_i16(int16_t, c4m_stream_t *);
extern int16_t c4m_unmarshal_i16(c4m_stream_t *);

extern void      c4m_sub_marshal(c4m_obj_t,
                                 c4m_stream_t *,
                                 c4m_dict_t *,
                                 int64_t *);
extern c4m_obj_t c4m_sub_unmarshal(c4m_stream_t *, c4m_dict_t *);
extern void      c4m_marshal(c4m_obj_t, c4m_stream_t *);
extern c4m_obj_t c4m_unmarshal(c4m_stream_t *);
extern void      c4m_marshal_unmanaged_object(void *,
                                              c4m_stream_t *,
                                              c4m_dict_t *,
                                              int64_t *,
                                              c4m_marshal_fn);
extern void     *c4m_unmarshal_unmanaged_object(size_t,
                                                c4m_stream_t *,
                                                c4m_dict_t *,
                                                c4m_unmarshal_fn);
extern void      c4m_dump_c_static_instance_code(c4m_obj_t,
                                                 char *,
                                                 c4m_utf8_t *);
static inline void
c4m_marshal_i8(int8_t c, c4m_stream_t *s)
{
    c4m_stream_raw_write(s, 1, (char *)&c);
}

static inline int8_t
c4m_unmarshal_i8(c4m_stream_t *s)
{
    int8_t ret;

    c4m_stream_raw_read(s, 1, (char *)&ret);

    return ret;
}

static inline void
c4m_marshal_u8(uint8_t n, c4m_stream_t *s)
{
    c4m_marshal_i8((int8_t)n, s);
}

static inline uint8_t
c4m_unmarshal_u8(c4m_stream_t *s)
{
    return (uint8_t)c4m_unmarshal_i8(s);
}

static inline void
c4m_marshal_bool(bool value, c4m_stream_t *s)
{
    c4m_marshal_i8(value ? 1 : 0, s);
}

static inline bool
c4m_unmarshal_bool(c4m_stream_t *s)
{
    return (bool)c4m_unmarshal_i8(s);
}

static inline void
c4m_marshal_u64(uint64_t n, c4m_stream_t *s)
{
    c4m_marshal_i64((int64_t)n, s);
}

static inline uint64_t
c4m_unmarshal_u64(c4m_stream_t *s)
{
    return (uint64_t)c4m_unmarshal_i64(s);
}

static inline void
c4m_marshal_u32(uint32_t n, c4m_stream_t *s)
{
    c4m_marshal_i32((int32_t)n, s);
}

static inline uint32_t
c4m_unmarshal_u32(c4m_stream_t *s)
{
    return (uint32_t)c4m_unmarshal_i32(s);
}

static inline void
c4m_marshal_u16(uint16_t n, c4m_stream_t *s)
{
    c4m_marshal_i16((int16_t)n, s);
}

static inline uint16_t
c4m_unmarshal_u16(c4m_stream_t *s)
{
    return (uint16_t)c4m_unmarshal_i16(s);
}

static inline c4m_buf_t *
c4m_marshal_to_buf(c4m_obj_t obj)
{
    c4m_buf_t    *b = c4m_new(c4m_tspec_buffer(),
                           c4m_kw("length", c4m_ka(16)));
    c4m_stream_t *s = c4m_new(c4m_tspec_stream(),
                              c4m_kw("buffer",
                                     c4m_ka(b),
                                     "write",
                                     c4m_ka(1),
                                     "read",
                                     c4m_ka(0)));

    c4m_marshal(obj, s);
    c4m_stream_close(s);

    return b;
}

static inline c4m_obj_t
c4m_mem_unmarshal(char *mem, int64_t len)
{
    c4m_buf_t    *b = c4m_new(c4m_tspec_buffer(),
                           c4m_kw("length",
                                  c4m_ka(len),
                                  "ptr",
                                  mem));
    c4m_stream_t *s = c4m_new(c4m_tspec_stream(), c4m_kw("buffer", c4m_ka(b)));

    c4m_obj_t result = c4m_unmarshal(s);

    c4m_stream_close(s);
    return result;
}

static inline c4m_dict_t *
c4m_alloc_marshal_memos()
{
    return c4m_new(c4m_tspec_dict(c4m_tspec_ref(), c4m_tspec_u64()));
}

static inline c4m_dict_t *
c4m_alloc_unmarshal_memos()
{
    return c4m_new(c4m_tspec_dict(c4m_tspec_u64(), c4m_tspec_ref()));
}

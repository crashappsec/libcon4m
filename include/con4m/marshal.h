#pragma once

void    marshal_cstring(char *, FILE *);
char   *unmarshal_cstring(FILE *);
void    marshal_i64(int64_t, FILE *);
int64_t unmarshal_i64(FILE *);
void    marshal_i32(int32_t, FILE *);
int32_t unmarshal_i32(FILE *);
void    marshal_i16(int16_t, FILE *);
int16_t unmarshal_i16(FILE *);

void     con4m_sub_marshal(object_t , FILE *, struct dict_t *, int64_t *);
object_t con4m_sub_unmarshal(FILE *, struct dict_t *);
void     con4m_marshal(object_t, FILE *);
object_t con4m_unmarshal(FILE *);

static inline void
marshal_i8(int8_t c, FILE *f)
{
    fwrite(&c, 1, 1, f);
}

static inline int8_t
unmarshal_i8(FILE *f)
{
    int8_t ret;

    fread(&ret, 1, 1, f);

    return ret;
}

static inline void
marshal_bool(int value, FILE *f)
{
    marshal_i8(value ? 1 : 0, f);
}

static inline bool
unmarshal_bool(FILE *f)
{
    return (bool)unmarshal_i8(f);
}

static inline void
marshal_u64(uint64_t n, FILE *s)
{
    marshal_i64((int64_t)n, s);
}

static inline uint64_t
unmarshal_u64(FILE *s)
{
    return (uint64_t)unmarshal_i64(s);
}

static inline void
marshal_u32(uint32_t n, FILE *s)
{
    marshal_i32((int32_t)n, s);
}

static inline uint32_t
unmarshal_u32(FILE *s)
{
    return (uint32_t)unmarshal_i32(s);
}

static inline void
marshal_u16(uint16_t n, FILE *s)
{
    marshal_i16((int16_t)n, s);
}

static inline uint16_t
unmarshal_u16(FILE *s)
{
    return (uint16_t)unmarshal_i16(s);
}

static inline void
marshal_u8(uint8_t n, FILE *s)
{
    marshal_i8((int8_t)n, s);
}

static inline uint8_t
unmarshal_u8(FILE *s)
{
    return (uint8_t)unmarshal_i64(s);
}

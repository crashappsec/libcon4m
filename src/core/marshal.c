#include "con4m.h"

void
c4m_marshal_i64(int64_t i, c4m_stream_t *s)
{
    little_64(i);

    c4m_stream_raw_write(s, sizeof(int64_t), (char *)&i);
}

int64_t
c4m_unmarshal_i64(c4m_stream_t *s)
{
    int64_t result;

    c4m_stream_raw_read(s, sizeof(int64_t), (char *)&result);
    little_64(result);

    return result;
}

void
c4m_marshal_i32(int32_t i, c4m_stream_t *s)
{
    little_32(i);

    c4m_stream_raw_write(s, sizeof(int32_t), (char *)&i);
}

int32_t
c4m_unmarshal_i32(c4m_stream_t *s)
{
    int32_t result;

    c4m_stream_raw_read(s, sizeof(int32_t), (char *)&result);
    little_32(result);

    return result;
}

void
c4m_marshal_i16(int16_t i, c4m_stream_t *s)
{
    little_16(i);
    c4m_stream_raw_write(s, sizeof(int16_t), (char *)&i);
}

int16_t
c4m_unmarshal_i16(c4m_stream_t *s)
{
    int16_t result;

    c4m_stream_raw_read(s, sizeof(int16_t), (char *)&result);
    little_16(result);
    return result;
}

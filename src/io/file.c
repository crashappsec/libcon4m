#include "con4m.h"

c4m_utf8_t *
c4m_read_utf8_file(c4m_str_t *path)
{
    c4m_utf8_t   *result  = NULL;
    c4m_stream_t *stream  = NULL;
    bool          success = false;

    C4M_TRY
    {
        stream  = c4m_file_instream(c4m_to_utf8(path), C4M_T_UTF8);
        result  = (c4m_utf8_t *)c4m_stream_read_all(stream);
        success = true;
        c4m_stream_close(stream);
    }
    C4M_EXCEPT
    {
        if (stream != NULL) {
            c4m_stream_close(stream);
        }
    }
    C4M_TRY_END;

    // Don't return partial if we had a weird failure.
    if (success) {
        return result;
    }
    return NULL;
}

c4m_buf_t *
c4m_read_binary_file(c4m_str_t *path)
{
    c4m_buf_t    *result  = NULL;
    c4m_stream_t *stream  = NULL;
    bool          success = false;

    C4M_TRY
    {
        stream  = c4m_file_instream(c4m_to_utf8(path), C4M_T_BUFFER);
        result  = (c4m_buf_t *)c4m_stream_read_all(stream);
        success = true;
        c4m_stream_close(stream);
    }
    C4M_EXCEPT
    {
        if (stream != NULL) {
            c4m_stream_close(stream);
        }
    }
    C4M_TRY_END;

    // Don't return partial if we had a weird failure.
    if (success) {
        return result;
    }
    return NULL;
}

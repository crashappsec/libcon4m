#pragma once
#include <con4m.h>

object_t stream_raw_read(stream_t *, int64_t, char *);
size_t   stream_raw_write(stream_t *, int64_t, char *);
int64_t  _stream_write_object(stream_t *, object_t, ...);
bool     stream_at_eof(stream_t *);
int64_t  stream_get_location(stream_t *);
void     stream_set_location(stream_t *, int64_t);
void     stream_close(stream_t *);
void     stream_flush(stream_t *);

#define stream_write_object(s, o, ...) _stream_write_object(s, o,              \
                                                           KFUNC(__VA_ARGS__))


static inline object_t
stream_read(stream_t *stream, int64_t len)
{
    return stream_raw_read(stream, len, NULL);
}

// For nim integration.
static inline stream_t *
string_instream(any_str_t *instring)
{
    return con4m_new(tspec_stream(), "instring", instring);
}

static inline stream_t *
buffer_instream(buffer_t *inbuf)
{
    return con4m_new(tspec_stream(), "buffer", inbuf, "read", 1);
}

static inline stream_t *
buffer_outstream(buffer_t *outbuf)
{
    return con4m_new(tspec_stream(), "buffer", outbuf, "read", 0,
		     "write", 1);
}

static inline stream_t *
buffer_iostream(buffer_t *buf)
{
    return con4m_new(tspec_stream(), "buffer", buf, "read", 1,
		     "write", 1);
}

static inline stream_t *
file_instream(any_str_t *filename, con4m_builtin_t output_type)
{
    return con4m_new(tspec_stream(),
		     "filename", filename,
		     "out_type", output_type);
}

static inline stream_t *
file_outstream(any_str_t *filename, int can_create, int append)
{
    return con4m_new(tspec_stream(),
		     "filename", filename,
		     "read", 0,
		     "append", append,
		     "write", 1,
		     "can_create", can_create);
}


static inline stream_t *
file_iostream(any_str_t *filename, int can_create)
{
    return con4m_new(tspec_stream(),
		     "filename", filename,
		     "read", 0,
		     "write", 1,
		     "can_create", can_create);
}

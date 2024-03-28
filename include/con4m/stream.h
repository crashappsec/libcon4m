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
static inline bool
stream_putc(stream_t *s, char c)
{
    return stream_raw_write(s, 1, &c) == 1;
}

static inline int
stream_puts(stream_t *s, char *c)
{
    return stream_raw_write(s, strlen(c), c);
}

static inline object_t
stream_read(stream_t *stream, int64_t len)
{
    return stream_raw_read(stream, len, NULL);
}

static inline void
stream_puti(stream_t *s, int64_t n)
{
    if (!n) {
	stream_putc(s, '0');
	return;
    }
    if (n < 0) {
	stream_putc(s, '-');
	n *= -1;
    }
    char  buf[21] = {0,};
    char *p       = buf + 20;

    while (n != 0) {
	*--p = (n % 10) + '0';
	n /= 10;
    }

    stream_puts(s, p);
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

static inline stream_t *
get_stdin()
{
    return con4m_new(tspec_stream(), "fd", 0);
}

static inline stream_t *
get_stdout()
{
    return con4m_new(tspec_stream(), "fd", 1, "read", 0, "write", 1);
}

static inline stream_t *
get_stderr()
{
    return con4m_new(tspec_stream(), "fd", 2, "read", 0, "write", 1);
}

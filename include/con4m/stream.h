#pragma once
#include "con4m.h"

object_t stream_raw_read(stream_t *, int64_t, char *);
size_t   stream_raw_write(stream_t *, int64_t, char *);
void     _stream_write_object(stream_t *, object_t, bool);
bool     stream_at_eof(stream_t *);
int64_t  stream_get_location(stream_t *);
void     stream_set_location(stream_t *, int64_t);
void     stream_close(stream_t *);
void     stream_flush(stream_t *);
void     _print(object_t, ...);

#define stream_write_object(s, o, ...) _stream_write_object(s, o, IF(ISEMPTY(__VA_ARGS__))(false) __VA_ARGS__)

#define print(s, ...) _print(s, KFUNC(__VA_ARGS__))

static inline bool
stream_putc(stream_t *s, char c)
{
    return stream_raw_write(s, 1, &c) == 1;
}

static inline bool
stream_putcp(stream_t *s, codepoint_t cp)
{
    uint8_t utf8[5];

    size_t n = utf8proc_encode_char(cp, utf8);
    utf8[n]  = 0;

    return stream_raw_write(s, n, (char *)utf8) == n;
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
    char buf[21] = {
        0,
    };
    char *p = buf + 20;

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
    return con4m_new(tspec_stream(), kw("instring", ka(instring)));
}

static inline stream_t *
buffer_instream(buffer_t *inbuf)
{
    return con4m_new(tspec_stream(), kw("buffer", ka(inbuf), "read", ka(true)));
}

static inline stream_t *
buffer_outstream(buffer_t *outbuf)
{
    return con4m_new(tspec_stream(), kw("buffer", ka(outbuf), "read", ka(false), "write", ka(true)));
}

static inline stream_t *
buffer_iostream(buffer_t *buf)
{
    return con4m_new(tspec_stream(), kw("buffer", ka(buf), "read", ka(true), "write", ka(true)));
}

static inline stream_t *
file_instream(any_str_t *filename, con4m_builtin_t output_type)
{
    return con4m_new(tspec_stream(),
                     kw("filename", ka(filename), "out_type", ka(output_type)));
}

static inline stream_t *
file_outstream(any_str_t *filename, bool no_create, bool append)
{
    return con4m_new(tspec_stream(),
                     kw("filename", ka(filename), "read", ka(false), "append", ka(append), "write", ka(true), "can_create", ka(no_create)));
}

static inline stream_t *
file_iostream(any_str_t *filename, bool no_create)
{
    return con4m_new(tspec_stream(),
                     kw("filename", ka(filename), "read", ka(false), "write", ka(true), "no_create", ka(no_create)));
}

static inline stream_t *
get_stdin()
{
    return con4m_new(tspec_stream(), kw("cstream", ka(stdin)));
}

static inline stream_t *
get_stdout()
{
    return con4m_new(tspec_stream(), kw("cstream", ka(stdout)));
}

static inline stream_t *
get_stderr()
{
    return con4m_new(tspec_stream(), kw("cstream", ka(stderr)));
}

static inline bool
stream_using_cookie(stream_t *s)
{
    return (bool)(s->flags & F_STREAM_USING_COOKIE);
}

static inline int
stream_fileno(stream_t *s)
{
    if (stream_using_cookie(s)) {
        return -1;
    }

    return fileno(s->contents.f);
}

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
    return c4m_new(tspec_stream(), c4m_kw("instring", c4m_ka(instring)));
}

static inline stream_t *
buffer_instream(buffer_t *inbuf)
{
    return c4m_new(tspec_stream(), c4m_kw("buffer", c4m_ka(inbuf), "read", c4m_ka(true)));
}

static inline stream_t *
buffer_outstream(buffer_t *outbuf)
{
    return c4m_new(tspec_stream(), c4m_kw("buffer", c4m_ka(outbuf), "read", c4m_ka(false), "write", c4m_ka(true)));
}

static inline stream_t *
buffer_iostream(buffer_t *buf)
{
    return c4m_new(tspec_stream(), c4m_kw("buffer", c4m_ka(buf), "read", c4m_ka(true), "write", c4m_ka(true)));
}

static inline stream_t *
file_instream(any_str_t *filename, c4m_builtin_t output_type)
{
    return c4m_new(tspec_stream(),
                   c4m_kw("filename", c4m_ka(filename), "out_type", c4m_ka(output_type)));
}

static inline stream_t *
file_outstream(any_str_t *filename, bool no_create, bool append)
{
    return c4m_new(tspec_stream(),
                   c4m_kw("filename",
                          c4m_ka(filename),
                          "read",
                          c4m_ka(false),
                          "append",
                          c4m_ka(append),
                          "write",
                          c4m_ka(true),
                          "can_create",
                          c4m_ka(no_create)));
}

static inline stream_t *
file_iostream(any_str_t *filename, bool no_create)
{
    return c4m_new(tspec_stream(),
                   c4m_kw("filename",
                          c4m_ka(filename),
                          "read",
                          c4m_ka(false),
                          "write",
                          c4m_ka(true),
                          "no_create",
                          c4m_ka(no_create)));
}

static inline stream_t *
get_stdin()
{
    return c4m_new(tspec_stream(), c4m_kw("cstream", c4m_ka(stdin)));
}

static inline stream_t *
get_stdout()
{
    return c4m_new(tspec_stream(), c4m_kw("cstream", c4m_ka(stdout)));
}

static inline stream_t *
get_stderr()
{
    return c4m_new(tspec_stream(), c4m_kw("cstream", c4m_ka(stderr)));
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

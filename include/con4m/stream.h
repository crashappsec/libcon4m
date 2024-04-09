#pragma once
#include "con4m.h"

object_t c4m_stream_raw_read(stream_t *, int64_t, char *);
size_t   c4m_stream_raw_write(stream_t *, int64_t, char *);
void     _c4m_stream_write_object(stream_t *, object_t, bool);
bool     c4m_stream_at_eof(stream_t *);
int64_t  c4m_stream_get_location(stream_t *);
void     c4m_stream_set_location(stream_t *, int64_t);
void     c4m_stream_close(stream_t *);
void     c4m_stream_flush(stream_t *);
void     _c4m_print(object_t, ...);

#define c4m_stream_write_object(s, o, ...) \
    _c4m_stream_write_object(s, o, IF(ISEMPTY(__VA_ARGS__))(false) __VA_ARGS__)

#define c4m_print(s, ...) _c4m_print(s, KFUNC(__VA_ARGS__))

static inline bool
c4m_stream_putc(stream_t *s, char c)
{
    return c4m_stream_raw_write(s, 1, &c) == 1;
}

static inline bool
c4m_stream_putcp(stream_t *s, codepoint_t cp)
{
    uint8_t utf8[5];

    size_t n = utf8proc_encode_char(cp, utf8);
    utf8[n]  = 0;

    return c4m_stream_raw_write(s, n, (char *)utf8) == n;
}

static inline int
c4m_stream_puts(stream_t *s, char *c)
{
    return c4m_stream_raw_write(s, strlen(c), c);
}

static inline object_t
c4m_stream_read(stream_t *stream, int64_t len)
{
    return c4m_stream_raw_read(stream, len, NULL);
}

static inline void
c4m_stream_puti(stream_t *s, int64_t n)
{
    if (!n) {
        c4m_stream_putc(s, '0');
        return;
    }
    if (n < 0) {
        c4m_stream_putc(s, '-');
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

    c4m_stream_puts(s, p);
}

// For nim integration.
static inline stream_t *
c4m_string_instream(any_str_t *instring)
{
    return c4m_new(c4m_tspec_stream(), c4m_kw("instring", c4m_ka(instring)));
}

static inline stream_t *
c4m_buffer_instream(buffer_t *inbuf)
{
    return c4m_new(c4m_tspec_stream(),
                   c4m_kw("buffer", c4m_ka(inbuf), "read", c4m_ka(true)));
}

static inline stream_t *
c4m_buffer_outstream(buffer_t *outbuf)
{
    return c4m_new(c4m_tspec_stream(),
                   c4m_kw("buffer",
                          c4m_ka(outbuf),
                          "read",
                          c4m_ka(false),
                          "write",
                          c4m_ka(true)));
}

static inline stream_t *
buffer_iostream(buffer_t *buf)
{
    return c4m_new(c4m_tspec_stream(),
                   c4m_kw("buffer",
                          c4m_ka(buf),
                          "read",
                          c4m_ka(true),
                          "write",
                          c4m_ka(true)));
}

static inline stream_t *
file_instream(any_str_t *filename, c4m_builtin_t output_type)
{
    return c4m_new(c4m_tspec_stream(),
                   c4m_kw("filename",
                          c4m_ka(filename),
                          "out_type",
                          c4m_ka(output_type)));
}

static inline stream_t *
c4m_file_outstream(any_str_t *filename, bool no_create, bool append)
{
    return c4m_new(c4m_tspec_stream(),
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
c4m_file_iostream(any_str_t *filename, bool no_create)
{
    return c4m_new(c4m_tspec_stream(),
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
c4m_get_stdin()
{
    return c4m_new(c4m_tspec_stream(), c4m_kw("cstream", c4m_ka(stdin)));
}

static inline stream_t *
c4m_get_stdout()
{
    return c4m_new(c4m_tspec_stream(), c4m_kw("cstream", c4m_ka(stdout)));
}

static inline stream_t *
c4m_get_stderr()
{
    return c4m_new(c4m_tspec_stream(), c4m_kw("cstream", c4m_ka(stderr)));
}

static inline bool
c4m_stream_using_cookie(stream_t *s)
{
    return (bool)(s->flags & F_STREAM_USING_COOKIE);
}

static inline int
c4m_stream_fileno(stream_t *s)
{
    if (c4m_stream_using_cookie(s)) {
        return -1;
    }

    return fileno(s->contents.f);
}

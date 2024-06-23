#pragma once
#include "con4m.h"

extern c4m_obj_t *c4m_stream_raw_read(c4m_stream_t *, int64_t, char *);
extern size_t     c4m_stream_raw_write(c4m_stream_t *, int64_t, char *);
extern void       _c4m_stream_write_object(c4m_stream_t *, c4m_obj_t, bool);
extern bool       c4m_stream_at_eof(c4m_stream_t *);
extern int64_t    c4m_stream_get_location(c4m_stream_t *);
extern void       c4m_stream_set_location(c4m_stream_t *, int64_t);
extern void       c4m_stream_close(c4m_stream_t *);
extern void       c4m_stream_flush(c4m_stream_t *);
extern void       _c4m_print(c4m_obj_t, ...);
extern c4m_obj_t *c4m_stream_read_all(c4m_stream_t *);

#define c4m_stream_write_object(s, o, ...) \
    _c4m_stream_write_object(s, o, IF(ISEMPTY(__VA_ARGS__))(false) __VA_ARGS__)

#define c4m_print(s, ...) _c4m_print(s, KFUNC(__VA_ARGS__))

static inline bool
c4m_stream_put_binary(c4m_stream_t *s, void *p, uint64_t len)
{
    return c4m_stream_raw_write(s, len, (char *)p) == len;
}

static inline bool
c4m_stream_putc(c4m_stream_t *s, char c)
{
    return c4m_stream_raw_write(s, 1, &c) == 1;
}

static inline bool
c4m_stream_putcp(c4m_stream_t *s, c4m_codepoint_t cp)
{
    uint8_t utf8[5];

    size_t n = utf8proc_encode_char(cp, utf8);
    utf8[n]  = 0;

    return c4m_stream_raw_write(s, n, (char *)utf8) == n;
}

static inline int
c4m_stream_puts(c4m_stream_t *s, char *c)
{
    return c4m_stream_raw_write(s, strlen(c), c);
}

static inline c4m_obj_t
c4m_stream_read(c4m_stream_t *stream, int64_t len)
{
    return c4m_stream_raw_read(stream, len, NULL);
}

static inline void
c4m_stream_puti(c4m_stream_t *s, int64_t n)
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
static inline c4m_stream_t *
c4m_string_instream(c4m_str_t *instring)
{
    return c4m_new(c4m_type_stream(), c4m_kw("instring", c4m_ka(instring)));
}

static inline c4m_stream_t *
c4m_buffer_instream(c4m_buf_t *inbuf)
{
    return c4m_new(c4m_type_stream(),
                   c4m_kw("buffer", c4m_ka(inbuf), "read", c4m_ka(true)));
}

static inline c4m_stream_t *
c4m_buffer_outstream(c4m_buf_t *outbuf, bool append)
{
    return c4m_new(c4m_type_stream(),
                   c4m_kw("buffer",
                          c4m_ka(outbuf),
                          "read",
                          c4m_ka(false),
                          "write",
                          c4m_ka(true),
                          "append",
                          c4m_ka(append)));
}

static inline c4m_stream_t *
buffer_iostream(c4m_buf_t *buf)
{
    return c4m_new(c4m_type_stream(),
                   c4m_kw("buffer",
                          c4m_ka(buf),
                          "read",
                          c4m_ka(true),
                          "write",
                          c4m_ka(true)));
}

static inline c4m_stream_t *
c4m_file_instream(c4m_str_t *filename, c4m_builtin_t output_type)
{
    return c4m_new(c4m_type_stream(),
                   c4m_kw("filename",
                          c4m_ka(filename),
                          "out_type",
                          c4m_ka(output_type)));
}

static inline c4m_stream_t *
c4m_file_outstream(c4m_str_t *filename, bool no_create, bool append)
{
    return c4m_new(c4m_type_stream(),
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

static inline c4m_stream_t *
c4m_file_iostream(c4m_str_t *filename, bool no_create)
{
    return c4m_new(c4m_type_stream(),
                   c4m_kw("filename",
                          c4m_ka(filename),
                          "read",
                          c4m_ka(false),
                          "write",
                          c4m_ka(true),
                          "no_create",
                          c4m_ka(no_create)));
}

c4m_stream_t *c4m_get_stdin();
c4m_stream_t *c4m_get_stdout();
c4m_stream_t *c4m_get_stderr();
void          c4m_init_std_streams();

static inline bool
c4m_stream_using_cookie(c4m_stream_t *s)
{
    return (bool)(s->flags & C4M_F_STREAM_USING_COOKIE);
}

static inline int
c4m_stream_fileno(c4m_stream_t *s)
{
    if (c4m_stream_using_cookie(s)) {
        return -1;
    }

    return fileno(s->contents.f);
}

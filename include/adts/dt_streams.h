#pragma once

#include "con4m.h"

typedef struct c4m_cookie_t c4m_cookie_t;

typedef void (*c4m_stream_setup_fn)(c4m_cookie_t *);
typedef size_t (*c4m_stream_read_fn)(c4m_cookie_t *, char *, int64_t);
typedef size_t (*c4m_stream_write_fn)(c4m_cookie_t *, char *, int64_t);
typedef void (*c4m_stream_close_fn)(c4m_cookie_t *);
typedef bool (*c4m_stream_seek_fn)(c4m_cookie_t *, int64_t);

typedef struct c4m_cookie_t {
    c4m_obj_t           object;
    char               *extra; // Whatever the implementation wants.
    int64_t             position;
    int64_t             eof;
    int64_t             flags;
    c4m_stream_setup_fn ptr_setup;
    c4m_stream_read_fn  ptr_read;
    c4m_stream_write_fn ptr_write;
    c4m_stream_close_fn ptr_close;
    c4m_stream_seek_fn  ptr_seek;
} c4m_cookie_t;

typedef struct {
    union {
        FILE         *f;
        c4m_cookie_t *cookie;
    } contents;
    int64_t flags;
} c4m_stream_t;

#define C4M_F_STREAM_READ         0x0001
#define C4M_F_STREAM_WRITE        0x0002
#define C4M_F_STREAM_APPEND       0x0004
#define C4M_F_STREAM_CLOSED       0x0008
#define C4M_F_STREAM_BUFFER_IN    0x0010
#define C4M_F_STREAM_STR_IN       0x0020
#define C4M_F_STREAM_UTF8_OUT     0x0040
#define C4M_F_STREAM_UTF32_OUT    0x0080
#define C4M_F_STREAM_USING_COOKIE 0x0100

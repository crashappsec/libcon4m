#pragma once

#include <con4m.h>

typedef struct cookie_t cookie_t;

typedef void (*stream_setup_fn)(cookie_t *);
typedef size_t (*stream_read_fn)(cookie_t *, char *, int64_t);
typedef size_t (*stream_write_fn)(cookie_t *, char *, int64_t);
typedef void (*stream_close_fn)(cookie_t *);
typedef _Bool (*stream_seek_fn)(cookie_t *, int64_t);


typedef struct cookie_t {
    object_t        object;
    char           *extra; // Whatever the implementation wants.
    int64_t         position;
    int64_t         eof;
    int64_t         flags;
    stream_setup_fn ptr_setup;
    stream_read_fn  ptr_read;
    stream_write_fn ptr_write;
    stream_close_fn ptr_close;
    stream_seek_fn  ptr_seek;
} cookie_t;

typedef struct {
    union {
	FILE     *f;
	cookie_t *cookie;
    } contents;
    int64_t flags;
} stream_t;

#define F_STREAM_READ         0x0001
#define F_STREAM_WRITE        0x0002
#define F_STREAM_APPEND       0x0004
#define F_STREAM_CLOSED       0x0008
#define F_STREAM_BUFFER_IN    0x0010
#define F_STREAM_STR_IN       0x0020
#define F_STREAM_UTF8_OUT     0x0040
#define F_STREAM_UTF32_OUT    0x0080
#define F_STREAM_USING_COOKIE 0x0100

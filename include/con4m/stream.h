#pragma once
#include <con4m.h>

object_t stream_read(stream_t *, int64_t);
size_t   stream_raw_write(stream_t *, char *, int64_t);
int64_t  _stream_write_object(stream_t *, object_t, ...);
bool     stream_at_eof(stream_t *);
int64_t  stream_get_location(stream_t *);
void     stream_set_location(stream_t *, int64_t);
void     stream_close(stream_t *);
void     stream_flush(stream_t *);

#define stream_write_object(s, o, ...) _stream_write_object(s, o,              \
                                                           KFUNC(__VA_ARGS__))

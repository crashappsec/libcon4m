#pragma once
#include "con4m.h"

typedef enum {
    c4m_http_get    = 0,
    c4m_http_header = 1,
    c4m_http_post   = 2,

} c4m_http_method_t;

typedef struct {
    CURL           *curl;
    c4m_buf_t      *buf;
    c4m_stream_t   *to_send;
    c4m_stream_t   *output_stream;
    char           *errbuf;
    // Internal lock makes sure that two threads don't call at once.
    pthread_mutex_t lock;
    CURLcode        code; // Holds the last result code.
} c4m_basic_http_t;

typedef struct {
    c4m_buf_t  *contents;
    c4m_utf8_t *error;
    CURLcode    code;
} c4m_basic_http_response_t;

static inline CURLcode
c4m_basic_http_raw_setopt(c4m_basic_http_t *self,
                          CURLoption        option,
                          void             *param)
{
    return curl_easy_setopt(self->curl, option, param);
}

static inline void
c4m_basic_http_reset(c4m_basic_http_t *self)
{
    curl_easy_reset(self->curl);
}

static inline CURLcode
c4m_basic_http_run_request(c4m_basic_http_t *self)
{
    pthread_mutex_lock(&self->lock);
    self->code = curl_easy_perform(self->curl);
    pthread_mutex_unlock(&self->lock);

    return self->code;
}

// Somewhat short-term TODO:
// Header access.
// Mime.
// POST.
// Cert pin.

extern c4m_basic_http_response_t *_c4m_http_get(c4m_str_t *, ...);
extern c4m_basic_http_response_t *_c4m_http_upload(c4m_str_t *,
                                                   c4m_buf_t *,
                                                   ...);

static inline bool
c4m_validate_url(c4m_utf8_t *candidate)
{
    CURLU *handle = curl_url();

    return curl_url_set(handle, CURLUPART_URL, candidate->data, 0) == CURLUE_OK;
}

static inline bool
c4m_http_op_succeded(c4m_basic_http_response_t *op)
{
    return op->code == CURLE_OK;
}

static inline c4m_buf_t *
c4m_http_op_get_output_buffer(c4m_basic_http_response_t *op)
{
    if (!c4m_http_op_succeded(op)) {
        return NULL;
    }

    return op->contents;
}

static inline c4m_utf8_t *
c4m_http_op_get_output_utf8(c4m_basic_http_response_t *op)
{
    if (!c4m_http_op_succeded(op)) {
        return NULL;
    }

    return c4m_buf_to_utf8_string(op->contents);
}

#define c4m_http_get(u, ...) \
    _c4m_http_get(u, C4M_VA(__VA_ARGS__))

#define c4m_http_upload(u, b, ...) \
    _c4m_http_upload(u, b, C4M_VA(__VA_ARGS__))

#define c4m_vp(x) ((void *)(int64_t)(x))

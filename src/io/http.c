#include "con4m.h"

static pthread_once_t init = PTHREAD_ONCE_INIT;

static void
init_curl(void)
{
    curl_global_init(CURL_GLOBAL_ALL);
}

static inline void
ensure_curl(void)
{
    pthread_once(&init, init_curl);
}

static char *
format_cookies(c4m_dict_t *cookies)
{
    uint64_t             n;
    hatrack_dict_item_t *view = hatrack_dict_items_sort(cookies, &n);
    // Start with one = and one ; per item, plus a null byte.
    int                  len  = n * 2 + 1;

    for (unsigned int i = 0; i < n; i++) {
        c4m_utf8_t *s = c4m_to_utf8(view[i].key);
        view[i].key   = s;
        len += c4m_str_byte_len(s);

        s             = c4m_to_utf8(view[i].value);
        view[i].value = s;
        len += c4m_str_byte_len(s);
    }

    char *res = c4m_gc_raw_alloc(len, C4M_GC_SCAN_NONE);
    char *p   = res;

    for (unsigned int i = 0; i < n; i++) {
        c4m_utf8_t *s = view[i].key;
        int         l = c4m_str_byte_len(s);

        memcpy(p, s->data, l);
        p += l;
        *p++ = '=';

        s = view[i].value;
        l = c4m_str_byte_len(s);

        memcpy(p, s->data, l);
        p += l;
        *p++ = ';';
    }

    return res;
}

static void
c4m_basic_http_teardown(c4m_basic_http_t *self)
{
    if (self->curl) {
        curl_easy_cleanup(self->curl);
    }
}

static size_t
http_out_to_stream(char *ptr, size_t size, size_t nmemb, c4m_basic_http_t *self)
{
    return c4m_stream_raw_write(self->output_stream, size * nmemb, ptr);
}

static size_t
internal_http_send(char *ptr, size_t size, size_t nmemb, c4m_basic_http_t *self)
{
    size_t     to_write  = size * nmemb;
    c4m_buf_t *out       = c4m_stream_read(self->to_send, to_write);
    size_t     to_return = c4m_buffer_len(out);

    memcpy(ptr, out->data, to_return);

    return to_return;
}

void
c4m_basic_http_set_gc_bits(uint64_t *bitmap, c4m_basic_http_t *self)
{
    c4m_mark_raw_to_addr(bitmap, self, &self->errbuf);
}

#define HTTP_CORE_COMMON_START(provided_url, vararg_name) \
    c4m_basic_http_response_t *result;                    \
    int64_t                    connect_timeout = -1;      \
    int64_t                    total_timeout   = -1;      \
    c4m_str_t                 *aws_sig         = NULL;    \
    c4m_str_t                 *access_key      = NULL;    \
    c4m_dict_t                *cookies         = NULL;    \
    c4m_basic_http_t          *connection      = NULL;    \
                                                          \
    c4m_karg_only_init(vararg_name);                      \
    c4m_kw_int64("connect_timeout", connect_timeout);     \
    c4m_kw_int64("total_timeout", total_timeout);         \
    c4m_kw_ptr("aws_sig", aws_sig);                       \
    c4m_kw_ptr("access_key", access_key);                 \
    c4m_kw_ptr("cookies", cookies);                       \
                                                          \
    connection = c4m_new(c4m_type_http(),                 \
                         c4m_kw("url",                    \
                                c4m_ka(provided_url),     \
                                "connect_timeout",        \
                                c4m_ka(connect_timeout),  \
                                "total_timeout",          \
                                c4m_ka(total_timeout),    \
                                "aws_sig",                \
                                c4m_ka(aws_sig),          \
                                "access_key",             \
                                c4m_ka(access_key),       \
                                "cookies",                \
                                c4m_ka(cookies)));        \
    result     = c4m_gc_alloc_mapped(c4m_basic_http_response_t, C4M_GC_SCAN_ALL)

#define HTTP_CORE_COMMON_END()                            \
    c4m_basic_http_run_request(connection);               \
    result->code = connection->code;                      \
                                                          \
    if (connection->errbuf && connection->errbuf[0]) {    \
        result->error = c4m_new_utf8(connection->errbuf); \
    }                                                     \
                                                          \
    result->contents = connection->buf;                   \
                                                          \
    return result

c4m_basic_http_response_t *
_c4m_http_get(c4m_str_t *url, ...)
{
    HTTP_CORE_COMMON_START(url, url);
    c4m_basic_http_raw_setopt(connection, CURLOPT_HTTPGET, c4m_vp(1L));
    HTTP_CORE_COMMON_END();
}

c4m_basic_http_response_t *
_c4m_http_upload(c4m_str_t *url, c4m_buf_t *data, ...)
{
    // Does NOT make a copy of the buffer.

    HTTP_CORE_COMMON_START(url, data);
    c4m_basic_http_raw_setopt(connection, CURLOPT_UPLOAD, c4m_vp(1L));
    c4m_basic_http_raw_setopt(connection, CURLOPT_READFUNCTION, internal_http_send);
    c4m_basic_http_raw_setopt(connection, CURLOPT_READDATA, (void *)connection);
    c4m_basic_http_raw_setopt(connection,
                              CURLOPT_INFILESIZE_LARGE,
                              c4m_vp(data->byte_len));
    connection->to_send = c4m_buffer_instream(data);

    HTTP_CORE_COMMON_END();
}

static void
c4m_basic_http_init(c4m_basic_http_t *self, va_list args)
{
    int64_t       connect_timeout = -1;
    int64_t       total_timeout   = -1;
    c4m_str_t    *url             = NULL;
    c4m_str_t    *aws_sig         = NULL;
    c4m_str_t    *access_key      = NULL;
    c4m_dict_t   *cookies         = NULL;
    c4m_stream_t *output_stream   = NULL;

    ensure_curl();

    c4m_karg_va_init(args);

    c4m_kw_int64("connect_timeout", connect_timeout);
    c4m_kw_int64("total_timeout", total_timeout);
    c4m_kw_ptr("aws_sig", aws_sig);
    c4m_kw_ptr("access_key", access_key);
    c4m_kw_ptr("cookies", cookies);
    c4m_kw_ptr("url", url);
    c4m_kw_ptr("output_stream", output_stream);

    pthread_mutex_init(&self->lock, NULL);

    // TODO: Do these in the near future (after objects)
    // bool       cert_info  = false;
    // c4m_kw_ptr("cert_info", cert_info);

    self->curl = curl_easy_init();

    if (url) {
        c4m_basic_http_raw_setopt(self, CURLOPT_URL, c4m_to_utf8(url)->data);
    }

    if (total_timeout >= 0 && total_timeout > connect_timeout) {
        c4m_basic_http_raw_setopt(self, CURLOPT_TIMEOUT_MS, c4m_vp(total_timeout));
    }

    if (connect_timeout >= 0) {
        c4m_basic_http_raw_setopt(self,
                                  CURLOPT_CONNECTTIMEOUT_MS,
                                  c4m_vp(connect_timeout));
    }

    if (aws_sig != NULL) {
        c4m_basic_http_raw_setopt(self, CURLOPT_AWS_SIGV4, c4m_to_utf8(aws_sig)->data);
    }

    if (access_key != NULL) {
        c4m_basic_http_raw_setopt(self, CURLOPT_USERPWD, c4m_to_utf8(access_key)->data);
    }

    if (cookies) {
        c4m_basic_http_raw_setopt(self, CURLOPT_COOKIE, format_cookies(cookies));
    }

    if (output_stream) {
        self->output_stream = output_stream;
    }
    else {
        self->buf           = c4m_buffer_empty();
        self->output_stream = c4m_buffer_outstream(self->buf, true);
    }

    c4m_basic_http_raw_setopt(self, CURLOPT_WRITEFUNCTION, http_out_to_stream);
    c4m_basic_http_raw_setopt(self, CURLOPT_WRITEDATA, (void *)self);

    self->errbuf = c4m_gc_value_alloc(CURL_ERROR_SIZE);

    c4m_basic_http_raw_setopt(self, CURLOPT_ERRORBUFFER, self->errbuf);
}

const c4m_vtable_t c4m_basic_http_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)c4m_basic_http_init,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)c4m_basic_http_set_gc_bits,
        [C4M_BI_FINALIZER]   = (c4m_vtable_entry)c4m_basic_http_teardown,
    },
};

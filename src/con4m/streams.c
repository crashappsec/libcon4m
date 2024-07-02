#include "con4m.h"

// Currently, for strings, this does NOT inject ANSI codes.  It's a
// TODO to add an ansi streaming mode for strings.  I also want to add
// an ANSI parser to lift codes out of strings too.
static void
mem_c4m_stream_setup(c4m_cookie_t *c)
{
    if (c->object == NULL) {
        c->object = c4m_new(c4m_type_buffer());
        c->extra  = NULL;
        return;
    }
    else {
        switch (c4m_get_base_type_id(c->object)) {
        case C4M_T_UTF8:
        case C4M_T_UTF32: {
            c4m_str_t *s = (c4m_str_t *)c->object;
            c->extra     = s->data;
            c->eof       = s->byte_len;
            return;
        }
        case C4M_T_BUFFER: {
            c4m_buf_t *b = (c4m_buf_t *)c->object;
            c->extra     = b->data;
            c->eof       = b->byte_len;
            return;
        }
        default:
            assert(false);
        }
    }
}

static size_t
mem_c4m_stream_read(c4m_cookie_t *c, char *dst, int64_t request)
{
    // For buffers, which are mutable, this approach doesn't really
    // work when another thread can also be mutating the buffer.  So
    // for now, don't do that, even though we still take the time to
    // reset the buffer length each call (again, without any locking
    // this is basically meaningless).

    if (c->flags & C4M_F_STREAM_BUFFER_IN) {
        c4m_buf_t *b = (c4m_buf_t *)c->object;

        c->eof = b->byte_len;
    }

    if (request < 0) {
        request += c->eof;
    }

    int64_t read_len = c4m_min((int64_t)request, c->eof - c->position);

    if (read_len <= 0) {
        return 0;
    }

    memcpy(dst, c->extra, read_len);

    c->extra += read_len;
    c->position += read_len;

    return read_len;
}

static size_t
mem_c4m_stream_write(c4m_cookie_t *c, char *buf, int64_t request)
{
    // Same comment on buffers as above.
    c4m_buf_t *b = (c4m_buf_t *)c->object;
    c->eof       = b->byte_len;

    if (c->flags & C4M_F_STREAM_APPEND) {
        c->position = c->eof;
    }

    if (c->position + request > c->eof) {
        c->eof = c->position + request;
        c4m_buffer_resize(b, c->eof);
        c->extra = b->data + c->position;
    }

    memcpy(c->extra, buf, request);
    c->extra += request;
    c->position += request;

    return request;
}

static void
mem_c4m_stream_close(c4m_cookie_t *c)
{
    // Get rid of our heap pointers.
    c->object = NULL;
    c->extra  = NULL;
}

static bool
mem_c4m_stream_seek(c4m_cookie_t *c, int64_t pos)
{
    if (pos < 0) {
        return false;
    }

    if (pos > c->eof) {
        return false;
    }

    c->extra += (pos - c->position);
    c->position = pos;
    return true;
}

static inline c4m_cookie_t *
new_mem_cookie()
{
    c4m_cookie_t *result = c4m_gc_alloc(c4m_cookie_t);

    result->ptr_setup = mem_c4m_stream_setup;
    result->ptr_read  = mem_c4m_stream_read;
    result->ptr_write = mem_c4m_stream_write;
    result->ptr_close = mem_c4m_stream_close;
    result->ptr_seek  = mem_c4m_stream_seek;

    return result;
}

static void
c4m_stream_init(c4m_stream_t *stream, va_list args)
{
    c4m_str_t    *filename      = NULL;
    c4m_str_t    *instring      = NULL;
    c4m_buf_t    *buffer        = NULL;
    c4m_cookie_t *cookie        = NULL;
    FILE         *fstream       = NULL;
    int           fd            = -1;
    bool          read          = true;
    bool          write         = false;
    bool          append        = false;
    bool          no_create     = false;
    bool          close_on_exec = true;
    c4m_builtin_t out_type      = C4M_T_UTF8;

    c4m_karg_va_init(args);
    c4m_kw_ptr("filename", filename);
    c4m_kw_ptr("instring", instring);
    c4m_kw_ptr("buffer", buffer);
    c4m_kw_ptr("cookie", cookie);
    c4m_kw_ptr("cstream", fstream);
    c4m_kw_int32("fd", fd);
    c4m_kw_bool("read", read);
    c4m_kw_bool("write", write);
    c4m_kw_bool("append", append);
    c4m_kw_bool("no_create", no_create);
    c4m_kw_bool("close_on_exec", close_on_exec);
    c4m_kw_int64("out_type", out_type);

    int64_t src_count = 0;
    char    buf[10]   = {
        0,
    };
    int     i     = 0;
    int64_t flags = 0;

    src_count += (int64_t) !!filename;
    src_count += (int64_t) !!instring;
    src_count += (int64_t) !!buffer;
    src_count += (int64_t) !!cookie;
    src_count += (int64_t) !!fstream;
    src_count += (fd >= 0);

    switch (out_type) {
    case C4M_T_UTF8:
        flags = C4M_F_STREAM_UTF8_OUT;
        break;
    case C4M_T_UTF32:
        flags = C4M_F_STREAM_UTF32_OUT;
        break;
    case C4M_T_BUFFER:
        break;
    default:
        C4M_CRAISE("Invalid output type for streams.");
    }

    switch (src_count) {
    case 0:
        C4M_CRAISE("No stream source provided.");
    case 1:
        break;
    default:
        C4M_CRAISE("Cannot provide multiple stream sources.");
    }

    if (append) {
        buf[i++] = 'a';
        buf[i++] = 'b';
        if (read) {
            buf[i++] = '+';
        }

        if (no_create) {
            buf[i++] = 'x';
        }
    }
    else {
        if (write) {
            buf[i++] = 'w';
            buf[i++] = 'b';

            if (read) {
                buf[i++] = '+';
            }
            if (no_create) {
                buf[i++] = 'x';
            }
        }
        else {
            buf[i++] = 'r';
            buf[i++] = 'b';
        }
    }

    if (close_on_exec) {
        buf[i++] = 'e';
    }

    if (read) {
        flags |= C4M_F_STREAM_READ;
    }
    if (write) {
        flags |= C4M_F_STREAM_WRITE;
    }
    if (append) {
        flags |= C4M_F_STREAM_APPEND;
    }

    if (filename != NULL) {
        filename           = c4m_to_utf8(filename);
        stream->contents.f = fopen(filename->data, buf);
        stream->flags      = flags;

err_check:
        if (stream->contents.f == NULL) {
            c4m_raise_errno();
        }

        return;
    }

    if (fstream != NULL) {
        stream->contents.f = fstream;
        return;
    }

    if (fd != -1) {
        stream->contents.f = fdopen(fd, buf);
        goto err_check;
    }

    flags |= C4M_F_STREAM_USING_COOKIE;

    if (cookie == NULL) {
        if (instring && write) {
            C4M_CRAISE(
                "Cannot open string for writing "
                "(they are non-mutable).");
        }

        cookie = new_mem_cookie();
    }
    else {
        if (read && !cookie->ptr_read) {
            C4M_CRAISE(
                "Custom stream implementation does not "
                "support reading.");
        }
        if ((write || append) && !cookie->ptr_write) {
            C4M_CRAISE(
                "Custom stream implementation does not support "
                "writing.");
        }
    }

    if (instring) {
        cookie->object = instring;
        flags |= C4M_F_STREAM_STR_IN;
    }
    if (buffer) {
        cookie->object = buffer;
        flags |= C4M_F_STREAM_BUFFER_IN;
        if (append) {
            cookie->position = c4m_buffer_len(buffer);
        }
    }

    cookie->flags           = flags;
    stream->contents.cookie = cookie;
    stream->flags           = flags;

    if (cookie->ptr_setup != NULL) {
        (*cookie->ptr_setup)(cookie);
    }
}

static c4m_obj_t
c4m_stream_bytes_to_output(int64_t flags, char *buf, int64_t len)
{
    if (flags & C4M_F_STREAM_UTF8_OUT) {
        return c4m_new(c4m_type_utf8(),
                       c4m_kw("cstring", c4m_ka(buf), "length", c4m_ka(len)));
    }

    if (flags & C4M_F_STREAM_UTF32_OUT) {
        return c4m_new(c4m_type_utf32(),
                       c4m_kw("cstring",
                              c4m_ka(buf),
                              "codepoints",
                              c4m_ka(len / 4)));
    }

    else {
        // Else, it's going to a buffer.
        return c4m_new(c4m_type_buffer(),
                       c4m_kw("raw", c4m_ka(buf), "length", c4m_ka(len)));
    }
}

// We generally assume c4m is passing around 64 bit sizes, but when
// dealing w/ the C API, things are often 32 bits (e.g., size_t).
// Therefore, for the internal API, we accept a 64-bit value in, but
// expect the write length to be a size_t because that's what fread()
// will give us.
//
// The final parameter here is meant for internal use, mainly for
// marshal, so we don't have to go through an object to read out
// things like ints that we plan on returning.

c4m_obj_t *
c4m_stream_raw_read(c4m_stream_t *stream, int64_t len, char *buf)
{
    // If a buffer is provided, return the length and write into
    // the buffer.
    bool return_len = (buf != NULL);

    if (stream->flags & C4M_F_STREAM_CLOSED) {
        C4M_CRAISE("Stream is already closed.");
    }

    if (!len) {
        if (return_len) {
            return (c4m_obj_t)(0); // I.e., null
        }
        return c4m_stream_bytes_to_output(stream->flags, "", 0);
    }

    int64_t flags  = stream->flags;
    size_t  actual = 0;

    if (!return_len) {
        buf = alloca(len);
    }

    if (!(flags & C4M_F_STREAM_READ)) {
        C4M_CRAISE("Cannot read; stream was not opened with read enabled.");
    }

    if (flags & C4M_F_STREAM_UTF32_OUT) {
        len *= 4;
    }

    if (stream->flags & C4M_F_STREAM_USING_COOKIE) {
        c4m_cookie_t      *cookie = stream->contents.cookie;
        c4m_stream_read_fn f      = cookie->ptr_read;

        actual = (*f)(cookie, buf, len);
    }
    else {
        actual = fread(buf, 1, len, stream->contents.f);
    }

    if (return_len) {
        return (c4m_obj_t)(actual);
    }
    else {
        if (actual) {
            return c4m_stream_bytes_to_output(stream->flags, buf, actual);
        }
        return (c4m_obj_t *)c4m_empty_string();
    }
}

c4m_obj_t *
c4m_stream_read_all(c4m_stream_t *stream)
{
    c4m_list_t *l;
    int         outkind;

    outkind = stream->flags & (C4M_F_STREAM_UTF8_OUT | C4M_F_STREAM_UTF32_OUT);

    switch (outkind) {
    case C4M_F_STREAM_UTF8_OUT:
        l = c4m_new(c4m_type_list(c4m_type_utf8()));
        break;
    case C4M_F_STREAM_UTF32_OUT:
        l = c4m_new(c4m_type_list(c4m_type_utf32()));
        break;
    default:
        // Buffers.
        l = c4m_new(c4m_type_list(c4m_type_buffer()));
        break;
    }
    while (true) {
        c4m_obj_t *one = c4m_stream_raw_read(stream, PIPE_BUF, NULL);

        if (outkind) {
            if (c4m_str_codepoint_len((c4m_str_t *)one) == 0) {
                break;
            }
        }
        else {
            if (c4m_buffer_len((c4m_buf_t *)one) == 0) {
                break;
            }
        }

        c4m_list_append(l, one);
    }
    if (outkind) {
        c4m_str_t *s = c4m_str_join(l, c4m_empty_string());

        if (outkind == C4M_F_STREAM_UTF8_OUT) {
            return (c4m_obj_t *)c4m_to_utf8(s);
        }
        else {
            return (c4m_obj_t *)c4m_to_utf32(s);
        }
    }
    else {
        return (c4m_obj_t *)c4m_buffer_join(l, NULL);
    }
}
size_t
c4m_stream_raw_write(c4m_stream_t *stream, int64_t len, char *buf)
{
    size_t        actual = 0;
    c4m_cookie_t *cookie = NULL;

    if (stream->flags & C4M_F_STREAM_CLOSED) {
        C4M_CRAISE("Stream is already closed.");
    }

    if (len <= 0) {
        return 0;
    }

    if (stream->flags & C4M_F_STREAM_USING_COOKIE) {
        cookie = stream->contents.cookie;

        c4m_stream_write_fn f = cookie->ptr_write;

        actual = (*f)(cookie, buf, len);
    }
    else {
        actual = fwrite(buf, len, 1, stream->contents.f);
    }

    if (actual > 0) {
        return actual;
    }

    if (cookie != NULL) {
        if (cookie->eof == cookie->position) {
            C4M_CRAISE("Custom stream implementation could write past EOF.");
        }
        else {
            C4M_CRAISE("Custom stream implementation could not finish write.");
        }
    }

    else {
        if (!feof(stream->contents.f)) {
            c4m_raise_errcode(ferror(stream->contents.f));
        }
    }

    return 0;
}

void
c4m_stream_write_object(c4m_stream_t *stream, c4m_obj_t obj, bool ansi)
{
    if (stream->flags & C4M_F_STREAM_CLOSED) {
        C4M_CRAISE("Stream is already closed.");
    }

    // c4m_str_t *s = c4m_value_obj_to_str(obj);
    c4m_str_t *s = c4m_to_str(obj, c4m_get_my_type(obj));

    if (ansi) {
        c4m_ansi_render(s, stream);
    }
    else {
        s = c4m_to_utf8(s);
        c4m_stream_raw_write(stream, s->byte_len, s->data);
    }
}

void
c4m_stream_write_to_width(c4m_stream_t *stream, c4m_obj_t obj)
{
    if (stream->flags & C4M_F_STREAM_CLOSED) {
        C4M_CRAISE("Stream is already closed.");
    }

    c4m_str_t *s = c4m_to_str(obj, c4m_get_my_type(obj));
    c4m_ansi_render_to_width(s, c4m_terminal_width(), 0, stream);
}

bool
c4m_stream_at_eof(c4m_stream_t *stream)
{
    if (stream->flags & C4M_F_STREAM_CLOSED) {
        return true;
    }

    if (stream->flags & C4M_F_STREAM_USING_COOKIE) {
        c4m_cookie_t *cookie = stream->contents.cookie;

        return cookie->position >= cookie->eof;
    }

    return feof(stream->contents.f);
}

int64_t
c4m_stream_get_location(c4m_stream_t *stream)
{
    if (stream->flags & C4M_F_STREAM_CLOSED) {
        C4M_CRAISE("Stream is already closed.");
    }

    if (stream->flags & C4M_F_STREAM_USING_COOKIE) {
        return stream->contents.cookie->position;
    }

    return ftell(stream->contents.f);
}

void
c4m_stream_set_location(c4m_stream_t *stream, int64_t offset)
{
    if (stream->flags & C4M_F_STREAM_CLOSED) {
        C4M_CRAISE("Stream is already closed.");
    }

    if (stream->flags & C4M_F_STREAM_USING_COOKIE) {
        c4m_cookie_t *cookie = stream->contents.cookie;

        if (offset < 0) {
            offset += cookie->eof;
        }

        c4m_stream_seek_fn fn = cookie->ptr_seek;

        if (fn == NULL) {
            C4M_CRAISE("Custom stream does not have the ability to seek.");
        }

        if ((*fn)(cookie, offset) == false) {
            C4M_CRAISE("Seek position out of bounds for stream.");
        }
    }

    else {
        int result;

        if (offset < 0) {
            offset *= -1;

            result = fseek(stream->contents.f, offset, SEEK_END);
        }
        else {
            result = fseek(stream->contents.f, offset, SEEK_SET);
        }

        if (result != 0) {
            stream->flags = C4M_F_STREAM_CLOSED;
            c4m_raise_errno();
        }
    }
}

void
c4m_stream_close(c4m_stream_t *stream)
{
    if (stream->flags & C4M_F_STREAM_CLOSED) {
        return;
    }

    if (stream->flags & C4M_F_STREAM_USING_COOKIE) {
        c4m_cookie_t *cookie = stream->contents.cookie;

        if (!cookie) {
            return;
        }
        c4m_stream_close_fn fn = cookie->ptr_close;

        cookie->flags = C4M_F_STREAM_CLOSED;

        if (fn) {
            (*fn)(cookie);
        }
    }

    else {
        while (fclose(stream->contents.f) != 0) {
            if (errno != EINTR) {
                break;
            }
        }
    }

    stream->contents.f = NULL;
    stream->flags      = C4M_F_STREAM_CLOSED;
}

void
c4m_stream_flush(c4m_stream_t *stream)
{
    if (stream->flags & C4M_F_STREAM_CLOSED) {
        C4M_CRAISE("Stream is already closed.");
    }

    if (stream->flags & C4M_F_STREAM_USING_COOKIE) {
        return; // Not actually buffering ATM.
    }

    if (fflush(stream->contents.f)) {
        stream->flags = C4M_F_STREAM_CLOSED;
        c4m_raise_errno();
    }
}

void
_c4m_print(c4m_obj_t first, ...)
{
    va_list          args;
    c4m_obj_t        cur       = first;
    c4m_karg_info_t *_c4m_karg = NULL;
    c4m_stream_t    *stream    = NULL;
    c4m_codepoint_t  sep       = ' ';
    c4m_codepoint_t  end       = '\n';
    bool             flush     = false;
    bool             force     = false;
    bool             nocolor   = false;
    int              numargs;
    bool             ansi;
    bool             truncate = true;

    va_start(args, first);

    if (first == NULL) {
        c4m_stream_putc(c4m_get_stdout(), '\n');
        return;
    }

    if (c4m_get_my_type(first) == c4m_type_kargs()) {
        _c4m_karg = first;
        numargs   = 0;
    }
    else {
        _c4m_karg = c4m_get_kargs_and_count(args, &numargs);
        numargs++;
    }

    if (_c4m_karg != NULL) {
        if (!c4m_kw_ptr("stream", stream)) {
            stream = c4m_get_stdout();
        }
        c4m_kw_codepoint("sep", sep);
        c4m_kw_codepoint("end", end);
        c4m_kw_bool("flush", flush);
        c4m_kw_bool("truncate", truncate);

        if (!c4m_kw_bool("c4m_to_color", force)) {
            c4m_kw_bool("no_color", nocolor);
        }
        else {
            if (c4m_kw_bool("no_color", nocolor)) {
                C4M_CRAISE(
                    "Cannot specify `c4m_to_color` and `no_color` "
                    "together.");
            }
        }
    }

    if (stream == NULL) {
        stream = c4m_get_stdout();
    }
    if (force) {
        ansi = true;
    }
    else {
        if (nocolor) {
            ansi = false;
        }
        else {
            int fno = c4m_stream_fileno(stream);

            if (fno == -1 || !isatty(fno)) {
                ansi = false;
            }
            else {
                ansi = true;
            }
        }
    }

    for (int i = 0; i < numargs; i++) {
        if (i && sep) {
            c4m_stream_putcp(stream, sep);
        }

        if (ansi && truncate) {
            // truncate requires ansi.
            c4m_stream_write_to_width(stream, cur);
        }
        else {
            c4m_stream_write_object(stream, cur, ansi);
        }
        cur = va_arg(args, c4m_obj_t);
    }

    if (end) {
        c4m_stream_putcp(stream, end);
    }

    if (flush) {
        c4m_stream_flush(stream);
    }

    va_end(args);
}

static c4m_stream_t *c4m_stream_stdin  = NULL;
static c4m_stream_t *c4m_stream_stdout = NULL;
static c4m_stream_t *c4m_stream_stderr = NULL;

void
c4m_init_std_streams()
{
    if (c4m_stream_stdin == NULL) {
        c4m_stream_stdin  = c4m_new(c4m_type_stream(),
                                   c4m_kw("cstream", c4m_ka(stdin)));
        c4m_stream_stdout = c4m_new(c4m_type_stream(),
                                    c4m_kw("cstream", c4m_ka(stdout)));
        c4m_stream_stderr = c4m_new(c4m_type_stream(),
                                    c4m_kw("cstream", c4m_ka(stderr)));
        c4m_gc_register_root(&c4m_stream_stdin, 1);
        c4m_gc_register_root(&c4m_stream_stdout, 1);
        c4m_gc_register_root(&c4m_stream_stderr, 1);
    }
}

c4m_stream_t *
c4m_get_stdin()
{
    return c4m_stream_stdin;
}

c4m_stream_t *
c4m_get_stdout()
{
    return c4m_stream_stdout;
}

c4m_stream_t *
c4m_get_stderr()
{
    return c4m_stream_stderr;
}

static void
c4m_stream_set_gc_bits(uint64_t *bitfield, int alloc_words)
{
    int ix;
    c4m_set_object_header_bits(bitfield, &ix);
    c4m_set_bit(bitfield, ix);
}

const c4m_vtable_t c4m_stream_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)c4m_stream_init,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)c4m_stream_set_gc_bits,
        // This is not supposed to be necessary, but it sometimes crashes w/o.
        [C4M_BI_FINALIZER]   = (c4m_vtable_entry)c4m_stream_close,
        NULL,
    },
};

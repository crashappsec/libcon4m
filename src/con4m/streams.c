#include <con4m.h>

// Currently, for strings, this does NOT inject ANSI codes.  It's a
// TODO to add an ansi streaming mode for strings.  I also want to add
// an ANSI parser to lift codes out of strings too.
static void
mem_stream_setup(cookie_t *c)
{
    if (c->object == NULL) {
	c->object = con4m_new(tspec_buffer());
	c->extra = NULL;
	return;
    }
    else {
	switch(get_base_type_id(c->object)) {
	case T_UTF8:
	case T_UTF32:
	{
	    any_str_t *s = (any_str_t *)c->object;
	    c->extra     = s->data;
	    c->eof       = s->byte_len;
	    return;
	}
	case T_BUFFER:
	{
	    buffer_t *b = (buffer_t *)c->object;
	    c->extra    = b->data;
	    c->eof      = b->byte_len;
	    return;
	}
	default:
	    assert(false);
	}
    }
}

static size_t
mem_stream_read(cookie_t *c, char *dst, int64_t request)
{
    // For buffers, which are mutable, this approach doesn't really
    // work when another thread can also be mutating the buffer.  So
    // for now, don't do that, even though we still take the time to
    // reset the buffer length each call (again, without any locking
    // this is basically meaningless).

    if (c->flags & F_STREAM_BUFFER_IN) {
	buffer_t *b = (buffer_t *)c->object;

	c->eof = b->byte_len;
    }

    if (request < 0) {
	request += c->eof;
    }

    int64_t read_len = min((int64_t)request, c->eof - c->position);

    if (read_len <= 0) {
	return 0;
    }

    memcpy(dst, c->extra, read_len);

    c->extra    += read_len;
    c->position += read_len;

    return read_len;
}

static size_t
mem_stream_write(cookie_t *c, char *buf, int64_t request)
{
    // Same comment on buffers as above.
    buffer_t *b = (buffer_t *)c->object;
    c->eof      = b->byte_len;

    if (c->flags & F_STREAM_APPEND) {
	c->position = c->eof;
    }

    if (c->position + request > c->eof) {
	c->eof = c->position + request;
	buffer_resize(b, c->eof);
	c->extra = b->data + c->position;
    }

    memcpy(c->extra, buf, request);
    c->extra    += request;
    c->position += request;

    return request;
}

static void
mem_stream_close(cookie_t *c)
{
    // Get rid of our heap pointers.
    c->object = NULL;
    c->extra  = NULL;
}

static _Bool
mem_stream_seek(cookie_t *c, int64_t pos)
{
    if (pos < 0) {
	return false;
    }

    if (pos > c->eof) {
	return false;
    }

    c->extra   += (pos - c->position);
    c->position = pos;
    return true;
}

static inline cookie_t *
new_mem_cookie()
{
    cookie_t *result = gc_alloc(cookie_t);

    result->ptr_setup = mem_stream_setup;
    result->ptr_read  = mem_stream_read;
    result->ptr_write = mem_stream_write;
    result->ptr_close = mem_stream_close;
    result->ptr_seek  = mem_stream_seek;

    return result;
}

static void
stream_init(stream_t *stream, va_list args)
{
    DECLARE_KARGS(
	any_str_t      *filename      = NULL;
	any_str_t      *instring      = NULL;
	buffer_t       *buffer        = NULL;
	cookie_t       *cookie        = NULL;
	FILE           *fstream       = NULL;
	int             fd            = -1;
	int             read          = 1;
	int             write         = 0;
	int             append        = 0;
	int             no_create     = 0;
	int             close_on_exec = 1;
	con4m_builtin_t out_type      = T_UTF8;
	);

    method_kargs(args, filename, instring, buffer, cookie, fstream, fd,
		 read, write, append, no_create, close_on_exec, out_type);

    int64_t src_count = 0;
    char    buf[10]   = {0,};
    int     i         = 0;
    int64_t  flags    = 0;

    src_count += (int64_t)!!filename;
    src_count += (int64_t)!!instring;
    src_count += (int64_t)!!buffer;
    src_count += (int64_t)!!cookie;
    src_count += (int64_t)!!fstream;
    src_count += (fd >= 0);

    switch (out_type) {
    case T_UTF8:
	flags = F_STREAM_UTF8_OUT;
	break;
    case T_UTF32:
	flags = F_STREAM_UTF32_OUT;
	break;
    case T_BUFFER:
	break;
    default:
	CRAISE("Invalid output type for streams.");
    }

    switch (src_count) {
    case 0:
	CRAISE("No stream source provided.");
    case 1:
	break;
    default:
	CRAISE("Cannot provide multiple stream sources.");
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

	    if(read) {
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
	flags |= F_STREAM_READ;
    }
    if (write) {
	flags |= F_STREAM_WRITE;
    }
    if (append) {
	flags |= F_STREAM_APPEND;
    }

    if (filename != NULL) {
	filename           = force_utf8(filename);
	stream->contents.f = fopen(filename->data, buf);
	stream->flags      = flags;

    err_check:
	if (stream->contents.f == NULL) {
	    raise_errno();
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

    flags |= F_STREAM_USING_COOKIE;

    if (cookie == NULL) {
	if (instring && write) {
	    CRAISE("Cannot open string for writing (they are non-mutable).");
	}

	cookie = new_mem_cookie();
    }
    else {
	if (read && !cookie->ptr_read) {
	    CRAISE("Custom stream implementation does not support reading.");
	}
	if ((write || append) && !cookie->ptr_write) {
	    CRAISE("Custom stream implementation does not support writing.");
	}
    }

    if (instring) {
	cookie->object = instring;
	flags |= F_STREAM_STR_IN;
    }
    if (buffer) {
	cookie->object = buffer;
	flags |= F_STREAM_BUFFER_IN;
    }

    cookie->flags           = flags;
    stream->contents.cookie = cookie;
    stream->flags           = flags;

    if (cookie->ptr_setup != NULL) {
	(*cookie->ptr_setup)(cookie);
    }
}

static object_t
stream_bytes_to_output(int64_t flags, char *buf, int64_t len)
{
    if (flags & F_STREAM_UTF8_OUT) {
	return con4m_new(tspec_utf8(), "cstring", buf, "length", len);
    }

    if (flags & F_STREAM_UTF32_OUT) {
	return con4m_new(tspec_utf32(), "cstring", buf, "codepoints", len / 4);
    }

    else {
	// Else, it's going to a buffer.
	return con4m_new(tspec_buffer(), "raw", buf, "length", len);
    }
}

// We generally assume con4m is passing around 64 bit sizes, but when
// dealing w/ the C API, things are often 32 bits (e.g., size_t).
// Therefore, for the internal API, we accept a 64-bit value in, but
// expect the write length to be a size_t because that's what fread()
// will give us.
//
// The final parameter here is meant for internal use, mainly for
// marshal, so we don't have to go through an object to read out
// things like ints that we plan on returning.

object_t
stream_raw_read(stream_t *stream, int64_t len, char *buf)
{
    // If a buffer is provided, return the length and write into
    // the buffer.
    bool return_len = (buf != NULL);

    if (stream->flags & F_STREAM_CLOSED) {
	CRAISE("Stream is already closed.");
    }

    if (!len) {
	if (return_len) {
	    return (object_t)(0); // I.e., null
	}
	return stream_bytes_to_output(stream->flags, "", 0);
    }

    int64_t flags  = stream->flags;
    size_t  actual = 0;

    if (!return_len) {
	buf = alloca(len);
    }

    if (! (flags & F_STREAM_READ)) {
	CRAISE("Cannot read; stream was not opened with read enabled.");
    }

    if (flags & F_STREAM_UTF32_OUT) {
	len *= 4;
    }

    if (stream->flags & F_STREAM_USING_COOKIE) {
	cookie_t      *cookie = stream->contents.cookie;
	stream_read_fn f      = cookie->ptr_read;

	actual = (*f)(cookie, buf, len);
    }
    else {
	actual = fread(buf, 1, len, stream->contents.f);
    }

    if (return_len) {
	return (object_t)(actual);
    }
    else {
	return stream_bytes_to_output(stream->flags, buf, actual);
    }
}

size_t
stream_raw_write(stream_t *stream, int64_t len, char *buf)
{
    size_t    actual = 0;
    cookie_t *cookie = NULL;

    if (stream->flags & F_STREAM_CLOSED) {
	CRAISE("Stream is already closed.");
    }

    if (len <= 0) {
	return 0;
    }

    if (stream->flags & F_STREAM_USING_COOKIE) {
	cookie = stream->contents.cookie;

	stream_write_fn f = cookie->ptr_write;

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
	    CRAISE("Custom stream implementation could write past EOF.");
	}
	else {
	    CRAISE("Custom stream implementation could not complete write.");
	}
    }

    else {
	if (!feof(stream->contents.f)) {
	    raise_errcode(ferror(stream->contents.f));
	}
    }

    return 0;
}

void
_stream_write_object(stream_t *stream, object_t obj, bool ansi)
{
    if (stream->flags & F_STREAM_CLOSED) {
	CRAISE("Stream is already closed.");
    }

    any_str_t *s = con4m_value_obj_repr(obj);
    if (ansi) {
	ansi_render(s, stream);
    }
    else {
	s = force_utf8(s);
	stream_raw_write(stream, s->byte_len, s->data);
    }
}

bool
stream_at_eof(stream_t *stream)
{
    if (stream->flags & F_STREAM_CLOSED) {
	return true;
    }

    if (stream->flags & F_STREAM_USING_COOKIE) {
	cookie_t *cookie = stream->contents.cookie;

	return cookie->position >= cookie->eof;
    }

    return feof(stream->contents.f);
}

int64_t
stream_get_location(stream_t *stream)
{
    if (stream->flags & F_STREAM_CLOSED) {
	CRAISE("Stream is already closed.");
    }

    if (stream->flags & F_STREAM_USING_COOKIE) {
	return stream->contents.cookie->position;
    }

    return ftell(stream->contents.f);
}

void
stream_set_location(stream_t *stream, int64_t offset)
{
    if (stream->flags & F_STREAM_CLOSED) {
	CRAISE("Stream is already closed.");
    }

    if (stream->flags & F_STREAM_USING_COOKIE) {
	cookie_t *cookie = stream->contents.cookie;

	if (offset < 0) {
	    offset += cookie->eof;
	}

	stream_seek_fn fn = cookie->ptr_seek;

	if (fn == NULL) {
	    CRAISE("Custom stream does not have the ability to seek.");
	}

	if ((*fn)(cookie, offset) == false) {
	    CRAISE("Seek position out of bounds for stream.");
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
	    stream->flags = F_STREAM_CLOSED;
	    raise_errno();
	}
    }
}

void
stream_close(stream_t *stream)
{
    if (stream->flags & F_STREAM_USING_COOKIE) {
	cookie_t *cookie = stream->contents.cookie;

	stream_close_fn fn = cookie->ptr_close;

	cookie->flags = F_STREAM_CLOSED;

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
    stream->flags      = F_STREAM_CLOSED;
}

void
stream_flush(stream_t *stream)
{
    if (stream->flags & F_STREAM_CLOSED) {
	CRAISE("Stream is already closed.");
    }

    if (stream->flags & F_STREAM_USING_COOKIE) {
	return; // Not actually buffering ATM.
    }

    if (fflush(stream->contents.f)) {
	stream->flags = F_STREAM_CLOSED;
	raise_errno();
    }
}

void
_print(object_t first, ...)
{

    va_list      args;
    object_t     cur     = first;
    karg_info_t *kargs   = NULL;
    stream_t    *stream  = NULL;
    codepoint_t  sep     = ' ';
    codepoint_t  end     = '\n';
    bool         flush   = false;
    int          ansi    = -1;
    int          numargs;

    va_start(args, first);

    if (first == NULL) {
	stream_putc(get_stdout(), '\n');
	return;
    }

    if (get_my_type(first) == tspec_kargs()) {
	kargs   = first;
	numargs = 0;
    }
    else {
	kargs = get_kargs_and_count(args, &numargs);
	numargs++;
    }

    if (kargs != NULL) {

	int64_t tmp;

	if (!kw_get(kargs, "stream", (int64_t *)&stream)) {
	    stream = get_stdout();
	}
	if (kw_get(kargs, "sep", &tmp)) {
	    sep = (codepoint_t)tmp;
	}
	if (kw_get(kargs, "end", &tmp)) {
	    end = (codepoint_t)tmp;
	}
	if (kw_get(kargs, "flush", &tmp)) {
	    end = (bool)flush;
	}
	if (kw_get(kargs, "force_color", &tmp)) {
	    ansi = 1;
	}
	else {
	    if (kw_get(kargs, "no_color", &tmp)) {
		ansi = tmp ? 0 : -1;
	    }
	}
    }

    if (stream == NULL) {
	stream = get_stdout();
    }

    if (ansi == -1) {
	int fno = stream_fileno(stream);

	if (fno == -1 || !isatty(fno)) {
	    ansi = 0;
	}
	else {
	    ansi = 1;
	}
    }

    for (int i = 0; i < numargs; i++) {

	if (i && sep) {
	    stream_putcp(stream, sep);
	}

	stream_write_object(stream, cur, (bool)ansi);
	cur = va_arg(args, object_t);
    }

    if (end) {
	stream_putcp(stream, end);
    }

    if (flush) {
	stream_flush(stream);
    }

    va_end(args);
}

const con4m_vtable stream_vtable = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	(con4m_vtable_entry)stream_init,
	NULL, // Aboslutelty nothing else.
    }
};

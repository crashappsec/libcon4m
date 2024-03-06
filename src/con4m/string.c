#include <con4m.h>

const int str_header_size = sizeof(int64_t) + sizeof(style_info_t *);

// From the start of the GC header, currently the
// 2nd (64-bit) word of the string struction is a pointer to track.
// first value indicates there's only one 64-bit value in the pmap.
const uint64_t pmap_str[2] = {
                              0x0000000000000001,
                              0x4000000000000000
                             };

static inline real_str_t *
c4str_internal_alloc_u8(int64_t len)
{
    real_str_t *result = con4m_gc_alloc(get_real_alloc_len(len), PMAP_STR);
    result->byte_len   = len;
    result->codepoints = 0;
    result->styling    = NULL;

    return result;
}

// Input parameter is number of codepoints, but internally we
// will work with the byte length of the string (without the
// null terminator, which is a 4 byte terminator w/ u32.
static inline real_str_t *
c4str_internal_alloc_u32(int64_t len)
{
    int32_t     byte_len = len << 2;
    real_str_t *result   = con4m_gc_alloc(get_real_alloc_len(byte_len),
                                         PMAP_STR);
    result->byte_len     = byte_len;
    result->codepoints   = ~0;
    result->styling      = NULL;

    return result;
}

static inline real_str_t *
c4str_internal_alloc(bool u32, int64_t len)
{
    if (u32) {
	return c4str_internal_alloc_u32(len);
    }
    else {
	return c4str_internal_alloc_u8(len);
    }
}

static inline void
apply_style_to_real_string(real_str_t *s, style_t style)
{
    alloc_styles(s, 1);
    s->styling->styles[0].start = 0;
    s->styling->styles[0].end   = s->byte_len;
    s->styling->styles[0].info  = style;
}

void
c4str_apply_style(str_t *s, style_t style)
{
    real_str_t *p = to_internal(s);
    apply_style_to_real_string(p, style);
}

static void
internal_set_u8_codepoint_count(real_str_t *instr)
{
    uint8_t *p   = (uint8_t *)instr->data;
    uint8_t *end = p + instr->byte_len;
    int32_t  cp;

    instr->codepoints = 0;

    while (p < end) {
	instr->codepoints += 1;
	p                 += utf8proc_iterate(p, 4, &cp);
    }
}

int64_t
c4str_byte_len(str_t *s)
{
    real_str_t *p = to_internal(s);
    return p->byte_len;
}

int64_t
c4str_len(str_t *s)
{
    real_str_t *p = to_internal(s);
    if (internal_is_u32(p)) {
	return ~(p->codepoints);
    }
    else {
	return p->codepoints;
    }
}

str_t *
c4str_concat(str_t *p1, str_t *p2)
{

    real_str_t *s1          = to_internal(p1);
    real_str_t *s2          = to_internal(p2);
    bool        u32         = internal_is_u32(s1);
    int64_t     cp_offset   = internal_num_cp(s1);
    int64_t     cp          = cp_offset + internal_num_cp(s2);
    real_str_t *r           = c4str_internal_alloc(u32, cp);
    uint64_t    num_entries = style_num_entries(s1) + style_num_entries(s2);

    assert(internal_is_u32(s1) == internal_is_u32(s2));

    alloc_styles(r, num_entries);

    if (style_num_entries(s1)) {
        copy_styles(r->styling, s1->styling, 0);
    }

    if (style_num_entries(s2)) {
        copy_styles(r->styling, s2->styling, style_num_entries(s1));

	// Here, we loop through after the copy to adjust the offsets.
	for (uint64_t i = style_num_entries(s1); i < num_entries; i++) {
	    r->styling->styles[i].start += cp_offset;
	    r->styling->styles[i].end   += cp_offset;
	}
    }

    // Now copy the actual string data.
    memcpy(r->data, s1->data, s1->byte_len);
    memcpy(&(r->data[s1->byte_len]), s2->data, s2->byte_len);

    r->codepoints = internal_num_cp(s1) + internal_num_cp(s2);
    if (internal_is_u32(s1)) {
	r->codepoints = ~r->codepoints;
    }

    // Null terminator was handled with the `new` operation.
    return r->data;
}

str_t *
c4str_u32_to_u8(str_t *instr)
{
    real_str_t *inp = to_internal(instr);

    if (!internal_is_u32(inp)) {
	return instr;
    }

    // Allocates 4 bytes per codepoint; this is an overestimate in cases
    // where UTF8 codepoints are above U+00ff. We need to count the actual
    // codepoints and adjust the `len` field at the end.

    real_str_t *r       = c4str_internal_alloc_u8(c4str_byte_len(instr));
    uint32_t   *p       = (uint32_t *)(inp->data);
    uint8_t    *outloc  = (uint8_t *)(&r->data[0]);
    int         l;

    for (int i = 0; i < inp->byte_len; i++) {
	l       = utf8proc_encode_char(p[i], outloc);
	outloc += l;
    }

    internal_set_u8_codepoint_count(r);
    r->byte_len = (int32_t)(outloc - (uint8_t *)(r->data));

    copy_style_info(inp, r);

    return r->data;
}

str_t *
c4str_u8_to_u32(str_t *instr)
{
    real_str_t *inraw = to_internal(instr);

    if (internal_is_u32(inraw)) {
	return instr;
    }
    int64_t     len    = (int64_t)internal_num_cp(inraw);
    real_str_t *outraw = c4str_internal_alloc_u32(len);
    uint8_t    *inp    = (uint8_t *)(inraw->data);
    int32_t    *outp   = (int32_t *)(outraw->data);

    for (int i = 0; i < len; i++) {
	inp += utf8proc_iterate(inp, 4, outp + i);
    }

    copy_style_info(inraw, outraw);

    outraw->codepoints = ~len;

    return outraw->data;
}

static inline str_t *
c4str_internal_base(_Bool u32, va_list args)
{
    // Begin keyword arguments
    int64_t  length  = -1;
    int64_t  start   = 0;
    char    *cstring = NULL;
    style_t  style   = STYLE_INVALID;

    method_kargs(args, length, start, cstring, style);
    // End keyword arguments

    real_str_t *p;

    if (cstring != NULL) {
	if (length == -1) {
	    length = strlen(cstring);
	}

	if (start > length) {
	    fprintf(stderr, "Invalid string constructor call: "
		    "len(cstring) is less than the start index");
	    abort();
	}
    }

    p = c4str_internal_alloc(u32, length);

    if (cstring) {
	if (u32) {
	    for (int64_t i = 0; i < length; i++) {
		((uint64_t *)p->data)[i] = (uint64_t)(cstring[i]);
	    }
	    p->codepoints = ~length;
	}
	else {
	    memcpy(p->data, cstring, length);
	  internal_set_u8_codepoint_count(p);
	}
    }

    if (style != STYLE_INVALID) {
	apply_style_to_real_string(p, style);
    } else {
	p->styling = NULL;
    }

    return p->data;
}

str_t *
c4str_internal_new_u8(va_list args)
{
    return c4str_internal_base(false, args);
}

// Input parameter is number of codepoints, but internally we
// will work with the byte length of the string (without the
// null terminator, which is a 4 byte terminator w/ u32.
str_t *
c4str_internal_new_u32(va_list args)
{
    return c4str_internal_base(true, args);
}

str_t *
c4str_from_file(char *name, int *err)
{
    // Assumes file is UTF-8.
    //
    // On BSDs, we might add O_EXLOCK. Should do similar on Linux too.
    int fd = open(name, O_RDONLY);
    if (fd == -1) {
	*err = errno;
	return NULL;
    }

    off_t len = lseek(fd, 0, SEEK_END);

    if (len == -1) {
    err:
	*err = errno;
	close(fd);
	return NULL;
    }
    if (lseek(fd, 0, SEEK_SET) == -1) {
	goto err;
    }

    str_t *result = con4m_new(T_STR, "length", len);
    char *p       = result;

    while (1) {
	ssize_t num_read = read(fd, p, len);

	if (num_read == -1) {
	    if (errno == EINTR || errno == EAGAIN) {
		continue;
	    }
	    goto err;
	}

	if (num_read == len) {
	    real_str_t *raw = to_internal(result);
	    internal_set_u8_codepoint_count(raw);
	    return result;
	}

	p   += num_read;
	len -= num_read;
    }
}

const uint64_t str_ptr_info[] = {
	  0x0000000000000001,
	  0x4000000000000000
};

const con4m_vtable u8str_vtable = {
    .num_entries = 1,
    .methods     = {
	(con4m_vtable_entry)c4str_internal_new_u8
    }
};

const con4m_vtable u32str_vtable = {
    .num_entries = 1,
    .methods     = {
	(con4m_vtable_entry)c4str_internal_new_u32
    }
};

#include <con4m_breaks.h>

const int str_header_size = sizeof(int64_t) + sizeof(style_info_t *);

str_t *
c4str_new(int64_t len)
{
    real_str_t *p = zalloc(len + str_header_size + 1);

    p->byte_len   = len;
    p->codepoints = 0;
    p->styling    = NULL;

    return p->data;
}

// Input parameter is number of codepoints, but internally we
// will work with the byte length of the string (without the
// null terminator, which is a 4 byte terminator w/ u32.
str_t *
c4str_new_u32(int64_t len)
{
    real_str_t *p = zalloc(4 * (len + 1) + str_header_size);
    p->byte_len   = len * 4;
    p->codepoints = ~(len);
    p->styling    = NULL;

    return p->data;
}

static inline void
apply_style_to_real_string(real_str_t *s, style_t style)
{
    alloc_styles(s, 1);
    s->styling->styles[0].start = 0;
    s->styling->styles[0].end   = s->byte_len;
    s->styling->styles[0].info  = style;
}

str_t *
c4str_new_with_style(int64_t len, style_t style)
{
    real_str_t *p = zalloc(len + str_header_size + 1);

    p->byte_len   = len;
    p->codepoints = 0;

    apply_style_to_real_string(p, style);
    return p->data;
}

str_t *
c4str_new_u32_with_style(int64_t len, style_t style)
{
    real_str_t *p = zalloc(4 * (len + 1) + str_header_size);
    p->byte_len   = len * 4;
    p->codepoints = ~(len);

    apply_style_to_real_string(p, style);

    return p->data;
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

str_t *
c4str_from_cstr(char *s)
{
    int64_t     l      = (int64_t)strlen(s);
    str_t      *result = c4str_new(l);
    real_str_t *r      = to_internal(result);

    memcpy(r->data, s, (size_t)(l + 1));

    internal_set_u8_codepoint_count(r);

    return result;
}

str_t *
c4str_from_cstr_styled(char *str, style_t style)
{
    str_t *res = c4str_from_cstr(str);
    c4str_apply_style(res, style);

    return res;
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

    str_t *result = c4str_new(len);
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

void
c4str_free(str_t *s)
{
    real_str_t *r = to_internal(s);

    if (r->styling != NULL) {
	free(r->styling);
    }

    free(r);
}

str_t *
c4str_concat(str_t *p1, str_t *p2, ownership_t ownership)
{

    real_str_t *s1          = to_internal(p1);
    real_str_t *s2          = to_internal(p2);
    bool        u32         = internal_is_u32(s1);
    int64_t     cp_offset   = u32 ? ~(s1->codepoints) : s1->codepoints;
    int64_t     newlen      = s1->byte_len + s2->byte_len;
    str_t      *result      = u32 ? c4str_new_u32(newlen >> 2) :
	                            c4str_new(newlen);
    real_str_t *r           = to_internal(result);
    uint64_t    num_entries = style_num_entries(s1) + style_num_entries(s2);
    uint64_t    st1_sz      = style_size(style_num_entries(s1));
    uint64_t    st2_sz      = style_size(style_num_entries(s2));


    assert(internal_is_u32(s1) == internal_is_u32(s2));

    alloc_styles(r, num_entries);

    if (style_num_entries(s1)) {
	memcpy(r->styling->styles, s1->styling->styles, st1_sz);
    }

    if (style_num_entries(s2)) {
	memcpy(&(r->styling->styles[style_num_entries(s1)]),
	       s2->styling->styles, st2_sz);

	// We loop through after the copy to adjust the offsets.
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

    if (ownership & CALLEE_P1) {
	c4str_free(p1);
    }

    if (ownership & CALLEE_P2) {
	c4str_free(p2);
    }

    // Null terminator was handled with the `new` operation.
    return result;
}

str_t *
c4str_u32_to_u8(str_t *instr, ownership_t ownership)
{
    real_str_t *inp = to_internal(instr);

    if (!internal_is_u32(inp)) {
	return instr;
    }

    // Allocates 4 bytes per codepoint; this is an overestimate in cases
    // where UTF8 codepoints are above U+00ff. We need to count the actual
    // codepoints and adjust the `len` field at the end.

    str_t      *result  = c4str_new(c4str_byte_len(instr));
    real_str_t *r       = to_internal(result);
    uint32_t   *p       = (uint32_t *)(inp->data);
    uint8_t    *outloc  = (uint8_t *)(&r->data[0]);
    int         l;

    for (int i = 0; i < inp->byte_len; i++) {
	l       = utf8proc_encode_char(p[i], outloc);
	outloc += l;
    }

    r->byte_len = (int32_t)(outloc - (uint8_t *)(r->data));
    internal_set_u8_codepoint_count(r);

    copy_style_info(inp, r);

    if (ownership) {
	c4str_free(instr);
    }

    return result;
}

str_t *
c4str_u8_to_u32(str_t *instr, ownership_t ownership)
{
    real_str_t *inraw = to_internal(instr);

    if (internal_is_u32(inraw)) {
	return instr;
    }
    int32_t     len    = internal_num_cp(inraw);
    str_t      *result = c4str_new_u32(len);
    real_str_t *outraw = to_internal(result);
    uint8_t    *inp    = (uint8_t *)(&inraw->data[0]);
    int32_t    *outp   = (int32_t *)(&outraw->data[0]);

    for (int i = 0; i < len; i++) {
	inp += utf8proc_iterate(inp, 4, outp + i);
    }

    copy_style_info(inraw, outraw);

    if (ownership) {
	c4str_free(instr);
    }

    return result;
}

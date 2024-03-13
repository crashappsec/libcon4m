#include <con4m.h>

const int str_header_size = sizeof(int64_t) + sizeof(style_info_t *);
// From the start of the GC header, currently the
// 2nd (64-bit) word of the string struction is a pointer to track.
// first value indicates there's only one 64-bit value in the pmap.
const uint64_t pmap_str[2] = {
                              0x0000000000000001,
                              0x4000000000000000
                             };

STATIC_ASCII_STR(empty_string_const, "");
STATIC_ASCII_STR(newline_const, "\n");
STATIC_ASCII_STR(crlf_const, "\r\n");

static inline real_str_t *
c4str_internal_alloc_u8(int64_t len)
{
    con4m_obj_t *obj    = con4m_gc_alloc(get_real_alloc_len(len), PMAP_STR);
    real_str_t  *result = (real_str_t *)obj->data;
    result->byte_len    = len;
    result->codepoints  = 0;
    result->styling     = NULL;
    obj->base_data_type = (con4m_dt_info *)&builtin_type_info[T_STR];
    obj->concrete_type  = T_STR;

    return result;
}

// Input parameter is number of codepoints, but internally we
// will work with the byte length of the string (without the
// null terminator, which is a 4 byte terminator w/ u32.
static inline real_str_t *
c4str_internal_alloc_u32(int64_t len)
{
    int32_t     byte_len = len << 2;
    con4m_obj_t *obj     = con4m_gc_alloc(get_real_alloc_len(byte_len),
                                         PMAP_STR);
    real_str_t *result   = (real_str_t *)obj->data;
    result->byte_len     = byte_len;
    result->codepoints   = ~0;
    result->styling      = NULL;
    obj->base_data_type  = (con4m_dt_info *)&builtin_type_info[T_STR];
    obj->concrete_type   = T_STR;

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

void
c4str_apply_style(str_t *s, style_t style)
{
    if (!c4str_len(s)) {
	return;
    }

    real_str_t *p = to_internal(s);
    apply_style_to_real_string(p, style);
}

static void
internal_set_u8_codepoint_count(real_str_t *instr)
{
    uint8_t     *p   = (uint8_t *)instr->data;
    uint8_t     *end = p + instr->byte_len;
    codepoint_t  cp;

    instr->codepoints = 0;

    while (p < end) {
	instr->codepoints += 1;
	p                 += utf8proc_iterate(p, 4, &cp);
    }
}

int64_t
c4str_byte_len(const str_t *s)
{
    real_str_t *p = to_internal(s);
    return p->byte_len;
}

int64_t
c4str_len(const str_t *s)
{
    if (!s) {
	return 0;
    }
    real_str_t *p = to_internal(s);
    if (internal_is_u32(p)) {
	return ~(p->codepoints);
    }
    else {
	return p->codepoints;
    }
}

// For now, we're going to do this just for u32, so u8 will convert to
// u32, in full.
str_t *
c4str_slice(const str_t *instr, int64_t start, int64_t end)
{
    if (!instr) {
	return (str_t *)instr;
    }
    str_t      *s   = force_utf32(instr);
    real_str_t *r   = to_internal(s);
    int64_t     len = internal_num_cp(r);

    if (start < 0) {
	start += len;
    }
    else {
	if (start >= len) {
	    return empty_string();
	}
    }
    if (end < 0) {
	end += len + 1;
    }
    else {
	if (end > len) {
	    end = len;
	}
    }
    if ((start | end) < 0 || start >= end) {
	return empty_string();
    }

    int64_t slice_len = end - start;
    real_str_t *res   = c4str_internal_alloc_u32(slice_len);
    res->codepoints   = ~(slice_len);

    codepoint_t *src = (codepoint_t *)(s);
    codepoint_t *dst = (codepoint_t *)(res->data);

    for (int i = 0; i < slice_len; i++) {
	dst[i] = src[start + i];
    }

    if (r->styling && r->styling->num_entries) {
	int64_t first = -1;
	int64_t last  = 0;
	int64_t i     = 0;

	for (i = 0; i < r->styling->num_entries; i++) {
	    if (r->styling->styles[i].end < start) {
		continue;
	    }
	    if (r->styling->styles[i].start >= end) {
		break;
	    }
	    if (first == -1) {
		first = i;
	    }
	    last = i + 1;
	}

	if (first == -1) {
	    goto finish_up;
	}

	last = i;
	while (true) {
	    if (r->styling->styles[++last].end >= end) {
		break;
	    }
	    if (i == r->styling->num_entries) {
		break;
	    }
	    if (r->styling->styles[last].start >= end) {
		break;
	    }
	}

	if (last == -1) {
	    last = first + 1;
	}
	int64_t sliced_style_count = last - first;
	alloc_styles(res, sliced_style_count);

	for (i = 0; i < sliced_style_count; i++) {
	    int64_t sold = r->styling->styles[i + first].start;
	    int64_t eold = r->styling->styles[i + first].end;
	    style_t info = r->styling->styles[i + first].info;
	    int64_t snew = max(sold - start, 0);
	    int64_t enew = min(r->styling->styles[i + first].end, end) - start;

	    if (enew > slice_len) {
		enew = slice_len;
	    }

	    if (snew >= enew) {
		res->styling->num_entries = i;
		break;
	    }

	    res->styling->styles[i].start = snew;
	    res->styling->styles[i].end   = enew;
	    res->styling->styles[i].info  = info;
	}

    }

finish_up:
    if (!internal_is_u32(to_internal(instr))) {
	return c4str_u32_to_u8(res->data);
    }
    else {
	return res->data;
    }
}

str_t *
_c4str_strip(const str_t *s, ...)
{
    // TODO: this is needlessly slow for u8 since we convert it to u32
    // twice, both here and in slice.
    DECLARE_KARGS(
	bool front = true;
	bool back  = true;
	);

    kargs(s, front, back);

    real_str_t  *real  = to_internal(force_utf32(s));
    codepoint_t *p     = (codepoint_t *)real->data;
    int64_t      start = 0;
    int          len   = c4str_len(s);
    int          end   = len;

    if (front) {
	while (start < end && internal_is_space(p[start])) start++;
    }

    if (back) {
	while (--end != start) {
	    if (!internal_is_space(p[end])) {
		break;
	    }
	}
	end++;
    }

    if (!start && end == len) {
	return (str_t *)s;
    }

    return c4str_slice(s, start, end);
}

str_t *
c4str_copy(const str_t *s)
{
    real_str_t *cur = to_internal(s);
    bool        u32 = internal_is_u32(cur);
    real_str_t *res = c4str_internal_alloc(u32, internal_num_cp(cur));
    res->codepoints = cur->codepoints;
    memcpy(res->data, cur->data, cur->byte_len);
    copy_style_info(cur, res);

    return res->data;
}

str_t *
c4str_concat(const str_t *p1, const str_t *p2)
{

    real_str_t *s1          = to_internal(force_utf32(p1));
    real_str_t *s2          = to_internal(force_utf32(p2));
    int64_t     s1_len      = internal_num_cp(s1);
    int64_t     s2_len      = internal_num_cp(s2);
    int64_t     n           = s1_len + s2_len;
    real_str_t *r           = c4str_internal_alloc_u32(n);
    uint64_t    num_entries = style_num_entries(s1) + style_num_entries(s2);

    if (!s1_len) {
	return (str_t *)p2;
    }

    if (!s2_len) {
	return (str_t *)p1;
    }

    alloc_styles(r, num_entries);

    int start = style_num_entries(s1);
    if (start) {
	for (unsigned int i = 0; i < s1->styling->num_entries; i++) {
	    r->styling->styles[i] = s1->styling->styles[i];
	}
    }

    if (style_num_entries(s2)) {
	int start = style_num_entries(s1);
	for (unsigned int i = 0; i < s2->styling->num_entries; i++) {
	    r->styling->styles[i + start] = s2->styling->styles[i];
	}

	// Here, we loop through after the copy to adjust the offsets.
	for (uint64_t i = style_num_entries(s1); i < num_entries; i++) {
	    r->styling->styles[i].start += s1_len;
	    r->styling->styles[i].end   += s1_len;
	}
    }

    // Now copy the actual string data.
    uint32_t *ptr = (uint32_t *)r->data;
    memcpy(r->data, s1->data, s1_len * 4);
    memcpy(ptr + s1_len, s2->data, s2_len * 4);

    r->codepoints = ~n;

    // Null terminator was handled with the `new` operation.
    return r->data;
}

str_t *
_c4str_join(const xlist_t *l, const str_t *joiner, ...)
{
    DECLARE_KARGS(
	bool add_trailing = false;
	bool utf8         = false;
	);

    kargs(joiner, add_trailing, utf8);

    int64_t n_parts  = xlist_len(l);
    int64_t n_styles = 0;
    int64_t joinlen  = c4str_len(joiner);
    int64_t len      = joinlen * n_parts; // overestimate when !add_trailing.

    for (int i = 0; i < n_parts; i++) {
	str_t *line = (str_t *)xlist_get(l, i, NULL);
	len        += c4str_len(line);
	n_styles   += cstr_num_styles(line);
    }


    real_str_t  *r        = c4str_internal_alloc_u32(len);
    real_str_t  *j        = to_internal(force_utf32(joiner));
    codepoint_t *p        = (codepoint_t *)r->data;
    int          txt_ix   = 0;
    int          style_ix = 0;


    r->codepoints = ~len;
    alloc_styles(r, n_styles);

    if (!add_trailing) {
	--n_parts; // skip the last item during the loop.
    }

    for (int i = 0; i < n_parts; i++) {
	real_str_t *line = to_internal(
	    force_utf32((str_t *)xlist_get(l, i, NULL)));
	int64_t     n_cp = internal_num_cp(line);

	memcpy(p, line->data, n_cp * 4);
	p       += n_cp;
	style_ix = copy_and_offset_styles(line, r, style_ix, txt_ix);
	txt_ix  += n_cp;

	memcpy(p, j->data, joinlen * 4);
	p += joinlen;
	style_ix = copy_and_offset_styles(j, r, style_ix, txt_ix);
	txt_ix  += joinlen;
    }

    if (!add_trailing) {
	real_str_t *line = to_internal(
	    force_utf32((str_t *)xlist_get(l, n_parts, NULL)));
	int64_t     n_cp = internal_num_cp(line);

	memcpy(p, line->data, n_cp * 4);
	style_ix = copy_and_offset_styles(line, r, style_ix, txt_ix);
    }

    if (utf8) {
	return force_utf8((str_t *)r->data);
    } else {
	return (str_t *)r->data;
    }
}

str_t *
c4str_u32_to_u8(const str_t *instr)
{
    real_str_t *inp = to_internal(instr);
    int64_t     len = internal_num_cp(inp);

    if (!internal_is_u32(inp)) {
	return (str_t *)instr;
    }

    // Allocates 4 bytes per codepoint; this is an overestimate in cases
    // where UTF8 codepoints are above U+00ff. We need to count the actual
    // codepoints and adjust the `len` field at the end.

    real_str_t  *r       = c4str_internal_alloc_u8(c4str_byte_len(instr));
    codepoint_t *p       = (codepoint_t *)(inp->data);
    uint8_t     *outloc  = (uint8_t *)(&r->data[0]);
    int          l;

    for (int i = 0; i < len; i++) {
	l       = utf8proc_encode_char(p[i], outloc);
	outloc += l;
    }

    r->codepoints = len;
    r->byte_len   = (int32_t)(outloc - (uint8_t *)(r->data));

    copy_style_info(inp, r);

    return r->data;
}

str_t *
c4str_u8_to_u32(const str_t *instr)
{
    real_str_t *inraw = to_internal(instr);

    if (internal_is_u32(inraw)) {
	return (str_t *)instr;
    }
    int64_t      len    = (int64_t)internal_num_cp(inraw);
    real_str_t  *outraw = c4str_internal_alloc_u32(len);
    uint8_t     *inp    = (uint8_t *)(inraw->data);
    codepoint_t *outp   = (codepoint_t *)(outraw->data);

    for (int i = 0; i < len; i++) {
	inp += utf8proc_iterate(inp, 4, outp + i);
    }

    outraw->codepoints = ~len;

    copy_style_info(inraw, outraw);

    return outraw->data;
}

static inline str_t *
c4str_internal_base(_Bool u32, va_list args)
{
    DECLARE_KARGS(
	int64_t  length  = -1;
	int64_t  start   = 0;
	char    *cstring = NULL;
	style_t  style   = STYLE_INVALID;
	);

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
c4str_from_int(int64_t n)
{
    char  buf[21] = {0, };
    char *p       = &buf[20];
    char *end     = p;

    if (!n) {
	return c4str_repeat_u8('0', 1);
    }

    int64_t i = n;

    while (i) {
	*--p = '0' + (i % 10);
	i /= 10;
    }

    if (n < 0) {
	*--p = '-';
    }
    return con4m_new(T_STR, "cstring", p);
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

// For repeat, we leave an extra alloc'd character to ensure we
// can easily drop in a newline.
str_t *
c4str_repeat_u8(codepoint_t cp, int64_t num)
{
    uint8_t     buf[4] = {0, };
    int         l      = utf8proc_encode_char(cp, &buf[0]);
    int         blen   = l * num;
    real_str_t *res    = c4str_internal_alloc_u8(blen + 1);
    int         buf_ix = 0;

    res->codepoints = l;
    char *p         = res->data;

    for (int i = 0; i < blen; i++) {
	p[i] = buf[buf_ix++];
	buf_ix %= l;
    }

    return (str_t *)p;
}

str_t *
c4str_repeat(codepoint_t cp, int64_t num)
{
    if (num <= 0) {
	return empty_string();
    }

    real_str_t *res = c4str_internal_alloc_u32(num + 1);
    res->codepoints = ~num;

    codepoint_t *p = (codepoint_t *)res->data;

    for (int i = 0; i < num; i++) {
	*p++ = cp;
    }

    return res->data;
}

int64_t
c4str_render_len(const str_t *s)
{
    int64_t     result = 0;
    real_str_t *r      = to_internal(s);
    int64_t    n       = internal_num_cp(r);

    if (internal_is_u32(r)) {
	codepoint_t *s = (codepoint_t *)r->data;

	for (int i = 0; i < n; i++) {
	    result += codepoint_width(s[i]);
	}
    }
    else {
	uint8_t     *p = (uint8_t *)r->data;
	codepoint_t  cp;

	for (int i = 0; i < n; i++) {
	    p += utf8proc_iterate(p, 4, &cp);
	    result += codepoint_width(cp);
	}
    }
    return result;
}

str_t *
_c4str_truncate(const str_t *s, int64_t len, ...)
{
    DECLARE_KARGS(
	bool use_render_width = false;
	);

    kargs(len, use_render_width);

    real_str_t *r = to_internal(s);
    int64_t     n = internal_num_cp(r);
    int64_t     c = 0;

    if (internal_is_u32(r)) {
	if (use_render_width) {
	    for (int i = 0; i < n; i++) {
		int w = codepoint_width(s[i]);
		if (c + w > len) {
		    return c4str_slice(s, 0, i);
		}
	    }
	    return (str_t *)s; // Didn't need to truncate.
	}
	if (n <= len) {
	    return (str_t *)s;
	}
	return c4str_slice(s, 0, len);
    }
    else {
	uint8_t    *p = (uint8_t *)r->data;
	uint8_t    *next;
	codepoint_t cp;
	int64_t     num_cp = 0;

	if (use_render_width) {
	    for (int i = 0; i < n; i++) {
		next   = p + utf8proc_iterate(p, 4, &cp);
		int w  = codepoint_width(cp);
		if (c + w > len) {
		    goto u8_slice;
		}
		num_cp++;
		p = next;
	    }
	    return (str_t *)s;
	}

	else {
	    num_cp = len;

	    for (int i = 0; i < n; i++) {
		if (c++ == len) {
		u8_slice:
		    {
			uint8_t    *start = (uint8_t *)r->data;
			int64_t     blen  = p - start;
			real_str_t *res   = c4str_internal_alloc_u8(blen);

			memcpy(res->data, start, blen);
			copy_style_info(res, r);

			if (res->styling != NULL) {
			    for (int i = 0; i < res->styling->num_entries; i++)
			    {
				style_entry_t e = res->styling->styles[i];
				if (e.start > num_cp) {
				    res->styling->num_entries = i;
				    return res->data;
				}
				if (e.end > num_cp) {
				    res->styling->styles[i].end = num_cp;
				    res->styling->num_entries = i + 1;
				    return res->data;
				}
			    }
			}
			return res->data;
		    }
		}
   	        p += utf8proc_iterate(p, 4, &cp);
	    }
	    return (str_t *)s;
	}
    }
}

str_t *
c4str_from_file(const char *name, int *err)
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

static str_t *
c4str_repr(str_t *str, to_str_use_t how)
{
    // TODO: actually implement string quoting.
    if (how == TO_STR_USE_QUOTED) {
	str_t *q = con4m_new(T_UTF32, "cstring", "\"");
	return c4str_concat(c4str_concat(q, ((real_str_t *)str)->data), q);
    }
    else {
	return ((real_str_t *)str)->data;
    }
}

const con4m_vtable u8str_vtable = {
    .num_entries = 2,
    .methods     = {
	(con4m_vtable_entry)c4str_internal_new_u8,
	(con4m_vtable_entry)c4str_repr,
    }
};

const con4m_vtable u32str_vtable = {
    .num_entries = 2,
    .methods     = {
	(con4m_vtable_entry)c4str_internal_new_u32,
	(con4m_vtable_entry)c4str_repr,
    }
};

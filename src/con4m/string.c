#include <con4m.h>

// The object header has 4 words that we don't need to scan (there is
// a heap pointer in there, but it points to something definitely
// always available from the roots).
//
// Our pointer shows in our second word. Therefore, the 6th most
// significant bit gets set here.
const uint64_t pmap_str[2] = {
                              0x0000000000000001,
                              0x0400000000000000
                             };

STATIC_ASCII_STR(empty_string_const, "");
STATIC_ASCII_STR(newline_const, "\n");
STATIC_ASCII_STR(crlf_const, "\r\n");

static void
utf8_set_codepoint_count(utf8_t *instr)
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
utf8_validate(const utf8_t *instr)
{
    uint8_t     *p   = (uint8_t *)instr->data;
    uint8_t     *end = p + instr->byte_len;
    codepoint_t  cp;
    int64_t      n   = 0;

    while (p < end) {
	int to_add = utf8proc_iterate(p, 4, &cp);

	if (to_add < 0) {
	    return n;
	}

	p += to_add;

	n++;
    }

    return 0;
}

// For now, we're going to do this just for u32, so u8 will convert to
// u32, in full.
utf32_t *
string_slice(const any_str_t *instr, int64_t start, int64_t end)
{
    if (!instr) {
	return force_utf32(instr);
    }
    utf32_t *s   = force_utf32(instr);
    int64_t  len = string_codepoint_len(s);

    if (start < 0) {
	start += len;
    }
    else {
	if (start >= len) {
	    return force_utf32(empty_string());
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
	return force_utf32(empty_string());
    }

    int64_t slice_len = end - start;
    utf32_t *res      = con4m_new(tspec_utf32(), "length", slice_len);
    res->codepoints   = ~(slice_len);

    codepoint_t *src = (codepoint_t *)s->data;
    codepoint_t *dst = (codepoint_t *)res->data;

    for (int i = 0; i < slice_len; i++) {
	dst[i] = src[start + i];
    }

    if (s->styling && s->styling->num_entries) {
	int64_t first = -1;
	int64_t last  = 0;
	int64_t i     = 0;

	for (i = 0; i < s->styling->num_entries; i++) {
	    if (s->styling->styles[i].end < start) {
		continue;
	    }
	    if (s->styling->styles[i].start >= end) {
		break;
	    }
	    if (first == -1) {
		first = i;
	    }
	    last = i + 1;
	}

	if (first == -1) {
	    return res;
	}

	last = i;
	while (true) {
	    if (s->styling->styles[++last].end >= end) {
		break;
	    }
	    if (i == s->styling->num_entries) {
		break;
	    }
	    if (s->styling->styles[last].start >= end) {
		break;
	    }
	}

	if (last == -1) {
	    last = first + 1;
	}
	int64_t sliced_style_count = last - first;
	alloc_styles(res, sliced_style_count);

	for (i = 0; i < sliced_style_count; i++) {
	    int64_t sold = s->styling->styles[i + first].start;
	    style_t info = s->styling->styles[i + first].info;
	    int64_t snew = max(sold - start, 0);
	    int64_t enew = min(s->styling->styles[i + first].end, end) - start;

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
    return res;
}

codepoint_t
utf8_index(const utf8_t *s, int64_t n)
{
    if (!string_is_u8(s)) {
	return utf32_index(s, n);
    }

    int64_t l = string_codepoint_len(s);

    if (n < 0) {
	n += l;

	if (n < 0) {
	    CRAISE("Index would be before the start of the string.");
	}
    }

    if (n >= l) {
	CRAISE("Index out of bounds.");
    }

    char       *p = (char *)s->data;
    codepoint_t cp;

    for (int i = 0; i <= n; i++) {
	p += utf8proc_iterate((uint8_t *)p, 4, &cp);
    }

    return cp;
}

codepoint_t
utf32_index(const utf32_t *s, int64_t i)
{
    if (string_is_u8(s)) {
	return utf8_index(s, i);
    }

    int64_t l = string_codepoint_len(s);

    if (i < 0) {
	i += l;

	if (i < 0) {
	    CRAISE("Index would be before the start of the string.");
	}
    }

    if (i >= l) {
	CRAISE("Index out of bounds.");
    }

    codepoint_t *p = (codepoint_t *)s->data;

    return p[i];
}

utf32_t *
_string_strip(const any_str_t *s, ...)
{
    // TODO: this is needlessly slow for u8 since we convert it to u32
    // twice, both here and in slice.
    DECLARE_KARGS(
	bool front = true;
	bool back  = true;
	);

    kargs(s, front, back);

    utf32_t     *as32  = force_utf32(s);
    codepoint_t *p     = (codepoint_t *)as32->data;
    int64_t      start = 0;
    int          len   = string_codepoint_len(as32);
    int          end   = len;

    if (front) {
	while (start < end && codepoint_is_space(p[start])) start++;
    }

    if (back) {
	while (--end != start) {
	    if (!codepoint_is_space(p[end])) {
		break;
	    }
	}
	end++;
    }

    if (!start && end == len) {
	return as32;
    }

    return string_slice(as32, start, end);
}

any_str_t *
string_copy(const any_str_t *s)
{
    if (s == NULL) {
	return NULL;
    }
    bool        u8  = string_is_u8(s);
    uint64_t     l  = u8 ? s->byte_len : ~s->codepoints;
    any_str_t *res  = con4m_new(u8 ? tspec_utf8() : tspec_utf32(), "length", l);

    res->codepoints = s->codepoints;
    memcpy(res->data, s->data, s->byte_len);
    copy_style_info(s, res);

    return res;
}

utf32_t *
string_concat(const any_str_t *p1, const any_str_t *p2)
{
    utf32_t  *s1          = force_utf32(p1);
    utf32_t  *s2          = force_utf32(p2);
    int64_t   s1_len      = string_codepoint_len(s1);
    int64_t   s2_len      = string_codepoint_len(s2);
    int64_t   n           = s1_len + s2_len;
    utf32_t  *r           = con4m_new(tspec_utf32(), "length", n);
    uint64_t  num_entries = style_num_entries(s1) + style_num_entries(s2);

    if (!s1_len) {
	return s2;
    }

    if (!s2_len) {
	return s1;
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

    return r;
}

utf32_t *
_string_join(const xlist_t *l, const any_str_t *joiner, ...)
{
    DECLARE_KARGS(
	bool add_trailing = false;
	);

    kargs(joiner, add_trailing);

    int64_t n_parts  = xlist_len(l);
    int64_t n_styles = 0;
    int64_t joinlen  = string_codepoint_len(joiner);
    int64_t len      = joinlen * n_parts; // An overestimate when !add_trailing

    for (int i = 0; i < n_parts; i++) {
	any_str_t *line = (any_str_t *)xlist_get(l, i, NULL);
	len            += string_codepoint_len(line);
	n_styles       += style_num_entries(line);
    }


    utf32_t     *result   = con4m_new(tspec_utf32(), "length", len);
    codepoint_t *p        = (codepoint_t *)result->data;
    int          txt_ix   = 0;
    int          style_ix = 0;
    utf32_t     *j        = joinlen ? force_utf32(joiner) : NULL;

    result->codepoints = ~len;
    alloc_styles(result, n_styles);

    if (!add_trailing) {
	--n_parts; // skip the last item during the loop.
    }

    for (int i = 0; i < n_parts; i++) {
	utf32_t *line = force_utf32((any_str_t *)xlist_get(l, i, NULL));
	int64_t  n_cp = string_codepoint_len(line);

	memcpy(p, line->data, n_cp * 4);
	p       += n_cp;
	style_ix = copy_and_offset_styles(line, result, style_ix, txt_ix);
	txt_ix  += n_cp;

	if (joinlen != 0) {
	    memcpy(p, j->data, joinlen * 4);
	    p += joinlen;
	    style_ix = copy_and_offset_styles(j, result, style_ix, txt_ix);
	    txt_ix  += joinlen;
	}
    }

    if (!add_trailing) {
	utf32_t *line = force_utf32((any_str_t *)xlist_get(l, n_parts, NULL));
	int64_t  n_cp = string_codepoint_len(line);

	memcpy(p, line->data, n_cp * 4);
	style_ix = copy_and_offset_styles(line, result, style_ix, txt_ix);
    }
    return result;
}

utf8_t *
utf32_to_utf8(const utf32_t *inp)
{
    if (!string_is_u32(inp)) {
	return (utf8_t *)inp;
    }

    // Allocates 4 bytes per codepoint; this is an overestimate in
    // cases where UTF8 codepoints are above U+00ff. But nbd.

    utf8_t      *res     = con4m_new(tspec_utf8(), "length", inp->byte_len);
    codepoint_t *p       = (codepoint_t *)inp->data;
    uint8_t     *outloc  = (uint8_t *)res->data;
    int          l;

    res->codepoints = string_codepoint_len(inp);

    for (int i = 0; i < res->codepoints; i++) {
	l       = utf8proc_encode_char(p[i], outloc);
	outloc += l;
    }

    copy_style_info(inp, res);

    return res;
}

utf32_t *
utf8_to_utf32(const utf8_t *instr)
{
    if (!instr || string_is_u32(instr)) {
	return (utf32_t *)instr;
    }

    int64_t      len    = (int64_t)string_codepoint_len(instr);
    utf32_t     *outstr = con4m_new(tspec_utf32(), "length", len);
    uint8_t     *inp    = (uint8_t *)(instr->data);
    codepoint_t *outp   = (codepoint_t *)(outstr->data);

    for (int i = 0; i < len; i++) {
	inp += utf8proc_iterate(inp, 4, outp + i);
    }

    outstr->codepoints = ~len;

    copy_style_info(instr, outstr);

    return outstr;
}

static void
utf8_init(utf8_t *s, va_list args)
{
    DECLARE_KARGS(
	int64_t length        = -1;  // BYTE length.
	int64_t start         = 0;
	char   *cstring       = NULL;
	style_t style         = STYLE_INVALID;
	int     replace_style = 1;
	char   *tag           = NULL;
	);

    method_kargs(args, length, start, cstring, style, replace_style, tag);

    if (cstring != NULL) {
	if (length < 0) {
	    length = strlen(cstring);
	}

	if (start > length) {
	    CRAISE("Invalid string constructor call: "
		    "len(cstring) is less than the start index");
	}

	s->data     = con4m_gc_alloc(length + 1, NULL);
	s->byte_len = length;

	memcpy(s->data, cstring, length);
	utf8_set_codepoint_count(s);
    } else {
	if (length < 0) {
	    CRAISE("length cannot be < 0 for string initialization");
	}
	s->data = con4m_gc_alloc(length + 1, NULL);
    }

    if (style != STYLE_INVALID) {
	string_apply_style(s, style, replace_style ? true : false);
    }

    if (tag != NULL) {
	render_style_t *rs = lookup_cell_style(tag);
	if (rs != NULL) {
	    string_apply_style(s, rs->base_style, replace_style ? true : false);
	}
    }
}

static void
utf32_init(utf32_t *s, va_list args)
{
    DECLARE_KARGS(
	int64_t      length        = -1;  // NUMBER OF CODEPOINTS.
	int64_t      start         = 0;
	char        *cstring       = NULL;
	codepoint_t *codepoints    = NULL;
	style_t      style         = STYLE_INVALID;
	int          replace_style = 1;
	);

    method_kargs(args, length, start, cstring, codepoints, style);

    if (codepoints != NULL && cstring != NULL) {
	CRAISE("Cannot specify both 'codepoints' and 'cstring' keywords.");
    }
    if (codepoints != NULL) {
	if (length == 0) {
	    s->byte_len   = 4;
	    s->data       = con4m_gc_alloc(s->byte_len, NULL);
	    s->codepoints = ~0;
	    return;
	}

	if (length < 0) {
	    CRAISE("When specifying 'codepoints', must provide a valid "
		   "'length' containing the number of codepoints.");
	}
	s->byte_len = (length + 1) * 4;
	s->data     = con4m_gc_alloc(s->byte_len, NULL);

	codepoint_t *local = (codepoint_t *)s->data;

	for (int i = 0; i < length; i++) {
	    local[i] = codepoints[i];
	}

	s->codepoints = ~length;
    } else {
	if (cstring != NULL) {
	    if (length == -1) {
		length = strlen(cstring);
	    }

	    if (start > length) {
		CRAISE("Invalid string constructor call: "
		       "len(cstring) is less than the start index");
	    }
	    s->byte_len = (length + 1) * 4;
	    s->data     = con4m_gc_alloc(s->byte_len, NULL);

	    for (int64_t i = 0; i < length; i++) {
		((uint32_t *)s->data)[i] = (uint32_t)(cstring[i]);
	    }
	    s->codepoints = ~length;
	} else {
	    if (length < 0) {
		CRAISE("Must specify a valid length if not initializing "
		       "with a null-terminated cstring.");
	    }
	    s->byte_len = (length + 1) * 4;
	    s->data        = con4m_gc_alloc(s->byte_len, NULL);
	}
    }

    if (style != STYLE_INVALID) {
	string_apply_style(s, style, replace_style ? true : false);
    }
}

utf8_t *
string_from_int(int64_t n)
{
    char  buf[21] = {0, };
    char *p       = &buf[20];

    if (!n) {
	return utf8_repeat('0', 1);
    }

    int64_t i = n;

    while (i) {
	*--p = '0' + (i % 10);
	i /= 10;
    }

    if (n < 0) {
	*--p = '-';
    }

    return con4m_new(tspec_utf8(), "cstring", p);
}

// For repeat, we leave an extra alloc'd character to ensure we
// can easily drop in a newline.
utf8_t *
utf8_repeat(codepoint_t cp, int64_t num)
{
    uint8_t   buf[4] = {0, };
    int       buf_ix = 0;
    int       l      = utf8proc_encode_char(cp, &buf[0]);
    int       blen   = l * num;
    utf8_t   *res    = con4m_new(tspec_utf8(), "length", blen + 1);
    char     *p      = res->data;

    res->codepoints = l;


    for (int i = 0; i < blen; i++) {
	p[i] = buf[buf_ix++];
	buf_ix %= l;
    }

    return res;
}

utf32_t *
utf32_repeat(codepoint_t cp, int64_t num)
{
    if (num <= 0) {
	return empty_string();
    }

    utf32_t     *res = con4m_new(tspec_utf8(), "length", num + 1);
    codepoint_t *p   = (codepoint_t *)res->data;

    res->codepoints = ~num;

    for (int i = 0; i < num; i++) {
	*p++ = cp;
    }

    return res;
}

int64_t
string_render_len(const any_str_t *s)
{
    int64_t result = 0;
    int64_t n      = string_codepoint_len(s);

    if (string_is_u32(s)) {
	codepoint_t *p = (codepoint_t *)s->data;

	for (int i = 0; i < n; i++) {
	    result += codepoint_width(p[i]);
	}
    }
    else {
	uint8_t     *p = (uint8_t *)s->data;
	codepoint_t  cp;

	for (int i = 0; i < n; i++) {
	    p += utf8proc_iterate(p, 4, &cp);
	    result += codepoint_width(cp);
	}
    }
    return result;
}

any_str_t *
_string_truncate(const any_str_t *s, int64_t len, ...)
{
    DECLARE_KARGS(
	int use_render_width = 0;
	);

    kargs(len, use_render_width);

    int64_t    n = string_codepoint_len(s);
    int64_t    c = 0;

    if (string_is_u32(s)) {
	codepoint_t *p = (codepoint_t *)s->data;

	if (use_render_width) {
	    for (int i = 0; i < n; i++) {
		int w = codepoint_width(p[i]);
		if (c + w > len) {
		    return string_slice(s, 0, i);
		}
	    }
	    return (any_str_t *)s; // Didn't need to truncate.
	}
	if (n <= len) {
	    return (any_str_t *)s;
	}
	return string_slice(s, 0, len);
    }
    else {
	uint8_t    *p = (uint8_t *)s->data;
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
	    return (any_str_t *)s;
	}

	else {
	    num_cp = len;

	    for (int i = 0; i < n; i++) {
		if (c++ == len) {
		u8_slice:
                // Since we don't have a full u8 slice yet...
		    {
			uint8_t  *start = (uint8_t *)s->data;
			int64_t   blen  = p - start;
			utf8_t   *res   = con4m_new(tspec_utf8(),
						    "length", blen);

			memcpy(res->data, start, blen);
			copy_style_info(s, res);

			if (res->styling != NULL) {
			    for (int i = 0; i < res->styling->num_entries; i++)
			    {
				style_entry_t e = res->styling->styles[i];
				if (e.start > num_cp) {
				    res->styling->num_entries = i;
				    return res;
				}
				if (e.end > num_cp) {
				    res->styling->styles[i].end = num_cp;
				    res->styling->num_entries = i + 1;
				    return res;
				}
			    }
			}
			return res;
		    }
		}
   	        p += utf8proc_iterate(p, 4, &cp);
	    }
	    return (any_str_t *)s;
	}
    }
}

utf8_t *
utf8_from_file(const any_str_t *name, int *err)
{
    utf32_t *n = force_utf32(name);

    // Assumes file is UTF-8.
    //
    // On BSDs, we might add O_EXLOCK. Should do similar on Linux too.
    int fd = open(n->data, O_RDONLY);
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

    utf8_t *result = con4m_new(tspec_utf8(), "length", len);
    char *p       = result->data;

    while (1) {
	ssize_t num_read = read(fd, p, len);

	if (num_read == -1) {
	    if (errno == EINTR || errno == EAGAIN) {
		continue;
	    }
	    goto err;
	}

	if (num_read == len) {
	    utf8_set_codepoint_count(result);
	    return result;
	}

	p   += num_read;
	len -= num_read;
    }
}

int64_t
_string_find(any_str_t *str, any_str_t *sub, ...)
{
    DECLARE_KARGS(
	int64_t start = 0;
	int64_t end   = -1;
	);

    kargs(sub, start, end);

    str = force_utf32(str);
    sub = force_utf32(sub);

    uint64_t  strcp = string_codepoint_len(str);
    uint64_t  subcp = string_codepoint_len(sub);
    uint32_t *strp  = (uint32_t *)str->data;
    uint32_t *endp  = strp + end - subcp + 1;
    uint32_t *subp;
    uint32_t *p;

    if (start < 0) {
	start += strcp;
    }
    if (start < 0) {
	return -1;
    }
    if (end < 0) {
	end += strcp + 1;
    }
    if (end <= start) {
	return -1;
    }
    if ((uint64_t)end > strcp) {
	end = strcp;
    }
    if (subcp == 0) {
	return start;
    }

    strp += start;
    while (strp < endp) {
	p    = strp;
	subp = (uint32_t *)sub->data;
	for (uint64_t i = 0; i < subcp; i++) {
	    if (*p++ != *subp++) {
		goto next_start;
	    }
	}
	return start;
    next_start:
	strp++;
	start++;
    }
    return -1;
}

flexarray_t *
string_split(any_str_t *str, any_str_t *sub)
{
    str            = force_utf32(str);
    sub            = force_utf32(sub);
    uint64_t strcp = string_codepoint_len(str);
    uint64_t subcp = string_codepoint_len(sub);

    flexarray_t *result = con4m_new(tspec_list(tspec_utf32()),
				    "length", strcp);

    if (!subcp) {
	for (uint64_t i = 0; i < strcp; i++) {
	    flexarray_set(result, i, string_slice(str, i, i + 1));
	}
	return result;
    }

    int64_t start = 0;
    int64_t ix    = string_find(str, sub, "start", start);
    int     n     = 0;

    while (ix != -1) {
	flexarray_set(result, n++, string_slice(str, start, ix));
	start = ix + subcp;
	ix    = string_find(str, sub, "start", start);
    }

    if ((uint64_t)start != strcp) {
	flexarray_set(result, n++, string_slice(str, start, strcp));
    }

    flexarray_shrink(result, n);

    return result;
}

static void
con4m_string_marshal(any_str_t *s, stream_t *out, dict_t *memos, int64_t *mid)
{
    marshal_u32(s->codepoints, out);
    marshal_u32(s->byte_len, out);

    if (s->styling == NULL) {
	marshal_u32(0, out);
    }
    else {
	marshal_u32((int32_t)s->styling->num_entries, out);
	for (int i = 0; i < s->styling->num_entries; i++) {
	    marshal_i32(s->styling->styles[i].start, out);
	    marshal_i32(s->styling->styles[i].end, out);
	    marshal_u64(s->styling->styles[i].info, out);
	}
    }
    if (s->byte_len) {
	stream_raw_write(out, s->byte_len, s->data);
    }
}

static void
con4m_string_unmarshal(any_str_t *s, stream_t *in, dict_t *memos)
{
    s->codepoints = unmarshal_u32(in);
    s->byte_len   = unmarshal_u32(in);

    int32_t num_styles = unmarshal_u32(in);

    if (num_styles > 0) {
	alloc_styles(s, num_styles);
    }

    for (int i = 0; i < num_styles; i++) {
	s->styling->styles[i].start = unmarshal_i32(in);
	s->styling->styles[i].end   = unmarshal_i32(in);
	s->styling->styles[i].info  = unmarshal_u64(in);
    }

    if (s->byte_len) {
	s->data = con4m_gc_alloc(s->byte_len + 1, NULL);
	stream_raw_read(in, s->byte_len, s->data);
    }
}

utf8_t *
con4m_cstring(char *s, int64_t len)
{
    return con4m_new(tspec_utf8(), "cstring", s, "length", len);
}

utf8_t *
con4m_rich(utf8_t *to_copy, utf8_t *style)
{
    utf8_t *res        = string_copy(to_copy);
    render_style_t *rs = lookup_cell_style(style->data);

    if (rs != NULL) {
	string_apply_style(res, rs->base_style, 0);
    }

    return res;
}

static any_str_t *
string_repr(any_str_t *str, to_str_use_t how)
{
    // TODO: actually implement string quoting.
    if (how == TO_STR_USE_QUOTED) {
	utf32_t *q = con4m_new(tspec_utf32(), "cstring", "\"");
	return string_concat(string_concat(q, str), q);
    }
    else {
	return str;
    }
}

bool
string_can_coerce_to(type_spec_t *my_type, type_spec_t *target_type)
{
    if (tspecs_are_compat(target_type, tspec_utf8()) ||
	tspecs_are_compat(target_type, tspec_utf32()) ||
	tspecs_are_compat(target_type, tspec_buffer()) ||
	tspecs_are_compat(target_type, tspec_bool())) {
	return true;
    }

    return false;
}

object_t
string_coerce_to(const any_str_t *s, type_spec_t *target_type)
{
    if (tspecs_are_compat(target_type, tspec_utf8())) {
	return force_utf8(s);
    }
    if (tspecs_are_compat(target_type, tspec_utf32())) {
	return force_utf32(s);
    }
    if (tspecs_are_compat(target_type, tspec_buffer())) {
	// We can't just point into the UTF8 string, since buffers
	// are mutable but strings are not.

	s             = force_utf8(s);
	buffer_t *res = con4m_new(target_type, s->byte_len);
	memcpy(res->data, s->data, s->byte_len);

	return res;
    }
    if (tspecs_are_compat(target_type, tspec_bool())) {
	if (!s || !string_codepoint_len(s)) {
	    return (object_t)false;
	}
	else {
	    return (object_t)true;
	}
    }

    CRAISE("Invalid coersion.");
}

const con4m_vtable u8str_vtable = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	(con4m_vtable_entry)utf8_init,
	(con4m_vtable_entry)string_repr,
	NULL, // finalizer
	(con4m_vtable_entry)con4m_string_marshal,
	(con4m_vtable_entry)con4m_string_unmarshal,
	(con4m_vtable_entry)string_can_coerce_to,
	(con4m_vtable_entry)string_coerce_to,
	NULL, // From lit,
	(con4m_vtable_entry)string_copy,
	(con4m_vtable_entry)string_concat,
	NULL, // Subtract
	NULL, // Mul
	NULL, // Div
	NULL, // MOD
	(con4m_vtable_entry)string_codepoint_len,
	(con4m_vtable_entry)utf8_index,
	NULL, // Index set
	(con4m_vtable_entry)string_slice,
	NULL, // Slice set
    }
};

const con4m_vtable u32str_vtable = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	(con4m_vtable_entry)utf32_init,
	(con4m_vtable_entry)string_repr,
	NULL, // finalizer
	(con4m_vtable_entry)con4m_string_marshal,
	(con4m_vtable_entry)con4m_string_unmarshal,
	(con4m_vtable_entry)string_can_coerce_to,
	(con4m_vtable_entry)string_coerce_to,
	NULL, // From lit,
	(con4m_vtable_entry)string_copy,
	(con4m_vtable_entry)string_concat,
	NULL, // Subtract
	NULL, // Mul
	NULL, // Div
	NULL, // MOD
	(con4m_vtable_entry)string_codepoint_len,
	(con4m_vtable_entry)utf32_index,
	NULL, // Index set; strings are immutable.
	(con4m_vtable_entry)string_slice,
	NULL, // Slice set; strings are immutable.
    }
};

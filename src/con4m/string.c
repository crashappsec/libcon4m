#include "con4m.h"

// The object header has 4 words that we don't need to scan (there is
// a heap pointer in there, but it points to something definitely
// always available from the roots).
//
// Our pointer shows in our second word. Therefore, the 6th most
// significant bit gets set here.
const uint64_t c4m_pmap_str[2] = {
    0x0000000000000001,
    0x0400000000000000,
};

C4M_STATIC_ASCII_STR(c4m_empty_string_const, "");
C4M_STATIC_ASCII_STR(c4m_newline_const, "\n");
C4M_STATIC_ASCII_STR(c4m_crlf_const, "\r\n");

static void
utf8_set_codepoint_count(c4m_utf8_t *instr)
{
    uint8_t        *p   = (uint8_t *)instr->data;
    uint8_t        *end = p + instr->byte_len;
    c4m_codepoint_t cp;

    instr->codepoints = 0;

    while (p < end) {
        instr->codepoints += 1;
        p += utf8proc_iterate(p, 4, &cp);
    }
}

int64_t
c4m_utf8_validate(const c4m_utf8_t *instr)
{
    uint8_t        *p   = (uint8_t *)instr->data;
    uint8_t        *end = p + instr->byte_len;
    c4m_codepoint_t cp;
    int64_t         n = 0;

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
c4m_utf32_t *
c4m_str_slice(const c4m_str_t *instr, int64_t start, int64_t end)
{
    if (!instr) {
        return c4m_to_utf32(instr);
    }
    c4m_utf32_t *s   = c4m_to_utf32(instr);
    int64_t      len = c4m_str_codepoint_len(s);

    if (start < 0) {
        start += len;
    }
    else {
        if (start >= len) {
            return c4m_to_utf32(c4m_empty_string());
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
        return c4m_to_utf32(c4m_empty_string());
    }

    int64_t      slice_len = end - start;
    c4m_utf32_t *res       = c4m_new(c4m_tspec_utf32(),
                               c4m_kw("length", c4m_ka(slice_len)));
    res->codepoints        = ~(slice_len);

    c4m_codepoint_t *src = (c4m_codepoint_t *)s->data;
    c4m_codepoint_t *dst = (c4m_codepoint_t *)res->data;

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
        c4m_alloc_styles(res, sliced_style_count);

        for (i = 0; i < sliced_style_count; i++) {
            int64_t     sold = s->styling->styles[i + first].start;
            c4m_style_t info = s->styling->styles[i + first].info;
            int64_t     snew = max(sold - start, 0);
            int64_t     enew = min(s->styling->styles[i + first].end,
                               end)
                         - start;

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

static inline c4m_codepoint_t
c4m_utf8_index(const c4m_utf8_t *s, int64_t n)
{
    int64_t l = c4m_str_codepoint_len(s);

    if (n < 0) {
        n += l;

        if (n < 0) {
            C4M_CRAISE("Index would be before the start of the string.");
        }
    }

    if (n >= l) {
        C4M_CRAISE("Index out of bounds.");
    }

    char           *p = (char *)s->data;
    c4m_codepoint_t cp;

    for (int i = 0; i <= n; i++) {
        p += utf8proc_iterate((uint8_t *)p, 4, &cp);
    }

    return cp;
}

static inline c4m_codepoint_t
c4m_utf32_index(const c4m_utf32_t *s, int64_t i)
{
    int64_t l = c4m_str_codepoint_len(s);

    if (i < 0) {
        i += l;

        if (i < 0) {
            C4M_CRAISE("Index would be before the start of the string.");
        }
    }

    if (i >= l) {
        C4M_CRAISE("Index out of bounds.");
    }

    c4m_codepoint_t *p = (c4m_codepoint_t *)s->data;

    return p[i];
}

c4m_codepoint_t
c4m_index(const c4m_str_t *s, int64_t i)
{
    if (!c4m_str_is_u8(s)) {
        return c4m_utf32_index(s, i);
    }
    else {
        return c4m_utf8_index(s, i);
    }
}

c4m_utf32_t *
_c4m_str_strip(const c4m_str_t *s, ...)
{
    // TODO: this is needlessly slow for u8 since we convert it to u32
    // twice, both here and in slice.

    bool front = true;
    bool back  = true;

    c4m_karg_only_init(s);
    c4m_kw_bool("front", front);
    c4m_kw_bool("back", back);

    c4m_utf32_t     *as32  = c4m_to_utf32(s);
    c4m_codepoint_t *p     = (c4m_codepoint_t *)as32->data;
    int64_t          start = 0;
    int              len   = c4m_str_codepoint_len(as32);
    int              end   = len;

    if (front) {
        while (start < end && c4m_codepoint_is_space(p[start]))
            start++;
    }

    if (back) {
        while (--end != start) {
            if (!c4m_codepoint_is_space(p[end])) {
                break;
            }
        }
        end++;
    }

    if (!start && end == len) {
        return as32;
    }

    return c4m_str_slice(as32, start, end);
}

c4m_str_t *
c4m_str_copy(const c4m_str_t *s)
{
    if (s == NULL) {
        return NULL;
    }
    bool       u8  = c4m_str_is_u8(s);
    uint64_t   l   = u8 ? s->byte_len : ~s->codepoints;
    c4m_str_t *res = c4m_new(u8 ? c4m_tspec_utf8() : c4m_tspec_utf32(),
                             c4m_kw("length", c4m_ka(l)));

    res->codepoints = s->codepoints;
    memcpy(res->data, s->data, s->byte_len);
    c4m_copy_style_info(s, res);

    return res;
}

c4m_utf32_t *
c4m_str_concat(const c4m_str_t *p1, const c4m_str_t *p2)
{
    c4m_utf32_t *s1          = c4m_to_utf32(p1);
    c4m_utf32_t *s2          = c4m_to_utf32(p2);
    int64_t      s1_len      = c4m_str_codepoint_len(s1);
    int64_t      s2_len      = c4m_str_codepoint_len(s2);
    int64_t      n           = s1_len + s2_len;
    c4m_utf32_t *r           = c4m_new(c4m_tspec_utf32(),
                             c4m_kw("length", c4m_ka(n)));
    uint64_t     num_entries = c4m_style_num_entries(s1) + c4m_style_num_entries(s2);

    if (!s1_len) {
        return s2;
    }

    if (!s2_len) {
        return s1;
    }

    c4m_alloc_styles(r, num_entries);

    int start = c4m_style_num_entries(s1);
    if (start) {
        for (unsigned int i = 0; i < s1->styling->num_entries; i++) {
            r->styling->styles[i] = s1->styling->styles[i];
        }
    }

    if (c4m_style_num_entries(s2)) {
        int start = c4m_style_num_entries(s1);
        for (unsigned int i = 0; i < s2->styling->num_entries; i++) {
            r->styling->styles[i + start] = s2->styling->styles[i];
        }

        // Here, we loop through after the copy to adjust the offsets.
        for (uint64_t i = c4m_style_num_entries(s1); i < num_entries; i++) {
            r->styling->styles[i].start += s1_len;
            r->styling->styles[i].end += s1_len;
        }
    }

    // Now copy the actual string data.
    uint32_t *ptr = (uint32_t *)r->data;
    memcpy(r->data, s1->data, s1_len * 4);
    memcpy(ptr + s1_len, s2->data, s2_len * 4);

    r->codepoints = ~n;

    return r;
}

c4m_utf32_t *
_c4m_str_join(const c4m_xlist_t *l, const c4m_str_t *joiner, ...)
{
    c4m_karg_only_init(joiner);

    bool add_trailing = false;

    c4m_kw_bool("add_trailing", add_trailing);

    int64_t n_parts  = c4m_xlist_len(l);
    int64_t n_styles = 0;
    int64_t joinlen  = c4m_str_codepoint_len(joiner);
    int64_t len      = joinlen * n_parts; // An overestimate when !add_trailing

    for (int i = 0; i < n_parts; i++) {
        c4m_str_t *line = (c4m_str_t *)c4m_xlist_get(l, i, NULL);
        len += c4m_str_codepoint_len(line);
        n_styles += c4m_style_num_entries(line);
    }

    c4m_utf32_t     *result   = c4m_new(c4m_tspec_utf32(),
                                  c4m_kw("length", c4m_ka(len)));
    c4m_codepoint_t *p        = (c4m_codepoint_t *)result->data;
    int              txt_ix   = 0;
    int              style_ix = 0;
    c4m_utf32_t     *j        = joinlen ? c4m_to_utf32(joiner) : NULL;

    result->codepoints = ~len;
    c4m_alloc_styles(result, n_styles);

    if (!add_trailing) {
        --n_parts; // skip the last item during the loop.
    }

    for (int i = 0; i < n_parts; i++) {
        c4m_utf32_t *line = c4m_to_utf32((c4m_str_t *)c4m_xlist_get(l,
                                                                    i,
                                                                    NULL));
        int64_t      n_cp = c4m_str_codepoint_len(line);

        memcpy(p, line->data, n_cp * 4);
        p += n_cp;
        style_ix = c4m_copy_and_offset_styles(line, result, style_ix, txt_ix);
        txt_ix += n_cp;

        if (joinlen != 0) {
            memcpy(p, j->data, joinlen * 4);
            p += joinlen;
            style_ix = c4m_copy_and_offset_styles(j, result, style_ix, txt_ix);
            txt_ix += joinlen;
        }
    }

    if (!add_trailing) {
        c4m_utf32_t *line = c4m_to_utf32((c4m_str_t *)c4m_xlist_get(l,
                                                                    n_parts,
                                                                    NULL));
        int64_t      n_cp = c4m_str_codepoint_len(line);

        memcpy(p, line->data, n_cp * 4);
        style_ix = c4m_copy_and_offset_styles(line, result, style_ix, txt_ix);
    }
    return result;
}

c4m_utf8_t *
c4m_to_utf8(const c4m_utf32_t *inp)
{
    if (!c4m_str_is_u32(inp)) {
        return (c4m_utf8_t *)inp;
    }

    // Allocates 4 bytes per codepoint; this is an overestimate in
    // cases where UTF8 codepoints are above U+00ff. But nbd.

    c4m_utf8_t      *res    = c4m_new(c4m_tspec_utf8(),
                              c4m_kw("length", c4m_ka(inp->byte_len)));
    c4m_codepoint_t *p      = (c4m_codepoint_t *)inp->data;
    uint8_t         *outloc = (uint8_t *)res->data;
    int              l;

    res->codepoints = c4m_str_codepoint_len(inp);

    for (int i = 0; i < res->codepoints; i++) {
        l = utf8proc_encode_char(p[i], outloc);
        outloc += l;
    }

    res->byte_len = (int32_t)(outloc - (uint8_t *)res->data);

    c4m_copy_style_info(inp, res);

    return res;
}

c4m_utf32_t *
c4m_to_utf32(const c4m_utf8_t *instr)
{
    if (!instr || c4m_str_is_u32(instr)) {
        return (c4m_utf32_t *)instr;
    }

    int64_t          len    = (int64_t)c4m_str_codepoint_len(instr);
    c4m_utf32_t     *outstr = c4m_new(c4m_tspec_utf32(), c4m_kw("length", c4m_ka(len)));
    uint8_t         *inp    = (uint8_t *)(instr->data);
    c4m_codepoint_t *outp   = (c4m_codepoint_t *)(outstr->data);

    for (int i = 0; i < len; i++) {
        inp += utf8proc_iterate(inp, 4, outp + i);
    }

    outstr->codepoints = ~len;

    c4m_copy_style_info(instr, outstr);

    return outstr;
}

static void
utf8_init(c4m_utf8_t *s, va_list args)
{
    int64_t     length        = -1; // BYTE length.
    int64_t     start         = 0;
    char       *cstring       = NULL;
    c4m_style_t style         = C4M_STY_BAD;
    bool        replace_style = true;
    char       *tag           = NULL;

    c4m_karg_va_init(args);
    c4m_kw_int64("length", length);
    c4m_kw_int64("start", start);
    c4m_kw_ptr("cstring", cstring);
    c4m_kw_uint64("style", style);
    c4m_kw_bool("replace_style", replace_style);
    c4m_kw_ptr("tag", tag);

    if (cstring != NULL) {
        if (length < 0) {
            length = strlen(cstring);
        }

        if (start > length) {
            C4M_CRAISE(
                "Invalid string constructor call: "
                "len(cstring) is less than the start index");
        }

        s->data     = c4m_gc_raw_alloc(length + 1, NULL);
        s->byte_len = length;

        memcpy(s->data, cstring, length);
        utf8_set_codepoint_count(s);
    }
    else {
        if (length < 0) {
            s->data = 0;
            C4M_CRAISE("length cannot be < 0 for string initialization");
        }
        s->data = c4m_gc_raw_alloc(length + 1, NULL);
    }

    if (style != C4M_STY_BAD) {
        c4m_str_apply_style(s, style, replace_style);
    }

    if (tag != NULL) {
        c4m_render_style_t *rs = c4m_lookup_cell_style(tag);
        if (rs != NULL) {
            c4m_str_apply_style(s, rs->base_style, replace_style);
        }
    }
}

static void
utf32_init(c4m_utf32_t *s, va_list args)
{
    int64_t          length        = -1; // NUMBER OF CODEPOINTS.
    int64_t          start         = 0;
    char            *cstring       = NULL;
    c4m_codepoint_t *codepoints    = NULL;
    c4m_style_t      style         = C4M_STY_BAD;
    bool             replace_style = true;
    char            *tag           = NULL;

    c4m_karg_va_init(args);
    c4m_kw_int64("length", length);
    c4m_kw_int64("start", start);
    c4m_kw_ptr("cstring", cstring);
    c4m_kw_uint64("style", style);
    c4m_kw_ptr("codepoints", codepoints);
    c4m_kw_bool("replace_style", replace_style);
    c4m_kw_ptr("tag", tag);

    if (codepoints != NULL && cstring != NULL) {
        C4M_CRAISE("Cannot specify both 'codepoints' and 'cstring' keywords.");
    }
    if (codepoints != NULL) {
        if (length == 0) {
            s->byte_len   = 4;
            s->data       = c4m_gc_raw_alloc(s->byte_len, NULL);
            s->codepoints = ~0;
            return;
        }

        if (length < 0) {
            C4M_CRAISE(
                "When specifying 'codepoints', must provide a valid "
                "'length' containing the number of codepoints.");
        }
        s->byte_len = (length + 1) * 4;
        s->data     = c4m_gc_raw_alloc(s->byte_len, NULL);

        c4m_codepoint_t *local = (c4m_codepoint_t *)s->data;

        for (int i = 0; i < length; i++) {
            local[i] = codepoints[i];
        }

        s->codepoints = ~length;
    }
    else {
        if (cstring != NULL) {
            if (length == -1) {
                length = strlen(cstring);
            }

            if (start > length) {
                C4M_CRAISE(
                    "Invalid string constructor call: "
                    "len(cstring) is less than the start index");
            }
            s->byte_len = (length + 1) * 4;
            s->data     = c4m_gc_raw_alloc(s->byte_len, NULL);

            for (int64_t i = 0; i < length; i++) {
                ((uint32_t *)s->data)[i] = (uint32_t)(cstring[i]);
            }
            s->codepoints = ~length;
        }
        else {
            if (length < 0) {
                C4M_CRAISE(
                    "Must specify a valid length if not initializing "
                    "with a null-terminated cstring.");
            }
            s->byte_len = (length + 1) * 4;
            s->data     = c4m_gc_raw_alloc(s->byte_len, NULL);
        }
    }

    if (style != C4M_STY_BAD) {
        c4m_str_apply_style(s, style, replace_style);
    }

    if (tag != NULL) {
        c4m_render_style_t *rs = c4m_lookup_cell_style(tag);
        if (rs != NULL) {
            c4m_str_apply_style(s, rs->base_style, replace_style);
        }
    }
}

c4m_utf8_t *
c4m_str_from_int(int64_t n)
{
    char buf[21] = {
        0,
    };
    char *p = &buf[20];

    if (!n) {
        return c4m_utf8_repeat('0', 1);
    }

    int64_t i = n;

    while (i) {
        *--p = '0' + (i % 10);
        i /= 10;
    }

    if (n < 0) {
        *--p = '-';
    }

    return c4m_new(c4m_tspec_utf8(), c4m_kw("cstring", c4m_ka(p)));
}

// For repeat, we leave an extra alloc'd character to ensure we
// can easily drop in a newline.
c4m_utf8_t *
c4m_utf8_repeat(c4m_codepoint_t cp, int64_t num)
{
    uint8_t buf[4] = {
        0,
    };
    int         buf_ix = 0;
    int         l      = utf8proc_encode_char(cp, &buf[0]);
    int         blen   = l * num;
    c4m_utf8_t *res    = c4m_new(c4m_tspec_utf8(), c4m_kw("length", c4m_ka(blen + 1)));
    char       *p      = res->data;

    res->codepoints = l;

    for (int i = 0; i < blen; i++) {
        p[i] = buf[buf_ix++];
        buf_ix %= l;
    }

    return res;
}

c4m_utf32_t *
c4m_utf32_repeat(c4m_codepoint_t cp, int64_t num)
{
    if (num <= 0) {
        return c4m_empty_string();
    }

    c4m_utf32_t     *res = c4m_new(c4m_tspec_utf8(), c4m_kw("length", c4m_ka(num + 1)));
    c4m_codepoint_t *p   = (c4m_codepoint_t *)res->data;

    res->codepoints = ~num;

    for (int i = 0; i < num; i++) {
        *p++ = cp;
    }

    return res;
}

int64_t
c4m_str_render_len(const c4m_str_t *s)
{
    int64_t result = 0;
    int64_t n      = c4m_str_codepoint_len(s);

    if (c4m_str_is_u32(s)) {
        c4m_codepoint_t *p = (c4m_codepoint_t *)s->data;

        for (int i = 0; i < n; i++) {
            result += c4m_codepoint_width(p[i]);
        }
    }
    else {
        uint8_t        *p = (uint8_t *)s->data;
        c4m_codepoint_t cp;

        for (int i = 0; i < n; i++) {
            p += utf8proc_iterate(p, 4, &cp);
            result += c4m_codepoint_width(cp);
        }
    }
    return result;
}

c4m_str_t *
_c4m_str_truncate(const c4m_str_t *s, int64_t len, ...)
{
    c4m_karg_only_init(len);

    bool use_render_width = false;

    c4m_kw_bool("use_render_width", use_render_width);

    int64_t n = c4m_str_codepoint_len(s);
    int64_t c = 0;

    if (c4m_str_is_u32(s)) {
        c4m_codepoint_t *p = (c4m_codepoint_t *)s->data;

        if (use_render_width) {
            for (int i = 0; i < n; i++) {
                int w = c4m_codepoint_width(p[i]);
                if (c + w > len) {
                    return c4m_str_slice(s, 0, i);
                }
            }
            return (c4m_str_t *)s; // Didn't need to truncate.
        }
        if (n <= len) {
            return (c4m_str_t *)s;
        }
        return c4m_str_slice(s, 0, len);
    }
    else {
        uint8_t        *p = (uint8_t *)s->data;
        uint8_t        *next;
        c4m_codepoint_t cp;
        int64_t         num_cp = 0;

        if (use_render_width) {
            for (int i = 0; i < n; i++) {
                next  = p + utf8proc_iterate(p, 4, &cp);
                int w = c4m_codepoint_width(cp);
                if (c + w > len) {
                    goto u8_slice;
                }
                num_cp++;
                p = next;
            }
            return (c4m_str_t *)s;
        }

        else {
            num_cp = len;

            for (int i = 0; i < n; i++) {
                if (c++ == len) {
u8_slice:
                    // Since we don't have a full u8 slice yet...
                    {
                        uint8_t    *start = (uint8_t *)s->data;
                        int64_t     blen  = p - start;
                        c4m_utf8_t *res   = c4m_new(c4m_tspec_utf8(),
                                                  c4m_kw("length",
                                                         c4m_ka(blen)));

                        memcpy(res->data, start, blen);
                        c4m_copy_style_info(s, res);

                        if (res->styling != NULL) {
                            for (int i = 0; i < res->styling->num_entries; i++) {
                                c4m_style_entry_t e = res->styling->styles[i];
                                if (e.start > num_cp) {
                                    res->styling->num_entries = i;
                                    return res;
                                }
                                if (e.end > num_cp) {
                                    res->styling->styles[i].end = num_cp;
                                    res->styling->num_entries   = i + 1;
                                    return res;
                                }
                            }
                        }
                        return res;
                    }
                }
                p += utf8proc_iterate(p, 4, &cp);
            }
            return (c4m_str_t *)s;
        }
    }
}

c4m_utf8_t *
c4m_from_file(const c4m_str_t *name, int *err)
{
    c4m_utf32_t *n = c4m_to_utf32(name);

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

    c4m_utf8_t *result = c4m_new(c4m_tspec_utf8(), c4m_kw("length", c4m_ka(len)));
    char       *p      = result->data;

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

        p += num_read;
        len -= num_read;
    }
}

int64_t
_c4m_str_find(c4m_str_t *str, c4m_str_t *sub, ...)
{
    int64_t start = 0;
    int64_t end   = -1;

    c4m_karg_only_init(sub);

    c4m_kw_int64("start", start);
    c4m_kw_int64("end", end);

    str = c4m_to_utf32(str);
    sub = c4m_to_utf32(sub);

    uint64_t strcp = c4m_str_codepoint_len(str);
    uint64_t subcp = c4m_str_codepoint_len(sub);

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

    uint32_t *strp = (uint32_t *)str->data;
    uint32_t *endp = &strp[end - subcp + 1];
    uint32_t *subp;
    uint32_t *p;

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
c4m_str_split(c4m_str_t *str, c4m_str_t *sub)
{
    str            = c4m_to_utf32(str);
    sub            = c4m_to_utf32(sub);
    uint64_t strcp = c4m_str_codepoint_len(str);
    uint64_t subcp = c4m_str_codepoint_len(sub);

    flexarray_t *result = c4m_new(c4m_tspec_list(c4m_tspec_utf32()),
                                  c4m_kw("length", c4m_ka(strcp)));

    if (!subcp) {
        for (uint64_t i = 0; i < strcp; i++) {
            flexarray_set(result, i, c4m_str_slice(str, i, i + 1));
        }
        return result;
    }

    int64_t start = 0;
    int64_t ix    = c4m_str_find(str, sub, c4m_kw("start", c4m_ka(start)));
    int     n     = 0;

    while (ix != -1) {
        flexarray_set(result, n++, c4m_str_slice(str, start, ix));
        start = ix + subcp;
        ix    = c4m_str_find(str, sub, c4m_kw("start", c4m_ka(start)));
    }

    if ((uint64_t)start != strcp) {
        flexarray_set(result, n++, c4m_str_slice(str, start, strcp));
    }

    flexarray_shrink(result, n);

    return result;
}

c4m_xlist_t *
c4m_str_xsplit(c4m_str_t *str, c4m_str_t *sub)
{
    str            = c4m_to_utf32(str);
    sub            = c4m_to_utf32(sub);
    uint64_t strcp = c4m_str_codepoint_len(str);
    uint64_t subcp = c4m_str_codepoint_len(sub);

    c4m_xlist_t *result = c4m_new(c4m_tspec_xlist(c4m_tspec_utf32()));

    if (!subcp) {
        for (uint64_t i = 0; i < strcp; i++) {
            c4m_xlist_append(result, c4m_str_slice(str, i, i + 1));
        }
        return result;
    }

    int64_t start = 0;
    int64_t ix    = c4m_str_find(str, sub, c4m_kw("start", c4m_ka(start)));

    while (ix != -1) {
        c4m_xlist_append(result, c4m_str_slice(str, start, ix));
        start = ix + subcp;
        ix    = c4m_str_find(str, sub, c4m_kw("start", c4m_ka(start)));
    }

    if ((uint64_t)start != strcp) {
        c4m_xlist_append(result, c4m_str_slice(str, start, strcp));
    }

    return result;
}

static void
c4m_string_marshal(c4m_str_t    *s,
                   c4m_stream_t *out,
                   c4m_dict_t   *memos,
                   int64_t      *mid)
{
    c4m_marshal_u32(s->codepoints, out);
    c4m_marshal_u32(s->byte_len, out);

    if (s->styling == NULL) {
        c4m_marshal_u32(0, out);
    }
    else {
        c4m_marshal_u32((int32_t)s->styling->num_entries, out);
        for (int i = 0; i < s->styling->num_entries; i++) {
            c4m_marshal_i32(s->styling->styles[i].start, out);
            c4m_marshal_i32(s->styling->styles[i].end, out);
            c4m_marshal_u64(s->styling->styles[i].info, out);
        }
    }
    if (s->byte_len) {
        c4m_stream_raw_write(out, s->byte_len, s->data);
    }
}

static void
c4m_string_unmarshal(c4m_str_t *s, c4m_stream_t *in, c4m_dict_t *memos)
{
    s->codepoints = c4m_unmarshal_u32(in);
    s->byte_len   = c4m_unmarshal_u32(in);

    int32_t num_styles = c4m_unmarshal_u32(in);

    if (num_styles > 0) {
        c4m_alloc_styles(s, num_styles);
    }

    for (int i = 0; i < num_styles; i++) {
        s->styling->styles[i].start = c4m_unmarshal_i32(in);
        s->styling->styles[i].end   = c4m_unmarshal_i32(in);
        s->styling->styles[i].info  = c4m_unmarshal_u64(in);
    }

    if (s->byte_len) {
        s->data = c4m_gc_raw_alloc(s->byte_len + 1, NULL);
        c4m_stream_raw_read(in, s->byte_len, s->data);
    }
}

c4m_utf8_t *
c4m_cstring(char *s, int64_t len)
{
    return c4m_new(c4m_tspec_utf8(),
                   c4m_kw("cstring", c4m_ka(s), "length", c4m_ka(len)));
}

c4m_utf8_t *
c4m_rich(c4m_utf8_t *to_copy, c4m_utf8_t *style)
{
    c4m_utf8_t         *res = c4m_str_copy(to_copy);
    c4m_render_style_t *rs  = c4m_lookup_cell_style(style->data);

    if (rs != NULL) {
        c4m_str_apply_style(res, rs->base_style, 0);
    }

    return res;
}

static c4m_str_t *
c4m_str_repr(c4m_str_t *str, to_str_use_t how)
{
    // TODO: actually implement string quoting.
    if (how == C4M_REPR_QUOTED) {
        c4m_utf32_t *q = c4m_new(c4m_tspec_utf32(), c4m_kw("cstring", c4m_ka("\"")));
        return c4m_str_concat(c4m_str_concat(q, str), q);
    }
    else {
        return str;
    }
}

bool
c4m_str_can_coerce_to(c4m_type_t *my_type, c4m_type_t *target_type)
{
    if (c4m_tspecs_are_compat(target_type, c4m_tspec_utf8()) || c4m_tspecs_are_compat(target_type, c4m_tspec_utf32()) || c4m_tspecs_are_compat(target_type, c4m_tspec_buffer()) || c4m_tspecs_are_compat(target_type, c4m_tspec_bool())) {
        return true;
    }

    return false;
}

c4m_obj_t
c4m_str_coerce_to(const c4m_str_t *s, c4m_type_t *target_type)
{
    if (c4m_tspecs_are_compat(target_type, c4m_tspec_utf8())) {
        return c4m_to_utf8(s);
    }
    if (c4m_tspecs_are_compat(target_type, c4m_tspec_utf32())) {
        return c4m_to_utf32(s);
    }
    if (c4m_tspecs_are_compat(target_type, c4m_tspec_buffer())) {
        // We can't just point into the UTF8 string, since buffers
        // are mutable but strings are not.

        s              = c4m_to_utf8(s);
        c4m_buf_t *res = c4m_new(target_type, s->byte_len);
        memcpy(res->data, s->data, s->byte_len);

        return res;
    }
    if (c4m_tspecs_are_compat(target_type, c4m_tspec_bool())) {
        if (!s || !c4m_str_codepoint_len(s)) {
            return (c4m_obj_t) false;
        }
        else {
            return (c4m_obj_t) true;
        }
    }

    C4M_CRAISE("Invalid coersion.");
}

static c4m_obj_t
c4m_str_lit(char            *s,
            c4m_lit_syntax_t st,
            char            *litmod,
            c4m_lit_error_t *err)
{
    if (*litmod == 0 || !strcmp(litmod, "u8") || !strcmp(litmod, "utf8")) {
        return c4m_new(c4m_tspec_utf8(), c4m_kw("cstring", c4m_ka(s)));
    }

    if (!strcmp(litmod, "u32") || !strcmp(litmod, "utf32")) {
        return c4m_new(c4m_tspec_utf32(), c4m_kw("cstring", c4m_ka(s)));
    }

    return c4m_rich_lit(s);
}

static bool
c4m_str_eq(c4m_str_t *s1, c4m_str_t *s2)
{
    bool s1_is_u32 = c4m_str_is_u32(s1);
    bool s2_is_u32 = c4m_str_is_u32(s2);

    if (s1_is_u32 ^ s2_is_u32) {
        if (s1_is_u32) {
            s2 = c4m_to_utf32(s2);
        }
        else {
            s1 = c4m_to_utf32(s1);
        }
    }

    if (s1->byte_len != s2->byte_len) {
        return false;
    }

    return memcmp(s1->data, s2->data, s1->byte_len) == 0;
}

const c4m_vtable_t c4m_u8str_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        (c4m_vtable_entry)utf8_init,
        (c4m_vtable_entry)c4m_str_repr,
        NULL, // finalizer
        (c4m_vtable_entry)c4m_string_marshal,
        (c4m_vtable_entry)c4m_string_unmarshal,
        (c4m_vtable_entry)c4m_str_can_coerce_to,
        (c4m_vtable_entry)c4m_str_coerce_to,
        NULL, // From lit,
        (c4m_vtable_entry)c4m_str_copy,
        (c4m_vtable_entry)c4m_str_concat,
        NULL, // Subtract
        NULL, // Mul
        NULL, // Div
        NULL, // MOD
        NULL, // EQ
        NULL, // LT
        NULL, // GT
        (c4m_vtable_entry)c4m_str_codepoint_len,
        (c4m_vtable_entry)c4m_utf8_index,
        NULL, // Index set
        (c4m_vtable_entry)c4m_str_slice,
        NULL, // Slice set
    }};

const c4m_vtable_t c4m_u32str_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        (c4m_vtable_entry)utf32_init,
        (c4m_vtable_entry)c4m_str_repr,
        NULL, // finalizer
        (c4m_vtable_entry)c4m_string_marshal,
        (c4m_vtable_entry)c4m_string_unmarshal,
        (c4m_vtable_entry)c4m_str_can_coerce_to,
        (c4m_vtable_entry)c4m_str_coerce_to,
        (c4m_vtable_entry)c4m_str_lit,
        (c4m_vtable_entry)c4m_str_copy,
        (c4m_vtable_entry)c4m_str_concat,
        NULL,                         // Subtract
        NULL,                         // Mul
        NULL,                         // Div
        NULL,                         // MOD
        (c4m_vtable_entry)c4m_str_eq, // EQ
        NULL,                         // LT
        NULL,                         // GT
        (c4m_vtable_entry)c4m_str_codepoint_len,
        (c4m_vtable_entry)c4m_utf32_index,
        NULL, // Index set; strings are immutable.
        (c4m_vtable_entry)c4m_str_slice,
        NULL, // Slice set; strings are immutable.
    },
};

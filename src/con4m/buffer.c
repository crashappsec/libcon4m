#include "con4m.h"

/*
 * Note that, unlike strings, buffers are intended to be mutable.
 * BUT! Currently, this implementation does not guard against
 * writes happenening concurrent to any operation.
 *
 * We should add locking to allow multiple concurrent readers, but
 * give exclusivity and priority to writes. But it's fine to wait on
 * that until needed.
 */

static void
buffer_init(buffer_t *obj, va_list args)
{
    int64_t    length = -1;
    char      *raw    = NULL;
    any_str_t *hex    = NULL;
    char      *ptr    = NULL;

    c4m_karg_va_init(args);

    c4m_kw_int64("length", length);
    c4m_kw_ptr("raw", raw);
    c4m_kw_ptr("hex", hex);
    c4m_kw_ptr("ptr", ptr);

    if (raw == NULL && hex == NULL && ptr == NULL) {
        if (length < 0) {
            C4M_CRAISE("Invalid buffer initializer.");
        }
    }
    if (length < 0) {
        if (hex == NULL) {
            C4M_CRAISE("Must specify length for raw / ptr initializer.");
        }
        else {
            length = string_codepoint_len(hex) >> 1;
        }
    }

    if (length > 0) {
        int64_t alloc_len = hatrack_round_up_to_power_of_2(length);

        obj->data      = c4m_gc_raw_alloc(alloc_len, NULL);
        obj->alloc_len = alloc_len;
    }

    if (raw != NULL) {
        if (hex != NULL) {
            C4M_CRAISE("Cannot set both 'hex' and 'raw' fields.");
        }
        if (ptr != NULL) {
            C4M_CRAISE("Cannot set both 'ptr' and 'raw' fields.");
        }

        memcpy(obj->data, raw, length);
    }

    if (ptr != NULL) {
        if (hex != NULL) {
            C4M_CRAISE("Cannot set both 'hex' and 'ptr' fields.");
        }

        obj->data = ptr;
    }

    if (hex != NULL) {
        uint8_t cur         = 0;
        int     valid_count = 0;

        hex = force_utf8(hex);

        for (int i = 0; i < hex->byte_len; i++) {
            uint8_t byte = hex->data[i];
            if (byte >= '0' && byte <= '9') {
                if ((++valid_count) % 2 == 1) {
                    cur = (byte - '0') << 4;
                }
                else {
                    cur |= (byte - '0');
                    obj->data[obj->byte_len++] = cur;
                }
                continue;
            }
            if (byte >= 'a' && byte <= 'f') {
                if ((++valid_count) % 2 == 1) {
                    cur = ((byte - 'a') + 10) << 4;
                }
                else {
                    cur |= (byte - 'a') + 10;
                    obj->data[obj->byte_len++] = cur;
                }
                continue;
            }
            if (byte >= 'A' && byte <= 'F') {
                if ((++valid_count) % 2 == 1) {
                    cur = ((byte - 'A') + 10) << 4;
                }
                else {
                    cur |= (byte - 'A') + 10;
                    obj->data[obj->byte_len++] = cur;
                }
                continue;
            }
        }
    }
    else {
        obj->byte_len = length;
    }
}

void
c4m_buffer_resize(buffer_t *buffer, uint64_t new_sz)
{
    if ((int64_t)new_sz <= buffer->alloc_len) {
        buffer->byte_len = new_sz;
        return;
    }

    // Resize up, copying old data and leaving the rest zero'd.
    uint64_t new_alloc_sz = hatrack_round_up_to_power_of_2(new_sz);
    char    *new_data     = c4m_gc_raw_alloc(new_alloc_sz, NULL);

    memcpy(new_data, buffer->data, buffer->byte_len);

    buffer->data      = new_data;
    buffer->byte_len  = new_sz;
    buffer->alloc_len = new_alloc_sz;
}

static char to_hex_map[] = "0123456789abcdef";

static utf8_t *
buffer_repr(buffer_t *buf, to_str_use_t how)
{
    utf8_t *result;

    if (how == TO_STR_USE_QUOTED) {
        result  = c4m_new(tspec_utf8(),
                         c4m_kw("length", c4m_ka(buf->byte_len * 4 + 2)));
        char *p = result->data;

        *p++ = '"';

        for (int i = 0; i < buf->byte_len; i++) {
            char c = buf->data[i];
            *p++   = '\\';
            *p++   = 'x';
            *p++   = to_hex_map[(c >> 4)];
            *p++   = to_hex_map[c & 0x0f];
        }
        *p++ = '"';
    }
    else {
        result  = c4m_new(tspec_utf8(),
                         c4m_kw("length", c4m_ka(buf->byte_len * 2)));
        char *p = result->data;

        for (int i = 0; i < buf->byte_len; i++) {
            uint8_t c = ((uint8_t *)buf->data)[i];
            *p++      = to_hex_map[(c >> 4)];
            *p++      = to_hex_map[c & 0x0f];
        }

        result->codepoints = p - result->data;
        result->byte_len   = result->codepoints;
    }
    return result;
}

buffer_t *
c4m_buffer_add(buffer_t *b1, buffer_t *b2)
{
    int64_t   l1     = max(c4m_buffer_len(b1), 0);
    int64_t   l2     = max(c4m_buffer_len(b2), 0);
    int64_t   lnew   = l1 + l2;
    buffer_t *result = c4m_new(tspec_buffer(),
                               c4m_kw("length", c4m_ka(lnew)));

    if (l1 > 0) {
        memcpy(result->data, b1->data, l1);
    }

    if (l2 > 0) {
        char *p = result->data + l1;
        memcpy(p, b2->data, l2);
    }

    return result;
}

buffer_t *
c4m_buffer_join(xlist_t *list, buffer_t *joiner)
{
    int64_t num_items = xlist_len(list);
    int64_t new_len   = 0;
    int     jlen      = 0;

    for (int i = 0; i < num_items; i++) {
        buffer_t *n = xlist_get(list, i, NULL);

        new_len += c4m_buffer_len(n);
    }

    if (joiner != NULL) {
        jlen = c4m_buffer_len(joiner);
        new_len += jlen * (num_items - 1);
    }

    buffer_t *result = c4m_new(tspec_buffer(),
                               c4m_kw("length", c4m_ka(new_len)));
    char     *p      = result->data;
    buffer_t *cur    = xlist_get(list, 0, NULL);
    int       clen   = c4m_buffer_len(cur);

    memcpy(p, cur->data, clen);

    for (int i = 1; i < num_items; i++) {
        p += clen;

        if (jlen != 0) {
            memcpy(p, joiner->data, jlen);
            p += jlen;
        }

        cur  = xlist_get(list, i, NULL);
        clen = c4m_buffer_len(cur);
        memcpy(p, cur->data, clen);
    }

    assert(p - result->data == new_len);

    return result;
}

int64_t
c4m_buffer_len(buffer_t *buffer)
{
    return (int64_t)buffer->byte_len;
}

utf8_t *
c4m_buffer_to_utf8_string(buffer_t *buffer)
{
    utf8_t *result = c4m_new(tspec_utf8(),
                             c4m_kw("cstring",
                                    c4m_ka(buffer->data),
                                    "length",
                                    c4m_ka(buffer->byte_len)));

    if (utf8_validate(result) < 0) {
        C4M_CRAISE("Buffer contains invalid UTF-8.");
    }

    return result;
}

static void
c4m_buffer_marshal(buffer_t *b, stream_t *s, dict_t *memos, int64_t *mid)
{
    marshal_u32(b->byte_len, s);
    marshal_u32(b->flags, s); // Not currently used btw.
    stream_raw_write(s, b->byte_len, b->data);
}

static void
c4m_buffer_unmarshal(buffer_t *b, stream_t *s, dict_t *memos)
{
    b->byte_len = unmarshal_u32(s);
    b->flags    = unmarshal_u32(s); // Not currently used btw.
    if (b->byte_len) {
        b->data = c4m_gc_raw_alloc(b->byte_len, NULL);
        stream_raw_read(s, b->byte_len, b->data);
    }
}

static bool
buffer_can_coerce_to(type_spec_t *my_type, type_spec_t *target_type)
{
    if (tspecs_are_compat(target_type, tspec_utf8()) || tspecs_are_compat(target_type, tspec_buffer()) || tspecs_are_compat(target_type, tspec_bool())) {
        return true;
    }

    return false;
}

static object_t
buffer_coerce_to(const buffer_t *b, type_spec_t *target_type)
{
    if (tspecs_are_compat(target_type, tspec_buffer())) {
        return (object_t)b;
    }

    if (tspecs_are_compat(target_type, tspec_bool())) {
        if (!b || b->byte_len == 0) {
            return (object_t) false;
        }
        else {
            return (object_t) true;
        }
    }

    if (tspecs_are_compat(target_type, tspec_utf8())) {
        int32_t     count = 0;
        uint8_t    *p     = (uint8_t *)b->data;
        uint8_t    *end   = p + b->byte_len;
        codepoint_t cp;
        int         cplen;

        while (p < end) {
            count++;
            cplen = utf8proc_iterate(p, 4, &cp);
            if (cplen < 0) {
                C4M_CRAISE("Buffer contains invalid UTF-8");
            }
            p += cplen;
        }

        utf8_t *result     = c4m_new(target_type,
                                 c4m_kw("length", c4m_ka(b->byte_len)));
        result->codepoints = count;

        memcpy(result->data, b->data, b->byte_len);
    }

    C4M_CRAISE("Invalid conversion from buffer type");
}

static uint8_t
buffer_get_index(const buffer_t *b, int64_t n)
{
    if (n < 0) {
        n += b->byte_len;

        if (n < 0) {
            C4M_CRAISE("Index would be before the start of the buffer.");
        }
    }

    if (n >= b->byte_len) {
        C4M_CRAISE("Index out of bounds.");
    }

    return b->data[n];
}

static void
buffer_set_index(buffer_t *b, int64_t n, int8_t c)
{
    if (n < 0) {
        n += b->byte_len;

        if (n < 0) {
            C4M_CRAISE("Index would be before the start of the buffer.");
        }
    }

    if (n >= b->byte_len) {
        C4M_CRAISE("Index out of bounds.");
    }

    b->data[n] = c;
}

static buffer_t *
buffer_get_slice(const buffer_t *b, int64_t start, int64_t end)
{
    int64_t len = b->byte_len;

    if (start < 0) {
        start += len;
    }
    else {
        if (start >= len) {
            return c4m_new(tspec_buffer(), c4m_kw("length", c4m_ka(0)));
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
        return c4m_new(tspec_buffer(), c4m_kw("length", c4m_ka(0)));
    }

    int64_t   slice_len = end - start;
    buffer_t *result    = c4m_new(tspec_buffer(),
                               c4m_kw("length", c4m_ka(slice_len)));

    memcpy(result->data, b->data + start, slice_len);
    return result;
}

static void
buffer_set_slice(buffer_t *b, int64_t start, int64_t end, buffer_t *val)
{
    int64_t len = b->byte_len;

    if (start < 0) {
        start += len;
    }
    else {
        if (start >= len) {
            C4M_CRAISE("Slice out-of-bounds.");
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
        C4M_CRAISE("Slice out-of-bounds.");
    }

    int64_t slice_len   = end - start;
    int64_t new_len     = b->byte_len - slice_len;
    int64_t replace_len = 0;

    if (val != NULL) {
        replace_len = val->byte_len;
        new_len += replace_len;
    }

    if (new_len < b->byte_len) {
        if (val != NULL && val->byte_len > 0) {
            memcpy(b->data + start, val->data, replace_len);
        }

        if (end < b->byte_len) {
            memcpy(b->data + start + replace_len,
                   b->data + end,
                   b->byte_len - end);
        }
    }
    else {
        char *new_buf = c4m_gc_raw_alloc(new_len, NULL);
        if (start > 0) {
            memcpy(new_buf, b->data, start);
        }
        if (replace_len != 0) {
            memcpy(new_buf + start, val->data, replace_len);
        }
        if (end < b->byte_len) {
            memcpy(new_buf + start + replace_len,
                   b->data + end,
                   b->byte_len - end);
        }
        b->data = new_buf;
    }

    b->byte_len = new_len;
}

static object_t
buffer_lit(char *s, syntax_t st, char *litmod, lit_error_t *err)
{
    if (!strcmp(litmod, "h") || !strcmp(litmod, "hex")) {
        int length = strlen(s);
        if (length & 2) {
            err->code = LE_WrongNumDigits;
            return NULL;
        }
        return c4m_new(tspec_buffer(),
                       c4m_kw("length", c4m_ka(length), "hex", c4m_ka(s)));
    }

    return c4m_new(tspec_buffer(), c4m_kw("raw", c4m_ka(s)));
}

static buffer_t *
buffer_copy(buffer_t *inbuf)
{
    buffer_t *outbuf = c4m_new(tspec_buffer(),
                               c4m_kw("length", c4m_ka(inbuf->byte_len)));

    memcpy(outbuf->data, inbuf->data, inbuf->byte_len);

    return outbuf;
}

const c4m_vtable buffer_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        (c4m_vtable_entry)buffer_init,
        (c4m_vtable_entry)buffer_repr,
        NULL, // finalizer
        (c4m_vtable_entry)c4m_buffer_marshal,
        (c4m_vtable_entry)c4m_buffer_unmarshal,
        (c4m_vtable_entry)buffer_can_coerce_to,
        (c4m_vtable_entry)buffer_coerce_to,
        (c4m_vtable_entry)buffer_lit,
        (c4m_vtable_entry)buffer_copy,
        (c4m_vtable_entry)c4m_buffer_add,
        NULL, // Subtract
        NULL, // Mul
        NULL, // Div
        NULL, // MOD
        NULL, // EQ
        NULL, // LT
        NULL, // GT
        (c4m_vtable_entry)c4m_buffer_len,
        (c4m_vtable_entry)buffer_get_index,
        (c4m_vtable_entry)buffer_set_index,
        (c4m_vtable_entry)buffer_get_slice,
        (c4m_vtable_entry)buffer_set_slice,
    },
};

// First word really means 5th word due to the GC header.
const uint64_t pmap_first_word[2] = {0x1, 0x0800000000000000};

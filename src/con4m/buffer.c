#include <con4m.h>

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
    DECLARE_KARGS(
	int64_t    length = -1;
	char      *raw    = NULL;
	any_str_t *hex    = NULL;
	);

    method_kargs(args, length, raw, hex);


    if (length < 0 && hex == NULL) {
	abort();
    }
    if (length >= 0 && hex != NULL) {
	abort();
    }

    if (length < 0) {
	length = string_codepoint_len(hex) >> 1;
    }

    if (length > 0) {
	int64_t alloc_len = hatrack_round_up_to_power_of_2(length);

	obj->data      = con4m_gc_alloc(alloc_len, NULL);
	obj->alloc_len = alloc_len;
    }

    if (raw != NULL) {
	if (hex != NULL) {
	    CRAISE("Cannot set both hex and raw fields.");
	}

	memcpy(obj->data, raw, length);
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
buffer_resize(buffer_t *buffer, uint64_t new_sz)
{
    if ((int64_t)new_sz <= buffer->alloc_len) {
	buffer->byte_len = new_sz;
	return;
    }

    // Resize up, copying old data and leaving the rest zero'd.
    uint64_t new_alloc_sz = hatrack_round_up_to_power_of_2(new_sz);
    char    *new_data     = con4m_gc_alloc(new_alloc_sz, NULL);

    memcpy(new_data, buffer->data, buffer->byte_len);

    buffer->data      = new_data;
    buffer->byte_len  = new_sz;
    buffer->alloc_len = new_alloc_sz;

}

static char to_hex_map[] = "0123456789abcdef";

utf8_t *
buffer_repr(buffer_t *buf, to_str_use_t how)
{
    utf8_t *result;

    if (how == TO_STR_USE_QUOTED) {
	result = con4m_new(tspec_utf8(), "length", buf->byte_len * 4 + 2);
	char *p = result->data;

	*p++ = '"';

	for (int i = 0; i < buf->byte_len; i++) {
	    char c = buf->data[i];
	    *p++ = '\\';
	    *p++ = 'x';
	    *p++ = to_hex_map[(c >> 4)];
	    *p++ = to_hex_map[c & 0x0f];
	}
	*p++ = '"';
    }
    else {
	result = con4m_new(tspec_utf8(), "length", buf->byte_len * 2);
	char *p = result->data;

	for (int i = 0; i < buf->byte_len; i++) {
	    uint8_t c = ((uint8_t *)buf->data)[i];
	    *p++ = to_hex_map[(c >> 4)];
	    *p++ = to_hex_map[c & 0x0f];
	}

	result->codepoints = p - result->data;
	result->byte_len   = result->codepoints;
    }
    return result;
}

buffer_t *
buffer_add(buffer_t *b1, buffer_t *b2)
{
    int64_t l1       = max(buffer_len(b1), 0);
    int64_t l2       = max(buffer_len(b2), 0);
    int64_t lnew     =  l1 + l2;
    buffer_t *result = con4m_new(tspec_buffer(), "length", lnew);

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
buffer_join(xlist_t *list, buffer_t *joiner)
{
    int64_t num_items = xlist_len(list);
    int64_t new_len   = 0;
    int     jlen      = 0;

    for (int i = 0; i < num_items; i++) {
	buffer_t *n = xlist_get(list, i, NULL);

	new_len += buffer_len(n);
    }

    if (joiner != NULL) {
	jlen     = buffer_len(joiner);
	new_len += jlen * (num_items - 1);
    }

    buffer_t *result = con4m_new(tspec_buffer(), "length", new_len);
    char     *p      = result->data;
    buffer_t *cur    = xlist_get(list, 0, NULL);
    int       clen   = buffer_len(cur);

    memcpy(p, cur->data, clen);

    for (int i = 1; i < num_items; i++) {
	p += clen;

	if (jlen != 0) {
	    memcpy(p, joiner->data, jlen);
	    p += jlen;
	}

	cur  = xlist_get(list, i, NULL);
	clen = buffer_len(cur);
	memcpy(p, cur->data, clen);
    }

    assert(p - result->data == new_len);

    return result;
}

int64_t
buffer_len(buffer_t *buffer)
{
    return (int64_t)buffer->byte_len;
}

static void
con4m_buffer_marshal(buffer_t *b, FILE *f, dict_t *memos, int64_t *mid)
{
    marshal_u32(b->byte_len, f);
    marshal_u32(b->flags, f);  // Not currently used btw.
    fwrite(b->data, b->byte_len, 1, f);
}

static void
con4m_buffer_unmarshal(buffer_t *b, FILE *f, dict_t *memos)
{
    b->byte_len = unmarshal_u32(f);
    b->flags    = unmarshal_u32(f); // Not currently used btw.
    if (b->byte_len) {
	b->data = con4m_gc_alloc(b->byte_len, NULL);
	fread(b->data, b->byte_len, 1, f);
    }
}

static bool
buffer_can_coerce_to(type_spec_t *my_type, type_spec_t *target_type)
{
    if (tspecs_are_compat(target_type, tspec_utf8()) ||
	tspecs_are_compat(target_type, tspec_buffer()) ||
	tspecs_are_compat(target_type, tspec_bool())) {
	return true;
    }

    return false;
}

object_t
buffer_coerce_to(const buffer_t *b, type_spec_t *target_type)
{
    if (tspecs_are_compat(target_type, tspec_buffer())) {
	return (object_t)b;
    }

    if (tspecs_are_compat(target_type, tspec_bool())) {
	if (!b || b->byte_len == 0) {
	    return (object_t)false;
	}
	else {
	    return (object_t)true;
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
		CRAISE("Buffer contains invalid UTF-8");
	    }
	    p += cplen;
	}

	utf8_t *result     = con4m_new(target_type, "length", b->byte_len);
	result->codepoints = count;

	memcpy(result->data, b->data, b->byte_len);
    }

    CRAISE("Invalid conversion from buffer type");
}

uint8_t
buffer_get_index(const buffer_t *b, int64_t n)
{
    if (n < 0) {
	n += b->byte_len;

	if (n < 0) {
	    CRAISE("Index would be before the start of the buffer.");
	}
    }

    if (n >= b->byte_len) {
	CRAISE("Index out of bounds.");
    }

    return b->data[n];
}

void
buffer_set_index(buffer_t *b, int64_t n, int8_t c)
{
    if (n < 0) {
	n += b->byte_len;

	if (n < 0) {
	    CRAISE("Index would be before the start of the buffer.");
	}
    }

    if (n >= b->byte_len) {
	CRAISE("Index out of bounds.");
    }

    b->data[n] = c;
}

buffer_t *
buffer_get_slice(const buffer_t *b, int64_t start, int64_t end)
{
    int64_t len = b->byte_len;

    if (start < 0) {
	start += len;
    }
    else {
	if (start >= len) {
	    return con4m_new(tspec_buffer(), "length", 0);
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
	return con4m_new(tspec_buffer(), "length", 0);
    }

    int64_t   slice_len = end - start;
    buffer_t *result = con4m_new(tspec_buffer(), "length", slice_len);

    memcpy(result->data, b->data + start, slice_len);
    return result;
}

void
buffer_set_slice(buffer_t *b, int64_t start, int64_t end, buffer_t *val)
{
    int64_t len = b->byte_len;

    if (start < 0) {
	start += len;
    }
    else {
	if (start >= len) {
	    CRAISE("Slice out-of-bounds.");
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
	    CRAISE("Slice out-of-bounds.");
    }

    int64_t   slice_len   = end - start;
    int64_t   new_len     = b->byte_len - slice_len;
    int64_t   replace_len = 0;

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
	char *new_buf = con4m_gc_alloc(new_len, NULL);
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

buffer_t *
buffer_copy(buffer_t *inbuf)
{
    buffer_t *outbuf = con4m_new(tspec_buffer(), "length", inbuf->byte_len);

    memcpy(outbuf->data, inbuf->data, inbuf->byte_len);

    return outbuf;
}

const con4m_vtable buffer_vtable = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	(con4m_vtable_entry)buffer_init,
	(con4m_vtable_entry)buffer_repr,
	NULL, // finalizer
	(con4m_vtable_entry)con4m_buffer_marshal,
	(con4m_vtable_entry)con4m_buffer_unmarshal,
	(con4m_vtable_entry)buffer_can_coerce_to,
	(con4m_vtable_entry)buffer_coerce_to,
	NULL, // From lit,
	(con4m_vtable_entry)buffer_copy,
	(con4m_vtable_entry)buffer_add,
	NULL, // Subtract
	NULL, // Mul
	NULL, // Div
	NULL, // MOD
	(con4m_vtable_entry)buffer_len,
	(con4m_vtable_entry)buffer_get_index,
	(con4m_vtable_entry)buffer_set_index,
	(con4m_vtable_entry)buffer_get_slice,
	(con4m_vtable_entry)buffer_set_slice,
    }
};


// First word really means 5th word due to the GC header.
const uint64_t pmap_first_word[2] = { 0x1, 0x0800000000000000 };

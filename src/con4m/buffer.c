#include <con4m.h>

static void
buffer_init(buffer_t *obj, va_list args)
{
    DECLARE_KARGS(
	int64_t    length = -1;
	any_str_t *hex    = NULL;
	);
    method_kargs(args, length, hex);

    if (length == -1 && hex == NULL) {
	abort();
    }
    if (length != -1 && hex != NULL) {
	abort();
    }

    if (length == -1) {
	length = string_codepoint_len(hex) >> 1;
    }

    obj->data = con4m_gc_alloc(length, NULL);

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

static char to_hex_map[] = "0123456789abcdef";

utf8_t *
buffer_repr(buffer_t *buf, to_str_use_t how)
{
    utf8_t *result;

    if (how == TO_STR_USE_QUOTED) {
	result = con4m_new(T_UTF8, "length", buf->byte_len * 4 + 2);
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
	result = con4m_new(T_UTF8, "length", buf->byte_len * 2);
	char *p = result->data;

	for (int i = 0; i < buf->byte_len; i++) {
	    uint8_t c = (uint8_t *)buf->data[i];
	    *p++ = to_hex_map[(c >> 4)];
	    *p++ = to_hex_map[c & 0x0f];
	}

	result->codepoints = p - result->data;
	result->byte_len   = result->codepoints;
    }
    return result;
}

const con4m_vtable buffer_vtable = {
    .num_entries = 2,
    .methods     = {
	(con4m_vtable_entry)buffer_init,
	(con4m_vtable_entry)buffer_repr,
    }
};

const uint64_t pmap_first_word[2] = { 0x1, 0x8000000000000000 };

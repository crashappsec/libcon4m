#include <con4m.h>

static buffer_t *
buffer_new(va_list args)
{
    DECLARE_KARGS(
	int64_t     len = -1;
	real_str_t *hex = NULL;
	);
    method_kargs(args, len, hex);

    if (len == -1 && hex == NULL) {
	abort();
    }
    if (len != -1 && hex != NULL) {
	abort();
    }

    if (len == -1) {
	len = internal_num_cp(hex) >> 1;
    }

    con4m_obj_t *obj     = con4m_gc_alloc(get_real_alloc_len(len), NULL);
    buffer_t     *result = (buffer_t *)obj->data;



    obj->base_data_type = (con4m_dt_info *)&builtin_type_info[T_BUFFER];
    obj->concrete_type  = T_BUFFER;


    if (hex != NULL) {
	uint8_t cur         = 0;
	int     valid_count = 0;

	hex = to_internal(force_utf8(hex->data));

	for (int i = 0; i < hex->byte_len; i++) {
	    uint8_t byte = hex->data[i];
	    if (byte >= '0' && byte <= '9') {
		if ((++valid_count) % 2 == 1) {
		    cur = (byte - '0') << 4;
		}
		else {
		    cur |= (byte - '0');
		    result->data[result->byte_len++] = cur;
		}
		continue;
	    }
	    if (byte >= 'a' && byte <= 'f') {
		if ((++valid_count) % 2 == 1) {
		    cur = ((byte - 'a') + 10) << 4;
		}
		else {
		    cur |= (byte - 'a') + 10;
		    result->data[result->byte_len++] = cur;
		}
		continue;
	    }
	    if (byte >= 'A' && byte <= 'F') {
		if ((++valid_count) % 2 == 1) {
		    cur = ((byte - 'A') + 10) << 4;
		}
		else {
		    cur |= (byte - 'A') + 10;
		    result->data[result->byte_len++] = cur;
		}
		continue;
	    }
	}
    }
    else {
	result->byte_len = len;
    }

    return result;
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
	    char c = buf->data[i];
	    *p++ = to_hex_map[(c >> 4)];
	    *p++ = to_hex_map[c & 0x0f];
	}
    }
    return result;
}

const con4m_vtable buffer_vtable = {
    .num_entries = 2,
    .methods     = {
	(con4m_vtable_entry)buffer_new,
	(con4m_vtable_entry)buffer_repr,
    }
};

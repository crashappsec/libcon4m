#include <con4m.h>


// I realize some of this is redundant, but it's just easier.
typedef struct {
    char      addr[sizeof(struct sockaddr_in6)];
    uint16_t  port;
    int32_t   af;
} ipaddr_t;
void
ipaddr_set_address(ipaddr_t *obj, any_str_t *s, uint16_t port)
{
    s = force_utf8(s);

    obj->port = port;

    if (inet_pton(obj->af, s->data, (void *)obj->addr) <= 0) {
	CRAISE("Invalid ip address");
    }

    if (obj->af == AF_INET) {
	((struct sockaddr_in *)&(obj->addr))->sin_port = port;
    }
    else {
	((struct sockaddr_in6 *)&(obj->addr))->sin6_port = port;
    }
}

static void
ipaddr_init(ipaddr_t *obj, va_list args)
{
    DECLARE_KARGS(
	any_str_t *address = NULL;
	int32_t    port    = -1;
	int32_t    ipv6    = 0;
	);

    method_kargs(args, address, port);

    if (ipv6) {
	obj->af = AF_INET6;
    }
    else {
	obj->af = AF_INET;
    }

    if (address != NULL) {
	if (port < 0 || port > 0xffff) {
	    CRAISE("Invalid port for IP address.");
	}
	ipaddr_set_address(obj, address, (uint16_t)port);
    }
}

// TODO: currently this isn't at all portable across platforms.
// Too quick and dirty.
static void
ipaddr_marshal(ipaddr_t *obj, stream_t *s, dict_t *memos, int64_t *mid)
{
    marshal_u32(sizeof(struct sockaddr_in6), s);
    stream_raw_write(s, sizeof(struct sockaddr_in6), obj->addr);
    marshal_u16(obj->port, s);
    marshal_i32(obj->af, s);
}

static void
ipaddr_unmarshal(ipaddr_t *obj, stream_t *s, dict_t *memos)
{
    uint32_t struct_sz = unmarshal_u32(s);

    if (struct_sz != sizeof(struct sockaddr_in6)) {
	CRAISE("Cannot unmarshal ipaddr on different platform.");
    }

    stream_raw_read(s, struct_sz, obj->addr);
    obj->port = unmarshal_u16(s);
    obj->af   = unmarshal_i32(s);
}

static any_str_t *
ipaddr_repr(ipaddr_t *obj)
{
    char buf[INET6_ADDRSTRLEN + 1] = {0,};

    if (!inet_ntop(obj->af, &obj->addr, buf, sizeof(struct sockaddr_in6))) {
	CRAISE("Unable to format ip address");
    }

    if (obj->port == 0) {
	return con4m_new(tspec_utf8(), "cstring", buf);
    }

    return string_concat(con4m_new(tspec_utf8(), "cstring", buf),
			string_concat(get_colon_no_space_const(),
				     string_from_int((int64_t)obj->port)));
}

const con4m_vtable ipaddr_vtable = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	(con4m_vtable_entry)ipaddr_init,
	(con4m_vtable_entry)ipaddr_repr,
	NULL,
	(con4m_vtable_entry)ipaddr_marshal,
	(con4m_vtable_entry)ipaddr_unmarshal,
	NULL,
    }
};

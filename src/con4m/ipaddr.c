#include "con4m.h"

// I realize some of this is redundant, but it's just easier.
typedef struct {
    char     addr[sizeof(struct sockaddr_in6)];
    uint16_t port;
    int32_t  af;
} ipaddr_t;

void
ipaddr_set_address(ipaddr_t *obj, any_str_t *s, uint16_t port)
{
    s = c4m_to_utf8(s);

    obj->port = port;

    if (inet_pton(obj->af, s->data, (void *)obj->addr) <= 0) {
        C4M_CRAISE("Invalid ip address");
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
    any_str_t *address = NULL;
    int32_t    port    = -1;
    bool       ipv6    = false;

    c4m_karg_va_init(args);
    c4m_kw_ptr("address", address);
    c4m_kw_int32("port", port);
    c4m_kw_bool("ipv6", ipv6);

    if (ipv6) {
        obj->af = AF_INET6;
    }
    else {
        obj->af = AF_INET;
    }

    if (address != NULL) {
        if (port < 0 || port > 0xffff) {
            C4M_CRAISE("Invalid port for IP address.");
        }
        ipaddr_set_address(obj, address, (uint16_t)port);
    }
}

// TODO: currently this isn't at all portable across platforms.
// Too quick and dirty.
static void
ipaddr_marshal(ipaddr_t *obj, stream_t *s, dict_t *memos, int64_t *mid)
{
    c4m_marshal_u32(sizeof(struct sockaddr_in6), s);
    c4m_stream_raw_write(s, sizeof(struct sockaddr_in6), obj->addr);
    c4m_marshal_u16(obj->port, s);
    c4m_marshal_i32(obj->af, s);
}

static void
ipaddr_unmarshal(ipaddr_t *obj, stream_t *s, dict_t *memos)
{
    uint32_t struct_sz = c4m_unmarshal_u32(s);

    if (struct_sz != sizeof(struct sockaddr_in6)) {
        C4M_CRAISE("Cannot unmarshal ipaddr on different platform.");
    }

    c4m_stream_raw_read(s, struct_sz, obj->addr);
    obj->port = c4m_unmarshal_u16(s);
    obj->af   = c4m_unmarshal_i32(s);
}

static any_str_t *
ipaddr_repr(ipaddr_t *obj)
{
    char buf[INET6_ADDRSTRLEN + 1] = {
        0,
    };

    if (!inet_ntop(obj->af, &obj->addr, buf, sizeof(struct sockaddr_in6))) {
        C4M_CRAISE("Unable to format ip address");
    }

    if (obj->port == 0) {
        return c4m_new(c4m_tspec_utf8(), c4m_kw("cstring", c4m_ka(buf)));
    }

    return c4m_str_concat(c4m_new(c4m_tspec_utf8(), c4m_kw("cstring", c4m_ka(buf))),
                          c4m_str_concat(c4m_get_colon_no_space_const(),
                                         c4m_str_from_int((int64_t)obj->port)));
}

const c4m_vtable_t c4m_ipaddr_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        (c4m_vtable_entry)ipaddr_init,
        (c4m_vtable_entry)ipaddr_repr,
        NULL,
        (c4m_vtable_entry)ipaddr_marshal,
        (c4m_vtable_entry)ipaddr_unmarshal,
        NULL,
    },
};

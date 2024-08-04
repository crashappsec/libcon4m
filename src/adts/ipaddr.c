#include "con4m.h"

void
c4m_ipaddr_set_address(c4m_ipaddr_t *obj, c4m_str_t *s, uint16_t port)
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
ipaddr_init(c4m_ipaddr_t *obj, va_list args)
{
    c4m_str_t *address = NULL;
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
        c4m_ipaddr_set_address(obj, address, (uint16_t)port);
    }
}

static c4m_str_t *
ipaddr_repr(c4m_ipaddr_t *obj)
{
    char buf[INET6_ADDRSTRLEN + 1] = {
        0,
    };

    if (!inet_ntop(obj->af, &obj->addr, buf, sizeof(struct sockaddr_in6))) {
        C4M_CRAISE("Unable to format ip address");
    }

    if (obj->port == 0) {
        return c4m_new(c4m_type_utf8(), c4m_kw("cstring", c4m_ka(buf)));
    }

    return c4m_str_concat(c4m_new(c4m_type_utf8(), c4m_kw("cstring", c4m_ka(buf))),
                          c4m_str_concat(c4m_get_colon_no_space_const(),
                                         c4m_str_from_int((int64_t)obj->port)));
}

static c4m_ipaddr_t *
ipaddr_lit(c4m_utf8_t          *s_u8,
           c4m_lit_syntax_t     st,
           c4m_utf8_t          *litmod,
           c4m_compile_error_t *err)
{
    c4m_ipaddr_t *result = c4m_new(c4m_type_ip());

    if (inet_pton(AF_INET, s_u8->data, result) == 1) {
        return result;
    }

    if (inet_pton(AF_INET6, s_u8->data, result) == 1) {
        return result;
    }

    *err = c4m_err_invalid_ip;

    return NULL;
}

const c4m_vtable_t c4m_ipaddr_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR]  = (c4m_vtable_entry)ipaddr_init,
        [C4M_BI_TO_STR]       = (c4m_vtable_entry)ipaddr_repr,
        [C4M_BI_GC_MAP]       = (c4m_vtable_entry)C4M_GC_SCAN_NONE,
        [C4M_BI_FROM_LITERAL] = (c4m_vtable_entry)ipaddr_lit,
        // Explicit because some compilers don't seem to always properly
        // zero it (Was sometimes crashing on a `c4m_stream_t` on my mac).
        [C4M_BI_FINALIZER]    = NULL,
    },
};

#pragma once
#include "con4m.h"

// I realize some of this is redundant, but it's just easier.
typedef struct {
    char     addr[sizeof(struct sockaddr_in6)];
    uint16_t port;
    int32_t  af;
} c4m_ipaddr_t;

extern void
c4m_ipaddr_set_address(c4m_ipaddr_t *obj, c4m_str_t *s, uint16_t port);

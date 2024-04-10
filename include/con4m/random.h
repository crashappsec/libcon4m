#pragma once

#include "con4m.h"

#if defined(__APPLE__) || defined(BSD)

static inline uint64_t
c4m_rand64()
{
    uint64_t res;

    arc4random_buf(&res, 8);

    return res;
}

#elif defined(__linux__)

static inline uint64_t
c4m_rand64()
{
    uint64_t res;

    while (getrandom(&res, 8, GRND_NONBLOCK) != 8)
        // retry on interrupt.
        ;

    return res;
}

#else
#error "Unsupported platform."
#endif

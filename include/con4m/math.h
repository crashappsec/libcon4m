// Random math stuff
#include "con4m.h"

extern uint64_t c4m_clz(uint64_t);

static inline uint64_t
c4m_int_log2(uint64_t n)
{
    return 63 - __builtin_clzll(n);
}

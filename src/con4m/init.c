// When libcon4m is actually used as a library, call this, because the
// constructors are likely to not get called properly.

#include "con4m.h"

__attribute__((constructor)) void
c4m_init()
{
    c4m_initialize_gc();
    c4m_initialize_global_types();
    c4m_gc_openssl();
}

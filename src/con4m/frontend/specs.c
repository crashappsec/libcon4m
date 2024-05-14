#include "con4m.h"

c4m_spec_t *
c4m_new_spec()
{
    c4m_spec_t *result = c4m_gc_alloc(c4m_spec_t);

    result->section_specs = c4m_new(c4m_tspec_dict(c4m_tspec_utf8(),
                                                   c4m_tspec_ref()));

    return result;
}

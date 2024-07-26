#pragma once
#include "con4m.h"

extern c4m_spec_t         *c4m_new_spec();
extern c4m_attr_info_t    *c4m_get_attr_info(c4m_spec_t *, c4m_list_t *);
extern c4m_spec_field_t   *c4m_new_spec_field();
extern c4m_spec_section_t *c4m_new_spec_section();
extern c4m_grid_t         *c4m_repr_spec(c4m_spec_t *);
extern c4m_grid_t         *c4m_grid_repr_section(c4m_spec_section_t *);

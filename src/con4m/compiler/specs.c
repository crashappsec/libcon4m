#include "con4m.h"

c4m_spec_t *
c4m_new_spec()
{
    c4m_spec_t *result = c4m_gc_alloc(c4m_spec_t);

    result->section_specs = c4m_new(c4m_type_dict(c4m_type_utf8(),
                                                   c4m_type_ref()));

    return result;
}

c4m_attr_info_t *
c4m_get_attr_info(c4m_spec_t *spec, c4m_list_t *fqn)
{
    c4m_attr_info_t *result = c4m_gc_alloc(c4m_attr_info_t);

    if (!spec || !spec->root_section) {
        result->kind = c4m_attr_user_def_field;
        return result;
    }

    c4m_spec_section_t *cur_sec = spec->root_section;
    int                 i       = 0;
    int                 n       = c4m_list_len(fqn) - 1;

    while (true) {
        c4m_utf8_t       *cur_name = c4m_to_utf8(c4m_list_get(fqn, i, NULL));
        c4m_spec_field_t *field    = hatrack_dict_get(cur_sec->fields,
                                                   cur_name,
                                                   NULL);
        if (field != NULL) {
            if (i != n) {
                result->err = c4m_attr_err_sec_under_field;
                return result;
            }
            else {
                result->info.field_info = field;
                result->kind            = c4m_attr_field;
                return result;
            }
        }

        cur_sec = hatrack_dict_get(spec->section_specs, cur_name, NULL);

        if (cur_sec == NULL) {
            if (i == n) {
                if (cur_sec->user_def_ok) {
                    result->kind = c4m_attr_user_def_field;
                    return result;
                }
                else {
                    result->err     = c4m_attr_err_field_not_allowed;
                    result->err_arg = cur_name;
                    return result;
                }
            }
            else {
                result->err_arg = cur_name;
                result->err     = c4m_attr_err_no_such_sec;
                return result;
            }
        }

        if (!c4m_set_contains(cur_sec->allowed_sections, cur_name)) {
            if (!c4m_set_contains(cur_sec->required_sections, cur_name)) {
                result->err_arg = cur_name;
                result->err     = c4m_attr_err_sec_not_allowed;
                return result;
            }
        }

        if (i == n) {
            result->info.sec_info = cur_sec;
            if (cur_sec->singleton) {
                result->kind = c4m_attr_singleton;
            }
            else {
                result->kind = c4m_attr_object_type;
            }

            return result;
        }

        if (cur_sec->singleton != 0) {
            i += 1;
            if (i == n) {
                result->kind = c4m_attr_instance;
                return result;
            }
        }
        i += 1;
    }
}

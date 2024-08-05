#include "con4m.h"

static int
section_sort(c4m_spec_section_t **sp1, c4m_spec_section_t **sp2)
{
    return strcmp((*sp1)->name->data, (*sp2)->name->data);
}

static inline c4m_utf8_t *
format_field_opts(c4m_spec_field_t *field)
{
    c4m_list_t *opts = c4m_list(c4m_type_utf8());
    if (field->required) {
        c4m_list_append(opts, c4m_new_utf8("required"));
    }
    else {
        c4m_list_append(opts, c4m_new_utf8("optional"));
    }
    if (field->user_def_ok) {
        c4m_list_append(opts, c4m_new_utf8("user fields ok"));
    }
    else {
        c4m_list_append(opts, c4m_new_utf8("no user fields"));
    }
    if (field->lock_on_write) {
        c4m_list_append(opts, c4m_new_utf8("write lock"));
    }
    if (field->validate_range) {
        c4m_list_append(opts, c4m_new_utf8("range"));
    }
    else {
        if (field->validate_choice) {
            c4m_list_append(opts, c4m_new_utf8("choice"));
        }
    }
    if (field->validator != NULL) {
        c4m_list_append(opts, c4m_new_utf8("validator"));
    }
    if (field->hidden) {
        c4m_list_append(opts, c4m_new_utf8("hide field"));
    }

    return c4m_to_utf8(c4m_str_join(opts, c4m_new_utf8(", ")));
}

c4m_grid_t *
c4m_grid_repr_section(c4m_spec_section_t *section)
{
    c4m_list_t       *l;
    c4m_utf8_t       *s;
    c4m_renderable_t *r;
    c4m_grid_t       *one;
    c4m_grid_t       *result = c4m_new(c4m_type_grid(),
                                 c4m_kw("start_rows",
                                        c4m_ka(1),
                                        "start_cols",
                                        c4m_ka(1),
                                        "container_tag",
                                        c4m_ka(c4m_new_utf8("flow"))));

    if (section->name) {
        c4m_grid_add_row(result,
                         c4m_cstr_format("[h2]Section[/] [h3]{}[/]",

                                         section->name));
    }
    else {
        c4m_grid_add_row(result, c4m_cstr_format("[h2]Root Section"));
    }

    if (section->short_doc) {
        c4m_grid_add_row(result, section->short_doc);
    }
    else {
        l = c4m_list(c4m_type_utf8());
        c4m_list_append(l, c4m_cstr_format("[h1]No short doc provided"));
        c4m_grid_add_row(result, l);
    }
    if (section->long_doc) {
        l = c4m_list(c4m_type_utf8());
        c4m_list_append(l, section->long_doc);
        c4m_grid_add_row(result, l);
    }
    else {
        l = c4m_list(c4m_type_utf8());
        c4m_list_append(l, c4m_cstr_format("[em]Overview not provided[/]"));
        c4m_grid_add_row(result, l);
    }

    c4m_list_t *reqs   = c4m_set_to_xlist(section->required_sections);
    c4m_list_t *allows = c4m_set_to_xlist(section->allowed_sections);
    uint64_t    n      = c4m_list_len(reqs);

    if (!n) {
        l = c4m_list(c4m_type_utf8());
        c4m_list_append(l, c4m_cstr_format("[h1]No required subsections."));
        c4m_grid_add_row(result, l);
    }
    else {
        one = c4m_new(c4m_type_grid(),
                      c4m_kw("start_rows",
                             c4m_ka(2),
                             "start_cols",
                             c4m_ka(n),
                             "container_tag",
                             c4m_ka(c4m_new_utf8("table"))));

        s = c4m_new_utf8("Required Subsections");
        r = c4m_to_str_renderable(s, c4m_new_utf8("h3"));

        // tmp dummy row.
        c4m_grid_add_row(one, c4m_list(c4m_type_utf8()));
        c4m_grid_add_col_span(one, r, 0, 0, n);
        c4m_grid_add_row(one, reqs);
        c4m_grid_add_row(result, one);
    }

    n = c4m_list_len(allows);

    if (!n) {
        l = c4m_list(c4m_type_utf8());
        c4m_list_append(l, c4m_cstr_format("[i]No allowed subsections."));
        c4m_grid_add_row(result, l);
    }
    else {
        one = c4m_new(c4m_type_grid(),
                      c4m_kw("start_rows",
                             c4m_ka(2),
                             "start_cols",
                             c4m_ka(n),
                             "container_tag",
                             c4m_ka(c4m_new_utf8("table"))));

        s = c4m_new_utf8("Allowed Subsections");
        r = c4m_to_str_renderable(s, c4m_new_utf8("h3"));

        c4m_grid_add_row(one, c4m_list(c4m_type_utf8()));
        c4m_grid_add_col_span(one, r, 0, 0, n);
        c4m_grid_add_row(one, allows);
        c4m_grid_add_row(result, one);
    }

    c4m_spec_field_t **fields = (void *)hatrack_dict_values(section->fields,
                                                            &n);

    if (n == 0) {
        l = c4m_list(c4m_type_grid());
        c4m_list_append(l, c4m_callout(c4m_new_utf8("No field specs.")));

        c4m_grid_add_row(result, l);
        return result;
    }

    one = c4m_new(c4m_type_grid(),
                  c4m_kw("start_cols",
                         c4m_ka(7),
                         "start_rows",
                         c4m_ka(2),
                         "th_tag",
                         c4m_ka(c4m_new_utf8("h2")),
                         "stripe",
                         c4m_ka(true),
                         "container_tag",
                         c4m_ka(c4m_new_utf8("table2"))));
    s   = c4m_new_utf8("FIELD SPECIFICATIONS");
    r   = c4m_to_str_renderable(s, c4m_new_utf8("h3"));

    c4m_grid_add_row(one, c4m_list(c4m_type_utf8()));
    c4m_grid_add_col_span(one, r, 0, 0, 7);

    l = c4m_list(c4m_type_utf8());

    c4m_list_append(l, c4m_rich_lit("[h2]Name"));
    c4m_list_append(l, c4m_rich_lit("[h2]Short Doc"));
    c4m_list_append(l, c4m_rich_lit("[h2]Long Doc"));
    c4m_list_append(l, c4m_rich_lit("[h2]Type"));
    c4m_list_append(l, c4m_rich_lit("[h2]Default"));
    c4m_list_append(l, c4m_rich_lit("[h2]Options"));
    c4m_list_append(l, c4m_rich_lit("[h2]Exclusions"));
    c4m_grid_add_row(one, l);

    for (unsigned int i = 0; i < n; i++) {
        l                       = c4m_list(c4m_type_utf8());
        c4m_spec_field_t *field = fields[i];

        c4m_list_append(l, c4m_to_utf8(field->name));
        if (field->short_doc && field->short_doc->codepoints) {
            c4m_list_append(l, field->short_doc);
        }
        else {
            c4m_list_append(l, c4m_new_utf8("None"));
        }
        if (field->long_doc && field->long_doc->codepoints) {
            c4m_list_append(l, field->long_doc);
        }
        else {
            c4m_list_append(l, c4m_new_utf8("None"));
        }
        if (field->have_type_pointer) {
            c4m_list_append(l,
                            c4m_cstr_format("-> {}",
                                            field->tinfo.type_pointer));
        }
        else {
            c4m_list_append(l, c4m_cstr_format("{}", field->tinfo.type));
        }
        if (field->default_provided) {
            c4m_list_append(l, c4m_cstr_format("{}", field->default_value));
        }
        else {
            c4m_list_append(l, c4m_new_utf8("None"));
        }

        c4m_list_append(l, format_field_opts(field));

        c4m_list_t *exclude = c4m_set_to_xlist(field->exclusions);

        if (!c4m_list_len(exclude)) {
            c4m_list_append(l, c4m_new_utf8("None"));
        }
        else {
            c4m_list_append(l, c4m_str_join(exclude, c4m_new_utf8(", ")));
        }

        c4m_grid_add_row(one, l);
    }

    c4m_grid_add_row(result, one);

    return result;
}

c4m_grid_t *
c4m_repr_spec(c4m_spec_t *spec)
{
    if (!spec || !spec->in_use) {
        return c4m_callout(c4m_new_utf8("No specification provided."));
    }

    c4m_grid_t *result = c4m_new(c4m_type_grid(),
                                 c4m_kw("start_rows",
                                        c4m_ka(1),
                                        "start_cols",
                                        c4m_ka(1),
                                        "container_tag",
                                        c4m_ka(c4m_new_utf8("flow"))));

    if (spec->short_doc) {
        c4m_grid_add_row(result, spec->short_doc);
    }
    else {
        c4m_grid_add_row(result,
                         c4m_to_str_renderable(
                             c4m_new_utf8("Configuration Specification"),
                             c4m_new_utf8("h2")));
    }
    if (spec->long_doc) {
        c4m_grid_add_row(result, spec->long_doc);
    }
    else {
        c4m_grid_add_row(result,
                         c4m_cstr_format("[i]Overview not provided[/]"));
    }

    c4m_grid_add_row(result, c4m_grid_repr_section(spec->root_section));

    uint64_t             n;
    c4m_spec_section_t **secs = (void *)hatrack_dict_values(spec->section_specs,
                                                            &n);

    qsort(secs, (size_t)n, sizeof(c4m_spec_section_t *), (void *)section_sort);

    for (unsigned int i = 0; i < n; i++) {
        c4m_grid_add_row(result, c4m_grid_repr_section(secs[i]));
    }

    if (spec->locked) {
        c4m_grid_add_row(result, c4m_cstr_format("[em]This spec is locked."));
    }

    return result;
}

void
c4m_spec_gc_bits(uint64_t *bitmap, c4m_spec_t *spec)
{
    c4m_mark_raw_to_addr(bitmap, spec, &spec->section_specs);
}

void
c4m_attr_info_gc_bits(uint64_t *bitmap, c4m_attr_info_t *ai)
{
    c4m_mark_raw_to_addr(bitmap, ai, &ai->info.field_info);
}

void
c4m_section_gc_bits(uint64_t *bitmap, c4m_spec_section_t *sec)
{
    c4m_mark_raw_to_addr(bitmap, sec, &sec->validator);
}

void
c4m_spec_field_gc_bits(uint64_t *bitmap, c4m_spec_field_t *field)
{
    c4m_mark_raw_to_addr(bitmap, field, &field->exclusions);
}

c4m_spec_field_t *
c4m_new_spec_field(void)
{
    return c4m_gc_alloc_mapped(c4m_spec_field_t, c4m_spec_field_gc_bits);
}

c4m_spec_section_t *
c4m_new_spec_section(void)
{
    c4m_spec_section_t *section = c4m_gc_alloc_mapped(c4m_spec_section_t,
                                                      c4m_section_gc_bits);

    section->fields            = c4m_new(c4m_type_dict(c4m_type_utf8(),
                                            c4m_type_ref()));
    section->allowed_sections  = c4m_new(c4m_type_set(c4m_type_utf8()));
    section->required_sections = c4m_new(c4m_type_set(c4m_type_utf8()));

    return section;
}

c4m_spec_t *
c4m_new_spec(void)
{
    c4m_spec_t *result = c4m_gc_alloc_mapped(c4m_spec_t, c4m_spec_gc_bits);

    result->section_specs           = c4m_new(c4m_type_dict(c4m_type_utf8(),
                                                  c4m_type_ref()));
    result->root_section            = c4m_new_spec_section();
    result->root_section->singleton = true;

    return result;
}

c4m_attr_info_t *
c4m_get_attr_info(c4m_spec_t *spec, c4m_list_t *fqn)
{
    c4m_attr_info_t *result = c4m_gc_alloc_mapped(c4m_attr_info_t,
                                                  c4m_attr_info_gc_bits);

    if (!spec || !spec->root_section || !spec->in_use) {
        result->kind = c4m_attr_user_def_field;
        return result;
    }

    c4m_spec_section_t *cur_sec = spec->root_section;
    c4m_spec_section_t *next_sec;
    int                 i = 0;
    int                 n = c4m_list_len(fqn) - 1;

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

        next_sec = hatrack_dict_get(spec->section_specs, cur_name, NULL);

        if (next_sec == NULL) {
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
            if (next_sec->singleton) {
                result->kind = c4m_attr_singleton;
            }
            else {
                result->kind = c4m_attr_object_type;
            }

            result->info.sec_info = next_sec;

            return result;
        }

        cur_sec = next_sec;

        if (cur_sec->singleton == false) {
            i += 1;
            if (i == n) {
                result->kind          = c4m_attr_instance;
                result->info.sec_info = next_sec;

                return result;
            }
        }
        i += 1;
    }
}

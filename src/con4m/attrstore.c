#include "con4m.h"

static void
apply_section_defaults(c4m_vmthread_t *tstate, c4m_zsection_spec_t *spec, c4m_str_t *prefix)
{
    // When we're first populating a program, if there are defaults we can
    // set without instantiating objects, we do so. Similarly, when a `set`
    // ends up being the first write to some new object section, we call
    // this to populate any defaults in the object.

    if (c4m_len(prefix) > 0) {
        c4m_codepoint_t cp = (int64_t)c4m_index_get(prefix, (c4m_obj_t)-1);
        if (cp != '.') {
            C4M_STATIC_ASCII_STR(dot, ".");
            prefix = c4m_str_concat(prefix, dot);
        }
    }

    uint64_t             num;
    hatrack_dict_item_t *items = hatrack_dict_items_nosort(spec->fields, &num);
    for (uint64_t i = 0; i < num; ++i) {
        c4m_zfield_spec_t *f = items[i].value;
        if (f->have_default) {
            c4m_str_t *key = c4m_str_concat(prefix, items[i].key);
            c4m_vm_attr_set(tstate,
                            key,
                            &f->default_value,
                            f->lock_on_write,
                            false,
                            true);
        }
    }
    free(items);

    for (int64_t i = 0; i < c4m_xlist_len(spec->allowed_sections); ++i) {
        c4m_str_t *key = c4m_xlist_get(spec->allowed_sections, i, NULL);

        bool                 found;
        c4m_zsection_spec_t *s = hatrack_dict_get(tstate->vm->obj->spec->sec_specs,
                                                  key,
                                                  &found);
        if (!found) {
            // TODO print: f"error: No specification provided for section {key}"
            continue;
        }

        if (s->max_allowed <= 1) {
            c4m_str_t *p = c4m_str_concat(prefix, key);
            apply_section_defaults(tstate, s, p);
        }
    }
}

static void
populate_defaults(c4m_vmthread_t *tstate, c4m_str_t *key)
{
    bool found;
    C4M_STATIC_ASCII_STR(dot, ".");

    c4m_xlist_t *parts = c4m_str_xsplit(key, (c4m_str_t *)dot);
    int64_t      l     = c4m_xlist_len(parts) - 1;
    c4m_str_t   *cur   = (c4m_str_t *)c4m_newline_const;
    int64_t      start = -1;

    for (int64_t i = 0; i < l; ++i) {
        if (!hatrack_set_contains(tstate->vm->all_sections, cur)) {
            start = i;
            hatrack_set_add(tstate->vm->all_sections, cur);
            for (int64_t n = i; n < l; ++n) {
                if (!n) {
                    cur = c4m_xlist_get(parts, 0, NULL);
                }
                else {
                    cur = c4m_str_concat(c4m_str_concat(cur, dot),
                                         c4m_xlist_get(parts, n, NULL));
                }
                hatrack_set_add(tstate->vm->all_sections, cur);
            }
            break;
        }
        if (!i && i != l) {
            cur = c4m_xlist_get(parts, 0, NULL);
        }
        else if (i != l) {
            cur = c4m_str_concat(c4m_str_concat(cur, dot),
                                 c4m_xlist_get(parts, i, NULL));
        }
    }
    if (-1 == start || NULL == tstate->vm->obj->spec) {
        return;
    }

    int64_t              i   = 0;
    c4m_zsection_spec_t *sec = tstate->vm->obj->spec->root_spec;
    c4m_str_t           *attr;

    if (!start) {
        apply_section_defaults(tstate,
                               sec,
                               (c4m_str_t *)c4m_empty_string_const);
    }

    while (i < start) {
        cur = c4m_xlist_get(parts, i, NULL);
        if (!i) {
            attr = cur;
        }
        else {
            attr = c4m_str_concat(c4m_str_concat(attr, dot), cur);
        }

        c4m_zsection_spec_t *spec = hatrack_dict_get(tstate->vm->obj->spec->sec_specs,
                                                     attr,
                                                     &found);
        if (!found) {
            return;
        }

        ++i;
        if (spec->max_allowed <= 1) {
            continue;
        }
        if (i + 1 == l) {
            start = i;
            break;
        }
        attr = c4m_str_concat(c4m_str_concat(attr, dot),
                              c4m_xlist_get(parts, i, NULL));
        ++i;
    }

    i = start;

    while (i < l) {
        if (sec->max_allowed <= 1) {
            apply_section_defaults(tstate, sec, attr);
        }
        else {
            if (!i) {
                attr = c4m_xlist_get(parts, i, NULL);
            }
            else {
                attr = c4m_str_concat(c4m_str_concat(attr, dot),
                                      c4m_xlist_get(parts, i, NULL));
            }
            apply_section_defaults(tstate, sec, attr);
            ++i;
            if (i == l) {
                break;
            }
        }

        cur = c4m_xlist_get(parts, i, NULL);
        sec = hatrack_dict_get(tstate->vm->obj->spec->sec_specs, cur, &found);
        if (!found) {
            return;
        }
        if (!i) {
            attr = cur;
        }
        else {
            attr = c4m_str_concat(c4m_str_concat(attr, dot), cur);
        }
        ++i;
    }
}

c4m_value_t *
c4m_vm_attr_get(c4m_vmthread_t *tstate, c4m_str_t *key, c4m_type_t *expected_type)
{
    populate_defaults(tstate, key);

    bool                 found;
    c4m_attr_contents_t *info = hatrack_dict_get(tstate->vm->attrs, key, &found);
    if (!found || !info->is_set) {
        // Nim version uses Con4mError stuff that doesn't exist in
        // libcon4m (yet?)
        C4M_STATIC_ASCII_STR(errstr, "attribute does not exist: ");
        c4m_utf8_t *msg = c4m_to_utf8(c4m_str_concat(errstr, key));
        C4M_RAISE(msg);
    }

    if (expected_type != NULL) {
        c4m_type_t *type = c4m_unify(expected_type,
                                     info->contents.type_info,
                                     c4m_global_type_env);
        if (c4m_tspec_error() == type) {
            C4M_STATIC_ASCII_STR(errstr, "attribute type does not match: ");
            c4m_utf8_t *msg = c4m_to_utf8(c4m_str_concat(errstr, key));
            C4M_RAISE(msg);
        }
    }

    return &info->contents;
}

void
c4m_vm_attr_set(c4m_vmthread_t *tstate,
                c4m_str_t      *key,
                c4m_value_t    *value,
                bool            lock,
                bool            override,
                bool            internal)
{
    c4m_vm_t *vm    = tstate->vm;
    vm->using_attrs = true;

    if (!internal) {
        populate_defaults(tstate, key);
    }

    // We will create a new entry on every write, just to avoid any race
    // conditions with multiple threads updating via reference.

    bool                 found;
    c4m_attr_contents_t *old_info = hatrack_dict_get(vm->attrs, key, &found);
    if (found) {
        // Nim code does this after allocating new_info and never settings it's
        // override field here, so that's clearly wrong. We do it first to avoid
        // wasting the allocation and swap to use old_info->override instead,
        // which makes more sense.
        if (old_info->override && !override) {
            return; // Pretend it was successful
        }
    }

    c4m_attr_contents_t *new_info
        = c4m_gc_raw_alloc(sizeof(c4m_attr_contents_t), GC_SCAN_ALL);
    *new_info = (c4m_attr_contents_t){
        .contents = *value,
        .is_set   = true,
    };

    if (found) {
        bool locked = (old_info->locked
                       || (old_info->module_lock != 0
                           && old_info->module_lock
                                  != tstate->current_module->module_id));
        if (locked) {
            if (!override) {
                if (!c4m_eq(value->type_info, value->obj, &old_info->contents)) {
                    // Nim version uses Con4mError stuff that doesn't exist in
                    // libcon4m (yet?)
                    C4M_STATIC_ASCII_STR(errstr, "attribute is locked: ");
                    c4m_utf8_t *msg = c4m_to_utf8(c4m_str_concat(errstr, key));
                    C4M_RAISE(msg);
                }
                // Set to same value; ignore it basically
                return;
            }
        }

        new_info->module_lock = old_info->module_lock;
        new_info->locked      = old_info->lock_on_write;
    }

    new_info->override = override;

    if (tstate->running) {
        new_info->lastset = c4m_xlist_get(tstate->current_module->instructions,
                                          tstate->pc - 1,
                                          NULL);
        if (c4m_xlist_len(tstate->module_lock_stack) > 0) {
            new_info->module_lock
                = (int32_t)(int64_t)c4m_xlist_get(tstate->module_lock_stack,
                                                  -1,
                                                  NULL);
        }
    }

    // Don't trigger write lock if we're setting a default (i.e., internal is set).
    if (lock && !internal) {
        new_info->locked = true;
    }

    hatrack_dict_put(vm->attrs, key, new_info);
}

void
c4m_vm_attr_lock(c4m_vmthread_t *tstate, c4m_str_t *key, bool on_write)
{
    c4m_vm_t *vm = tstate->vm;

    // We will create a new entry on every write, just to avoid any race
    // conditions with multiple threads updating via reference.

    bool                 found;
    c4m_attr_contents_t *old_info = hatrack_dict_get(vm->attrs, key, &found);
    if (found && old_info->locked) {
        // Nim version uses Con4mError stuff that doesn't exist in
        // libcon4m (yet?)
        C4M_STATIC_ASCII_STR(errstr, "attribute already locked: ");
        c4m_utf8_t *msg = c4m_to_utf8(c4m_str_concat(errstr, key));
        C4M_RAISE(msg);
    }

    c4m_attr_contents_t *new_info
        = c4m_gc_raw_alloc(sizeof(c4m_attr_contents_t), GC_SCAN_ALL);
    *new_info = (c4m_attr_contents_t){
        .lock_on_write = true,
    };

    if (found) {
        new_info->contents = old_info->contents;
        new_info->is_set   = old_info->is_set;
    }

    hatrack_dict_put(vm->attrs, key, new_info);
}

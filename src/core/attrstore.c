#include "con4m.h"

static void
populate_one_section(c4m_vmthread_t     *tstate,
                     c4m_spec_section_t *section,
                     c4m_str_t          *path)
{
    uint64_t           n;
    c4m_utf8_t        *key;
    c4m_spec_field_t  *f;
    c4m_spec_field_t **fields;

    c4m_vm_t *vm = tstate->vm;

    path = c4m_to_utf8(path);

    if (!hatrack_set_add(vm->all_sections, path)) {
        return;
    }

    if (!section) {
        return;
    }

    fields = (void *)hatrack_dict_values_sort(section->fields, &n);

    for (uint64_t i = 0; i < n; i++) {
        f = fields[i];

        if (!path || !c4m_str_byte_len(path)) {
            key = f->name;
        }
        else {
            key = c4m_cstr_format("{}.{}", path, f->name);
        }

        if (f->default_provided) {
            c4m_vm_attr_set(tstate,
                            key,
                            f->default_value,
                            f->tinfo.type,
                            f->lock_on_write,
                            false,
                            true);
        }
    }
}

static void
populate_defaults(c4m_vmthread_t *tstate, c4m_str_t *key)
{
    c4m_vm_t *vm = tstate->vm;

    if (!vm->obj->attr_spec || !vm->obj->attr_spec->in_use) {
        return;
    }

    int   ix = -1;
    char *p  = key->data + c4m_str_byte_len(key);

    while (--p > key->data) {
        if (*p == '.') {
            ix = p - key->data;
            break;
        }
    }

    if (!vm->root_populated) {
        populate_one_section(tstate,
                             vm->obj->attr_spec->root_section,
                             c4m_empty_string());
        vm->root_populated = true;
    }
    if (ix == -1) {
        return;
    }

    // The slice will also always ensure a private copy (right now).
    key = c4m_to_utf8(c4m_str_slice(key, 0, ix));

    if (c4m_set_contains(vm->all_sections, key)) {
        return;
    }

    p = key->data;

    int                 start_codepoints = 0;
    int                 n                = 0;
    char               *last_start       = p;
    char               *e                = p + c4m_str_byte_len(key);
    c4m_utf8_t         *dummy            = c4m_utf8_repeat(' ', 1);
    c4m_codepoint_t     cp;
    c4m_spec_section_t *section;

    key->byte_len   = 0;
    key->codepoints = 0;

    while (p < e) {
        n = utf8proc_iterate((const uint8_t *)p, 4, &cp);
        if (n < 0) {
            c4m_unreachable();
        }
        if (cp != '.') {
            key->codepoints += 1;
            p += n;
            continue;
        }

        // For the moment, turn the dot into a null terminator.
        *p = 0;

        // Set up the string we'll use to look up the section:
        dummy->data       = last_start;
        dummy->byte_len   = p - last_start;
        dummy->codepoints = key->codepoints - start_codepoints;
        key->byte_len     = p - key->data;

        // Can be null if it's an object; no worries.
        section = hatrack_dict_get(vm->obj->attr_spec->section_specs,
                                   dummy,
                                   NULL);

        populate_one_section(tstate, section, key);

        // Restore our fragile state.
        *p         = '.';
        last_start = ++p;
        key->codepoints += 1;
        start_codepoints = key->codepoints;
    }

    dummy->data       = last_start;
    dummy->byte_len   = p - last_start;
    dummy->codepoints = key->codepoints - start_codepoints;
    key->byte_len     = p - key->data;

    section = hatrack_dict_get(vm->obj->attr_spec->section_specs,
                               dummy,
                               NULL);

    populate_one_section(tstate, section, key);
}

void *
c4m_vm_attr_get(c4m_vmthread_t *tstate,
                c4m_str_t      *key,
                bool           *found)
{
    populate_defaults(tstate, key);

    c4m_attr_contents_t *info = hatrack_dict_get(tstate->vm->attrs, key, NULL);

    if (found != NULL) {
        if (info != NULL && info->is_set) {
            *found = true;
            return info->contents;
        }
        *found = false;
        return NULL;
    }

    if (info == NULL || !info->is_set) {
        // Nim version uses Con4mError stuff that doesn't exist in
        // libcon4m (yet?)
        C4M_STATIC_ASCII_STR(errstr, "attribute does not exist: ");
        c4m_utf8_t *msg = c4m_to_utf8(c4m_str_concat(errstr, key));
        C4M_RAISE(msg);
    }

    return info->contents;
}

void
c4m_vm_attr_set(c4m_vmthread_t *tstate,
                c4m_str_t      *key,
                void           *value,
                c4m_type_t     *type,
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
        = c4m_gc_raw_alloc(sizeof(c4m_attr_contents_t), C4M_GC_SCAN_ALL);
    *new_info = (c4m_attr_contents_t){
        .contents = value,
        .is_set   = true,
    };

    if (found) {
        bool locked = (old_info->locked
                       || (old_info->module_lock != 0
                           && old_info->module_lock
                                  != tstate->current_module->module_id));
        if (locked) {
            if (!override && old_info->is_set) {
                if (!c4m_eq(type, value, old_info->contents)) {
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
        new_info->lastset = c4m_list_get(tstate->current_module->instructions,
                                         tstate->pc - 1,
                                         NULL);
        /*
        if (c4m_list_len(tstate->module_lock_stack) > 0) {
            new_info->module_lock
                = (int32_t)(int64_t)c4m_list_get(tstate->module_lock_stack,
                                                 -1,
                                                 NULL);
        }
        */
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
        = c4m_gc_raw_alloc(sizeof(c4m_attr_contents_t), C4M_GC_SCAN_ALL);
    *new_info = (c4m_attr_contents_t){
        .lock_on_write = true,
    };

    if (found) {
        new_info->contents = old_info->contents;
        new_info->is_set   = old_info->is_set;
    }

    hatrack_dict_put(vm->attrs, key, new_info);
}

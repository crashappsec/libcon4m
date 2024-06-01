#include "con4m.h"

static void
populate_defaults(c4m_vm_t *vm, c4m_str_t *key)
{
    // TODO populate_defaults
}

c4m_value_t *
c4m_vm_attr_get(c4m_vmthread_t *tstate,
                c4m_str_t      *key,
                bool           *found)
{
    populate_defaults(tstate->vm, key);

    c4m_attr_contents_t *info = hatrack_dict_get(tstate->vm->attrs, key, NULL);
    if (found != NULL) {
        if (info != NULL && info->is_set) {
            *found = true;
            return &info->contents;
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
        populate_defaults(vm, key);
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

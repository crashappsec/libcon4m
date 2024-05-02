#include "con4m.h"
#include "con4m/datatypes/vm.h"
#include "con4m/string.h"

// This is a straight port from Nim. There's a ton of low hanging fruit for
// improved performance, but basically none of it really matters because
// validation isn't often run, so no consideration to performance has quite
// obviously been given here, though in at least a couple of places I could
// not let it go and changes things from what they were originally in the
// Nim code.

static bool
str_in_xlist(c4m_str_t *str, c4m_xlist_t *xlist)
{
    c4m_type_t *t = c4m_get_my_type(str);
    for (int64_t i = 0, l = c4m_xlist_len(xlist); i < l; ++i) {
        if (c4m_eq(t, str, c4m_xlist_get(xlist, i, NULL))) {
            return true;
        }
    }
    return false;
}

static c4m_value_t *
value_get(c4m_vmthread_t *tstate,
          c4m_str_t      *key,
          c4m_type_t     *expected_type,
          c4m_xlist_t    *errs)
{
    // This is just a wrapper around c4m_vm_attr_get that catches any errors
    // raised by c4m_vm_attr_get and records them in errs. Returns NULL if an
    // error occurred. This wrapper is for safety - nesting a setjmp in the
    // middle of validate_section wreaks all kinds of havoc, so this is both
    // easier and safer.

    c4m_value_t *volatile value;

    C4M_TRY
    {
        value = c4m_vm_attr_get(tstate, key, expected_type);
    }
    C4M_EXCEPT
    {
        // TODO record error in errs
        value = NULL;
        C4M_JUMP_TO_FINALLY();
    }
    C4M_FINALLY
    {
    }
    C4M_TRY_END;

    return value;
}

static void
validate_section(c4m_vmthread_t      *tstate,
                 c4m_attr_tree_t     *tree,
                 c4m_zsection_spec_t *spec,
                 c4m_xlist_t         *errs)
{
    // To validate a section we must:
    // 1. Look for sections that are present, but are not allowed.
    // 2. Look for required subsections that do not exist.
    // 3. Recurse into subsections to validate.
    // 4. Look for fields that don't exist, but should (noting exclusions)
    // 5. Check types of any fields with deferred_type set in its spec.
    // 6. Call any validation routines for the fields that do exist.
    // 7. Call any validation routines that cover the entire section.
    //
    // For instantiable sections, we do #3 once for each object (which will
    // cause #7 to run once for each object too.

    if (NULL == spec) {
        return;
    }
    c4m_vm_t *vm = tstate->vm;

    struct field_pair {
        c4m_str_t       *str;
        c4m_attr_tree_t *tree;
    };
    struct section_pair {
        int64_t    num;
        c4m_str_t *str;
    };

    c4m_xlist_t     *found_fields    = c4m_xlist(c4m_tspec_ref()); // field_pair
    c4m_xlist_t     *found_sections  = c4m_xlist(c4m_tspec_ref()); // section_pair
    c4m_xlist_t     *found_f_names   = c4m_xlist(c4m_tspec_utf8());
    c4m_xlist_t     *found_s_names   = c4m_xlist(c4m_tspec_utf8());
    c4m_xlist_t     *excluded_fields = c4m_xlist(c4m_tspec_utf8());
    const c4m_str_t *ready_to_append = c4m_empty_string_const;

    C4M_STATIC_ASCII_STR(dot, ".");
    C4M_STATIC_ASCII_STR(top, "the top-level configuration");
    if (!c4m_len(tree->path)) {
        tree->path = (c4m_str_t *)top;
    }
    else {
        ready_to_append = c4m_str_concat(tree->path, dot);
    }

    for (int64_t i = 0; i < c4m_xlist_len(tree->kids); ++i) {
        c4m_attr_tree_t *item = c4m_xlist_get(tree->kids, i, NULL);

        c4m_str_t *last = c4m_xlist_get(c4m_str_xsplit(item->path,
                                                       (c4m_str_t *)dot),
                                        -1,
                                        NULL);

        if (c4m_xlist_len(item->kids) == 0) {
            struct field_pair *pair = c4m_gc_alloc(struct field_pair);
            *pair                   = (struct field_pair){
                                  .str  = last,
                                  .tree = item,
            };
            c4m_xlist_append(found_fields, pair);
            c4m_xlist_append(found_f_names, last);
        }
        else {
            struct section_pair *pair = c4m_gc_alloc(struct section_pair);
            *pair                     = (struct section_pair){
                                    .num = i,
                                    .str = last,
            };
            c4m_xlist_append(found_sections, pair);
            c4m_xlist_append(found_s_names, last);
        }
    }

    for (int64_t i = 0; i < c4m_xlist_len(found_sections); ++i) {
        struct section_pair *secname = c4m_xlist_get(found_sections, i, NULL);
        if (!str_in_xlist(secname->str, spec->allowed_sections)) {
            if (!str_in_xlist(secname->str, spec->required_sections)) {
                // TODO error bad section
                continue;
            }
        }

        bool                 found;
        c4m_zsection_spec_t *new_spec = hatrack_dict_get(vm->obj->spec->sec_specs,
                                                         secname->str,
                                                         &found);
        c4m_attr_tree_t     *kid      = c4m_xlist_get(tree->kids, i, NULL);
        if (1 == new_spec->max_allowed) {
            validate_section(tstate, kid, new_spec, errs);
        }
        else {
            for (int64_t n = 0; n < c4m_xlist_len(kid->kids); ++n) {
                c4m_attr_tree_t *item = c4m_xlist_get(kid->kids, n, NULL);
                validate_section(tstate, item, new_spec, errs);
            }
        }
    }

    for (int64_t i = 0; i < c4m_xlist_len(spec->required_sections); ++i) {
        c4m_str_t *requirement = c4m_xlist_get(spec->required_sections, i, NULL);
        if (!str_in_xlist(requirement, found_s_names)) {
            // TODO error missing section
        }
    }

    if (!spec->user_def_ok) {
        for (int64_t i = 0; i < c4m_xlist_len(found_f_names); ++i) {
            c4m_str_t *name = c4m_xlist_get(found_f_names, i, NULL);

            bool found;
            (void)hatrack_dict_get(spec->fields, name, &found);
            if (!found) {
                // TODO error bad field
            }
        }
    }

    for (int64_t i = 0; i < c4m_xlist_len(found_f_names); ++i) {
        c4m_str_t *name = c4m_xlist_get(found_f_names, i, NULL);

        bool               found;
        c4m_zfield_spec_t *field_spec = hatrack_dict_get(spec->fields, name, &found);
        if (found) {
            for (int64_t n = 0; n < c4m_xlist_len(field_spec->exclusions); ++n) {
                c4m_str_t *item = c4m_xlist_get(field_spec->exclusions, n, NULL);
                if (str_in_xlist(item, found_f_names)) {
                    // TODO error field mutex
                }
                else {
                    c4m_xlist_append(excluded_fields, item);
                }
            }
        }
    }

    uint64_t             num;
    hatrack_dict_item_t *items = hatrack_dict_items_nosort(spec->fields, &num);
    for (uint64_t i = 0; i < num; ++i) {
        c4m_str_t         *name       = items[i].key;
        c4m_zfield_spec_t *field_spec = items[i].value;

        if (field_spec->required) {
            if (!str_in_xlist(name, found_f_names)) {
                if (!str_in_xlist(name, excluded_fields)) {
                    // TODO error missing field
                    continue;
                }
            }
        }

        c4m_type_t  *expected_type = NULL;
        c4m_str_t   *full_path;
        c4m_value_t *value;
        bool         found_val = false;

        if (c4m_len(field_spec->deferred_type)) {
            full_path = c4m_str_concat(ready_to_append, field_spec->deferred_type);

            value = value_get(tstate, full_path, NULL, errs);
            if (NULL == value) {
                continue;
            }

            expected_type = c4m_global_type_check(value->type_info,
                                                  c4m_tspec_typespec());
            if (c4m_tspec_error() == expected_type) {
                // TODO error not a tspec
                continue;
            }
        }

        full_path = c4m_str_concat(ready_to_append, name);

        for (int64_t n = 0; n < c4m_xlist_len(found_fields); ++n) {
            struct field_pair *pair = c4m_xlist_get(found_fields, n, NULL);

            c4m_str_t *last = c4m_xlist_get(c4m_str_xsplit(pair->tree->path, (c4m_str_t *)dot),
                                            -1,
                                            NULL);
            if (c4m_eq(c4m_get_my_type(pair->str), pair->str, last)) {
                value     = value_get(tstate, full_path, expected_type, errs);
                found_val = true;
                break;
            }
        }

        if (!found_val) {
            if (field_spec->required) {
                // This is in some sense a dupe check to above, but it's possible
                // for us to att an entry w/o setting a value.
                // TODO error missing field
            }
            continue;
        }

        for (int64_t n = 0; n < c4m_xlist_len(field_spec->validators); ++n) {
            c4m_zvalidator_t *v = c4m_xlist_get(field_spec->validators, n, NULL);

            c4m_obj_t r = v->fn.field_validator(tstate,
                                                full_path,
                                                value,
                                                v->params);
            if (r != NULL) {
                c4m_xlist_append(errs, r);
            }
        }
    }
    free(items);

    for (int64_t n = 0; n < c4m_xlist_len(spec->validators); ++n) {
        c4m_zvalidator_t *v = c4m_xlist_get(spec->validators, n, NULL);

        c4m_obj_t r = v->fn.section_validator(tstate,
                                              tree->path,
                                              found_f_names,
                                              v->params);
        if (r != NULL) {
            c4m_xlist_append(errs, r);
        }
    }
}

static c4m_attr_tree_t *
c4m_vm_attr_tree(c4m_vm_t *vm, bool view)
{
    C4M_STATIC_ASCII_STR(dot, ".");

    c4m_attr_tree_t *tree = c4m_gc_alloc(c4m_attr_tree_t);
    tree->kids            = c4m_xlist(c4m_tspec_ref());
    if (view) {
        tree->cache = vm->attrs;
    }

    uint64_t            num;
    hatrack_dict_key_t *keys = hatrack_dict_keys_sort(vm->attrs, &num);
    c4m_attr_tree_t    *cur  = tree;
    for (uint64_t i = 0; i < num; ++i) {
        c4m_xlist_t *parts = c4m_str_xsplit(((c4m_str_t **)keys)[i],
                                            (c4m_str_t *)dot);
        int64_t      l     = c4m_xlist_len(parts);

        for (int64_t i = 0; i < l; ++i) {
            c4m_xlist_t *slice = c4m_slice_get(parts, 0, i);
            c4m_str_t   *p     = c4m_str_join(slice, dot);
            bool         found = false;

            for (int64_t n = 0; n < c4m_xlist_len(cur->kids); ++n) {
                c4m_attr_tree_t *kid = c4m_xlist_get(cur->kids, n, NULL);
                if (c4m_eq(c4m_get_my_type(kid), kid->path, p)) {
                    found = true;
                    cur   = kid;
                    break;
                }
            }

            if (!found) {
                c4m_attr_tree_t *node = c4m_gc_alloc(c4m_attr_tree_t);
                node->kids            = c4m_xlist(c4m_tspec_ref());
                if (view) {
                    node->cache = vm->attrs;
                }
                c4m_xlist_append(cur->kids, node);
                cur = node;
            }
        }
    }
    free(keys);

    // TODO why? c4m_xlist_sort(tree->kids);
    return tree;
}

c4m_xlist_t *
c4m_run_validator(c4m_vmthread_t *tstate, c4m_str_t *startwith)
{
    c4m_vm_t *vm = tstate->vm;

    c4m_xlist_t *errs = c4m_xlist(c4m_tspec_error());
    if (NULL == vm->obj->spec || !vm->obj->spec->used) {
        return errs;
    }

    c4m_attr_tree_t *tree = c4m_vm_attr_tree(vm, false);
    if (!c4m_len(startwith)) {
        validate_section(tstate, tree, vm->obj->spec->root_spec, errs);
    }
    else {
        C4M_STATIC_ASCII_STR(dot, ".");

        c4m_xlist_t         *parts = c4m_str_xsplit(startwith, (c4m_str_t *)dot);
        c4m_str_t           *sofar = (c4m_str_t *)c4m_empty_string_const;
        c4m_zsection_spec_t *spec  = vm->obj->spec->root_spec;

        int64_t i = 0;
        while (i < c4m_xlist_len(parts)) {
            bool                 found;
            c4m_str_t           *key  = c4m_xlist_get(parts, i, NULL);
            c4m_zsection_spec_t *spec = hatrack_dict_get(vm->obj->spec->sec_specs,
                                                         key,
                                                         &found);
            if (!found) {
                // TODO error no spec for section
                break;
            }

            if (!i) {
                sofar = key;
            }
            else {
                sofar = c4m_str_concat(c4m_str_concat(sofar, dot), key);
            }
            ++i;

            for (int64_t n = 0; n < c4m_xlist_len(tree->kids); ++n) {
                c4m_attr_tree_t *item = c4m_xlist_get(tree->kids, n, NULL);
                if (c4m_eq(c4m_get_my_type(item->path), item->path, sofar)) {
                    tree = item;
                    break;
                }
            }

            if (!c4m_eq(c4m_get_my_type(tree->path), tree->path, sofar)) {
                // TODO error invalid start
                break;
            }

            if (1 == spec->max_allowed) {
                break;
            }
            if (c4m_xlist_len(parts) == i) {
                // TODO error no instance
                break;
            }

            sofar = c4m_str_concat(c4m_str_concat(sofar, dot),
                                   c4m_xlist_get(parts, i, NULL));
            ++i;

            for (int64_t n = 0; n < c4m_xlist_len(tree->kids); ++n) {
                c4m_attr_tree_t *item = c4m_xlist_get(tree->kids, n, NULL);
                if (c4m_eq(c4m_get_my_type(item->path), item->path, sofar)) {
                    tree = item;
                    break;
                }
            }

            if (!c4m_eq(c4m_get_my_type(tree->path), tree->path, sofar)) {
                // TODO error invalid start
                break;
            }
        }

        if (!c4m_xlist_len(errs)) {
            validate_section(tstate, tree, spec, errs);
        }
    }

    return errs;
}

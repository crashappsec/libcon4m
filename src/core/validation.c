// Validation of the con4m runtime state. By default, this will run
// any time there is a loaded confspec, and execution is in the
// process of halting, whether due to VM termination, or due to a
// callback running.
//
// This assumes that the attribute state will not change during
// the validation.

#include "con4m.h"

typedef enum : uint8_t {
    sk_singleton = 0,
    sk_blueprint = 1,
    sk_instance  = 2,
} section_kind;

typedef struct {
    c4m_dict_t         *contained_sections;
    c4m_dict_t         *contained_fields;
    c4m_spec_section_t *section_spec;
    section_kind        kind;
    bool                instantiation;
} section_vinfo;

typedef struct {
    c4m_attr_contents_t *record;
    c4m_spec_field_t    *field_spec;
} field_vinfo;

typedef struct {
    union {
        section_vinfo section;
        field_vinfo   field;
    } info;
    c4m_utf8_t *path;
    bool        field;
    bool        checked;
} spec_node_t;

typedef struct {
    c4m_dict_t  *attrs;
    c4m_dict_t  *section_cache;
    c4m_spec_t  *spec;
    c4m_list_t  *errors;
    spec_node_t *section_tree;
    c4m_vm_t    *vm;
    void        *cur;
    bool         at_object_name;
} validation_ctx;

static c4m_utf8_t *
current_path(validation_ctx *ctx)
{
    spec_node_t *s = ctx->cur;

    if (!s->path || !strlen(s->path->data)) {
        return c4m_new_utf8("the configuration root");
    }

    return c4m_cstr_format("section {}", s->path);
}

static c4m_utf8_t *
loc_from_decl(spec_node_t *n)
{
    if (n->field) {
        return n->info.field.field_spec->location_string;
    }
    else {
        return n->info.section.section_spec->location_string;
    }
}

static c4m_utf8_t *
loc_from_attr(validation_ctx *ctx, c4m_attr_contents_t *attr)
{
    c4m_zinstruction_t *ins = attr->lastset;
    c4m_module_t       *mi;

    if (!ins) {
        assert(attr->is_set);
        return c4m_new_utf8("Pre-execution");
    }

    mi = c4m_list_get(ctx->vm->obj->module_contents, ins->module_id, NULL);

    if (mi->full_uri) {
        return c4m_cstr_format("[b]{}:{:n}:[/]",
                               mi->full_uri,
                               c4m_box_i64(ins->line_no));
    }

    return c4m_cstr_format("[b]{}.{}:{:n}:[/]",
                           mi->package,
                           mi->name,
                           c4m_box_i64(ins->line_no));
}

static c4m_utf8_t *
best_field_loc(validation_ctx *ctx, spec_node_t *n)
{
    if (n->info.field.record->is_set) {
        return loc_from_attr(ctx, n->info.field.record);
    }
    return loc_from_decl(n);
}

static c4m_utf8_t *
spec_info_if_used(validation_ctx *ctx, spec_node_t *n)
{
    if (n->info.field.record->is_set) {
        return c4m_cstr_format("(field {} defined at: {})",
                               n->info.field.field_spec->name,
                               loc_from_decl(n));
    }

    return c4m_new_utf8("");
}

static void
_c4m_validation_error(validation_ctx     *ctx,
                      c4m_compile_error_t code,
                      c4m_utf8_t         *loc,
                      ...)
{
    va_list args;

    va_start(args, loc);

    c4m_base_runtime_error(ctx->errors,
                           code,
                           loc,
                           args);

    va_end(args);
}

#define c4m_validation_error(ctx, code, ...) \
    _c4m_validation_error(ctx, code, C4M_VA(__VA_ARGS__))

static spec_node_t *
spec_node_alloc(validation_ctx *ctx, c4m_utf8_t *path)
{
    spec_node_t   *res  = c4m_gc_alloc_mapped(spec_node_t, C4M_GC_SCAN_ALL);
    section_vinfo *info = &res->info.section;

    info->contained_sections = c4m_dict(c4m_type_utf8(), c4m_type_ref());
    info->contained_fields   = c4m_dict(c4m_type_utf8(), c4m_type_ref());
    res->path                = path;

    hatrack_dict_put(ctx->section_cache, path, res);

    return res;
}

static spec_node_t *
init_section_node(validation_ctx *ctx,
                  c4m_utf8_t     *path,
                  c4m_utf8_t     *section,
                  c4m_list_t     *subitems)
{
    // This sets up data structures from the section cache, but does not
    // populate any field information whatsoever.

    spec_node_t *sec_info;
    c4m_utf8_t  *full_path;
    bool         alloced = false;

    if (path == NULL) {
        full_path = section;
    }
    else {
        full_path = c4m_path_simple_join(path, section);
    }

    sec_info = hatrack_dict_get(ctx->section_cache, full_path, NULL);

    if (sec_info == NULL) {
        alloced                             = true;
        sec_info                            = spec_node_alloc(ctx, full_path);
        sec_info->info.section.section_spec = (c4m_spec_section_t *)ctx->cur;

        if (ctx->at_object_name) {
            sec_info->info.section.instantiation = true;
            sec_info->info.section.kind          = sk_instance;
        }
        else {
            if (sec_info->info.section.section_spec->singleton) {
                sec_info->info.section.kind = sk_singleton;
            }
            else {
                sec_info->info.section.kind = sk_blueprint;
            }
        }
    }

    if (!subitems) {
        return alloced ? sec_info : NULL;
    }

    int         n            = c4m_list_len(subitems);
    c4m_utf8_t *next_section = c4m_list_get(subitems, 0, NULL);
    c4m_list_t *next_items   = NULL;

    if (n > 1) {
        next_items = c4m_list_get_slice(subitems, 1, n);
    }

    // Set up the right section for the child.
    if (ctx->cur != NULL) {
        c4m_spec_section_t *cur_sec = (c4m_spec_section_t *)ctx->cur;

        if (cur_sec->singleton || ctx->at_object_name) {
            ctx->at_object_name = false;
            ctx->cur            = hatrack_dict_get(ctx->spec->section_specs,
                                        next_section,
                                        NULL);
        }
        else {
            ctx->at_object_name = true;
        }
    }

    spec_node_t *sub = init_section_node(ctx,
                                         full_path,
                                         next_section,
                                         next_items);

    if (sub != NULL) {
        hatrack_dict_add(sec_info->info.section.contained_sections,
                         next_section,
                         sub);
    }

    return alloced ? sec_info : NULL;
}

static inline void
init_one_section(validation_ctx *ctx, c4m_utf8_t *path)
{
    if (!path) {
        ctx->section_tree = init_section_node(ctx,
                                              NULL,
                                              c4m_new_utf8(""),
                                              NULL);
        return;
    }

    c4m_list_t *parts = c4m_u8_map(c4m_str_split(path, c4m_new_utf8(".")));
    c4m_utf8_t *next  = c4m_list_get(parts, 0, NULL);

    parts = c4m_list_get_slice(parts, 1, c4m_list_len(parts));

    if (!c4m_list_len(parts)) {
        parts = NULL;
    }

    ctx->cur = ctx->spec->root_section;

    init_section_node(ctx, NULL, next, parts);
}

// False if there's any sort of error. If the 'error' is no spec is installed,
// then the error list will be uninitialized.
static bool
spec_init_validation(validation_ctx *ctx, c4m_vm_t *runtime)
{
    if (!runtime->obj->using_attrs || !runtime->obj->attr_spec) {
        return false;
    }

    ctx->attrs         = runtime->attrs;
    ctx->spec          = runtime->obj->attr_spec;
    ctx->section_cache = c4m_dict(c4m_type_utf8(), c4m_type_ref());
    ctx->cur           = ctx->spec->root_section;
    ctx->errors        = c4m_list(c4m_type_ref());

    init_one_section(ctx, NULL);

    uint64_t     n;
    c4m_utf8_t **sections = hatrack_set_items_sort(runtime->all_sections, &n);

    for (unsigned int i = 0; i < n; i++) {
        init_one_section(ctx, sections[i]);
    }

    // At this point, the sections that have been used in the program
    // are loaded, but the fields used in the program are not, and
    // we haven't run any of our checks.

    return true;
}

static inline void
mark_required_field(c4m_flags_t *flags, c4m_list_t *req, c4m_utf8_t *name)
{
    for (int i = 0; i < c4m_list_len(req); i++) {
        c4m_spec_field_t *fspec = c4m_list_get(req, i, NULL);
        if (!strcmp(fspec->name->data, name->data)) {
            c4m_flags_set_index(flags, i, true);
            return;
        }
    }
}

static inline void
validate_field_contents(validation_ctx     *ctx,
                        spec_node_t        *node,
                        c4m_spec_section_t *secspec)
{
    uint64_t             num_fields;
    uint64_t             num_specs;
    uint64_t             num_req;
    c4m_dict_t          *fdict    = node->info.section.contained_fields;
    hatrack_dict_item_t *fields   = hatrack_dict_items(fdict, &num_fields);
    c4m_spec_field_t   **fspecs   = (void *)hatrack_dict_values(secspec->fields,
                                                            &num_specs);
    c4m_flags_t         *reqflags = NULL;
    c4m_list_t          *required = c4m_list(c4m_type_ref());

    // Scan the field specs to see how many are required.
    for (unsigned int i = 0; i < num_specs; i++) {
        if (fspecs[i]->required) {
            c4m_list_append(required, fspecs[i]);
        }
    }

    num_req = c4m_list_len(required);

    if (num_req != 0) {
        reqflags = c4m_new(c4m_type_flags(), c4m_kw("length", c4m_ka(num_req)));
    }

    for (unsigned int i = 0; i < num_fields; i++) {
        c4m_utf8_t          *name   = fields[i].key;
        spec_node_t         *fnode  = fields[i].value;
        c4m_spec_field_t    *fspec  = fnode->info.field.field_spec;
        c4m_attr_contents_t *record = fnode->info.field.record;
        c4m_type_t          *t;

        if (!record->is_set) {
            continue;
        }

        if (!fspec) {
            if (secspec->user_def_ok) {
                continue;
            }
            c4m_validation_error(ctx,
                                 c4m_spec_disallowed_field,
                                 loc_from_attr(ctx, record),
                                 current_path(ctx),
                                 name);
            continue;
        }

        if (fspec->required) {
            mark_required_field(reqflags, required, fspec->name);
        }

        if (fnode->checked) {
            continue;
        }

        uint64_t     num_ex;
        c4m_utf8_t **exclusions = hatrack_set_items(fspec->exclusions, &num_ex);

        for (unsigned int i = 0; i < num_ex; i++) {
            spec_node_t *n = hatrack_dict_get(fdict, exclusions[i], NULL);
            if (n != NULL) {
                c4m_validation_error(ctx,
                                     c4m_spec_mutex_field,
                                     loc_from_attr(ctx, n->info.field.record),
                                     current_path(ctx),
                                     exclusions[i],
                                     node->info.field.field_spec->name,
                                     loc_from_attr(ctx,
                                                   node->info.field.record));
            }

            c4m_spec_field_t *x = hatrack_dict_get(secspec->fields,
                                                   exclusions[i],
                                                   NULL);
            if (x && x->required) {
                mark_required_field(reqflags, required, x->name);
            }
        }

        if (fspec->deferred_type_field) {
            spec_node_t *bud = hatrack_dict_get(fdict,
                                                fspec->deferred_type_field,
                                                NULL);
            if (!bud || !bud->info.field.record->is_set) {
                c4m_validation_error(ctx,
                                     c4m_spec_missing_ptr,
                                     best_field_loc(ctx, node),
                                     current_path(ctx),
                                     node->info.field.field_spec->name,
                                     fspec->deferred_type_field,
                                     spec_info_if_used(ctx, node));
            }
            else {
                c4m_attr_contents_t *bud_rec = bud->info.field.record;

                t = c4m_merge_types(bud_rec->type, c4m_type_typespec(), NULL);
                if (c4m_type_is_error(t)) {
                    c4m_validation_error(ctx,
                                         c4m_spec_invalid_type_ptr,
                                         best_field_loc(ctx, node),
                                         current_path(ctx),
                                         node->info.field.field_spec->name,
                                         fspec->deferred_type_field,
                                         bud_rec->type,
                                         loc_from_attr(ctx, bud_rec));
                }
                else {
                    t = c4m_type_copy((c4m_type_t *)bud_rec->contents);
                    t = c4m_merge_types(t, record->type, NULL);
                    if (c4m_type_is_error(t)) {
                        c4m_validation_error(ctx,
                                             c4m_spec_ptr_typecheck,
                                             best_field_loc(ctx, node),
                                             current_path(ctx),
                                             node->info.field.field_spec->name,
                                             fspec->deferred_type_field,
                                             record->type,
                                             bud_rec->contents,
                                             loc_from_attr(ctx, bud_rec));
                    }
                }
            }
        }
        else {
            t = c4m_merge_types(c4m_type_copy(fspec->tinfo.type),
                                record->type,
                                NULL);
            if (c4m_type_is_error(t)) {
                c4m_validation_error(ctx,
                                     c4m_spec_field_typecheck,
                                     best_field_loc(ctx, node),
                                     current_path(ctx),
                                     node->info.field.field_spec->name,
                                     fspec->tinfo.type,
                                     record->type,
                                     spec_info_if_used(ctx, node));
            }
        }

        if (fspec->validate_range) {
            // TODO: extract the range... it's currently overwriting the
            // validator field and maybe isn't even checked.
            //
            // Remember this could be int, unsigned or float.
            // Get the boxing right.
            // c4m_spec_out_of_range
        }

        if (fspec->validate_choice) {
            // TODO: same stuff here.
            // Get the boxing and equality testing right.
            // c4m_spec_bad_choice
        }

        if (fspec->validator) {
            // TODO: call this callback, dawg.
            // But it's got to be there; don't be square.
            // c4m_spec_invalid_value,
        }
    }

    // Make sure we saw all required sections.
    for (unsigned int i = 0; i < c4m_list_len(required); i++) {
        if (!c4m_flags_index(reqflags, i)) {
            c4m_spec_field_t *missing = c4m_list_get(required, i, NULL);

            c4m_validation_error(ctx,
                                 c4m_spec_missing_field,
                                 loc_from_decl(ctx->cur),
                                 current_path(ctx),
                                 missing->name);
        }
    }
}

static void spec_validate_section(validation_ctx *);

static void
validate_subsection_names(validation_ctx *ctx, spec_node_t *node)
{
    // This does NOT get called for blueprint attributes.
    // It only gets called on instances and singletons.

    uint64_t             num_subs;
    c4m_dict_t          *secdict  = node->info.section.contained_sections;
    hatrack_dict_item_t *subsecs  = hatrack_dict_items(secdict, &num_subs);
    c4m_spec_section_t  *secspec  = node->info.section.section_spec;
    c4m_flags_t         *reqflags = NULL;
    uint64_t             num_req;
    c4m_utf8_t         **reqnames;

    reqnames = hatrack_set_items(secspec->required_sections, &num_req);

    if (num_req != 0) {
        reqflags = c4m_new(c4m_type_flags(), c4m_kw("length", c4m_ka(num_req)));
    }

    // Make sure this spec is allowed at all; we need to see it in the
    // 'required' list or the 'allow' list.

    for (unsigned int i = 0; i < num_subs; i++) {
        c4m_utf8_t  *name = subsecs[i].key;
        spec_node_t *sub  = subsecs[i].value;
        bool         ok   = false;

        // If this section name is required, mark that we saw it.
        if (num_req && hatrack_set_contains(secspec->required_sections, name)) {
            for (unsigned int j = 0; j < num_req; j++) {
                if (!strcmp(reqnames[j]->data, name->data)) {
                    c4m_flags_set_index(reqflags, j, true);
                    ok = true;
                    break;
                }
            }
        }
        if (!ok && !hatrack_set_contains(secspec->allowed_sections, name)) {
            // Set up the right node to error
            ctx->cur     = sub;
            // Keeps us from recursing into broken subsections.
            sub->checked = true;

            if (hatrack_dict_get(ctx->spec->section_specs, name, NULL)) {
                c4m_validation_error(ctx,
                                     c4m_spec_disallowed_section,
                                     loc_from_decl(ctx->cur),
                                     current_path(ctx),
                                     name);
            }
            else {
                c4m_validation_error(ctx,
                                     c4m_spec_unknown_section,
                                     loc_from_decl(ctx->cur),
                                     current_path(ctx),
                                     name);
            }
        }

        ctx->cur = node;
    }

    // Make sure we saw all required sections.
    for (unsigned int i = 0; i < num_req; i++) {
        if (!c4m_flags_index(reqflags, i)) {
            c4m_validation_error(ctx,
                                 c4m_spec_missing_require,
                                 loc_from_decl(ctx->cur),
                                 reqnames[i]);
        }
    }

    validate_field_contents(ctx, node, secspec);

    // Descend to any non-broken sections to check them.
    for (unsigned int i = 0; i < num_subs; i++) {
        spec_node_t *sub = subsecs[i].value;
        if (sub->checked) {
            continue;
        }
        ctx->cur = sub;
        spec_validate_section(ctx);
    }

    // TODO: Add back in a section validation callback here.

    ctx->cur = node;
}

static void
spec_validate_section(validation_ctx *ctx)
{
    spec_node_t   *node = (spec_node_t *)ctx->cur;
    section_vinfo *info = &node->info.section;

    if (info->kind == sk_blueprint) {
        uint64_t             num_fields;
        c4m_dict_t          *fdict  = info->contained_fields;
        hatrack_dict_item_t *fields = hatrack_dict_items(fdict, &num_fields);

        if (num_fields) {
            spec_node_t *fnode = fields[0].value;

            c4m_validation_error(ctx,
                                 c4m_spec_blueprint_fields,
                                 node->path,
                                 info->section_spec->name,
                                 fields[0].key,
                                 loc_from_attr(ctx, fnode->info.field.record));
        }
    }
    else {
        validate_subsection_names(ctx, node);
        validate_field_contents(ctx, node, info->section_spec);
    }
}

c4m_list_t *
c4m_validate_runtime(c4m_vm_t *runtime)
{
    validation_ctx ctx;

    spec_init_validation(&ctx, runtime);
    ctx.cur = ctx.section_tree;
    spec_validate_section(&ctx);

    return ctx.errors;
}
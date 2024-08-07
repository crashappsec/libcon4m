#define C4M_USE_INTERNAL_API
#include "con4m.h"

static void
cctx_gc_bits(uint64_t *bitfield, c4m_compile_ctx *ctx)
{
    c4m_mark_raw_to_addr(bitfield, ctx, &ctx->str_map);
}

c4m_compile_ctx *
c4m_new_compile_ctx()
{
    return c4m_gc_alloc_mapped(c4m_compile_ctx, cctx_gc_bits);
}

static hatrack_hash_t
module_ctx_hash(c4m_module_compile_ctx *ctx)
{
    return ctx->module_id;
}

c4m_compile_ctx *
c4m_new_compile_context(c4m_str_t *input)
{
    c4m_compile_ctx *result = c4m_new_compile_ctx();

    result->module_cache  = c4m_dict(c4m_type_u64(), c4m_type_ref());
    result->final_attrs   = c4m_new_scope(NULL, C4M_SCOPE_GLOBAL);
    result->final_globals = c4m_new_scope(NULL, C4M_SCOPE_ATTRIBUTES);
    result->final_spec    = c4m_new_spec();
    result->backlog       = c4m_new(c4m_type_set(c4m_type_ref()),
                              c4m_kw("hash", c4m_ka(module_ctx_hash)));
    result->processed     = c4m_new(c4m_type_set(c4m_type_ref()),
                                c4m_kw("hash", c4m_ka(module_ctx_hash)));
    result->const_data    = c4m_buffer_empty();
    result->const_memos   = c4m_alloc_marshal_memos();
    result->const_memoid  = 1;
    result->instance_map  = c4m_dict(c4m_type_ref(), c4m_type_i64());
    result->str_map       = c4m_dict(c4m_type_utf8(), c4m_type_i64());
    result->const_stream  = c4m_buffer_outstream(result->const_data, true);

    if (input != NULL) {
        result->entry_point = c4m_init_module_from_loc(result, input);
        c4m_add_module_to_worklist(result, result->entry_point);
    }

    return result;
}

static c4m_utf8_t *str_to_type_tmp_path = NULL;

c4m_type_t *
c4m_str_to_type(c4m_utf8_t *str)
{
    if (str_to_type_tmp_path == NULL) {
        str_to_type_tmp_path = c4m_new_utf8("<< string evaluation >>");
        c4m_gc_register_root(&str_to_type_tmp_path, 1);
    }

    c4m_type_t            *result = NULL;
    c4m_stream_t          *stream = c4m_string_instream(str);
    c4m_module_compile_ctx ctx    = {
           .module_id = 0xffffffff,
           .path      = str_to_type_tmp_path,
           .module    = str_to_type_tmp_path,
    };

    if (c4m_lex(&ctx, stream) != false) {
        c4m_parse_type(&ctx);
    }

    c4m_stream_close(stream);

    if (ctx.parse_tree != NULL) {
        c4m_dict_t *type_ctx = c4m_new(c4m_type_dict(c4m_type_utf8(),
                                                     c4m_type_ref()));

        result = c4m_node_to_type(&ctx, ctx.parse_tree, type_ctx);
    }

    if (ctx.parse_tree == NULL || c4m_fatal_error_in_module(&ctx)) {
        C4M_CRAISE("Invalid type.");
    }

    return result;
}

static void
merge_function_decls(c4m_compile_ctx *cctx, c4m_module_compile_ctx *fctx)
{
    c4m_scope_t          *scope = fctx->module_scope;
    hatrack_dict_value_t *items;
    uint64_t              n;

    items = hatrack_dict_values(scope->symbols, &n);

    for (uint64_t i = 0; i < n; i++) {
        c4m_symbol_t *new = items[i];

        if (new->kind != C4M_SK_FUNC &&new->kind != C4M_SK_EXTERN_FUNC) {
            continue;
        }
        c4m_fn_decl_t *decl = new->value;
        if (decl->private) {
            continue;
        }

        if (!hatrack_dict_add(cctx->final_globals->symbols, new->name, new)) {
            c4m_symbol_t *old = hatrack_dict_get(cctx->final_globals->symbols,
                                                 new->name,
                                                 NULL);

            if (old != new) {
                c4m_add_warning(fctx,
                                c4m_warn_cant_export,
                                new->declaration_node);
            }
        }
    }
}

// This loads all modules up through symbol declaration.
static void
c4m_perform_module_loads(c4m_compile_ctx *ctx)
{
    c4m_module_compile_ctx *cur;

    while (true) {
        cur = c4m_set_any_item(ctx->backlog, NULL);
        if (cur == NULL) {
            return;
        }

        if (cur->status < c4m_compile_status_code_loaded) {
            c4m_parse(cur);
            c4m_module_decl_pass(ctx, cur);
            if (c4m_fatal_error_in_module(cur)) {
                ctx->fatality = true;
                return;
            }
        }

        if (cur->status < c4m_compile_status_scopes_merged) {
            merge_function_decls(ctx, cur);
            c4m_module_set_status(cur, c4m_compile_status_scopes_merged);
        }

        c4m_set_put(ctx->processed, cur);
        c4m_set_remove(ctx->backlog, cur);
    }
}

typedef struct topologic_search_ctx {
    c4m_module_compile_ctx *cur;
    c4m_list_t             *visiting;
    c4m_compile_ctx        *cctx;
} tsearch_ctx;

static void
topological_order_process(tsearch_ctx *ctx)
{
    c4m_module_compile_ctx *cur = ctx->cur;

    if (c4m_list_contains(ctx->visiting, cur)) {
        // Cycle. I intend to add an info message here, otherwise
        // could avoid popping and just get this down to one test.
        return;
    }

    // If it already got added to the partial ordering, we don't need to
    // process it when it gets re-imported somewhere else.
    if (c4m_list_contains(ctx->cctx->module_ordering, cur)) {
        return;
    }

    if (cur->imports == NULL || cur->imports->symbols == NULL) {
        c4m_list_append(ctx->cctx->module_ordering, cur);
        return;
    }

    c4m_list_append(ctx->visiting, cur);

    uint64_t              num_imports;
    hatrack_dict_value_t *imports = hatrack_dict_values(cur->imports->symbols,
                                                        &num_imports);

    for (uint64_t i = 0; i < num_imports; i++) {
        c4m_symbol_t           *sym  = imports[i];
        c4m_tree_node_t        *n    = sym->declaration_node;
        c4m_pnode_t            *pn   = c4m_tree_get_contents(n);
        c4m_module_compile_ctx *next = (c4m_module_compile_ctx *)pn->value;

        if (next != cur) {
            ctx->cur = next;
            topological_order_process(ctx);
        }
        else {
            ctx->cctx->fatality = true;
            c4m_add_error(cur, c4m_err_self_recursive_use, n);
        }
    }

    cur = c4m_list_pop(ctx->visiting);
    c4m_list_append(ctx->cctx->module_ordering, cur);
}

static void
build_topological_ordering(c4m_compile_ctx *cctx)
{
    // While we don't strictly need this partial ordering, once we get
    // through phase 1 where we've pulled out symbols per-module, we
    // will process merging those symbols using a partial ordering, so
    // that, whenever possiblle, conflicts are raised when processing
    // the dependent code.
    //
    // That may not always happen with cycles, of course. We break
    // those cycles via keeping a "visiting" stack in a depth-first
    // search.

    tsearch_ctx search_state = {
        .cur      = cctx->sys_package,
        .visiting = c4m_new(c4m_type_list(c4m_type_ref())),
        .cctx     = cctx,
    };

    cctx->module_ordering = c4m_new(c4m_type_list(c4m_type_ref()));

    topological_order_process(&search_state);

    if (cctx->entry_point) {
        search_state.cur = cctx->entry_point;
        topological_order_process(&search_state);
    }
}

static void
merge_one_plain_scope(c4m_compile_ctx        *cctx,
                      c4m_module_compile_ctx *fctx,
                      c4m_scope_t            *local,
                      c4m_scope_t            *global)

{
    uint64_t              num_symbols;
    hatrack_dict_value_t *items;
    c4m_symbol_t         *new_sym;
    c4m_symbol_t         *old_sym;

    items = hatrack_dict_values(local->symbols, &num_symbols);

    for (uint64_t i = 0; i < num_symbols; i++) {
        new_sym = items[i];

        if (hatrack_dict_add(global->symbols,
                             new_sym->name,
                             new_sym)) {
            new_sym->local_module_id = fctx->local_module_id;
            continue;
        }

        old_sym = hatrack_dict_get(global->symbols,
                                   new_sym->name,
                                   NULL);
        if (c4m_merge_symbols(fctx, new_sym, old_sym)) {
            hatrack_dict_put(global->symbols, new_sym->name, old_sym);
            new_sym->linked_symbol   = old_sym;
            new_sym->local_module_id = old_sym->local_module_id;
        }
        // Else, there's some error and it doesn't matter, things won't run.
    }
}

static void
merge_var_scope(c4m_compile_ctx *cctx, c4m_module_compile_ctx *fctx)
{
    merge_one_plain_scope(cctx, fctx, fctx->global_scope, cctx->final_globals);
}

static void
merge_attrs(c4m_compile_ctx *cctx, c4m_module_compile_ctx *fctx)
{
    merge_one_plain_scope(cctx, fctx, fctx->attribute_scope, cctx->final_attrs);
}

static void
merge_one_confspec(c4m_compile_ctx *cctx, c4m_module_compile_ctx *fctx)
{
    if (fctx->local_confspecs == NULL) {
        return;
    }

    if (cctx->final_spec == NULL) {
        cctx->final_spec = c4m_new_spec();
    }

    uint64_t              num_sections;
    c4m_dict_t           *fspecs = cctx->final_spec->section_specs;
    hatrack_dict_value_t *sections;

    sections = hatrack_dict_values(fctx->local_confspecs->section_specs,
                                   &num_sections);

    if (num_sections || fctx->local_confspecs->root_section) {
        if (cctx->final_spec->locked) {
            c4m_add_error(fctx,
                          c4m_err_spec_locked,
                          fctx->local_confspecs->declaration_node);
        }
    }

    for (uint64_t i = 0; i < num_sections; i++) {
        c4m_spec_section_t *cur = sections[i];

        if (hatrack_dict_add(fspecs, cur->name, cur)) {
            continue;
        }

        c4m_spec_section_t *old = hatrack_dict_get(fspecs, cur->name, NULL);

        c4m_add_error(fctx,
                      c4m_err_spec_redef_section,
                      cur->declaration_node,
                      cur->name,
                      c4m_node_get_loc_str(old->declaration_node));
    }

    c4m_spec_section_t *root_adds = fctx->local_confspecs->root_section;
    c4m_spec_section_t *true_root = cctx->final_spec->root_section;
    uint64_t            num_fields;

    if (root_adds == NULL) {
        return;
    }

    hatrack_dict_value_t *fields = hatrack_dict_values(root_adds->fields,
                                                       &num_fields);
    for (uint64_t i = 0; i < num_fields; i++) {
        c4m_spec_field_t *cur = fields[i];

        if (hatrack_dict_add(true_root->fields, cur->name, cur)) {
            continue;
        }

        c4m_spec_field_t *old = hatrack_dict_get(root_adds->fields,
                                                 cur->name,
                                                 NULL);

        c4m_add_error(fctx,
                      c4m_err_spec_redef_field,
                      cur->declaration_node,
                      cur->name,
                      c4m_node_get_loc_str(old->declaration_node));
    }

    if (root_adds->allowed_sections != NULL) {
        if (true_root->allowed_sections == NULL) {
            true_root->allowed_sections = c4m_new(
                c4m_type_set(c4m_type_ref()));
        }

        uint64_t num_allows;
        void   **allows = c4m_set_items(root_adds->allowed_sections,
                                      &num_allows);

        for (uint64_t i = 0; i < num_allows; i++) {
            if (!c4m_set_add(true_root->allowed_sections, allows[i])) {
                c4m_add_warning(fctx,
                                c4m_warn_dupe_allow,
                                root_adds->declaration_node);
            }
        }
    }

    if (root_adds->required_sections != NULL) {
        if (true_root->required_sections == NULL) {
            true_root->required_sections = c4m_new(
                c4m_type_set(c4m_type_ref()));
        }

        uint64_t num_reqs;
        void   **reqs = c4m_set_items(root_adds->required_sections,
                                    &num_reqs);

        for (uint64_t i = 0; i < num_reqs; i++) {
            if (!c4m_set_add(true_root->required_sections, reqs[i])) {
                c4m_add_warning(fctx,
                                c4m_warn_dupe_require,
                                root_adds->declaration_node);
            }
        }
    }

    if (root_adds->validator == NULL) {
        return;
    }

    if (true_root->validator != NULL) {
        c4m_add_error(fctx,
                      c4m_err_dupe_validator,
                      root_adds->declaration_node);
    }
    else {
        true_root->validator = root_adds->validator;
    }
}

static void
merge_global_info(c4m_compile_ctx *cctx)
{
    c4m_module_compile_ctx *fctx = NULL;

    build_topological_ordering(cctx);

    uint64_t mod_len = c4m_list_len(cctx->module_ordering);

    for (uint64_t i = 0; i < mod_len; i++) {
        fctx = c4m_list_get(cctx->module_ordering, i, NULL);

        assert(fctx->local_module_id == 0 || fctx->local_module_id == i);

        fctx->local_module_id = i;

        merge_var_scope(cctx, fctx);
        merge_one_confspec(cctx, fctx);
        merge_attrs(cctx, fctx);
    }
}

c4m_list_t *
c4m_system_module_files()
{
    return c4m_path_walk(c4m_system_module_path());
}

c4m_compile_ctx *
c4m_compile_from_entry_point(c4m_str_t *entry)
{
    c4m_compile_ctx *result = c4m_new_compile_context(NULL);

    result->sys_package = c4m_find_module(result,
                                          c4m_con4m_root(),
                                          c4m_new_utf8(C4M_PACKAGE_INIT_MODULE),
                                          c4m_new_utf8("sys"),
                                          NULL,
                                          NULL,
                                          NULL);

    if (result->sys_package != NULL) {
        c4m_add_module_to_worklist(result, result->sys_package);
    }

    if (entry != NULL) {
        result->entry_point = c4m_init_module_from_loc(result, entry);
        c4m_add_module_to_worklist(result, result->entry_point);
        if (result->fatality) {
            return result;
        }
    }

    c4m_perform_module_loads(result);
    if (result->fatality) {
        return result;
    }
    merge_global_info(result);
    if (result->fatality) {
        return result;
    }
    c4m_check_pass(result);

    if (result->fatality) {
        return result;
    }

    return result;
}

c4m_vm_t *
c4m_generate_code(c4m_compile_ctx *ctx)
{
    c4m_vm_t *result = c4m_new_vm(ctx);

    c4m_vm_reset(result);
    c4m_internal_codegen(ctx, result);
    c4m_vm_setup_runtime(result);

    if (ctx->fatality) {
        return NULL;
    }

    c4m_vm_reset(result);
    return result;
}

#define C4M_INTERNAL_API
#include "con4m.h"

static c4m_xlist_t *con4m_path       = NULL;
static c4m_set_t   *con4m_extensions = NULL;

static void
init_con4m_path()
{
    c4m_xlist_t *parts;

    c4m_gc_register_root(&con4m_path, 1);
    c4m_gc_register_root(&con4m_extensions, 1);

    con4m_extensions = c4m_new(c4m_tspec_set(c4m_tspec_utf8()));

    c4m_set_add(con4m_extensions, c4m_new_utf8("c4m"));

    c4m_utf8_t *extra = c4m_get_env(c4m_new_utf8("CON4M_EXTENSIONS"));

    if (extra != NULL) {
        parts = c4m_str_xsplit(extra, c4m_new_utf8(":"));
        for (int i = 0; i < c4m_xlist_len(parts); i++) {
            c4m_set_add(con4m_extensions,
                        c4m_to_utf8(c4m_xlist_get(parts, i, NULL)));
        }
    }

    c4m_set_package_search_path(c4m_resolve_path(c4m_new_utf8(".")));

    extra = c4m_get_env(c4m_new_utf8("CON4M_PATH"));

    if (extra == NULL) {
        return;
    }

    parts = c4m_str_xsplit(extra, c4m_new_utf8(":"));

    c4m_xlist_t *new_path = c4m_new(c4m_tspec_xlist(c4m_tspec_utf8()));

    for (int i = 0; i < c4m_xlist_len(parts); i++) {
        c4m_utf8_t *s = c4m_to_utf8(c4m_xlist_get(parts, i, NULL));

        c4m_xlist_append(new_path, c4m_resolve_path(s));
    }

    // Always keep cwd in the path, but put it last.
    c4m_xlist_append(new_path, c4m_resolve_path(c4m_new_utf8(".")));

    con4m_path = new_path;
}

void
_c4m_set_package_search_path(c4m_utf8_t *dir, ...)
{
    con4m_path = c4m_new(c4m_tspec_xlist(c4m_tspec_utf8()));

    va_list args;

    va_start(args, dir);

    while (dir != NULL) {
        c4m_xlist_append(con4m_path, dir);
        dir = va_arg(args, c4m_utf8_t *);
    }
}

static c4m_utf8_t *
perform_path_search(c4m_utf8_t *package, c4m_utf8_t *module)
{
    if (con4m_path == NULL) {
        init_con4m_path();
    }

    uint64_t     n_items;
    c4m_utf8_t  *munged     = NULL;
    c4m_utf8_t **extensions = c4m_set_items_sort(con4m_extensions, &n_items);

    if (package != NULL && c4m_str_codepoint_len(package) != 0) {
        c4m_xlist_t *parts = c4m_str_xsplit(package, c4m_new_utf8("."));

        c4m_xlist_append(parts, module);
        munged = c4m_to_utf8(c4m_str_join(parts, c4m_new_utf8("/")));
    }

    int l = (int)c4m_xlist_len(con4m_path);

    if (munged != NULL) {
        for (int i = 0; i < l; i++) {
            c4m_str_t *dir = c4m_xlist_get(con4m_path, i, NULL);

            for (int j = 0; j < (int)n_items; j++) {
                c4m_utf8_t *ext = extensions[j];
                c4m_str_t  *s   = c4m_cstr_format("{}/{}.{}", dir, munged, ext);
                s               = c4m_to_utf8(s);

                struct stat info;

                if (!stat(s->data, &info)) {
                    return s;
                }
            }
        }
    }
    else {
        for (int i = 0; i < l; i++) {
            c4m_str_t *dir = c4m_xlist_get(con4m_path, i, NULL);
            for (int j = 0; j < (int)n_items; j++) {
                c4m_utf8_t *ext = extensions[j];
                c4m_str_t  *s   = c4m_cstr_format("{}/{}.{}", dir, module, ext);
                s               = c4m_to_utf8(s);

                struct stat info;

                if (!stat(s->data, &info)) {
                    return s;
                }
            }
        }
    }

    return NULL;
}

static inline uint64_t
module_key(c4m_utf8_t *package, c4m_utf8_t *module)
{
    c4m_sha_t sha;

    c4m_sha_init(&sha, NULL);
    c4m_sha_string_update(&sha, package);
    c4m_sha_int_update(&sha, '.');
    c4m_sha_string_update(&sha, module);

    c4m_buf_t *digest = c4m_sha_finish(&sha);

    return ((uint64_t *)digest->data)[0];
}

#define C4F_FILE         1
#define C4F_SECURE       2
#define C4F_ADD_TO_QUEUE 4

static c4m_file_compile_ctx *
get_file_compile_ctx(c4m_compile_ctx *ctx,
                     c4m_str_t       *path,
                     c4m_str_t       *module,
                     c4m_str_t       *package,
                     int              flags,
                     c4m_str_t       *authority)

{
    uint64_t              key;
    c4m_file_compile_ctx *result;

    if (con4m_extensions == NULL) {
        init_con4m_path();
    }

    module = c4m_to_utf8(module);

    if (path) {
        path = c4m_to_utf8(path);
    }

    if (package != NULL) {
        package = c4m_to_utf8(package);
        key     = module_key(package, module);
    }
    else {
        key = module_key(c4m_new_utf8("__default__"), module);
    }

    result = hatrack_dict_get(ctx->module_cache, (void *)key, NULL);

    if (result) {
        return result;
    }

    result = c4m_gc_alloc(c4m_file_compile_ctx);

    if (!path) {
        path = perform_path_search(package, module);

        if (!path) {
            if (package != NULL) {
                path = c4m_cstr_format("{}.{}", package, module);
            }
            else {
                path = module;
            }

            c4m_file_load_error(result, c4m_err_search_path);
        }
    }
    else {
        struct stat  info;
        c4m_utf8_t  *tmp    = c4m_resolve_path(path);
        c4m_xlist_t *pieces = c4m_str_xsplit(tmp, c4m_new_utf8("."));
        c4m_utf8_t  *last   = c4m_xlist_get(pieces,
                                         c4m_xlist_len(pieces) - 1,
                                         NULL);
        c4m_utf8_t  *tmp2;
        uint64_t     n_items;

        last = c4m_to_utf8(last);

        if (!stat(tmp->data, &info)) {
            c4m_utf8_t **extensions = c4m_set_items_sort(con4m_extensions,
                                                         &n_items);

            for (uint64_t i = 0; i < n_items; i++) {
                if (c4m_str_eq(last, extensions[i])) {
                    break;
                }
                tmp2 = c4m_cstr_format("{}.{}", tmp, extensions[i]);

                if (stat(tmp2->data, &info)) {
                    path = tmp2;
                    break;
                }
            }
        }
        else {
            path = tmp;
        }
    }

    // Do this after the lookup; we don't want to actually add __default__
    if (package == NULL) {
        package = c4m_new_utf8("__default__");
    }

    result->package   = package;
    result->errors    = c4m_new(c4m_tspec_xlist(c4m_tspec_ref()));
    result->module_id = key;
    result->module    = module;

    if (flags & C4F_FILE) {
        result->file = 1;
    }
    else {
        if (flags & C4F_SECURE) {
            result->secure = 1;
        }
        else {
            c4m_file_load_warn(result, c4m_warn_no_tls);
        }

        c4m_file_load_error(result, c4m_err_no_http);
    }

    result->path = path;

    c4m_validate_module_info(result);
    hatrack_dict_put(ctx->module_cache, (void *)key, result);

    if (flags & C4F_ADD_TO_QUEUE) {
        c4m_set_put(ctx->backlog, result);
    }

    return result;
}

bool
c4m_validate_module_info(c4m_file_compile_ctx *ctx)
{
    c4m_codepoint_t cp;

    int  plen   = c4m_str_codepoint_len(ctx->package);
    int  mlen   = c4m_str_codepoint_len(ctx->module);
    bool dot_ok = true; // We start at char 1.

    if (plen == 0 || mlen == 0) {
        return false;
    }

    cp = c4m_index(ctx->package, 0);
    if (!c4m_codepoint_is_c4m_id_start(cp)) {
        return false;
    }

    cp = c4m_index(ctx->module, 0);
    if (!c4m_codepoint_is_c4m_id_start(cp)) {
        return false;
    }

    for (int i = 1; i < plen; i++) {
        cp = c4m_index(ctx->package, i);

        if (c4m_codepoint_is_c4m_id_continue(cp)) {
            dot_ok = true;
            continue;
        }

        if (cp != '.' || !dot_ok) {
            return false;
        }

        // dot_ok being true is really only keeping track of whether
        // the previous character was a dot; however, the final
        // character of the package name cannot be a dot.
        if (i + 1 == plen) {
            return false;
        }

        dot_ok = false;
    }

    for (int i = 1; i < mlen; i++) {
        cp = c4m_index(ctx->module, i);

        if (!c4m_codepoint_is_c4m_id_continue(cp)) {
            return false;
        }
    }

    return true;
}

static inline c4m_file_compile_ctx *
ctx_init_from_web_uri(c4m_compile_ctx *ctx, c4m_utf8_t *path)
{
    c4m_file_compile_ctx *result;
    c4m_utf8_t           *module;
    c4m_utf8_t           *package;
    int                   flags = C4F_ADD_TO_QUEUE;

    if (c4m_str_codepoint_len(path) <= 8) {
        goto malformed;
    }

    char *s = &path->data[4];
    if (*s == 's') {
        flags |= C4F_SECURE;
        s++;
    }
    if (*s++ != ':') {
        goto malformed;
    }
    if (*s++ != '/') {
        goto malformed;
    }

    c4m_xlist_t *parts = c4m_str_xsplit(path, c4m_get_slash_const());
    c4m_utf8_t  *site  = c4m_to_utf8(c4m_xlist_get(parts, 2, NULL));
    int64_t      n     = c4m_xlist_len(parts);

    if (n < 4) {
        goto malformed;
    }

    c4m_utf8_t *last = c4m_to_utf8(c4m_xlist_get(parts, n - 1, NULL));

    if (c4m_str_codepoint_len(last) == 0) {
        if (n < 5) {
            goto malformed;
        }
        last = c4m_to_utf8(c4m_xlist_get(parts, n - 2, NULL));
    }

    c4m_utf8_t *dot = c4m_new_utf8(".");

    int ix = c4m_str_find(last, dot);

    if (ix == -1) {
        return get_file_compile_ctx(ctx, path, last, NULL, flags, site);
    }

    parts = c4m_str_xsplit(last, dot);

    n = c4m_xlist_len(parts);

    if (n == 2) {
        if (c4m_set_contains(con4m_extensions,
                             c4m_to_utf8(c4m_xlist_get(parts, 1, NULL)))) {
            module = c4m_to_utf8(c4m_xlist_get(parts, 0, NULL));
            return get_file_compile_ctx(ctx, path, module, NULL, flags, site);
        }
    }

    package = c4m_to_utf8(c4m_xlist_get(parts, 0, NULL));
    n       = n - 1;

    module = c4m_to_utf8(c4m_xlist_get(parts, n, NULL));

    for (int i = 1; i < n; i++) {
        package = c4m_cstr_format("{}.{}",
                                  package,
                                  c4m_to_utf8(c4m_xlist_get(parts, i, NULL)));
    }

    return get_file_compile_ctx(ctx, path, module, package, flags, site);

malformed:

    result = get_file_compile_ctx(ctx, path, NULL, NULL, flags, NULL);
    c4m_file_load_error(result, c4m_err_malformed_url);
    return result;
}

static inline c4m_file_compile_ctx *
ctx_init_from_file_uri(c4m_compile_ctx *ctx, c4m_utf8_t *path, int ix)
{
    int          prefix_len = 0;
    c4m_utf8_t  *package    = NULL;
    c4m_utf8_t  *module     = NULL;
    int          flags      = C4F_FILE | C4F_ADD_TO_QUEUE;
    c4m_xlist_t *path_parts;
    int          item_len;

    item_len = c4m_xlist_len(con4m_path);

    for (int i = 0; i < item_len; i++) {
        c4m_utf8_t *one = c4m_to_utf8(c4m_xlist_get(con4m_path, i, NULL));
        if (c4m_str_starts_with(path, one)) {
            prefix_len = c4m_str_codepoint_len(one);
            break;
        }
    }

    if (prefix_len) {
        c4m_utf8_t *suffix = c4m_str_slice(path, prefix_len, -1);
        // The package and module are after the path prefix. The package
        // should be slash-separated. We also expect to chop off a file
        // extension, but here we won't care what it is; we just look to
        // see if the last piece has a dot in it.
        path_parts         = c4m_str_xsplit(suffix, c4m_new_utf8("/"));
        item_len           = c4m_xlist_len(path_parts);

        c4m_xlist_t *module_parts = c4m_str_xsplit(c4m_xlist_get(path_parts,
                                                                 item_len - 1,
                                                                 NULL),
                                                   c4m_new_utf8("."));

        module = c4m_to_utf8(c4m_xlist_get(module_parts, 0, NULL));

fill_in_package:
        if (--item_len) {
            package = c4m_to_utf8(c4m_xlist_get(path_parts, 0, NULL));
            for (int i = 1; i < item_len; i++) {
                package = c4m_cstr_format("{}.{}",
                                          package,
                                          c4m_xlist_get(path_parts, i, NULL));
            }
        }
    }
    else {
        // The prefix is NOT found in our path, so we then assume the
        // module is a one-off somewhere outside the path. Here, to
        // avoid ambiguity about where the package starts, we only
        // look at the last path item to extract the module and
        // package, and packages must be dot separated.
        //
        // Here, we also assume the file extension is provided, unless
        // there is definitely no extension and no package. That means
        // the last dotted piece is dropped, and everything else to the
        // right of the last slash is the path / module.

        path_parts       = c4m_str_xsplit(path, c4m_new_utf8("/"));
        c4m_utf8_t *last = c4m_xlist_get(path_parts,
                                         c4m_xlist_len(path_parts) - 1,
                                         NULL);
        path_parts       = c4m_str_xsplit(last, c4m_new_utf8("."));
        item_len         = c4m_xlist_len(path_parts);

        if (item_len == 1) {
            module = c4m_to_utf8(last);
        }
        else {
            module = c4m_to_utf8(c4m_xlist_get(path_parts, item_len - 2, NULL));
            goto fill_in_package;
        }
    }

    return get_file_compile_ctx(ctx, path, module, package, flags, NULL);
}

c4m_file_compile_ctx *
c4m_init_module_from_loc(c4m_compile_ctx *ctx, c4m_str_t *path)
{
    path = c4m_to_utf8(path);

    if (c4m_str_starts_with(path, c4m_new_utf8("http"))) {
        return ctx_init_from_web_uri(ctx, path);
    }

    int64_t ix = c4m_str_rfind(path, c4m_new_utf8("/"));

    if (ix == -1) {
        path = c4m_cstr_format("./{}", path);
        ix   = 1;
    }

    return ctx_init_from_file_uri(ctx, path, ix);
}

c4m_file_compile_ctx *
c4m_init_from_use(c4m_compile_ctx *ctx,
                  c4m_str_t       *module,
                  c4m_str_t       *package,
                  c4m_str_t       *path)
{
    c4m_file_compile_ctx *result;
    c4m_xlist_t          *parts;
    c4m_utf8_t           *provided_path = NULL;
    bool                  error         = false;

    if (path != NULL && c4m_str_starts_with(path, c4m_new_utf8("http"))) {
        if (package) {
            parts   = c4m_u8_map(c4m_str_xsplit(package, c4m_new_utf8(".")));
            parts   = c4m_u8_map(parts);
            package = c4m_path_join(parts);
        }

        return ctx_init_from_web_uri(ctx, path);
    }

    if (path != NULL) {
        c4m_xlist_t *parts = c4m_new(c4m_tspec_xlist(c4m_tspec_utf8()));
        provided_path      = path;

        c4m_xlist_append(parts, c4m_to_utf8(path));

        if (package != NULL) {
            c4m_xlist_append(parts, c4m_to_utf8(package));
        }

        c4m_xlist_append(parts, c4m_to_utf8(module));

        path = c4m_to_utf8(c4m_path_join(parts));
    }
    else {
        path = perform_path_search(package, module);

        if (path == NULL) {
            if (package) {
                path = c4m_cstr_format("{}.{}", package, module);
            }
            else {
                path = module;
            }

            error = true;
        }
    }

    result = get_file_compile_ctx(ctx,
                                  path,
                                  module,
                                  package,
                                  C4F_FILE | C4F_ADD_TO_QUEUE,
                                  NULL);

    result->provided_path = provided_path;

    if (error) {
        result->path = module;
        if (package != NULL) {
            result->path = c4m_cstr_format("{}.{}", package, module);
        }
        c4m_file_load_error(result, c4m_err_search_path, path);
    }

    return result;
}

c4m_compile_ctx *
c4m_new_compile_context(c4m_str_t *input)
{
    c4m_compile_ctx *result = c4m_gc_alloc(c4m_compile_ctx);

    result->module_cache  = c4m_new(c4m_tspec_dict(c4m_tspec_u64(),
                                                  c4m_tspec_ref()));
    result->final_attrs   = c4m_new_scope(NULL, C4M_SCOPE_GLOBAL);
    result->final_globals = c4m_new_scope(NULL, C4M_SCOPE_ATTRIBUTES);
    result->final_spec    = c4m_new_spec();
    result->backlog       = c4m_new(c4m_tspec_set(c4m_tspec_ref()));
    result->processed     = c4m_new(c4m_tspec_set(c4m_tspec_ref()));

    if (input != NULL) {
        result->entry_point = c4m_init_module_from_loc(result, input);
    }

    return result;
}

// If this fails due to the source not being found or some other IO
// error, it will return NULL and add an error to the file compile
// ctx.
//
// However, if you call it wrong, at the API level, it raises an
// exception.
//
// Currently, this is only handling files on the local file system; need
// to add an API for easier http/https access.
static void
c4m_initial_load_one(c4m_compile_ctx *cctx, c4m_file_compile_ctx *ctx)
{
    c4m_stream_t *stream;

    if (ctx->fatal_errors) {
        return;
    }

    C4M_TRY
    {
        stream = c4m_file_instream(c4m_to_utf8(ctx->path), C4M_T_UTF8);

        if (c4m_lex(ctx, stream) != false) {
            c4m_parse(ctx);
            c4m_file_decl_pass(cctx, ctx);
        }
    }
    C4M_EXCEPT
    {
        c4m_utf8_t *msg = c4m_exception_get_message(C4M_X_CUR());

        if (errno == ENOENT) {
            if (ctx->package && strcmp(ctx->package->data, "__default__")) {
                ctx->path = c4m_cstr_format("{}.{}",
                                            ctx->package,
                                            ctx->module);
            }
            else {
                ctx->path = ctx->module;
            }

            if (ctx->provided_path != NULL) {
                ctx->path = c4m_cstr_format("{} (in {})",
                                            ctx->path,
                                            ctx->provided_path);
            }
        }

        c4m_file_load_error(ctx,
                            c4m_err_open_file,
                            ctx->path,
                            msg);
    }
    C4M_TRY_END;
}

// This loads all modules up through symbol declaration.
void
c4m_perform_module_loads(c4m_compile_ctx *ctx)
{
    c4m_file_compile_ctx *cur;

    while (true) {
        cur = c4m_set_any_item(ctx->backlog, NULL);
        if (cur == NULL) {
            return;
        }

        c4m_initial_load_one(ctx, cur);

        cur->status = c4m_compile_status_code_loaded;

        c4m_set_put(ctx->processed, cur);
        c4m_set_remove(ctx->backlog, cur);
    }
}

c4m_compile_ctx *
c4m_compile_from_entry_point(c4m_str_t *location)
{
    c4m_compile_ctx *result = c4m_new_compile_context(location);

    c4m_perform_module_loads(result);

    return result;
}

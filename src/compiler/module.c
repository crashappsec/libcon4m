#define C4M_USE_INTERNAL_API
#include "con4m.h"

c4m_grid_t *
c4m_get_module_summary_info(c4m_compile_ctx *ctx)
{
    int         n      = c4m_list_len(ctx->module_ordering);
    c4m_grid_t *result = c4m_new(c4m_type_grid(),
                                 c4m_kw("start_cols",
                                        c4m_ka(4),
                                        "header_rows",
                                        c4m_ka(1),
                                        "container_tag",
                                        c4m_ka("table2"),
                                        "stripe",
                                        c4m_ka(true)));

    c4m_list_t *row = c4m_new_table_row();

    c4m_list_append(row, c4m_new_utf8("Module"));
    c4m_list_append(row, c4m_new_utf8("Path"));
    c4m_list_append(row, c4m_new_utf8("Hash"));
    c4m_list_append(row, c4m_new_utf8("Obj module index"));
    c4m_grid_add_row(result, row);

    for (int i = 0; i < n; i++) {
        c4m_module_compile_ctx *f = c4m_list_get(ctx->module_ordering, i, NULL);

        c4m_utf8_t *spec;

        row = c4m_new_table_row();

        if (f->package == NULL) {
            spec = f->module;
        }
        else {
            spec = c4m_cstr_format("{}.{}", f->package, f->module);
        }

        c4m_utf8_t *hash = c4m_cstr_format("{:x}", c4m_box_u64(f->module_id));
        c4m_utf8_t *mod  = c4m_cstr_format("{}",
                                          c4m_box_u64(f->local_module_id));

        c4m_list_append(row, spec);
        c4m_list_append(row, f->path);
        c4m_list_append(row, hash);
        c4m_list_append(row, mod);
        c4m_grid_add_row(result, row);
    }

    c4m_set_column_style(result, 0, "snap");
    c4m_set_column_style(result, 1, "snap");
    c4m_set_column_style(result, 2, "snap");
    c4m_set_column_style(result, 3, "snap");

    return result;
}

static void
fcx_gc_bits(uint64_t *bitfield, c4m_module_compile_ctx *ctx)
{
    c4m_mark_raw_to_addr(bitfield, ctx, &ctx->extern_decls);
}

c4m_module_compile_ctx *
c4m_new_module_compile_ctx()
{
    return c4m_gc_alloc_mapped(c4m_module_compile_ctx, fcx_gc_bits);
}

static inline uint64_t
module_key(c4m_utf8_t *package, c4m_utf8_t *module)
{
    if (!package) {
        package = c4m_new_utf8("");
    }

    package = c4m_to_utf8(package);
    module  = c4m_to_utf8(module);

    c4m_sha_t sha;
    c4m_sha_init(&sha, NULL);
    c4m_sha_string_update(&sha, package);
    c4m_sha_int_update(&sha, '.');
    c4m_sha_string_update(&sha, module);

    c4m_buf_t *digest = c4m_sha_finish(&sha);

    return ((uint64_t *)digest->data)[0];
}

// package is the only string allowed to be null here.
// fext can also be null, in which case we take in the default values.
static c4m_module_compile_ctx *
one_lookup_try(c4m_compile_ctx *ctx,
               c4m_str_t       *path,
               c4m_str_t       *module,
               c4m_str_t       *package,
               c4m_list_t      *fext)
{
    if (package) {
        package = c4m_to_utf8(package);
    }

    module = c4m_to_utf8(module);

    // First check the cache.
    uint64_t                key    = module_key(package, module);
    c4m_module_compile_ctx *result = hatrack_dict_get(ctx->module_cache,
                                                      (void *)key,
                                                      NULL);

    if (result) {
        return result;
    }

    if (!fext) {
        fext = c4m_set_to_xlist(c4m_get_allowed_file_extensions());
    }

    c4m_list_t *l = c4m_list(c4m_type_utf8());

    c4m_list_append(l, path);

    if (package) {
        // For the package lookup, we need to replace dots in the package
        // name with slashes.
        c4m_utf8_t *s = c4m_str_replace(package, c4m_new_utf8("."), c4m_new_utf8("/"));
        c4m_list_append(l, s);
    }
    c4m_list_append(l, module);

    c4m_utf8_t *base    = c4m_path_join(l);
    int         num_ext = c4m_list_len(fext);
    c4m_utf8_t *contents;

    c4m_utf8_t *attempt = NULL;

    for (int i = 0; i < num_ext; i++) {
        attempt = c4m_cstr_format("{}.{}",
                                  base,
                                  c4m_list_get(fext, i, NULL));

        // clang-format off
	if (c4m_str_starts_with(attempt, c4m_new_utf8("http:")) ||
	    c4m_str_starts_with(attempt, c4m_new_utf8("https:"))) {
            // clang-format on
            c4m_basic_http_response_t *r = c4m_http_get(attempt);
            contents                     = c4m_http_op_get_output_utf8(r);
        }
        else {
            contents = c4m_read_utf8_file(attempt);
        }
        if (contents != NULL) {
            break;
        }
    }

    // Tell the caller to try again with a different path.
    if (!contents) {
        return NULL;
    }

    result              = c4m_new_module_compile_ctx();
    result->module      = module;
    result->package     = package;
    result->path        = path;
    result->raw         = contents;
    result->errors      = c4m_list(c4m_type_ref());
    result->loaded_from = attempt;
    result->module_id   = key;

    c4m_buf_t    *b = c4m_new(c4m_type_buffer(),
                           c4m_kw("length",
                                  c4m_ka(c4m_str_byte_len(contents)),
                                  "ptr",
                                  c4m_ka(contents->data)));
    c4m_stream_t *s = c4m_buffer_instream(b);

    if (!c4m_lex(result, s)) {
        ctx->fatality = true;
    }

    hatrack_dict_put(ctx->module_cache, (void *)key, result);

    return result;
}

static inline void
adjust_it(c4m_str_t **pathp,
          c4m_str_t **pkgp,
          c4m_utf8_t *path_string,
          c4m_utf8_t *matched_system_path,
          int         matched_len,
          int         path_len)
{
    path_string = c4m_str_slice(path_string, matched_len, path_len);
    path_string = c4m_to_utf8(path_string);

    if (path_string->data[0] == '/') {
        path_string->data = path_string->data + 1;
        path_string->byte_len--;
    }

    c4m_utf8_t *new_prefix = c4m_str_replace(path_string,
                                             c4m_new_utf8("/"),
                                             c4m_new_utf8("."));

    *pathp = matched_system_path;

    if (*pkgp == NULL || (*pkgp)->byte_len == 0) {
        *pkgp = new_prefix;
    }
    else {
        c4m_utf8_t *package = c4m_str_replace(*pkgp,
                                              c4m_new_utf8("/"),
                                              c4m_new_utf8("."));
        if (!package->byte_len) {
            *pkgp = new_prefix;
        }
        else {
            *pkgp = c4m_cstr_format("{}.{}", new_prefix, package);
        }
    }
}

static void
adjust_path_and_package(c4m_str_t **pathp, c4m_str_t **pkgp)
{
    c4m_utf8_t *path    = c4m_to_utf8(*pathp);
    int         pathlen = c4m_str_byte_len(path);
    int         otherlen;

    c4m_list_t *sp = c4m_get_module_search_path();

    for (int i = 0; i < c4m_list_len(sp); i++) {
        c4m_utf8_t *possible = c4m_to_utf8(c4m_list_get(sp, i, NULL));

        if (c4m_str_starts_with(path, possible)) {
            otherlen = c4m_str_byte_len(possible);

            if (pathlen == otherlen) {
                if (*pkgp) {
                    *pkgp = c4m_str_replace(*pkgp, c4m_new_utf8("/"), c4m_new_utf8("."));
                }
                break;
            }
            if (possible->data[otherlen - 1] == '/') {
                adjust_it(pathp, pkgp, path, possible, otherlen, pathlen);
                break;
            }
            if (path->data[otherlen] == '/') {
                adjust_it(pathp, pkgp, path, possible, otherlen, pathlen);
                break;
            }
        }
    }
}

static bool
path_is_url(c4m_str_t *path)
{
    if (c4m_str_starts_with(path, c4m_new_utf8("https:"))) {
        return true;
    }

    if (c4m_str_starts_with(path, c4m_new_utf8("http:"))) {
        return true;
    }

    return false;
}

// In this function, the 'path' parameter is either a full URL or
// file system path, or a URL. If it's missing, we need to search.
//
// If the package is missing, EITHER it is going to be the same as the
// context we're in when we're searching (i.e., when importing another
// module), or it will be a top-level module.
//
// Note that the module must have the file extension stripped; the
// extension can be provided in the list, but if it's not there, the
// system extensions get searched.
c4m_module_compile_ctx *
c4m_find_module(c4m_compile_ctx *ctx,
                c4m_str_t       *path,
                c4m_str_t       *module,
                c4m_str_t       *package,
                c4m_str_t       *relative_package,
                c4m_str_t       *relative_path,
                c4m_list_t      *fext)
{
    c4m_module_compile_ctx *result;

    // If a path was provided, then the package / module need to be
    // fully qualified.
    if (path != NULL) {
        if (!path_is_url(path)) {
            path = c4m_resolve_path(path);
        }
        adjust_path_and_package(&path, &package);

        result = one_lookup_try(ctx, path, module, package, fext);
        return result;
    }

    if (package == NULL && relative_package != NULL) {
        result = one_lookup_try(ctx,
                                relative_path,
                                module,
                                relative_package,
                                fext);
        if (result) {
            return result;
        }
    }

    // At this point, it could be in the top level of the relative
    // path, or else we have to do a full search.

    if (relative_path != NULL) {
        result = one_lookup_try(ctx, relative_path, module, NULL, fext);
        if (result) {
            return result;
        }
    }

    c4m_list_t *sp = c4m_get_module_search_path();
    int         n  = c4m_list_len(sp);

    for (int i = 0; i < n; i++) {
        c4m_utf8_t *one = c4m_list_get(sp, i, NULL);

        result = one_lookup_try(ctx, one, module, package, fext);

        if (result) {
            return result;
        }
    }

    // If we searched everywhere and nothing, then it's not found.
    return NULL;
}

bool
c4m_add_module_to_worklist(c4m_compile_ctx *cctx, c4m_module_compile_ctx *fctx)
{
    return c4m_set_add(cctx->backlog, fctx);
}

static c4m_module_compile_ctx *
postprocess_module(c4m_compile_ctx        *cctx,
                   c4m_module_compile_ctx *fctx,
                   c4m_utf8_t             *path,
                   bool                    http_err,
                   c4m_utf8_t             *errmsg)
{
    if (!fctx) {
        c4m_module_compile_ctx *result = c4m_new_module_compile_ctx();
        result->path                   = path;
        result->errors                 = c4m_list(c4m_type_ref());
        cctx->fatality                 = true;

        hatrack_dict_put(cctx->module_cache, NULL, result);

        if (!errmsg) {
            errmsg = c4m_new_utf8("Internal error");
        }
        c4m_module_load_error(result,
                              c4m_err_open_module,
                              path,
                              errmsg);

        return result;
    }

    if (http_err) {
        c4m_module_load_error(fctx, c4m_warn_no_tls);
    }

    return fctx;
}

static c4m_utf8_t *
package_from_path_prefix(c4m_utf8_t *path, c4m_utf8_t **path_loc)
{
    c4m_list_t *paths = c4m_get_module_search_path();
    c4m_utf8_t *one;

    int n = c4m_list_len(paths);

    for (int i = 0; i < n; i++) {
        one = c4m_to_utf8(c4m_list_get(paths, i, NULL));

        if (c4m_str_starts_with(one, path)) {
            *path_loc = one;

            int         ix = c4m_str_byte_len(one);
            c4m_utf8_t *s  = c4m_to_utf8(c4m_str_slice(path, ix, -1));

            while (s->byte_len != 0 && s->data[0] == '/') {
                s->data++;
                s->codepoints--;
                s->byte_len--;
            }
            for (int j = 0; j < s->byte_len; j++) {
                if (s->data[j] == '/') {
                    s->data[j] = '.';
                }
            }

            return s;
        }
    }

    return NULL;
}

static inline c4m_module_compile_ctx *
ctx_init_from_web_uri(c4m_compile_ctx *ctx,
                      c4m_utf8_t      *inpath,
                      bool             has_ext,
                      bool             https)
{
    c4m_module_compile_ctx *result;
    c4m_utf8_t             *module;
    c4m_utf8_t             *package;
    c4m_utf8_t             *path;

    if (c4m_str_codepoint_len(inpath) <= 8) {
        goto malformed;
    }

    char *s;

    // We know the colon is there; start on it, and look for
    // the two slashes.
    if (https) {
        s = &inpath->data[6];
    }
    else {
        s = &inpath->data[5];
    }

    if (*s++ != '/') {
        goto malformed;
    }
    if (*s++ != '/') {
        goto malformed;
    }

    if (has_ext) {
        int n  = c4m_str_rfind(inpath, c4m_new_utf8("/")) + 1;
        module = c4m_to_utf8(c4m_str_slice(inpath, n, -1));
        inpath = c4m_to_utf8(c4m_str_slice(inpath, 0, n - 1));
    }
    else {
        module = c4m_new_utf8(C4M_PACKAGE_INIT_MODULE);
    }

    package = package_from_path_prefix(inpath, &path);

    if (!package) {
        path = inpath;
    }

    result = c4m_find_module(ctx, path, module, package, NULL, NULL, NULL);
    return postprocess_module(ctx, result, NULL, !https, NULL);

malformed:

    result       = c4m_new_module_compile_ctx();
    result->path = inpath;

    c4m_module_load_error(result, c4m_err_malformed_url, !https);
    return result;
}

static c4m_module_compile_ctx *
ctx_init_from_local_file(c4m_compile_ctx *ctx, c4m_str_t *inpath)
{
    c4m_utf8_t *module;
    c4m_utf8_t *package;
    c4m_utf8_t *path;

    inpath    = c4m_resolve_path(inpath);
    int64_t n = c4m_str_rfind(inpath, c4m_new_utf8("/"));

    if (n == -1) {
        module  = inpath;
        path    = c4m_new_utf8("");
        package = c4m_new_utf8("");
    }
    else {
        int l;
        l       = c4m_str_codepoint_len(inpath);
        module  = c4m_to_utf8(c4m_str_slice(inpath, n + 1, l));
        inpath  = c4m_to_utf8(c4m_str_slice(inpath, 0, n));
        l       = c4m_str_codepoint_len(module);
        n       = c4m_str_rfind(module, c4m_new_utf8("."));
        module  = c4m_to_utf8(c4m_str_slice(module, 0, n));
        package = package_from_path_prefix(inpath, &path);

        if (!package) {
            path = inpath;
        }
    }

    return c4m_find_module(ctx, path, module, package, NULL, NULL, NULL);
}

static c4m_module_compile_ctx *
ctx_init_from_module_spec(c4m_compile_ctx *ctx, c4m_str_t *path)
{
    c4m_utf8_t *package = NULL;
    c4m_utf8_t *module;
    int64_t     n = c4m_str_rfind(path, c4m_new_utf8("."));

    if (n != -1) {
        package = c4m_to_utf8(c4m_str_slice(path, 0, n));
        module  = c4m_to_utf8(c4m_str_slice(path, n + 1, -1));
    }
    else {
        module = path;
    }

    return c4m_find_module(ctx, NULL, module, package, NULL, NULL, NULL);
}

static c4m_module_compile_ctx *
ctx_init_from_package_spec(c4m_compile_ctx *ctx, c4m_str_t *path)
{
    path               = c4m_resolve_path(path);
    char       *p      = &path->data[c4m_str_byte_len(path) - 1];
    c4m_utf8_t *module = c4m_new_utf8(C4M_PACKAGE_INIT_MODULE);
    char       *end_slash;
    c4m_utf8_t *package;

    // Avoid trailing slashes, including consecutive ones.
    while (p > path->data && *p == '/') {
        --p;
    }

    end_slash = p + 1;

    while (p > path->data) {
        if (*p == '/') {
            break;
        }
        --p;
    }

    // When this is true, the slash we saw was a trailing path, in
    // which case the whole thing is expected to be the package spec.
    if (*p != '/') {
        *end_slash = 0;
        package    = c4m_new_utf8(path->data);
        *end_slash = '/';
        path       = c4m_new_utf8("");
    }
    else {
        package = c4m_new_utf8(p + 1);
        *p      = 0;
        path    = c4m_new_utf8(path->data);
        *p      = '/';
    }

    return c4m_find_module(ctx, path, module, package, NULL, NULL, NULL);
}

static inline bool
has_c4m_extension(c4m_utf8_t *s)
{
    c4m_list_t *exts = c4m_set_to_xlist(c4m_get_allowed_file_extensions());

    for (int i = 0; i < c4m_list_len(exts); i++) {
        c4m_utf8_t *ext = c4m_to_utf8(c4m_list_get(exts, i, NULL));
        if (c4m_str_ends_with(s, c4m_cstr_format(".{}", ext))) {
            return true;
        }
    }

    return false;
}

c4m_module_compile_ctx *
c4m_init_module_from_loc(c4m_compile_ctx *ctx, c4m_str_t *path)
{
    // This function is meant for handling a module spec at the
    // command-line, not something via a 'use' statement.
    //
    // At the command line, someone could be trying to run a module
    // either the local directory or via an absolute path, or they
    // could be trying to load a package, in which case we actually
    // want to open the __init module inside that package.
    //
    // In these cases, we do NOT search the con4m path for stuff from
    // the command line.
    //
    // OR, they might specify 'package.module' in which case we *do*
    // want to go ahead and use the Con4m path (though, adding the
    // CWD).
    //
    // The key distinguisher is the file extension. So here are our rules:
    //
    // 1. If it starts with http: or https:// it's a URL to either a
    //    package or a module. If it's a module, it'll have a valid
    //    file extension, otherwise it's a package.
    //
    // 2. If we see one of our valid file extensions, then it's
    //    treated as specifying a single con4m file at the path
    //    provided (whether relative or absolute). In this case, we
    //    still might have a package, IF the con4m file lives
    //    somewhere in the module path. But we do not want to take the
    //    path from the command line.
    //
    // 3. If we do NOT see a valid file extension, and there are no
    //    slashes in the spec, then we treat it like a module spec
    //    where we search the path (just like a use statement).
    //
    // 4. If there is no file extension, but there IS a slash, then we
    //    assume it is a spec to a package location. If the location
    //    is in our path somewhere, then we calculate the package
    //    relative to that path. Otherwise, we put the package in the
    //    top-level.  In either case, we ensure there is a __init
    //    file.
    path                                  = c4m_to_utf8(path);
    c4m_module_compile_ctx *result        = NULL;
    bool                    has_extension = has_c4m_extension(path);

    if (c4m_str_starts_with(path, c4m_new_utf8("http:"))) {
        result = ctx_init_from_web_uri(ctx, path, has_extension, false);
    }

    if (!result && c4m_str_starts_with(path, c4m_new_utf8("https:"))) {
        result = ctx_init_from_web_uri(ctx, path, has_extension, true);
    }

    if (!result && has_extension) {
        result = ctx_init_from_local_file(ctx, path);
    }

    if (!result && c4m_str_find(path, c4m_new_utf8("/")) == -1) {
        result = ctx_init_from_module_spec(ctx, path);
    }

    if (!result) {
        result = ctx_init_from_package_spec(ctx, path);
    }

    return postprocess_module(ctx, result, path, false, NULL);
}

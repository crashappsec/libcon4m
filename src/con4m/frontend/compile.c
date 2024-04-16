#include "con4m.h"

static c4m_str_t *
module_name_from_path(c4m_str_t *path)
{
    c4m_xlist_t *parts     = c4m_str_xsplit(path, c4m_get_slash_const());
    int          l         = c4m_xlist_len(parts);
    c4m_str_t   *candidate = c4m_xlist_get(parts, l - 1, NULL);

    l = c4m_str_find(candidate, c4m_get_period_const());

    if (l == -1) {
        return candidate;
    }

    return c4m_str_slice(candidate, 0, l);
}

c4m_file_compile_ctx *
_c4m_new_compile_ctx(c4m_str_t *module_name, ...)
{
    c4m_file_compile_ctx *result;
    c4m_str_t            *scheme    = NULL;
    c4m_str_t            *authority = NULL;
    c4m_str_t            *path      = NULL;
    c4m_str_t            *package   = NULL;

    c4m_karg_only_init(module_name);
    c4m_kw_ptr("uri_scheme", scheme);
    c4m_kw_ptr("uri_authority", authority);
    c4m_kw_ptr("uri_path", path);
    c4m_kw_ptr("package", package);

    if (package == NULL) {
        package = c4m_new(c4m_tspec_utf8(),
                          c4m_kw("cstring", c4m_ka("__default__")));
    }

    if (module_name == NULL && path != NULL) {
        module_name = module_name_from_path(path);
    }

    result            = c4m_gc_alloc(c4m_file_compile_ctx);
    result->errors    = c4m_new(c4m_tspec_xlist(c4m_tspec_ref()));
    result->scheme    = scheme;
    result->authority = authority;
    result->path      = path;
    result->package   = package;
    result->module    = module_name;

    if (!c4m_validate_module_info(result)) {
        C4M_CRAISE(
            "Invalid module spec; the packages and the module name "
            "must all be valid identifiers; package parts must be "
            "separated by dots.");
    }

    return result;
}

bool
c4m_validate_module_info(c4m_file_compile_ctx *ctx)
{
    c4m_codepoint_t cp;

    if (ctx->package == NULL || ctx->module == NULL) {
        return false;
    }

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

// If this fails due to the source not being found or some other IO
// error, it will return NULL and add an error to the file compile
// ctx.
//
// However, if you call it wrong, at the API level, it raises an
// exception.
//
// Currently, this is only handling files on the local file system; need
// to add an API for easier http/https access.
c4m_stream_t *
c4m_load_code(c4m_file_compile_ctx *ctx)
{
    c4m_stream_t *result;

    if (ctx->scheme != NULL) {
        C4M_CRAISE("Non-file URI schemes are currently unimplemented.");
    }

    if (!ctx->path) {
        C4M_CRAISE("Do not call with a null path.");
    }

    C4M_TRY
    {
        result = c4m_file_instream(ctx->path, C4M_T_UTF8);
    }
    C4M_EXCEPT
    {
        c4m_compile_error *err = c4m_gc_alloc(c4m_compile_error);
        err->code              = c4m_err_open_file;
        err->exception_message = c4m_exception_get_message(C4M_X_CUR());

        c4m_xlist_append(ctx->errors, err);
        result = NULL;
    }
    C4M_TRY_END;

    return result;
}

#include "con4m.h"

// At the moment, most of this stuff is wasted; this is only saved for
// callback objects, and those should just take what they need past
// compile time.  But was more expedient to do this, and might be
// worth keeping more things around for debugging.

static void
marshal_one_param(c4m_fn_param_info_t *param,
                  c4m_stream_t        *stream,
                  c4m_dict_t          *memos,
                  int64_t             *mid)
{
    c4m_sub_marshal(param->name, stream, memos, mid);
    c4m_sub_marshal(param->type, stream, memos, mid);
    c4m_marshal_bool(param->ffi_holds, stream);
    c4m_marshal_bool(param->ffi_allocs, stream);
}

static void
unmarshal_one_param(c4m_fn_param_info_t *param,
                    c4m_stream_t        *stream,
                    c4m_dict_t          *memos)
{
    param->name       = c4m_sub_unmarshal(stream, memos);
    param->type       = c4m_sub_unmarshal(stream, memos);
    param->ffi_holds  = c4m_unmarshal_bool(stream);
    param->ffi_allocs = c4m_unmarshal_bool(stream);
}

static void
marshal_sig_info(c4m_sig_info_t *signature,
                 c4m_stream_t   *stream,
                 c4m_dict_t     *memos,
                 int64_t        *mid)
{
    c4m_sub_marshal(signature->full_type, stream, memos, mid);
    // For now, skip compile-time only stuff like the
    // formals.
    marshal_one_param(&signature->return_info, stream, memos, mid);
    c4m_marshal_i32(signature->num_params, stream);

    for (int i = 0; i < signature->num_params; i++) {
        marshal_one_param(&signature->param_info[i], stream, memos, mid);
    }
}

static void
unmarshal_sig(c4m_sig_info_t *signature,
              c4m_stream_t   *stream,
              c4m_dict_t     *memos)
{
    signature->full_type = c4m_sub_unmarshal(stream, memos);
    unmarshal_one_param(&signature->return_info, stream, memos);
    signature->num_params = c4m_unmarshal_i32(stream);
    signature->param_info = c4m_gc_array_alloc(c4m_fn_param_info_t,
                                               signature->num_params);

    for (int i = 0; i < signature->num_params; i++) {
        unmarshal_one_param(&signature->param_info[i], stream, memos);
    }
}

static void
marshal_extern_finfo(c4m_ffi_decl_t *ffi_info,
                     c4m_stream_t   *stream,
                     c4m_dict_t     *memos,
                     int64_t        *mid)
{
    c4m_sub_marshal(ffi_info->short_doc, stream, memos, mid);
    c4m_sub_marshal(ffi_info->long_doc, stream, memos, mid);
    c4m_sub_marshal(ffi_info->local_name, stream, memos, mid);
    c4m_marshal_unmanaged_object(ffi_info->local_params,
                                 stream,
                                 memos,
                                 mid,
                                 (c4m_marshal_fn)marshal_sig_info);
    c4m_sub_marshal(ffi_info->external_name, stream, memos, mid);
    c4m_sub_marshal(ffi_info->dll_list, stream, memos, mid);
    c4m_marshal_i32(ffi_info->num_ext_params, stream);
    for (int i = 0; i < ffi_info->num_ext_params; i++) {
        c4m_marshal_u8(ffi_info->external_params[i], stream);
    }
    c4m_marshal_u8(ffi_info->external_return_type, stream);
    c4m_marshal_bool(ffi_info->skip_boxes, stream);
    c4m_marshal_i32(ffi_info->global_ffi_call_ix, stream);
    // Don't marshal the CIF.
}

static void
unmarshal_extern_finfo(c4m_ffi_decl_t *ffi_info,
                       c4m_stream_t   *stream,
                       c4m_dict_t     *memos)
{
    ffi_info->short_doc  = c4m_sub_unmarshal(stream, memos);
    ffi_info->long_doc   = c4m_sub_unmarshal(stream, memos);
    ffi_info->local_name = c4m_sub_unmarshal(stream, memos);

    void *info = c4m_unmarshal_unmanaged_object(
        sizeof(c4m_sig_info_t),
        stream,
        memos,
        (c4m_unmarshal_fn)unmarshal_sig);

    ffi_info->local_params    = info;
    ffi_info->external_name   = c4m_sub_unmarshal(stream, memos);
    ffi_info->dll_list        = c4m_sub_unmarshal(stream, memos);
    ffi_info->num_ext_params  = c4m_unmarshal_i32(stream);
    ffi_info->external_params = c4m_gc_array_alloc(uint8_t,
                                                   ffi_info->num_ext_params);

    for (int i = 0; i < ffi_info->num_ext_params; i++) {
        ffi_info->external_params[i] = c4m_unmarshal_u8(stream);
    }

    ffi_info->external_return_type = c4m_unmarshal_u8(stream);
    ffi_info->skip_boxes           = c4m_unmarshal_bool(stream);
    ffi_info->global_ffi_call_ix   = c4m_unmarshal_i32(stream);
}

static void
marshal_local_finfo(c4m_fn_decl_t *local_info,
                    c4m_stream_t  *stream,
                    c4m_dict_t    *memos,
                    int64_t       *mid)
{
    c4m_sub_marshal(local_info->short_doc, stream, memos, mid);
    c4m_sub_marshal(local_info->long_doc, stream, memos, mid);
    c4m_marshal_unmanaged_object(local_info->signature_info,
                                 stream,
                                 memos,
                                 mid,
                                 (c4m_marshal_fn)marshal_sig_info);
    // We only keep the local_id, module and the offset; there's def.
    // some wasted space in here with stuff we don't need at runtime,
    // but a TODO to remove it.
    c4m_marshal_i32(local_info->local_id, stream);
    c4m_marshal_i32(local_info->module_id, stream);
    c4m_marshal_i32(local_info->offset, stream);
}

static void
unmarshal_local_finfo(c4m_fn_decl_t *local_info,
                      c4m_stream_t  *stream,
                      c4m_dict_t    *memos)
{
    local_info->short_doc = c4m_sub_unmarshal(stream, memos);
    local_info->long_doc  = c4m_sub_unmarshal(stream, memos);

    void *info = c4m_unmarshal_unmanaged_object(
        sizeof(c4m_sig_info_t),
        stream,
        memos,
        (c4m_unmarshal_fn)unmarshal_sig);

    local_info->signature_info = info;
    local_info->local_id       = c4m_unmarshal_i32(stream);
    local_info->module_id      = c4m_unmarshal_i32(stream);
    local_info->offset         = c4m_unmarshal_i32(stream);
}

void
c4m_unmarshal_funcinfo(c4m_funcinfo_t *finfo,
                       c4m_stream_t   *stream,
                       c4m_dict_t     *memos)
{
    finfo->ffi = c4m_unmarshal_bool(stream);
    finfo->va  = c4m_unmarshal_bool(stream);

    void *info;

    if (finfo->ffi) {
        info = c4m_unmarshal_unmanaged_object(
            sizeof(c4m_ffi_decl_t),
            stream,
            memos,
            (c4m_unmarshal_fn)unmarshal_extern_finfo);

        finfo->implementation.ffi_interface = info;
    }
    else {
        info = c4m_unmarshal_unmanaged_object(
            sizeof(c4m_fn_decl_t),
            stream,
            memos,
            (c4m_unmarshal_fn)unmarshal_local_finfo);

        finfo->implementation.local_interface = info;
    }
}

void
c4m_marshal_funcinfo(c4m_funcinfo_t *finfo,
                     c4m_stream_t   *stream,
                     c4m_dict_t     *memos,
                     int64_t        *mid)
{
    c4m_marshal_bool(finfo->ffi, stream);
    c4m_marshal_bool(finfo->va, stream);

    if (finfo->ffi) {
        c4m_marshal_unmanaged_object(finfo->implementation.ffi_interface,
                                     stream,
                                     memos,
                                     mid,
                                     (c4m_marshal_fn)marshal_extern_finfo);
    }
    else {
        c4m_marshal_unmanaged_object(finfo->implementation.local_interface,
                                     stream,
                                     memos,
                                     mid,
                                     (c4m_marshal_fn)marshal_local_finfo);
    }
}

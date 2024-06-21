#include "con4m.h"

typedef void (*marshalfn_t)(void *ref, c4m_stream_t *out, c4m_dict_t *memos, int64_t *mid);

static void
marshal_dict_value_ref(c4m_dict_t *in, c4m_stream_t *out, c4m_dict_t *memos, int64_t *mid, marshalfn_t fn)
{
    uint64_t             length;
    hatrack_dict_item_t *view = hatrack_dict_items_sort(in, &length);

    c4m_marshal_u32(length, out);
    for (uint64_t i = 0; i < length; ++i) {
        c4m_sub_marshal(view[i].key, out, memos, mid);
        fn(view[i].value, out, memos, mid);
    }
}

static void
marshal_xlist_ref(c4m_xlist_t *in, c4m_stream_t *out, c4m_dict_t *memos, int64_t *mid, marshalfn_t fn)
{
    c4m_marshal_i32(in->append_ix, out);
    c4m_marshal_i32(in->length, out);
    for (int32_t i = 0; i < in->length; ++i) {
        fn(in->data[i], out, memos, mid);
    }
}

typedef void *(*unmarshalfn_t)(c4m_stream_t *in, c4m_dict_t *memos);

static void
unmarshal_dict_value_ref(c4m_dict_t *out, c4m_stream_t *in, c4m_dict_t *memos, unmarshalfn_t fn)
{
    uint32_t length = c4m_unmarshal_u32(in);

    for (uint32_t i = 0; i < length; ++i) {
        c4m_obj_t key   = c4m_sub_unmarshal(in, memos);
        void     *value = fn(in, memos);
        hatrack_dict_put(out, key, value);
    }
}

static c4m_xlist_t *
unmarshal_xlist_ref(c4m_stream_t *in, c4m_dict_t *memos, unmarshalfn_t fn)
{
    c4m_xlist_t *x = c4m_xlist(c4m_type_ref());
    x->append_ix   = c4m_unmarshal_i32(in);
    x->length      = c4m_unmarshal_i32(in);
    x->data        = c4m_gc_array_alloc(int64_t *, x->length);

    for (int32_t i = 0; i < x->append_ix; ++i) {
        x->data[i] = fn(in, memos);
    }

    return x;
}

static void
marshal_instruction(void *ref, c4m_stream_t *out, c4m_dict_t *memos, int64_t *mid)
{
    c4m_zinstruction_t *in = ref;

    c4m_marshal_u8(in->op, out);
    c4m_marshal_u8(in->pad, out);
    c4m_marshal_i16(in->module_id, out);
    c4m_marshal_i32(in->line_no, out);
    c4m_marshal_i32(in->arg, out);
    c4m_marshal_i64(in->immediate, out);
    c4m_sub_marshal(in->type_info, out, memos, mid);
}

static void *
unmarshal_instruction(c4m_stream_t *in, c4m_dict_t *memos)
{
    c4m_zinstruction_t *out = c4m_gc_alloc(c4m_zinstruction_t);

    out->op        = c4m_unmarshal_u8(in);
    out->pad       = c4m_unmarshal_u8(in);
    out->module_id = c4m_unmarshal_i16(in);
    out->line_no   = c4m_unmarshal_i32(in);
    out->arg       = c4m_unmarshal_i32(in);
    out->immediate = c4m_unmarshal_i64(in);
    out->type_info = c4m_sub_unmarshal(in, memos);

    return out;
}

#if 0 // Removing for now
static void
marshal_ffi_arg_info(void *ref, c4m_stream_t *out, c4m_dict_t *memos, int64_t *mid)
{
    c4m_zffi_arg_info_t *in = ref;

    c4m_marshal_bool(in->held, out);
    c4m_marshal_bool(in->alloced, out);
    c4m_marshal_i16(in->arg_type, out);
    c4m_marshal_i32(in->our_type, out);
    c4m_sub_marshal(in->name, out, memos, mid);
}

static void *
unmarshal_ffi_arg_info(c4m_stream_t *in, c4m_dict_t *memos)
{

    c4m_zffi_arg_info_t *out = c4m_gc_alloc(c4m_zffi_arg_info_t);

    out->held     = c4m_unmarshal_bool(in);
    out->alloced  = c4m_unmarshal_bool(in);
    out->arg_type = c4m_unmarshal_i16(in);
    out->our_type = c4m_unmarshal_i32(in);
    out->name     = c4m_sub_unmarshal(in, memos);

    return out;
}
#endif

static void
marshal_ffi_info(void *ref, c4m_stream_t *out, c4m_dict_t *memos, int64_t *mid)
{
#if 0
    c4m_zffi_info_t *in = ref;

    c4m_marshal_i64(in->nameoffset, out);
    c4m_marshal_i64(in->localname, out);
    c4m_marshal_i32(in->mid, out);
    c4m_sub_marshal(in->tid, out, memos, mid);
    c4m_marshal_bool(in->va, out);
    c4m_sub_marshal(in->dlls, out, memos, mid);
    marshal_xlist_ref(in->arg_info, out, memos, mid, marshal_ffi_arg_info);
    c4m_sub_marshal(in->shortdoc, out, memos, mid);
    c4m_sub_marshal(in->longdoc, out, memos, mid);
#endif
}

static void *
unmarshal_ffi_info(c4m_stream_t *in, c4m_dict_t *memos)
{
#if 0
    c4m_zffi_info_t *out = c4m_gc_alloc(c4m_zffi_info_t);

    out->nameoffset = c4m_unmarshal_i64(in);
    out->localname  = c4m_unmarshal_i64(in);
    out->mid        = c4m_unmarshal_i32(in);
    out->tid        = c4m_sub_unmarshal(in, memos);
    out->va         = c4m_unmarshal_bool(in);
    out->dlls       = c4m_sub_unmarshal(in, memos);
    out->arg_info   = unmarshal_xlist_ref(in, memos, unmarshal_ffi_arg_info);
    out->shortdoc   = c4m_sub_unmarshal(in, memos);
    out->longdoc    = c4m_sub_unmarshal(in, memos);

    return out;
#endif
    return NULL;
}

static void
marshal_symbol(void *ref, c4m_stream_t *out, c4m_dict_t *memos, int64_t *mid)
{
    c4m_zsymbol_t *in = ref;

    c4m_marshal_i64(in->offset, out);
    c4m_sub_marshal(in->tid, out, memos, mid);
}

static void *
unmarshal_symbol(c4m_stream_t *in, c4m_dict_t *memos)
{
    c4m_zsymbol_t *out = c4m_gc_alloc(c4m_zsymbol_t);

    out->offset = c4m_unmarshal_i64(in);
    out->tid    = c4m_sub_unmarshal(in, memos);

    return out;
}

static void
marshal_fn_info(void *ref, c4m_stream_t *out, c4m_dict_t *memos, int64_t *mid)
{
    c4m_zfn_info_t *in = ref;

    c4m_sub_marshal(in->funcname, out, memos, mid);
    c4m_sub_marshal(in->syms, out, memos, mid);
    marshal_xlist_ref(in->sym_types, out, memos, mid, marshal_symbol);
    c4m_sub_marshal(in->tid, out, memos, mid);
    c4m_marshal_i32(in->mid, out);
    c4m_marshal_i32(in->offset, out);
    c4m_marshal_i32(in->size, out);
    c4m_sub_marshal(in->shortdoc, out, memos, mid);
    c4m_sub_marshal(in->longdoc, out, memos, mid);
}

static void *
unmarshal_fn_info(c4m_stream_t *in, c4m_dict_t *memos)
{
    c4m_zfn_info_t *out = c4m_gc_alloc(c4m_zfn_info_t);

    out->funcname  = c4m_sub_unmarshal(in, memos);
    out->syms      = c4m_sub_unmarshal(in, memos);
    out->sym_types = unmarshal_xlist_ref(in, memos, unmarshal_symbol);
    out->tid       = c4m_sub_unmarshal(in, memos);
    out->mid       = c4m_unmarshal_i32(in);
    out->offset    = c4m_unmarshal_i32(in);
    out->size      = c4m_unmarshal_i32(in);
    out->shortdoc  = c4m_sub_unmarshal(in, memos);
    out->longdoc   = c4m_sub_unmarshal(in, memos);

    return out;
}

static void
marshal_value(c4m_value_t *in, c4m_stream_t *out, c4m_dict_t *memos, int64_t *mid)
{
    c4m_sub_marshal(in->type_info, out, memos, mid);
    c4m_dt_info_t *tinfo = c4m_type_get_data_type_info(in->type_info);
    if (tinfo->by_value) {
        c4m_marshal_u64((uint64_t)in->obj, out);
    }
    else {
        c4m_sub_marshal(in->obj, out, memos, mid);
    }
}

static void
unmarshal_value(c4m_value_t *out, c4m_stream_t *in, c4m_dict_t *memos)
{
    out->type_info       = c4m_sub_unmarshal(in, memos);
    c4m_dt_info_t *tinfo = c4m_type_get_data_type_info(out->type_info);
    if (tinfo->by_value) {
        out->obj = (void *)c4m_unmarshal_u64(in);
    }
    else {
        out->obj = c4m_sub_unmarshal(in, memos);
    }
}

static void
marshal_param_info(void *ref, c4m_stream_t *out, c4m_dict_t *memos, int64_t *mid)
{
    c4m_zparam_info_t *in = ref;

    c4m_sub_marshal(in->attr, out, memos, mid);
    c4m_marshal_i64(in->offset, out);
    c4m_sub_marshal(in->tid, out, memos, mid);
    c4m_marshal_bool(in->have_default, out);
    c4m_marshal_bool(in->is_private, out);
    c4m_marshal_bool(in->v_native, out);
    c4m_marshal_bool(in->i_native, out);
    c4m_marshal_i32(in->v_fn_ix, out);
    c4m_marshal_i32(in->i_fn_ix, out);
    c4m_sub_marshal(in->shortdoc, out, memos, mid);
    c4m_sub_marshal(in->longdoc, out, memos, mid);
    if (in->have_default) {
        marshal_value(&in->default_value, out, memos, mid);
    }
}

static void *
unmarshal_param_info(c4m_stream_t *in, c4m_dict_t *memos)
{
    c4m_zparam_info_t *out = c4m_gc_alloc(c4m_zparam_info_t);

    out->attr         = c4m_sub_unmarshal(in, memos);
    out->offset       = c4m_unmarshal_i64(in);
    out->tid          = c4m_sub_unmarshal(in, memos);
    out->have_default = c4m_unmarshal_bool(in);
    out->is_private   = c4m_unmarshal_bool(in);
    out->v_native     = c4m_unmarshal_bool(in);
    out->i_native     = c4m_unmarshal_bool(in);
    out->v_fn_ix      = c4m_unmarshal_i32(in);
    out->i_fn_ix      = c4m_unmarshal_i32(in);
    out->shortdoc     = c4m_sub_unmarshal(in, memos);
    out->longdoc      = c4m_sub_unmarshal(in, memos);

    if (out->have_default) {
        unmarshal_value(&out->default_value, in, memos);
    }

    return out;
}

static void
marshal_module_info(void *ref, c4m_stream_t *out, c4m_dict_t *memos, int64_t *mid)
{
    c4m_zmodule_info_t *in = ref;

    c4m_marshal_i32(in->module_id, out);
    c4m_marshal_u64(in->module_hash, out);
    c4m_sub_marshal(in->modname, out, memos, mid);
    c4m_sub_marshal(in->authority, out, memos, mid);
    c4m_sub_marshal(in->path, out, memos, mid);
    c4m_sub_marshal(in->package, out, memos, mid);
    c4m_sub_marshal(in->source, out, memos, mid);
    c4m_sub_marshal(in->version, out, memos, mid);
    c4m_sub_marshal(in->shortdoc, out, memos, mid);
    c4m_sub_marshal(in->longdoc, out, memos, mid);
    c4m_marshal_i32(in->module_var_size, out);
    c4m_marshal_i32(in->init_size, out);
    marshal_xlist_ref(in->parameters, out, memos, mid, marshal_param_info);
    marshal_xlist_ref(in->instructions, out, memos, mid, marshal_instruction);
}

static void *
unmarshal_module_info(c4m_stream_t *in, c4m_dict_t *memos)
{
    c4m_zmodule_info_t *out = c4m_gc_alloc(c4m_zmodule_info_t);

    out->module_id       = c4m_unmarshal_i32(in);
    out->module_hash     = c4m_unmarshal_u64(in);
    out->modname         = c4m_sub_unmarshal(in, memos);
    out->authority       = c4m_sub_unmarshal(in, memos);
    out->path            = c4m_sub_unmarshal(in, memos);
    out->package         = c4m_sub_unmarshal(in, memos);
    out->source          = c4m_sub_unmarshal(in, memos);
    out->version         = c4m_sub_unmarshal(in, memos);
    //  unmarshal_xlist_ref(in, memos, unmarshal_symbol);
    // out->codesyms        = c4m_sub_unmarshal(in, memos);
    // out->datasyms        = c4m_sub_unmarshal(in, memos);
    out->shortdoc        = c4m_sub_unmarshal(in, memos);
    out->longdoc         = c4m_sub_unmarshal(in, memos);
    out->module_var_size = c4m_unmarshal_i32(in);
    out->init_size       = c4m_unmarshal_i32(in);
    out->parameters      = unmarshal_xlist_ref(in, memos, unmarshal_param_info);
    out->instructions    = unmarshal_xlist_ref(in, memos, unmarshal_instruction);

    return out;
}

static void
marshal_object_file(c4m_zobject_file_t *in, c4m_stream_t *out, c4m_dict_t *memos, int64_t *mid)
{
#if 0
    // XXX The Nim code does some funny business with nextmid and nextEntrypoint
    //     that I'm unclear about. The equivalent code is here, but I'm not sure
    //     that it does what's intended. For starters, it seems the Nim code is
    //     making the assumption that the VM is always marshaled at top-level
    //     and isn't really managed by the con4m type system's marshalling
    //     support, but it is here. So, does this adjustment make sense or even
    //     work? Second, it seems like next_entrypoint is supposed to be a
    //     module_id, so why does that even correlate with the marshal id?

    // Just to be clear, the code that didn't get copied in
    // essentially is just making sure that the saved object sets the
    // next entry point in the output module correctly.  It didn't
    // change the marshal memo index; it set the value we marshal for
    // the next entry point (which is where the program will start up
    // from if resume is turned on).
    //
    // Basically, that value can be changed from run to run, which is
    // why I was waiting till the end to commit it, exactly at the
    // time of marshaling.

    if (!*mid && in->next_entrypoint != 0) {
        *mid = in->next_entrypoint;
    }
#endif

    c4m_marshal_u64(in->zero_magic, out);
    c4m_marshal_u16(in->zc_object_vers, out);
    c4m_sub_marshal(in->static_data, out, memos, mid);
    c4m_marshal_i32(in->num_const_objs, out);
    marshal_xlist_ref(in->module_contents, out, memos, mid, marshal_module_info);
    c4m_marshal_i32(in->entrypoint, out);
    c4m_marshal_i32(in->next_entrypoint, out);
    marshal_xlist_ref(in->func_info, out, memos, mid, marshal_fn_info);
    marshal_xlist_ref(in->ffi_info, out, memos, mid, marshal_ffi_info);
    // TODO c4m_sub_marshal(in->spec, out, memos, mid);
}

static c4m_zobject_file_t *
unmarshal_object_file(c4m_stream_t *in, c4m_dict_t *memos)
{
    c4m_zobject_file_t *out = c4m_gc_alloc(c4m_zobject_file_t);

    out->zero_magic      = c4m_unmarshal_u64(in);
    out->zc_object_vers  = c4m_unmarshal_u16(in);
    out->static_data     = c4m_sub_unmarshal(in, memos);
    out->num_const_objs  = c4m_unmarshal_i32(in);
    out->module_contents = unmarshal_xlist_ref(in, memos, unmarshal_module_info);
    out->entrypoint      = c4m_unmarshal_i32(in);
    out->next_entrypoint = c4m_unmarshal_i32(in);
    out->func_info       = unmarshal_xlist_ref(in, memos, unmarshal_fn_info);
    out->ffi_info        = unmarshal_xlist_ref(in, memos, unmarshal_ffi_info);
    // TODO out->spec = c4m_sub_unmarshal(in, memos);

    return out;
}

static void
marshal_attr_contents(void         *ref,
                      c4m_stream_t *out,
                      c4m_dict_t   *memos,
                      int64_t      *mid)
{
    c4m_attr_contents_t *in = ref;

    c4m_marshal_bool(in->is_set, out);
    c4m_marshal_bool(in->lock_on_write, out);
    if (in->is_set) {
        marshal_value(&in->contents, out, memos, mid);
        c4m_marshal_bool(in->locked, out);
        c4m_marshal_bool(in->override, out);
    }
    else {
        c4m_sub_marshal(in->contents.type_info, out, memos, mid);
    }
}

static void *
unmarshal_attr_contents(c4m_stream_t *in, c4m_dict_t *memos)
{
    c4m_attr_contents_t *out = c4m_gc_alloc(c4m_attr_contents_t);

    out->is_set        = c4m_unmarshal_bool(in);
    out->lock_on_write = c4m_unmarshal_bool(in);
    if (out->is_set) {
        unmarshal_value(&out->contents, in, memos);
        out->locked   = c4m_unmarshal_bool(in);
        out->override = c4m_unmarshal_bool(in);
    }
    else {
        out->contents.type_info = c4m_sub_unmarshal(in, memos);
    }

    return out;
}

static void
marshal_docs_container(void *ref, c4m_stream_t *out, c4m_dict_t *memos, int64_t *mid)
{
    c4m_docs_container_t *in = ref;

    c4m_sub_marshal(in->shortdoc, out, memos, mid);
    c4m_sub_marshal(in->longdoc, out, memos, mid);
}

static void *
unmarshal_docs_container(c4m_stream_t *in, c4m_dict_t *memos)
{
    c4m_docs_container_t *out = c4m_gc_alloc(c4m_docs_container_t);

    out->shortdoc = c4m_sub_unmarshal(in, memos);
    out->longdoc  = c4m_sub_unmarshal(in, memos);

    return out;
}

static void
marshal_module_allocations(c4m_vm_t *vm, c4m_stream_t *out, c4m_dict_t *memos, int64_t *mid)
{
    const int64_t nmodules = c4m_xlist_len(vm->obj->module_contents);
    for (int64_t n = 0; n < nmodules; ++n) {
        c4m_zmodule_info_t *module = c4m_xlist_get(vm->obj->module_contents,
                                                   n,
                                                   NULL);
        for (int64_t i = 0; i < module->module_var_size; ++i) {
            marshal_value(&vm->module_allocations[n][i], out, memos, mid);
        }
    }
}

static void
unmarshal_module_allocations(c4m_vm_t *vm, c4m_stream_t *in, c4m_dict_t *memos)
{
    const int64_t nmodules = c4m_xlist_len(vm->obj->module_contents);
    for (int64_t n = 0; n < nmodules; ++n) {
        c4m_zmodule_info_t *module = c4m_xlist_get(vm->obj->module_contents,
                                                   n,
                                                   NULL);
        for (int64_t i = 0; i < module->module_var_size; ++i) {
            unmarshal_value(&vm->module_allocations[n][i], in, memos);
        }
    }
}

void
c4m_vm_marshal(c4m_vm_t *vm, c4m_stream_t *out, c4m_dict_t *memos, int64_t *mid)
{
    marshal_object_file(vm->obj, out, memos, mid);

    // XXX marshalling this seems fine, but unmarshalling it seems sketchy
    //     seems like there should also be a vm-specific type store that handles
    //     types specific to this vm so that we can have multiple vms?
    // c4m_sub_marshal(c4m_global_type_env, out, memos, mid);

    marshal_module_allocations(vm, out, memos, mid);

    c4m_marshal_bool(vm->using_attrs, out);
    if (vm->using_attrs) {
        c4m_sub_marshal(vm->all_sections, out, memos, mid);
        marshal_dict_value_ref(vm->attrs, out, memos, mid, marshal_attr_contents);
        marshal_dict_value_ref(vm->section_docs, out, memos, mid, marshal_docs_container);
    }
}

void
c4m_vm_unmarshal(c4m_vm_t *vm, c4m_stream_t *in, c4m_dict_t *memos)
{
    vm->obj = unmarshal_object_file(in, memos);

    // XXX marshalling this seems fine, but unmarshalling it seems sketchy
    // c4m_global_type_env = c4m_sub_unmarshal(in, memos);

    c4m_vm_reset(vm);
    unmarshal_module_allocations(vm, in, memos);

    vm->using_attrs = c4m_unmarshal_bool(in);
    if (vm->using_attrs) {
        vm->all_sections = c4m_sub_unmarshal(in, memos);
        unmarshal_dict_value_ref(vm->attrs, in, memos, unmarshal_attr_contents);
        unmarshal_dict_value_ref(vm->section_docs, in, memos, unmarshal_docs_container);
    }
}

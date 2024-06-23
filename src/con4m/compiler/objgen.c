#include "con4m.h"

// TODO: load const instantiations.
static void
c4m_setup_obj(c4m_buf_t *static_data, int32_t nc, c4m_zobject_file_t *obj)
{
    obj->zero_magic       = 0x0c001dea0c001dea;
    obj->zc_object_vers   = 0x02;
    obj->marshaled_consts = static_data;
    obj->num_const_objs   = nc;
    obj->module_contents  = c4m_new(c4m_type_xlist(c4m_type_ref()));
    obj->func_info        = c4m_new(c4m_type_xlist(c4m_type_ref()));
    obj->ffi_info         = c4m_new(c4m_type_xlist(c4m_type_ref()));
}

void
c4m_add_module(c4m_zobject_file_t *obj, c4m_zmodule_info_t *module)
{
    int existing_len = c4m_xlist_len(obj->module_contents);
    assert(existing_len == module->module_id);

    c4m_xlist_append(obj->module_contents, module);
}

c4m_vm_t *
c4m_new_vm(c4m_compile_ctx *cctx)
{
    c4m_vm_t *result = c4m_gc_alloc(c4m_vm_t);
    result->obj      = c4m_gc_alloc(c4m_zobject_file_t);
    c4m_setup_obj(cctx->const_data, cctx->const_instantiation_id, result->obj);

    return result;
}

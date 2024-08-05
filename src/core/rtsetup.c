#include "con4m.h"

const c4m_version_t c4m_current_version = {
    .dotted.preview = C4M_VERS_PREVIEW,
    .dotted.patch   = C4M_VERS_PATCH,
    .dotted.minor   = C4M_VERS_MINOR,
    .dotted.major   = C4M_VERS_MAJOR,
};

void
c4m_objfile_gc_bits(uint64_t *bitmap, c4m_zobject_file_t *obj)
{
    c4m_mark_raw_to_addr(bitmap, obj, &obj->ffi_info);
    // The above set 'zero magic' which is not a pointer, but must be first.
    *bitmap &= ~1ULL;
}

void
c4m_zrun_state_gc_bits(uint64_t *bitmap, c4m_zrun_state_t *state)
{
    // Mark all.
    *bitmap = ~0;
}

void
c4m_add_module(c4m_zobject_file_t *obj, c4m_module_t *module)
{
    uint32_t existing_len = (uint32_t)c4m_list_len(obj->module_contents);
    module->module_id     = existing_len;

    c4m_list_append(obj->module_contents, module);
}

static inline void
c4m_vm_setup_ffi(c4m_vm_t *vm)
{
    vm->obj->ffi_info_entries = c4m_list_len(vm->obj->ffi_info);

    if (vm->obj->ffi_info_entries == 0) {
        return;
    }

    for (int i = 0; i < vm->obj->ffi_info_entries; i++) {
        c4m_ffi_decl_t *ffi_info = c4m_list_get(vm->obj->ffi_info, i, NULL);
        c4m_zffi_cif   *cif      = &ffi_info->cif;

        cif->fptr = c4m_ffi_find_symbol(ffi_info->external_name,
                                        ffi_info->dll_list);

        if (!cif->fptr) {
            // TODO: warn. For now, just error if it gets called.
            continue;
        }

        int            n       = ffi_info->num_ext_params;
        c4m_ffi_type **arglist = c4m_gc_array_alloc(c4m_ffi_type *, n);

        if (n < 0) {
            n = 0;
        }
        for (int j = 0; j < n; j++) {
            uint8_t param = ffi_info->external_params[j];
            arglist[j]    = c4m_ffi_arg_type_map(param);

            if (param == C4M_CSTR_CTYPE_CONST && j < 63) {
                cif->str_convert |= (1UL << j);
            }
        }

        if (ffi_info->external_return_type == C4M_CSTR_CTYPE_CONST) {
            cif->str_convert |= (1UL << 63);
        }

        ffi_prep_cif(&cif->cif,
                     C4M_FFI_DEFAULT_ABI,
                     n,
                     c4m_ffi_arg_type_map(ffi_info->external_return_type),
                     arglist);
    }
}

void
c4m_setup_new_module_allocations(c4m_compile_ctx *cctx, c4m_vm_t *vm)
{
    // This only needs to be called to add space for new modules.

    int         old_modules = 0;
    int         new_modules = c4m_list_len(vm->obj->module_contents);
    c4m_obj_t **new_allocs;

    if (vm->module_allocations) {
        while (vm->module_allocations[old_modules++])
            ;
    }

    if (old_modules == new_modules) {
        return;
    }

    // New modules generally come with new static data.
    new_allocs = c4m_gc_array_alloc(c4m_obj_t, new_modules + 1);

    for (int i = 0; i < old_modules; i++) {
        new_allocs[i] = vm->module_allocations[i];
    }

    for (int i = old_modules; i < new_modules; i++) {
        c4m_module_t *m = c4m_list_get(vm->obj->module_contents,
                                       i,
                                       NULL);

        new_allocs[i] = c4m_gc_array_alloc(c4m_obj_t, m->static_size + 8);
    }

    vm->module_allocations = new_allocs;
}

c4m_buf_t *
c4m_vm_save(c4m_vm_t *vm)
{
    // Any future runs should reset to this point, if it's our first
    // run, and the original entry point != the current one.
    //
    // If it *is* current, the stash happens at the start of
    // execution.
    //
    // This VM could be saved without running, or saved after a run.
    // If it's saved without running, num_saved_runs is 0, and we
    // don't have to cache the original spec/attr state. If the number
    // is 1 however, we just finished our first run, and we DO want to
    // cache these things, plus we need to change our entry point.

    if (vm->num_saved_runs == 1) {
        vm->obj->ccache[C4M_CCACHE_ORIG_SECTIONS] = vm->all_sections;
        vm->obj->ccache[C4M_CCACHE_ORIG_ATTR]     = vm->attrs;
        vm->entry_point                           = vm->obj->default_entry;
    }

    vm->run_state = NULL;
    c4m_vm_remove_compile_time_data(vm);

    return c4m_automarshal(vm);
}

void
c4m_vm_global_run_state_init(c4m_vm_t *vm)
{
    // Global, meaning for all threads. But this is one time execution
    // state setup.

    vm->run_state = c4m_gc_alloc_mapped(c4m_zrun_state_t,
                                        c4m_zrun_state_gc_bits);
    c4m_vm_setup_ffi(vm);

    vm->last_saved_run_time = *c4m_now();

    // Go ahead and count it as saved at the beginning; we don't really
    // use this during the run, and if it doesn't get saved, then it'll
    // automatically reset.
    if (!vm->num_saved_runs++) {
        vm->first_saved_run_time = vm->last_saved_run_time;
    }

#ifdef C4M_DEV
    vm->run_state->print_buf    = c4m_buffer_empty();
    vm->run_state->print_stream = c4m_buffer_outstream(vm->run_state->print_buf,
                                                       false);
#endif
}

void
c4m_vm_gc_bits(uint64_t *bitmap, c4m_vm_t *vm)
{
    c4m_mark_raw_to_addr(bitmap, vm, &vm->all_sections);
}

c4m_vm_t *
c4m_vm_new(c4m_compile_ctx *cctx)
{
    c4m_vm_t *vm = c4m_gc_alloc_mapped(c4m_vm_t, c4m_vm_gc_bits);

    // c4m_objfile_gc_bits);
    vm->obj = c4m_gc_alloc_mapped(c4m_zobject_file_t, C4M_GC_SCAN_ALL);

    vm->obj->con4m_version   = c4m_current_version;
    vm->obj->module_contents = c4m_list(c4m_type_ref());
    vm->obj->func_info       = c4m_list(c4m_type_ref());
    vm->obj->ffi_info        = c4m_list(c4m_type_ref());

    if (cctx->final_spec && cctx->final_spec->in_use) {
        vm->obj->attr_spec = cctx->final_spec;
    }

    vm->attrs         = c4m_dict(c4m_type_utf8(), c4m_type_ref());
    vm->all_sections  = c4m_new(c4m_type_set(c4m_type_utf8()));
    vm->creation_time = *c4m_now();

    return vm;
}

void
c4m_vm_reset(c4m_vm_t *vm)
{
    void **cache = (void **)&vm->obj->ccache;

    vm->module_allocations        = NULL;
    vm->num_saved_runs            = 1; // Restore point is always 1.
    cache[C4M_CCACHE_CUR_SCONSTS] = c4m_copy(cache[C4M_CCACHE_ORIG_SCONSTS]);
    cache[C4M_CCACHE_CUR_OCONSTS] = c4m_copy(cache[C4M_CCACHE_ORIG_OCONSTS]);
    cache[C4M_CCACHE_CUR_VCONSTS] = c4m_copy(cache[C4M_CCACHE_ORIG_VCONSTS]);
    cache[C4M_CCACHE_CUR_ASCOPE]  = c4m_copy(cache[C4M_CCACHE_ORIG_ASCOPE]);
    cache[C4M_CCACHE_CUR_GSCOPE]  = c4m_copy(cache[C4M_CCACHE_ORIG_ASCOPE]);
    cache[C4M_CCACHE_CUR_MODULES] = c4m_copy(cache[C4M_CCACHE_ORIG_MODULES]);
    cache[C4M_CCACHE_CUR_TYPES]   = c4m_copy(cache[C4M_CCACHE_ORIG_TYPES]);
    vm->obj->module_contents      = c4m_copy(cache[C4M_CCACHE_ORIG_SORT]);
    vm->obj->attr_spec            = c4m_copy(cache[C4M_CCACHE_ORIG_SPEC]);
    vm->attrs                     = c4m_copy(cache[C4M_CCACHE_ORIG_ATTR]);
    vm->all_sections              = c4m_copy(cache[C4M_CCACHE_ORIG_SECTIONS]);
    vm->obj->static_contents      = c4m_copy(cache[C4M_CCACHE_ORIG_STATIC]);
    vm->entry_point               = vm->obj->default_entry;
}

void
c4m_vmthread_gc_bits(uint64_t *bitmap, c4m_vmthread_t *t)
{
    uint64_t diff = c4m_ptr_diff(t, &t->r3);
    for (unsigned int i = 0; i < diff; i++) {
        c4m_set_bit(bitmap, i);
    }
}

c4m_vmthread_t *
c4m_vmthread_new(c4m_vm_t *vm)
{
    if (vm->run_state == NULL) {
        // First thread only.
        c4m_vm_global_run_state_init(vm);
    }

    c4m_vmthread_t *tstate = c4m_gc_alloc_mapped(c4m_vmthread_t,
                                                 c4m_vmthread_gc_bits);
    tstate->vm             = vm;

    c4m_vmthread_reset(tstate);

    // tstate->thread_arena = arena;

    // c4m_internal_unstash_heap();
    return tstate;
}

void
c4m_vmthread_reset(c4m_vmthread_t *tstate)
{
    tstate->sp         = &tstate->stack[C4M_STACK_SIZE];
    tstate->fp         = tstate->sp;
    tstate->pc         = 0;
    tstate->num_frames = 1;
    tstate->r0         = NULL;
    tstate->r1         = NULL;
    tstate->r2         = NULL;
    tstate->r3         = NULL;
    tstate->running    = false;
    tstate->error      = false;

    tstate->current_module = c4m_list_get(tstate->vm->obj->module_contents,
                                          tstate->vm->entry_point,
                                          NULL);
}

void
c4m_vm_remove_compile_time_data(c4m_vm_t *vm)
{
    int n = c4m_list_len(vm->obj->module_contents);

    for (int i = 0; i < n; i++) {
        c4m_module_t *m         = c4m_list_get(vm->obj->module_contents,
                                       i,
                                       NULL);
        m->ct                   = NULL;
        m->module_scope->parent = NULL;
        uint64_t nsyms;
        void   **syms = hatrack_dict_values(m->module_scope->symbols, &nsyms);

        for (uint64_t j = 0; j < nsyms; j++) {
            c4m_symbol_t *sym = syms[j];
            sym->ct           = NULL;

            if (sym->kind == C4M_SK_FUNC) {
                c4m_fn_decl_t *fn      = sym->value;
                c4m_scope_t   *fnscope = fn->signature_info->fn_scope;
                uint64_t       nsub;
                void         **sub_syms = hatrack_dict_values(fnscope->symbols,
                                                      &nsub);

                for (uint64_t k = 0; k < nsub; k++) {
                    c4m_symbol_t *sub = sub_syms[k];
                    sub->ct           = NULL;
                }

                fnscope  = fn->signature_info->formals;
                sub_syms = hatrack_dict_values(fnscope->symbols, &nsub);

                for (uint64_t k = 0; k < nsub; k++) {
                    c4m_symbol_t *sub = sub_syms[k];
                    sub->ct           = NULL;
                }

                fn->cfg = NULL; // TODO: move to a ct object
            }
        }
    }
}

const c4m_vtable_t c4m_vm_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_GC_MAP] = (c4m_vtable_entry)c4m_vm_gc_bits,
    },
};

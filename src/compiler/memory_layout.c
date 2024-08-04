#define C4M_USE_INTERNAL_API
#include "con4m.h"

static uint64_t
store_static_item(c4m_compile_ctx *ctx, void *value)
{
    c4m_static_memory *mem  = ctx->memory_layout;
    c4m_mem_ptr        item = (c4m_mem_ptr){.v = value};
    uint64_t           result;

    if (mem->num_items == mem->alloc_len) {
        c4m_mem_ptr *items;

        items = c4m_gc_array_alloc_mapped(c4m_mem_ptr,
                                          mem->num_items + getpagesize() / 8,
                                          c4m_smem_gc_bits);
        if (mem->num_items) {
            memcpy(items, mem->items, mem->num_items * 8);
        }

        mem->alloc_len += getpagesize();
        mem->items = items;
    }

    result             = mem->num_items++;
    mem->items[result] = item;

    return result;
}

// Note that we do per-module offsets for most static data so that
// modules are easily relocatable, and so that we can eventually cache
// the info without having to recompile modules that don't need it.
//
// We'll even potentially be able to hot-reload modules that don't
// change their APIs or the static layout of existing variables.
//
// Global variables get a storage slot in the heap of the module that
// defines them.  That's why, if we see one that is linked to another
// symbol, we skip it. The actual static pointers are supposed to be
// 100% constant as far as the runtime is concerned.

uint64_t
c4m_add_static_object(void *obj, c4m_compile_ctx *ctx)
{
    c4m_type_t *t = c4m_get_my_type(obj);

    if (c4m_type_is_string(t)) {
        return c4m_add_static_string(obj, ctx);
    }

    bool found = false;

    uint64_t res = (uint64_t)hatrack_dict_get(ctx->obj_consts, obj, &found);

    if (found) {
        return res;
    }

    res = store_static_item(ctx, obj);
    hatrack_dict_put(ctx->obj_consts, obj, (void *)res);

    return res;
}

uint64_t
c4m_add_static_string(c4m_str_t *s, c4m_compile_ctx *ctx)
{
    // Strings that have style info are currently NOT cached by hash,
    // because the string hashing algorithm ignores the style info.
    // So if there's style info, we store it by pointer.
    //
    // TODO to change the hashing algorithm; we should take this into
    // account for sure.

    bool     found = false;
    uint64_t res;

    s = c4m_to_utf8(s);

    if (s->styling && s->styling->num_entries != 0) {
        res = (uint64_t)hatrack_dict_get(ctx->obj_consts, s, &found);
        if (found) {
            return res;
        }
        res = store_static_item(ctx, s);
        hatrack_dict_put(ctx->obj_consts, s, (void *)res);

        return res;
    }

    res = (uint64_t)hatrack_dict_get(ctx->str_consts, s, &found);

    if (found) {
        return res;
    }

    res = store_static_item(ctx, s);
    hatrack_dict_put(ctx->obj_consts, s, (void *)res);

    return res;
}

// This is meant for constant values associated with symbols.
uint64_t
c4m_add_value_const(uint64_t val, c4m_compile_ctx *ctx)
{
    bool     found = false;
    uint64_t res   = (uint64_t)hatrack_dict_get(ctx->value_consts,
                                              (void *)val,
                                              &found);

    if (found) {
        return res;
    }

    res = store_static_item(ctx, (void *)val);
    hatrack_dict_put(ctx->value_consts, (void *)val, (void *)res);

    return res;
}

static inline uint64_t
c4m_layout_static_obj(c4m_module_compile_ctx *ctx, int bytes, int alignment)
{
    uint64_t result = c4m_round_up_to_given_power_of_2(alignment,
                                                       ctx->static_size);

    ctx->static_size = result + bytes;

    return result;
}

uint32_t
_c4m_layout_const_obj(c4m_compile_ctx *cctx, c4m_obj_t obj, ...)
{
    va_list args;

    va_start(args, obj);

    c4m_module_compile_ctx *fctx = va_arg(args, c4m_module_compile_ctx *);
    c4m_tree_node_t        *loc  = NULL;
    c4m_utf8_t             *name = NULL;

    if (fctx != NULL) {
        loc  = va_arg(args, c4m_tree_node_t *);
        name = va_arg(args, c4m_utf8_t *);
    }

    va_end(args);

    // Sym is only needed if there's an error, which there shouldn't
    // be when used internally during codegen. In those cases sym wil
    // be NULL, which should be totally fine.
    if (!obj) {
        c4m_add_error(fctx, c4m_err_const_not_provided, loc, name);
        return 0;
    }

    c4m_type_t *objtype = c4m_get_my_type(obj);

    if (c4m_type_is_box(objtype)) {
        return c4m_add_value_const(c4m_unbox(obj), cctx);
    }

    return c4m_add_static_object(obj, cctx);
}

static void
layout_static(c4m_compile_ctx        *cctx,
              c4m_module_compile_ctx *fctx,
              void                  **view,
              uint64_t                n)
{
    for (unsigned int i = 0; i < n; i++) {
        c4m_symbol_t *my_sym_copy = view[i];
        c4m_symbol_t *sym         = my_sym_copy;

        if (sym->linked_symbol != NULL) {
            // Only allocate space for globals in the module where
            // we're most dependent.
            continue;
        }
        // We go ahead and add this to all symbols, but it's only
        // used for static allocations of non-const variables.
        sym->local_module_id = fctx->local_module_id;

        switch (sym->kind) {
        case C4M_SK_ENUM_VAL:
            if (c4m_types_are_compat(sym->type, c4m_type_utf8(), NULL)) {
                sym->static_offset = c4m_layout_const_obj(cctx,
                                                          sym->value,
                                                          fctx,
                                                          sym->declaration_node,
                                                          sym->name);
            }
            break;
        case C4M_SK_VARIABLE:
            // We might someday allow references to const vars, so go
            // ahead and stick them in static data always.
            if (c4m_sym_is_declared_const(sym)) {
                sym->static_offset = c4m_layout_const_obj(cctx,
                                                          sym->value,
                                                          fctx,
                                                          sym->declaration_node,
                                                          sym->name);
                break;
            }
            else {
                // For now, just lay everything in the world out as
                // 8 byte values (and thus 8 byte aligned).
                sym->static_offset = c4m_layout_static_obj(fctx, 8, 8);
                break;
            }
        default:
            continue;
        }
        my_sym_copy->static_offset = sym->static_offset;
    }
}

// This one measures in stack value slots, not in bytes.
static int64_t
layout_stack(void **view, uint64_t n)
{
    // Address 0 is always $result, if it exists.
    int32_t next_formal = -2;
    int32_t next_local  = 1;

    while (n--) {
        c4m_symbol_t *sym = view[n];

        // Will already be zero-allocated.
        if (!strcmp(sym->name->data, "$result")) {
            continue;
        }

        switch (sym->kind) {
        case C4M_SK_VARIABLE:
            sym->static_offset = next_local;
            next_local += 1;
            continue;
        case C4M_SK_FORMAL:
            sym->static_offset = next_formal;
            next_formal -= 1;
            continue;
        default:
            continue;
        }
    }

    return next_local;
}

static void
layout_func(c4m_module_compile_ctx *ctx,
            c4m_symbol_t           *sym,
            int                     i)
{
    uint64_t       n;
    c4m_fn_decl_t *decl       = sym->value;
    c4m_scope_t   *scope      = decl->signature_info->fn_scope;
    void         **view       = hatrack_dict_values_sort(scope->symbols, &n);
    int            frame_size = layout_stack(view, n);

    decl->frame_size = frame_size;

    if (decl->once) {
        decl->sc_bool_offset = c4m_layout_static_obj(ctx,
                                                     sizeof(bool),
                                                     8);
        decl->sc_lock_offset = c4m_layout_static_obj(ctx,
                                                     sizeof(pthread_mutex_t),
                                                     8);
        decl->sc_memo_offset = c4m_layout_static_obj(ctx,
                                                     sizeof(void *),
                                                     8);
    }
}

void
c4m_layout_module_symbols(c4m_compile_ctx *cctx, c4m_module_compile_ctx *fctx)
{
    uint64_t n;

    // Very first item in every module will be information about whether
    // there are parameters, and if they are set.
    void **view = hatrack_dict_values_sort(fctx->parameters, &n);
    int    pix  = 0;

    for (unsigned int i = 0; i < n; i++) {
        c4m_module_param_info_t *param = view[i];
        c4m_symbol_t            *sym   = param->linked_symbol;

        // These don't need an index; we test for the default by
        // asking the attr store.
        if (sym->kind == C4M_SK_ATTR) {
            continue;
        }

        param->param_index = pix++;
    }

    // We keep one bit per parameter, and the length is measured
    // in bytes, so we divide by 8, after adding 7 to make sure we
    // round up. We don't need the result; it's always at offset
    // 0, but it controls where the next variable is stored.
    c4m_layout_static_obj(fctx, (pix + 7) / 8, 8);

    view = hatrack_dict_values_sort(fctx->global_scope->symbols, &n);
    layout_static(cctx, fctx, view, n);

    view = hatrack_dict_values_sort(fctx->module_scope->symbols, &n);
    layout_static(cctx, fctx, view, n);

    n = c4m_list_len(fctx->fn_def_syms);

    for (unsigned int i = 0; i < n; i++) {
        c4m_symbol_t *sym = c4m_list_get(fctx->fn_def_syms, i, NULL);
        layout_func(fctx, sym, i);
    }
}

#define C4M_USE_INTERNAL_API
#include "con4m.h"

// Note that we do per-module offsets for most static data so that
// modules are easily relocatable, and so that we can eventually cache
// the info without having to recompile modules that don't need it.
//
// We'll even potentially be able to hot-reload modules that don't
// change their APIs or the static layout of existing variables.
//
// Global variables go into the heap of the module that defines them.
// That's why, if we see one that is linked to another symbol, we skip
// it.
//
// Currently, we do NOT do the same thing for constant data; they all
// go in one place, so that we can easily marshal and unmarshal
// interdependent bits at compile time (or dynamically if we keep our
// marshal state around).
//
// When we eventually get to caching modules, we'll have to deal with
// making such things relocatable, but that's a problem for another
// time.

static inline uint64_t
c4m_layout_static_obj(c4m_file_compile_ctx *ctx, int bytes, int alignment)
{
    uint64_t result = c4m_round_up_to_given_power_of_2(alignment,
                                                       ctx->static_size);

    ctx->static_size = result + bytes;

    return result;
}

int64_t
c4m_layout_string_const(c4m_compile_ctx *cctx,
                        c4m_str_t       *s)
{
    int64_t instance_id;
    bool    found;

    instance_id = (int64_t)hatrack_dict_get(cctx->str_map, s, &found);

    if (found == false) {
        c4m_sub_marshal(s,
                        cctx->const_stream,
                        cctx->const_memos,
                        &cctx->const_memoid);
        instance_id = cctx->const_instantiation_id++;

        c4m_marshal_i32((int32_t)instance_id, cctx->const_stream);
        hatrack_dict_put(cctx->instance_map, s, (void *)instance_id);
        hatrack_dict_put(cctx->str_map, s, (void *)instance_id);
    }

    return instance_id * 8;
}

static void
c4m_layout_const_obj(c4m_compile_ctx *cctx, c4m_scope_entry_t *sym)
{
    bool        found;
    int64_t     instance_id;
    c4m_utf8_t *s;

    if (c4m_type_is_value_type(sym->type)) {
        sym->static_offset = c4m_stream_get_location(cctx->const_stream);
        c4m_marshal_i64((int64_t)sym->value, cctx->const_stream);
        return;
    }

    switch (sym->type->typeid) {
        // Since the memo hash is by pointer, and we want to cache
        // strings by value, we add a second string cache. Currently,
        // we limit this to strings with no styling information; we'll
        // have to change the string object hash to include the syling
        // info to be able to fully memoize this. But it should get
        // all of the small utility strings!

    case C4M_T_UTF8:
    case C4M_T_UTF32:

        s = sym->value;

        if (!s->styling || s->styling->num_entries == 0) {
            sym->static_offset = c4m_layout_string_const(cctx, s);
            return;
        }
    default:
        break;
    }

    hatrack_dict_get(cctx->const_memos, sym->value, &found);

    if (found) {
        instance_id        = (int64_t)hatrack_dict_get(cctx->instance_map,
                                                sym->value,
                                                NULL);
        sym->static_offset = (8 * (int32_t)instance_id);
        return;
    }
    // It's not cached.
    c4m_sub_marshal(sym->value,
                    cctx->const_stream,
                    cctx->const_memos,
                    &cctx->const_memoid);

    instance_id        = cctx->const_instantiation_id++;
    sym->static_offset = (int32_t)(8 * instance_id);

    c4m_marshal_i32((int32_t)instance_id, cctx->const_stream);

    hatrack_dict_put(cctx->instance_map, sym->value, (void *)instance_id);
}

static void
layout_static(c4m_compile_ctx      *cctx,
              c4m_file_compile_ctx *fctx,
              void                **view,
              uint64_t              n)
{
    for (unsigned int i = 0; i < n; i++) {
        c4m_scope_entry_t *my_sym_copy = view[i];
        c4m_scope_entry_t *sym         = my_sym_copy;

        if (sym->linked_symbol != NULL) {
            // Only allocate space for globals in the module where
            // we're most dependent.
            continue;
        }
        // We go ahead and add this to all symbols, but it's only
        // used for static allocations of non-const variables.
        sym->local_module_id = fctx->local_module_id;

        switch (sym->kind) {
        case sk_enum_val:
            if (c4m_tspecs_are_compat(sym->type, c4m_tspec_utf8())) {
                c4m_layout_const_obj(cctx, sym);
            }
            break;
        case sk_variable:
            // We might someday allow references to consts vars, so go
            // ahead and stick them in static data always.
            if (sym->flags & C4M_F_DECLARED_CONST) {
                c4m_layout_const_obj(cctx, sym);
                break;
            }
            // For now, just lay everything in the world out as
            // 8 byte values (and thus 8 byte aligned).
            sym->static_offset = c4m_layout_static_obj(fctx, 8, 8);
            break;
        default:
            continue;
        }
        my_sym_copy->static_offset = sym->static_offset;
    }
}

// This one measures in stack value slots, not in bytes.
static int
layout_stack(void **view, uint64_t n)
{
    // Address 0 is always $result, if it exists.
    int next_formal = -1;
    int next_local  = 1;

    for (unsigned int i = 0; i < n; i++) {
        c4m_scope_entry_t *sym = view[i];

        // Will already be zero-allocated.
        if (!strcmp(sym->name->data, "$result")) {
            continue;
        }

        switch (sym->kind) {
        case sk_variable:
            sym->static_offset = next_local;
            next_local += 1;
            continue;
        case sk_formal:
            sym->static_offset = next_formal;
            next_local -= 1;
            continue;
        default:
            continue;
        }
    }

    return next_local;
}

static void
layout_func(c4m_file_compile_ctx *ctx,
            c4m_scope_entry_t    *sym,
            int                   i)

{
    uint64_t       n;
    c4m_fn_decl_t *decl       = sym->value;
    c4m_scope_t   *scope      = decl->signature_info->fn_scope;
    void         **view       = hatrack_dict_values_sort(scope->symbols, &n);
    int            end_offset = layout_stack(view, n);

    decl->frame_size = end_offset;

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
c4m_layout_module_symbols(c4m_compile_ctx *cctx, c4m_file_compile_ctx *fctx)
{
    uint64_t n;

    layout_static(cctx,
                  fctx,
                  hatrack_dict_values_sort(fctx->global_scope->symbols, &n),
                  n);
    layout_static(cctx,
                  fctx,
                  hatrack_dict_values_sort(fctx->module_scope->symbols, &n),
                  n);

    n = c4m_xlist_len(fctx->fn_def_syms);

    for (unsigned int i = 0; i < n; i++) {
        layout_func(fctx, c4m_xlist_get(fctx->fn_def_syms, i, NULL), i);
    }
}

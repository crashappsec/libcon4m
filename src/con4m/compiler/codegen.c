#define C4M_USE_INTERNAL_API
#include "con4m.h"

typedef struct {
    c4m_compile_ctx      *cctx;
    c4m_file_compile_ctx *fctx;
    c4m_xlist_t          *instructions;
    c4m_tree_node_t      *cur_node;
    c4m_zmodule_info_t   *cur_module;
    int                   instruction_counter;
} gen_ctx;

static void
_emit(gen_ctx *ctx, int32_t op32, ...)
{
    c4m_karg_only_init(op32);

    c4m_zop_t   op        = (c4m_zop_t)(op32);
    int64_t     arg       = 0;
    int64_t     immediate = 0;
    int64_t     module_id = ctx->fctx->local_module_id;
    c4m_type_t *type      = NULL;

    c4m_kw_int64("immediate", immediate);
    c4m_kw_int64("arg", arg);
    c4m_kw_int64("module_id", module_id);
    c4m_kw_ptr("type", type);

    c4m_zinstruction_t *instr = c4m_gc_alloc(c4m_zinstruction_t);
    instr->op                 = op;
    instr->module_id          = (int16_t)module_id;
    instr->line_no            = c4m_node_get_line_number(ctx->cur_node);
    instr->arg                = (int64_t)arg;
    instr->immediate          = immediate;
    instr->type_info          = type;

    c4m_xlist_append(ctx->instructions, instr);
    ctx->instruction_counter++;
}

#define emit(ctx, op, ...) _emit(ctx, (int32_t)op, KFUNC(__VA_ARGS__))

static inline int
calculate_offset(gen_ctx *ctx, int target)
{
    return ctx->instruction_counter - target;
}

// Returns true if the jump is fully emitted, false if a patch is required,
// in which case the caller is responsible for tracking the patch.
static inline bool
gen_jump_raw(gen_ctx         *ctx,
             c4m_jump_info_t *jmp_info,
             c4m_zop_t        opcode,
             bool             pop)
{
    bool    result = true;
    int32_t arg    = 0;

    if (!pop && opcode != C4M_ZJ) { // No builtin pop for C4M_ZJ.
        emit(ctx, C4M_ZDupTop);
    }

    jmp_info->code_offset = ctx->instruction_counter;

    // Look to see if we can fill in the jump target now.  If 'top' is
    // set, we are definitely jumping backwards, and can patch.
    if (jmp_info->top) {
        arg    = calculate_offset(ctx, jmp_info->target_info->entry_ip);
        result = false;
    }
    else {
        if (jmp_info->target_info->exit_ip != 0) {
            arg    = calculate_offset(ctx, jmp_info->target_info->exit_ip);
            result = false;
        }
    }

    emit(ctx, opcode, c4m_kw("arg", c4m_ka(arg)));

    return result;
}

static inline bool
gen_jz(gen_ctx *ctx, c4m_jump_info_t *jmp_info, bool pop)
{
    return gen_jump_raw(ctx, jmp_info, C4M_ZJz, pop);
}

static inline bool
gen_jnz(gen_ctx *ctx, c4m_jump_info_t *jmp_info, bool pop)
{
    return gen_jump_raw(ctx, jmp_info, C4M_ZJnz, pop);
}

static inline bool
gen_j(gen_ctx *ctx, c4m_jump_info_t *jmp_info)
{
    return gen_jump_raw(ctx, jmp_info, C4M_ZJ, false);
}

static inline void
gen_finish_jump(gen_ctx *ctx, c4m_jump_info_t *jmp_info)
{
    int32_t             arg   = calculate_offset(ctx, jmp_info->code_offset);
    c4m_zinstruction_t *instr = c4m_xlist_get(ctx->instructions,
                                              jmp_info->code_offset,
                                              NULL);
    instr->arg                = arg;
}

// Helpers for the common case where we have one exit only.
#define GEN_JNZ(user_code)                                            \
    do {                                                              \
        c4m_jump_info_t *ji          = c4m_gc_alloc(c4m_jump_info_t); \
        bool             needs_patch = gen_jnz(ctx, ji, true);        \
        user_code;                                                    \
        if (needs_patch) {                                            \
            gen_finish_jump(ctx, ji);                                 \
        }                                                             \
    } while (0)

#define GEN_JZ(user_code)                                             \
    do {                                                              \
        c4m_jump_info_t *ji          = c4m_gc_alloc(c4m_jump_info_t); \
        bool             needs_patch = gen_jz(ctx, ji, true);         \
        user_code;                                                    \
        if (needs_patch) {                                            \
            gen_finish_jump(ctx, ji);                                 \
        }                                                             \
    } while (0)

#define GEN_J(user_code)                                              \
    do {                                                              \
        c4m_jump_info_t *ji          = c4m_gc_alloc(c4m_jump_info_t); \
        bool             needs_patch = gen_j(ctx, ji);                \
        user_code;                                                    \
        if (needs_patch) {                                            \
            gen_finish_jump(ctx, ji);                                 \
        }                                                             \
    } while (0)

// TODO: when we add refs, these should change the types written to
// indicate the item on the stack is a ref to a particular type.
//
// For now, they just indicate tspec_ref().

static inline void
gen_sym_load_const(gen_ctx *ctx, c4m_scope_entry_t *sym, bool addressof)
{
    emit(ctx,
         addressof ? C4M_ZPushConstRef : C4M_ZPushConstObj,
         c4m_kw("arg", c4m_ka(sym->static_offset)));
}

static inline void
gen_sym_load_attr(gen_ctx *ctx, c4m_scope_entry_t *sym, bool addressof)
{
    int64_t offset = c4m_layout_string_const(ctx->cctx, sym->name);
    emit(ctx, C4M_ZPushConstObj, c4m_kw("arg", c4m_kw(offset)));
    emit(ctx, C4M_ZLoadFromAttr, c4m_kw("arg", c4m_kw(addressof)));
}

// Right now we only ever generate the version that returns the rhs
// not the attr storage address.
static inline void
gen_sym_load_attr_and_found(gen_ctx *ctx, c4m_scope_entry_t *sym, bool skipload)
{
    int64_t flag   = skipload ? C4M_F_ATTR_SKIP_LOAD : C4M_F_ATTR_PUSH_FOUND;
    int64_t offset = c4m_layout_string_const(ctx->cctx, sym->name);

    emit(ctx, C4M_ZPushConstObj, c4m_kw("arg", c4m_kw(offset)));
    emit(ctx, C4M_ZLoadFromAttr, c4m_kw("immediate", c4m_ka(flag)));
}

static inline void
gen_sym_load_stack(gen_ctx *ctx, c4m_scope_entry_t *sym, bool addressof)
{
    if (addressof) {
        C4M_CRAISE("Invalid to ever store a ref that points onto the stack.");
    }
    // This is measured in stack value slots.
    emit(ctx, C4M_ZPushLocalObj, c4m_kw("arg", c4m_kw(sym->static_offset)));
}

static inline void
gen_sym_load_static(gen_ctx *ctx, c4m_scope_entry_t *sym, bool addressof)
{
    // This is measured in stack value slots.
    emit(ctx, C4M_ZPushStaticObj, c4m_kw("arg", c4m_kw(sym->static_offset)));
}

// Load from the storage location referred to by the symbol,
// pushing onto onto the stack.
//
// Or, optionally, push the address of the symbol.
static void
gen_sym_load(gen_ctx *ctx, c4m_scope_entry_t *sym, bool addressof)
{
    switch (sym->kind) {
    case sk_enum_val:
        gen_sym_load_const(ctx, sym, addressof);
        return;
    case sk_attr:
        gen_sym_load_attr(ctx, sym, addressof);
        return;
    case sk_variable:
        switch (sym->flags & (C4M_F_DECLARED_CONST | C4M_F_USER_IMMUTIBLE)) {
        case C4M_F_DECLARED_CONST:
            gen_sym_load_const(ctx, sym, addressof);
            return;

            // If it's got the user-immutible flag set it's a loop-related
            // automatic variable like $i or $last.
        case C4M_F_USER_IMMUTIBLE:
            gen_sym_load_stack(ctx, sym, addressof);
            return;

        default:
            // Regular variables in a function are stack allocated.
            // Regular variables in a module are statically allocated.
            if (sym->flags & C4M_F_FUNCTION_SCOPE) {
                gen_sym_load_stack(ctx, sym, addressof);
            }
            else {
                gen_sym_load_static(ctx, sym, addressof);
            }
            return;
        }

    case sk_formal:
        gen_sym_load_stack(ctx, sym, addressof);
        return;
    default:
        c4m_unreachable();
    }
}

// Couldn't resist the variable name. For variables, if true, the flag
// pops the value stored off the stack (that always happens for attributes),
// and for attributes, the boolean locks the attribute.

static void
gen_sym_store(gen_ctx *ctx, c4m_scope_entry_t *sym, bool pop_and_lock)
{
    int64_t arg;
    switch (sym->kind) {
    case sk_attr:
        // Byte offset into the const object arena where the attribute
        // name can be found.
        arg = c4m_layout_string_const(ctx->cctx, sym->name);
        emit(ctx, C4M_ZPushConstObj, c4m_kw("arg", c4m_ka(arg)));
        emit(ctx, C4M_ZAssignAttr, c4m_kw("arg", c4m_ka(pop_and_lock)));
        return;
    case sk_variable:
    case sk_formal:
        if (!pop_and_lock) {
            emit(ctx, C4M_ZDupTop);
        }

        gen_sym_load(ctx, sym, true);
        emit(ctx, C4M_ZAssignToLoc);
        return;
    default:
        c4m_unreachable();
    }
}

static inline void
gen_load_string(gen_ctx *ctx, c4m_utf8_t *s)
{
    int64_t offset = c4m_layout_string_const(ctx->cctx, s);
    emit(ctx, C4M_ZPushStaticObj, c4m_kw("arg", c4m_ka(offset)));
}

static void
gen_bail(gen_ctx *ctx, c4m_utf8_t *s)
{
    gen_load_string(ctx, s);
    emit(ctx, C4M_ZBail);
}

static void
gen_load_immediate(gen_ctx *ctx, int64_t value)
{
    emit(ctx, C4M_ZPushImm, c4m_kw("immediate", c4m_ka(value)));
}

static void
gen_label(gen_ctx *ctx, c4m_utf8_t *s)
{
    int64_t offset = c4m_layout_string_const(ctx->cctx, s);
    emit(ctx, C4M_ZNop, c4m_kw("arg", c4m_ka(1), "immediate", c4m_ka(offset)));
}

static void
gen_tcall(gen_ctx *ctx, c4m_builtin_type_fn arg)
{
    emit(ctx, C4M_ZTCall, c4m_kw("arg", c4m_ka(arg)));
}

static void
gen_node_down(gen_ctx *ctx)
{
}

static void
gen_function(gen_ctx *ctx, c4m_fn_decl_t *fn_decl)
{
}

static inline void
gen_test_param_flag(gen_ctx                 *ctx,
                    c4m_module_param_info_t *param,
                    c4m_scope_entry_t       *sym)
{
    uint64_t index = param->param_index / 64;
    uint64_t flag  = 1 << (param->param_index % 64);

    emit(ctx, C4M_ZPushStaticObj, c4m_kw("arg", c4m_ka(index)));
    gen_load_immediate(ctx, flag);
    emit(ctx, C4M_ZBAnd); // Will be non-zero if the param is set.
}

static inline void
gen_param_via_default_value_type(gen_ctx                 *ctx,
                                 c4m_module_param_info_t *param,
                                 c4m_scope_entry_t       *sym)
{
    uint32_t offset = c4m_layout_const_obj(ctx->cctx, param->default_value);
    GEN_JNZ(emit(ctx, C4M_ZPushConstObj, c4m_kw("arg", c4m_ka(offset)));
            gen_sym_store(ctx, sym, true));
}

static inline void
gen_param_via_default_ref_type(gen_ctx                 *ctx,
                               c4m_module_param_info_t *param,
                               c4m_scope_entry_t       *sym)
{
    uint32_t offset = c4m_layout_const_obj(ctx->cctx, param->default_value);
    GEN_JNZ(emit(ctx, C4M_ZPushConstObj, c4m_kw("arg", c4m_ka(offset)));
            gen_tcall(ctx, C4M_BI_COPY);
            gen_sym_store(ctx, sym, true));
}

static inline void
gen_run_callback(gen_ctx *ctx, c4m_callback_t *cb)
{
    uint32_t    offset   = c4m_layout_const_obj(ctx->cctx, cb);
    c4m_type_t *t        = cb->info->type;
    int         nargs    = c4m_tspec_get_num_params(t) - 1;
    c4m_type_t *ret_type = c4m_tspec_get_param(t, nargs);
    bool        useret   = !(c4m_tspecs_are_compat(ret_type,
                                          c4m_tspec_void()));
    int         imm      = useret ? 1 : 0;

    emit(ctx, C4M_ZPushConstObj, c4m_kw("arg", c4m_ka(offset)));
    emit(ctx,
         C4M_ZRunCallback,
         c4m_kw("arg", c4m_ka(nargs), "immediate", c4m_ka(imm)));

    if (nargs) {
        emit(ctx, C4M_ZMoveSp, c4m_kw("arg", c4m_ka(-1 * nargs)));
    }

    if (useret) {
        emit(ctx, C4M_ZPushRes);
    }
}

static inline void
gen_param_via_callback(gen_ctx                 *ctx,
                       c4m_module_param_info_t *param,
                       c4m_scope_entry_t       *sym)
{
    gen_run_callback(ctx, param->callback);
    // The third parameter gives 'false' for attrs (where the
    // parameter would cause the attribute to lock) and 'true' for
    // variables, which leads to the value being popped after stored
    // (which happens automatically w/ the attr).
    gen_sym_store(ctx, sym, sym->kind != sk_attr);
    // TODO: call the validator too.
}

static inline void
gen_param_bail_if_missing(gen_ctx *ctx, c4m_scope_entry_t *sym)
{
    C4M_STATIC_ASCII_STR(fmt, "Parameter {} wasn't set on entering module {}");
    // TODO; do the format at runtime.
    c4m_utf8_t *error_msg = c4m_str_format(fmt, sym->name, ctx->fctx->path);
    GEN_JNZ(gen_bail(ctx, error_msg));
}

static inline uint64_t
gen_parameter_checks(gen_ctx *ctx)
{
    uint64_t n;
    void   **view = hatrack_dict_values_sort(ctx->fctx->parameters, &n);

    for (unsigned int i = 0; i < n; i++) {
        c4m_module_param_info_t *param = view[i];
        c4m_scope_entry_t       *sym   = param->linked_symbol;

        if (sym->kind == sk_attr) {
            gen_sym_load_attr_and_found(ctx, sym, true);
        }
        else {
            gen_test_param_flag(ctx, param, sym);
        }

        // Now, there's always an item on the top of the stack that tells
        // us if this parameter is loaded. We have to test, and generate
        // the right code if it's not loaded.
        //
        // If there's a default value, value types directly load from a
        // marshal'd location in const-land; ref types load by copying the
        // const.
        if (param->have_default) {
            if (c4m_type_is_value_type(sym->type)) {
                gen_param_via_default_value_type(ctx, param, sym);
            }
            else {
                gen_param_via_default_ref_type(ctx, param, sym);
            }
        }
        else {
            // If there's no default, the callback allows us to generate
            // dynamically if needed.
            if (param->callback) {
                gen_param_via_callback(ctx, param, sym);
            }

            // If neither default nor callback is provided, if a param
            // isn't set, we simply bail.
            gen_param_bail_if_missing(ctx, sym);
        }
    }

    return n;
}

static inline void
gen_module_entry(gen_ctx *ctx)
{
    gen_label(ctx, c4m_cstr_format("Module '{}': ", ctx->fctx->path));

    int num_params = gen_parameter_checks(ctx);

    emit(ctx, C4M_ZModuleEnter, c4m_kw("arg", c4m_ka(num_params)));
}

static void
gen_module_code(gen_ctx *ctx)
{
    c4m_zmodule_info_t *module = c4m_gc_alloc(c4m_zmodule_info_t);
    c4m_pnode_t        *root;

    ctx->cur_module            = module;
    ctx->fctx->module_object   = module;
    ctx->cur_node              = ctx->fctx->parse_tree;
    module->instructions       = c4m_new(c4m_tspec_xlist(c4m_tspec_ref()));
    ctx->instructions          = module->instructions;
    module->module_id          = ctx->fctx->local_module_id;
    module->source             = ctx->fctx->raw;
    module->modname            = ctx->fctx->module;
    root                       = get_pnode(ctx->cur_node);
    module->shortdoc           = c4m_token_raw_content(root->short_doc);
    module->longdoc            = c4m_token_raw_content(root->long_doc);
    ctx->fctx->call_patch_locs = c4m_new(c4m_tspec_xlist(c4m_tspec_ref()));
    // Still to fill in to the zmodule object (need to reshuffle to align):
    // authority/path/provided_path/package/module_id
    // Remove key / location / ext / url.
    //
    // Also need to do a bit of work aorund sym_types, codesyms, datasyms
    // and parameters.

    gen_module_entry(ctx);
    gen_node_down(ctx);
    emit(ctx, C4M_ZModuleRet);

    module->init_size = ctx->instruction_counter * sizeof(c4m_zinstruction_t);

    module->module_var_size = ctx->fctx->static_size;

    c4m_xlist_t *symlist = ctx->fctx->fn_def_syms;
    int          n       = c4m_xlist_len(symlist);

    if (n) {
        gen_label(ctx, c4m_new_utf8("Functions: "));
    }

    for (int i = 0; i < n; i++) {
        c4m_scope_entry_t *sym  = c4m_xlist_get(symlist, i, NULL);
        c4m_fn_decl_t     *decl = sym->value;

        gen_function(ctx, decl);
    }
}

void
c4m_codegen(c4m_compile_ctx *cctx)
{
    gen_ctx ctx = {
        .cctx = cctx,
    };

    int n = c4m_xlist_len(cctx->module_ordering);

    for (int i = 0; i < n; i++) {
        ctx.fctx = c4m_xlist_get(cctx->module_ordering, i, NULL);

        if (ctx.fctx->status >= c4m_compile_status_generated_code) {
            return;
        }

        if (c4m_fatal_error_in_module(ctx.fctx)) {
            C4M_CRAISE("Cannot generate code for files with fatal errors.");
        }

        if (ctx.fctx->status >= c4m_compile_status_generated_code) {
            continue;
        }
        gen_module_code(&ctx);

        ctx.fctx->status = c4m_compile_status_generated_code;
    }
}

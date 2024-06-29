#define C4M_USE_INTERNAL_API
#include "con4m.h"

// This is used in nested if/else only.
typedef struct {
    c4m_jump_info_t *targets;
    int              next;
} target_info_t;

typedef enum {
    assign_to_mem_slot,
    assign_via_index_set_call,
    assign_via_slice_set_call,
} assign_type_t;

typedef struct {
    c4m_fn_decl_t      *decl;
    c4m_zinstruction_t *i;
} call_backpatch_info_t;

typedef struct {
    c4m_compile_ctx      *cctx;
    c4m_file_compile_ctx *fctx;
    c4m_list_t          *instructions;
    c4m_list_t          *module_functions;
    c4m_tree_node_t      *cur_node;
    c4m_pnode_t          *cur_pnode;
    c4m_zmodule_info_t   *cur_module;
    c4m_list_t          *call_backpatches;
    target_info_t        *target_info;
    int                   instruction_counter;
    int                   current_stack_offset;
    int                   max_stack_size;
    bool                  lvalue;
    assign_type_t         assign_method;
    c4m_symbol_t    *retsym;
} gen_ctx;

static void gen_one_node(gen_ctx *);

static void
_emit(gen_ctx *ctx, int32_t op32, ...)
{
    c4m_karg_only_init(op32);

    c4m_zop_t            op        = (c4m_zop_t)(op32);
    int64_t              arg       = 0;
    int64_t              immediate = 0;
    int64_t              module_id = ctx->fctx->local_module_id;
    c4m_type_t          *type      = NULL;
    c4m_zinstruction_t **instrptr  = NULL;

    c4m_kw_int64("immediate", immediate);
    c4m_kw_int64("arg", arg);
    c4m_kw_int64("module_id", module_id);
    c4m_kw_ptr("type", type);
    c4m_kw_ptr("instrptr", instrptr);

    c4m_zinstruction_t *instr = c4m_gc_alloc(c4m_zinstruction_t);
    instr->op                 = op;
    instr->module_id          = (int16_t)module_id;
    instr->line_no            = c4m_node_get_line_number(ctx->cur_node);
    instr->arg                = (int64_t)arg;
    instr->immediate          = immediate;
    instr->type_info          = type;

    if (instrptr != NULL) {
        *instrptr = instr;
    }

    c4m_list_append(ctx->instructions, instr);

    ctx->instruction_counter++;
}

#define emit(ctx, op, ...) _emit(ctx, (int32_t)op, C4M_VA(__VA_ARGS__))

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

    // jmp_info->code_offset = ctx->instruction_counter;

    // Look to see if we can fill in the jump target now.  If 'top' is
    // set, we are definitely jumping backwards, and can patch.
    if (jmp_info->linked_control_structure != NULL) {
        if (jmp_info->top) {
            arg    = jmp_info->linked_control_structure->entry_ip;
            result = false;
        }
        else {
            if (jmp_info->linked_control_structure->exit_ip != 0) {
                arg    = jmp_info->linked_control_structure->exit_ip;
                result = false;
            }
        }
    }

    c4m_zinstruction_t *instr;

    emit(ctx, opcode, c4m_kw("arg", c4m_ka(arg), "instrptr", c4m_ka(&instr)));

    if (result) {
        jmp_info->to_patch     = instr;
        c4m_control_info_t *ci = jmp_info->linked_control_structure;

        if (ci != NULL) {
            c4m_list_append(ci->awaiting_patches, jmp_info);
        }
    }

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
    int32_t             arg   = ctx->instruction_counter;
    c4m_zinstruction_t *instr = jmp_info->to_patch;

    if (instr) {
        instr->arg = arg;
    }
}

static inline void
gen_apply_waiting_patches(gen_ctx *ctx, c4m_control_info_t *ci)
{
    int n       = c4m_list_len(ci->awaiting_patches);
    ci->exit_ip = ctx->instruction_counter;

    for (int i = 0; i < n; i++) {
        c4m_jump_info_t *one = c4m_list_get(ci->awaiting_patches, i, NULL);
        assert(one->to_patch);
        one->to_patch->arg = ci->exit_ip;
    }
}

static inline void
gen_tcall(gen_ctx *ctx, c4m_builtin_type_fn fn, c4m_type_t *t)
{
    if (t != NULL) {
        t = c4m_type_resolve(t);
        if (c4m_type_is_box(t)) {
            t = c4m_type_unbox(t);
        }
    }

    emit(ctx, C4M_ZTCall, c4m_kw("arg", c4m_ka(fn), "type", c4m_ka(t)));
}

// Two operands have been pushed onto the stack. But if it's a primitive
// type, we want to generate a raw ZCmp; otherwise, we want to generate
// a polymorphic tcall.

static inline void
gen_equality_test(gen_ctx *ctx, c4m_type_t *operand_type)
{
    if (c4m_type_is_value_type(operand_type)) {
        emit(ctx, C4M_ZCmp);
    }
    else {
        gen_tcall(ctx, C4M_BI_EQ, operand_type);
    }
}

// Helpers for the common case where we have one exit only.
#define JMP_TEMPLATE(g, user_code, ...)                                 \
    do {                                                                \
        c4m_jump_info_t *ji    = c4m_gc_alloc(c4m_jump_info_t);         \
        bool             patch = g(ctx, ji __VA_OPT__(, ) __VA_ARGS__); \
        user_code;                                                      \
        if (patch) {                                                    \
            gen_finish_jump(ctx, ji);                                   \
        }                                                               \
    } while (0)

#define GEN_J(user_code)         JMP_TEMPLATE(gen_j, user_code)
#define GEN_JZ(user_code)        JMP_TEMPLATE(gen_jz, user_code, true)
#define GEN_JNZ(user_code)       JMP_TEMPLATE(gen_jnz, user_code, true)
#define GEN_JZ_NOPOP(user_code)  JMP_TEMPLATE(gen_jz, user_code, false)
#define GEN_JNZ_NOPOP(user_code) JMP_TEMPLATE(gen_jnz, user_code, false)

static inline void
set_stack_offset(gen_ctx *ctx, c4m_symbol_t *sym)
{
    sym->static_offset = ctx->current_stack_offset++;
    if (ctx->current_stack_offset > ctx->max_stack_size) {
        ctx->max_stack_size = ctx->current_stack_offset;
    }
}

static void
gen_load_immediate(gen_ctx *ctx, int64_t value)
{
    emit(ctx, C4M_ZPushImm, c4m_kw("immediate", c4m_ka(value)));
}

// TODO: when we add refs, these should change the types written to
// indicate the item on the stack is a ref to a particular type.
//
// For now, they just indicate tspec_ref().

static inline bool
unbox_const_value(gen_ctx *ctx, c4m_obj_t obj, c4m_type_t *type)
{
    if (c4m_type_is_box(type) || c4m_type_is_value_type(type)) {
        gen_load_immediate(ctx, c4m_unbox(obj));
        return true;
    }

    return false;
}

static inline void
gen_load_const_obj(gen_ctx *ctx, c4m_obj_t obj)
{
    c4m_type_t *type = c4m_get_my_type(obj);

    if (unbox_const_value(ctx, obj, type)) {
        return;
    }

    uint32_t offset = c4m_layout_const_obj(ctx->cctx, obj);

    emit(ctx,
         C4M_ZPushConstObj,
         c4m_kw("arg", c4m_ka(offset), "type", c4m_ka(type)));
    return;
}

static void
gen_load_const_by_offset(gen_ctx *ctx, uint32_t offset, c4m_type_t *type)
{
    emit(ctx,
         C4M_ZPushConstObj,
         c4m_kw("arg", c4m_ka(offset), "type", c4m_ka(type)));
}

static inline void
gen_sym_load_const(gen_ctx *ctx, c4m_symbol_t *sym, bool addressof)
{
    c4m_type_t *type = sym->type;

    if (unbox_const_value(ctx, sym->value, type)) {
        return;
    }
    emit(ctx,
         addressof ? C4M_ZPushConstRef : C4M_ZPushConstObj,
         c4m_kw("arg", c4m_ka(sym->static_offset), "type", c4m_ka(type)));
}

static inline void
gen_sym_load_attr(gen_ctx *ctx, c4m_symbol_t *sym, bool addressof)
{
    int64_t offset = c4m_layout_string_const(ctx->cctx, sym->name);

    emit(ctx,
         C4M_ZPushConstObj,
         c4m_kw("arg", c4m_ka(offset), "type", c4m_ka(sym->type)));
    emit(ctx,
         C4M_ZLoadFromAttr,
         c4m_kw("arg", c4m_ka(addressof), "type", c4m_ka(sym->type)));
}

// Right now we only ever generate the version that returns the rhs
// not the attr storage address.
static inline void
gen_sym_load_attr_and_found(gen_ctx *ctx, c4m_symbol_t *sym, bool skipload)
{
    int64_t flag   = skipload ? C4M_F_ATTR_SKIP_LOAD : C4M_F_ATTR_PUSH_FOUND;
    int64_t offset = c4m_layout_string_const(ctx->cctx, sym->name);

    emit(ctx,
         C4M_ZPushConstObj,
         c4m_kw("arg", c4m_ka(offset), "type", c4m_ka(sym->type)));
    emit(ctx, C4M_ZLoadFromAttr, c4m_kw("immediate", c4m_ka(flag)));
}

static inline void
gen_sym_load_stack(gen_ctx *ctx, c4m_symbol_t *sym, bool addressof)
{
    // This is measured in stack value slots.
    if (addressof) {
        emit(ctx, C4M_ZPushLocalRef, c4m_kw("arg", c4m_ka(sym->static_offset)));
    }
    else {
        emit(ctx, C4M_ZPushLocalObj, c4m_kw("arg", c4m_ka(sym->static_offset)));
    }
}

static inline void
gen_sym_load_static(gen_ctx *ctx, c4m_symbol_t *sym, bool addressof)
{
    // This is measured in 64-bit value slots.
    if (addressof) {
        emit(ctx,
             C4M_ZPushStaticRef,
             c4m_kw("arg",
                    c4m_ka(sym->static_offset),
                    "module_id",
                    c4m_ka(sym->local_module_id)));
    }
    else {
        emit(ctx,
             C4M_ZPushStaticObj,
             c4m_kw("arg",
                    c4m_ka(sym->static_offset),
                    "module_id",
                    c4m_ka(sym->local_module_id)));
    }
}

// Load from the storage location referred to by the symbol,
// pushing onto onto the stack.
//
// Or, optionally, push the address of the symbol.
static void
gen_sym_load(gen_ctx *ctx, c4m_symbol_t *sym, bool addressof)
{
    switch (sym->kind) {
    case C4M_SK_ENUM_VAL:
        gen_sym_load_const(ctx, sym, addressof);
        return;
    case C4M_SK_ATTR:
        gen_sym_load_attr(ctx, sym, addressof);
        return;
    case C4M_SK_VARIABLE:
        // clang-format off
        switch (sym->flags & (C4M_F_DECLARED_CONST |
			      C4M_F_USER_IMMUTIBLE |
			      C4M_F_REGISTER_STORAGE)) {
        // clang-format on
        case C4M_F_DECLARED_CONST:
            gen_sym_load_const(ctx, sym, addressof);
            return;

            // If it's got the user-immutible flag set it's a loop-related
            // automatic variable like $i or $last.
        case C4M_F_USER_IMMUTIBLE:
            gen_sym_load_stack(ctx, sym, addressof);
            return;

        case C4M_F_REGISTER_STORAGE:
            assert(!addressof);
            emit(ctx, C4M_ZPushFromR0);
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

    case C4M_SK_FORMAL:
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
gen_sym_store(gen_ctx *ctx, c4m_symbol_t *sym, bool pop_and_lock)
{
    int64_t arg;
    switch (sym->kind) {
    case C4M_SK_ATTR:
        // Byte offset into the const object arena where the attribute
        // name can be found.
        arg = c4m_layout_string_const(ctx->cctx, sym->name);
        gen_load_const_by_offset(ctx, arg, c4m_type_utf8());

        emit(ctx, C4M_ZAssignAttr, c4m_kw("arg", c4m_ka(pop_and_lock)));
        return;
    case C4M_SK_VARIABLE:
    case C4M_SK_FORMAL:
        if (!pop_and_lock) {
            emit(ctx, C4M_ZDupTop);
        }

        if (sym->flags & C4M_F_REGISTER_STORAGE) {
            emit(ctx, C4M_ZPopToR0);
            break;
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

static bool
gen_label(gen_ctx *ctx, c4m_utf8_t *s)
{
    if (s == NULL) {
        return false;
    }

    int64_t offset = c4m_layout_string_const(ctx->cctx, s);
    emit(ctx, C4M_ZNop, c4m_kw("arg", c4m_ka(1), "immediate", c4m_ka(offset)));

    return true;
}

static inline void
gen_kids(gen_ctx *ctx)
{
    c4m_tree_node_t *saved = ctx->cur_node;

    for (int i = 0; i < saved->num_kids; i++) {
        ctx->cur_node = saved->children[i];
        gen_one_node(ctx);
    }

    ctx->cur_node = saved;
}

static inline void
gen_one_kid(gen_ctx *ctx, int n)
{
    c4m_tree_node_t *saved = ctx->cur_node;

    // TODO: Remove this when codegen is done.
    if (saved->num_kids <= n) {
        return;
    }

    ctx->cur_node = saved->children[n];

    gen_one_node(ctx);

    ctx->cur_node = saved;
}

static inline void
possible_restore_from_r3(gen_ctx *ctx, bool restore)
{
    if (restore) {
        emit(ctx, C4M_ZPushFromR3);
    }
}

// Helpers above, implementations below.
static inline void
gen_test_param_flag(gen_ctx                 *ctx,
                    c4m_module_param_info_t *param,
                    c4m_symbol_t       *sym)
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
                                 c4m_symbol_t       *sym)
{
    GEN_JNZ(gen_load_const_obj(ctx, param->default_value);
            gen_sym_store(ctx, sym, true));
}

static inline void
gen_param_via_default_ref_type(gen_ctx                 *ctx,
                               c4m_module_param_info_t *param,
                               c4m_symbol_t       *sym)
{
    uint32_t    offset = c4m_layout_const_obj(ctx->cctx, param->default_value);
    c4m_type_t *t      = c4m_get_my_type(param->default_value);

    GEN_JNZ(gen_load_const_by_offset(ctx, offset, t);
            gen_tcall(ctx, C4M_BI_COPY, t);
            gen_sym_store(ctx, sym, true));
}

static inline void
gen_run_callback(gen_ctx *ctx, c4m_callback_t *cb)
{
    uint32_t    offset   = c4m_layout_const_obj(ctx->cctx, cb);
    c4m_type_t *t        = cb->target_type;
    int         nargs    = c4m_type_get_num_params(t) - 1;
    c4m_type_t *ret_type = c4m_type_get_param(t, nargs);
    bool        useret   = !(c4m_types_are_compat(ret_type,
                                         c4m_type_void(),
                                         NULL));
    int         imm      = useret ? 1 : 0;

    gen_load_const_by_offset(ctx, offset, c4m_get_my_type(cb));
    emit(ctx,
         C4M_ZRunCallback,
         c4m_kw("arg", c4m_ka(nargs), "immediate", c4m_ka(imm)));

    if (nargs) {
        emit(ctx, C4M_ZMoveSp, c4m_kw("arg", c4m_ka(-1 * nargs)));
    }

    if (useret) {
        emit(ctx, C4M_ZPopToR0);
    }
}

static inline void
gen_box_if_needed(gen_ctx           *ctx,
                  c4m_tree_node_t   *n,
                  c4m_symbol_t *sym,
                  int                ix)
{
    c4m_pnode_t *pn          = c4m_get_pnode(n);
    c4m_type_t  *actual_type = c4m_type_resolve(pn->type);
    c4m_type_t  *param_type  = c4m_type_get_param(sym->type, ix);

    if (c4m_type_is_box(actual_type)) {
        actual_type = c4m_type_unbox(actual_type);
    }
    else {
        if (!c4m_type_is_value_type(actual_type)) {
            return;
        }
    }

    // We've already type checked, so we can play fast and loose
    // with the test here.
    if (!c4m_type_is_value_type(param_type)) {
        emit(ctx, C4M_ZBox, c4m_kw("type", c4m_ka(actual_type)));
    }
}
static inline void
gen_unbox_if_needed(gen_ctx           *ctx,
                    c4m_tree_node_t   *n,
                    c4m_symbol_t *sym)
{
    c4m_pnode_t *pn          = c4m_get_pnode(n);
    c4m_type_t  *actual_type = c4m_type_resolve(pn->type);
    int          ix          = c4m_type_get_num_params(sym->type) - 1;
    c4m_type_t  *param_type  = c4m_type_get_param(sym->type, ix);

    if (c4m_type_is_box(actual_type)) {
        actual_type = c4m_type_unbox(actual_type);
    }
    else {
        if (!c4m_type_is_value_type(actual_type)) {
            return;
        }
    }

    // We've already type checked, so we can play fast and loose
    // with the test here.
    if (!c4m_type_is_value_type(param_type)) {
        emit(ctx, C4M_ZUnbox, c4m_kw("type", c4m_ka(actual_type)));
    }
}

static void
gen_native_call(gen_ctx *ctx, c4m_symbol_t *fsym)
{
    // Needed to calculate the loc of module variables.
    // Currently that's done at runtime, tho could be done in
    // a proper link pass in the future.
    int            target_module;
    // Index into the object file's cache.
    int            target_fn_id;
    int            loc  = ctx->instruction_counter;
    // When the call is generated, we might not have generated the
    // function we're calling. In that case, we will just generate
    // a stub and add the actual call instruction to a backpatch
    // list that gets processed at the end of compilation.
    //
    // To test this reliably, we can check the 'offset' field of
    // the function info object, as a function never starts at
    // offset 0.
    c4m_fn_decl_t *decl = fsym->value;
    target_module       = decl->module_id;
    target_fn_id        = decl->local_id;

    emit(ctx,
         C4M_Z0Call,
         c4m_kw("arg", c4m_ka(target_fn_id), "module_id", target_module));

    if (target_fn_id == 0) {
        call_backpatch_info_t *bp;

        bp       = c4m_gc_alloc(call_backpatch_info_t);
        bp->decl = decl;
        bp->i    = c4m_list_get(ctx->instructions, loc, NULL);

        c4m_list_append(ctx->call_backpatches, bp);
    }

    int n = decl->signature_info->num_params;

    if (n != 0) {
        emit(ctx, C4M_ZMoveSp, c4m_kw("arg", c4m_ka(-n)));
    }

    if (!decl->signature_info->void_return) {
        emit(ctx, C4M_ZPushFromR0);
        gen_unbox_if_needed(ctx, ctx->cur_node, fsym);
    }
}

static void
gen_extern_call(gen_ctx *ctx, c4m_symbol_t *fsym)
{
    c4m_ffi_decl_t *decl = (c4m_ffi_decl_t *)fsym->value;

    emit(ctx, C4M_ZFFICall, c4m_kw("arg", c4m_ka(decl->global_ffi_call_ix)));

    if (decl->num_ext_params != 0) {
        emit(ctx, C4M_ZMoveSp, c4m_kw("arg", c4m_ka(-decl->num_ext_params)));
    }

    if (!decl->local_params->void_return) {
        emit(ctx, C4M_ZPushFromR0);
        gen_unbox_if_needed(ctx, ctx->cur_node, fsym);
    }
}

static void
gen_call(gen_ctx *ctx)
{
    c4m_call_resolution_info_t *info = ctx->cur_pnode->extra_info;
    c4m_symbol_t      *fsym = info->resolution;

    int n = ctx->cur_node->num_kids;

    // Pushes arguments onto the stack.
    for (int i = 1; i < n; i++) {
        gen_one_kid(ctx, i);
        gen_box_if_needed(ctx, ctx->cur_node->children[i], fsym, i - 1);
    }

    if (fsym->kind != C4M_SK_FUNC) {
        gen_extern_call(ctx, fsym);
    }
    else {
        gen_native_call(ctx, fsym);
    }
}

static void
gen_ret(gen_ctx *ctx)
{
    if (ctx->cur_node->num_kids != 0) {
        gen_kids(ctx);
        emit(ctx, C4M_ZPopToR0);
    }

    emit(ctx, C4M_ZRet);
}

static inline void
gen_param_via_callback(gen_ctx                 *ctx,
                       c4m_module_param_info_t *param,
                       c4m_symbol_t       *sym)
{
    gen_run_callback(ctx, param->callback);
    // The third parameter gives 'false' for attrs (where the
    // parameter would cause the attribute to lock) and 'true' for
    // variables, which leads to the value being popped after stored
    // (which happens automatically w/ the attr).
    gen_sym_store(ctx, sym, sym->kind != C4M_SK_ATTR);
    // TODO: call the validator too.
}

static inline void
gen_param_bail_if_missing(gen_ctx *ctx, c4m_symbol_t *sym)
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
        c4m_symbol_t       *sym   = param->linked_symbol;

        if (sym->kind == C4M_SK_ATTR) {
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
gen_module(gen_ctx *ctx)
{
    gen_label(ctx, c4m_cstr_format("Module '{}': ", ctx->fctx->path));

    int num_params = gen_parameter_checks(ctx);

    emit(ctx, C4M_ZModuleEnter, c4m_kw("arg", c4m_ka(num_params)));
    gen_kids(ctx);
}

static inline void
gen_elif(gen_ctx *ctx)
{
    gen_one_kid(ctx, 0);
    GEN_JZ(gen_one_kid(ctx, 1);
           gen_j(ctx, &ctx->target_info->targets[ctx->target_info->next++]));
}

static inline void
gen_if(gen_ctx *ctx)
{
    target_info_t end_info = {
        .targets = c4m_gc_array_alloc(c4m_jump_info_t, ctx->cur_node->num_kids),
        .next    = 0,
    };

    target_info_t *saved_info = ctx->target_info;
    ctx->target_info          = &end_info;

    gen_one_kid(ctx, 0);
    GEN_JZ(gen_one_kid(ctx, 1);
           gen_j(ctx, &ctx->target_info->targets[ctx->target_info->next++]));
    for (int i = 2; i < ctx->cur_node->num_kids; i++) {
        gen_one_kid(ctx, i);
    }

    for (int i = 0; i < ctx->target_info->next; i++) {
        c4m_jump_info_t *one_patch = &end_info.targets[i];

        gen_finish_jump(ctx, one_patch);
    }

    ctx->target_info = saved_info;
}

static inline void
gen_typeof(gen_ctx *ctx)
{
    c4m_tree_node_t    *id_node;
    c4m_pnode_t        *id_pn;
    c4m_symbol_t  *sym;
    c4m_tree_node_t    *n         = ctx->cur_node;
    c4m_pnode_t        *pnode     = c4m_get_pnode(n);
    c4m_control_info_t *ci        = pnode->extra_info;
    int                 target_ix = 0;
    int                 expr_ix   = 0;
    c4m_jump_info_t    *ji        = c4m_gc_array_alloc(c4m_jump_info_t,
                                             n->num_kids);

    for (int i = 0; i < n->num_kids; i++) {
        ji[i].linked_control_structure = pnode->extra_info;
    }

    if (gen_label(ctx, ci->label)) {
        expr_ix++;
    }

    id_node = c4m_get_match_on_node(n->children[expr_ix], c4m_id_node);
    id_pn   = c4m_get_pnode(id_node);
    sym     = id_pn->extra_info;

    gen_sym_load(ctx, sym, false);

    // pops the variable due to the arg, leaving just the type,
    // which we will dupe for each test.
    emit(ctx, C4M_ZPushObjType);

    // We could elide some dup/pop and jmp instructions here, but for
    // now, let's keep it simple.
    for (int i = expr_ix + 1; i < n->num_kids; i++) {
        c4m_tree_node_t *kid = n->children[i];
        pnode                = c4m_get_pnode(kid);

        // If it's the 'else' branch
        if (i + 1 == n->num_kids) {
            if (pnode->kind == c4m_nt_else) {
                emit(ctx, C4M_ZPop); // Remove the type we keep copying.
                gen_one_kid(ctx, i);
                break;
            }
        }

        ctx->cur_node = kid->children[1];

        // We stashed the type in the `nt_case` node's value field during
        // the check pass for easy access.
        emit(ctx, C4M_ZDupTop);
        gen_load_const_obj(ctx, pnode->value);
        emit(ctx, C4M_ZTypeCmp);
        GEN_JZ(emit(ctx, C4M_ZPop);
               gen_one_node(ctx);
               gen_j(ctx, &ji[target_ix++]));
    }

    // If there was no else branch, we still have to emit a pop if the last
    // elif was false.
    if (pnode->kind != c4m_nt_else) {
        emit(ctx, C4M_ZPop);
    }

    gen_apply_waiting_patches(ctx, ci);
}

static inline void
gen_switch(gen_ctx *ctx)
{
    int                 expr_ix = 0;
    c4m_tree_node_t    *n       = ctx->cur_node;
    c4m_pnode_t        *pnode   = c4m_get_pnode(n);
    c4m_control_info_t *ci      = pnode->extra_info;
    c4m_jump_info_t    *ji      = c4m_gc_array_alloc(c4m_jump_info_t,
                                             n->num_kids);
    c4m_type_t         *expr_type;

    for (int i = 0; i < n->num_kids; i++) {
        ji[i].linked_control_structure = pnode->extra_info;
    }

    if (gen_label(ctx, ci->label)) {
        expr_ix++;
    }

    // Get the value to test to the top of the stack.
    gen_one_kid(ctx, expr_ix);
    pnode     = c4m_get_pnode(n->children[expr_ix]);
    expr_type = pnode->type;

    for (int i = expr_ix + 1; i < n->num_kids; i++) {
        c4m_tree_node_t *kid = n->children[i];
        pnode                = c4m_get_pnode(kid);

        ctx->cur_node = kid;

        if (i + 1 == n->num_kids) {
            emit(ctx, C4M_ZPop);
            gen_one_node(ctx);
            break;
        }

        int n_conds = kid->num_kids - 1;
        if (n_conds == 1) {
            emit(ctx, C4M_ZDupTop);
            gen_one_kid(ctx, 0);
            gen_equality_test(ctx, expr_type);
            GEN_JZ(gen_one_kid(ctx, 1));
        }
        else {
            c4m_jump_info_t *local_jumps = c4m_gc_array_alloc(c4m_jump_info_t,
                                                              n_conds);

            for (int j = 0; j < n_conds; j++) {
                emit(ctx, C4M_ZDupTop);
                gen_one_kid(ctx, j);
                gen_equality_test(ctx, expr_type);
                gen_jnz(ctx, &local_jumps[j], true);
            }

            for (int j = 0; j < n_conds; j++) {
                gen_finish_jump(ctx, &local_jumps[j]);
            }
        }

        emit(ctx, C4M_ZPop);
        gen_one_kid(ctx, n_conds);
        gen_j(ctx, &ji[i - (expr_ix + 1)]);
    }

    if (pnode->kind != c4m_nt_else) {
        emit(ctx, C4M_ZPop);
    }

    gen_apply_waiting_patches(ctx, ci);
}

static inline bool
gen_index_var_init(gen_ctx *ctx, c4m_loop_info_t *li)
{
    if (li->loop_ix && c4m_list_len(li->loop_ix->sym_uses) > 0) {
        li->gen_ix = 1;
        set_stack_offset(ctx, li->loop_ix);
    }
    if (li->named_loop_ix && c4m_list_len(li->named_loop_ix->sym_uses) > 0) {
        li->gen_named_ix = 1;
        if (!li->gen_ix) {
            set_stack_offset(ctx, li->named_loop_ix);
        }
    }

    if (!(li->gen_ix | li->gen_named_ix)) {
        return false;
    }

    if (li->gen_ix && li->gen_named_ix) {
        assert(li->loop_ix == li->named_loop_ix);
    }

    gen_load_immediate(ctx, 0);

    // The value gets stored to the symbol at the beginning of the loop.
    return true;
}

static inline void
gen_len_var_init(gen_ctx *ctx, c4m_loop_info_t *li)
{
    c4m_symbol_t *sym = NULL;

    if (li->loop_last && c4m_list_len(li->loop_last->sym_uses) > 0) {
        sym = li->loop_last;
    }
    if (li->named_loop_last && c4m_list_len(li->named_loop_last->sym_uses) > 0) {
        if (sym != NULL) {
            assert(sym == li->named_loop_last);
        }
        else {
            sym = li->named_loop_last;
        }
    }
    if (sym) {
        set_stack_offset(ctx, sym);
        gen_sym_store(ctx, sym, false);
    }
}

static inline bool
gen_index_var_increment(gen_ctx *ctx, c4m_loop_info_t *li)
{
    if (!(li->gen_ix | li->gen_named_ix)) {
        return false;
    }

    if (li->gen_ix) {
        gen_sym_store(ctx, li->loop_ix, false);
    }
    if (li->gen_named_ix) {
        gen_sym_store(ctx, li->named_loop_ix, false);
    }
    // Index var would be on the stack here; the add will clobber it,
    // making this a +=.
    gen_load_immediate(ctx, 1);
    emit(ctx, C4M_ZAdd);
    return true;
}

static inline void
gen_index_var_cleanup(gen_ctx *ctx, c4m_loop_info_t *li)
{
    if (!(li->gen_ix | li->gen_named_ix)) {
        return;
    }

    emit(ctx, C4M_ZPop);
}

static inline void
set_loop_entry_point(gen_ctx *ctx, c4m_loop_info_t *li)
{
    li->branch_info.entry_ip = ctx->instruction_counter;
}

static inline void
store_view_item(gen_ctx *ctx, c4m_loop_info_t *li)
{
    gen_sym_store(ctx, li->lvar_1, true);
    if (li->lvar_2 != NULL) {
        gen_sym_store(ctx, li->lvar_2, true);
    }
}

static inline void
deal_with_iteration_count(gen_ctx         *ctx,
                          c4m_loop_info_t *li,
                          bool             have_index_var)
{
    if (have_index_var) {
        gen_index_var_increment(ctx, li);
    }
    else {
        gen_load_immediate(ctx, 1);
        emit(ctx, C4M_ZAdd);
    }
}

static inline void
gen_ranged_for(gen_ctx *ctx, c4m_loop_info_t *li)
{
    c4m_jump_info_t ji_top = {
        .linked_control_structure = &li->branch_info,
        .top                      = true,
    };
    // In a ranged for, the left value got pushed on, then the right
    // value. So if it's 0 .. 10, and we call subtract to calculate the
    // length, we're going to be computing 0 - 10, not the opposite.
    // So we first swap the two numbers.
    emit(ctx, C4M_ZSwap);
    // Subtract the 2 numbers now, but DO NOT POP; we need these.
    emit(ctx, C4M_ZSubNoPop);

    bool calc_last       = false;
    bool calc_named_last = false;

    if (li->loop_last && c4m_list_len(li->loop_last->sym_uses) > 0) {
        calc_last = true;
    }
    if (li->named_loop_last && c4m_list_len(li->named_loop_last->sym_uses) > 0) {
        calc_named_last = true;
    }
    if (calc_last || calc_named_last) {
        emit(ctx, C4M_ZDupTop);
        emit(ctx, C4M_ZAbs);

        if (calc_last && calc_named_last) {
            emit(ctx, C4M_ZDupTop);
        }

        if (calc_last) {
            gen_sym_store(ctx, li->loop_last, true);
        }
        if (calc_named_last) {
            gen_sym_store(ctx, li->named_loop_last, true);
        }
    }
    // Now, we have the number of iterations on top, but we actually
    // want this to be either 1 or -1, depending on what we have to add
    // to the count for each loop. So we use a magic instruction to
    // convert the difference into a 1 or -1.
    emit(ctx, C4M_ZGetSign);
    //
    // Now our loop is set up, with the exception of any possible iteration
    // variable. When we're using one, we'll leave it on top of the stack.
    gen_index_var_init(ctx, li);

    // Now we're ready for the loop!
    set_loop_entry_point(ctx, li);
    bool using_index = gen_index_var_increment(ctx, li);
    if (using_index) {
        emit(ctx, C4M_ZPopToR3);
    }
    // pop the step for a second.
    emit(ctx, C4M_ZPopToR1);
    // If the two items are equal, we bail from the loop.
    emit(ctx, C4M_ZCmpNoPop);

    // Pop the step back to add it;
    // Store a copy of the result w/o popping.
    // Then push the step back on,
    // And, if it's being used, the $i from R3.

    GEN_JNZ(gen_sym_store(ctx, li->lvar_1, false);
            emit(ctx, C4M_ZPushFromR1);
            emit(ctx, C4M_ZAdd);
            emit(ctx, C4M_ZPushFromR1);
            possible_restore_from_r3(ctx, using_index);
            gen_one_kid(ctx, ctx->cur_node->num_kids - 1);
            gen_j(ctx, &ji_top));
    gen_apply_waiting_patches(ctx, &li->branch_info);
    emit(ctx, C4M_ZMoveSp, c4m_kw("arg", c4m_ka(-3)));
}

static inline void
gen_container_for(gen_ctx *ctx, c4m_loop_info_t *li)
{
    c4m_jump_info_t ji_top = {
        .linked_control_structure = &li->branch_info,
        .top                      = true,
    };
    // In between iterations we will push these items on...
    //   sp     -------> Size of container
    //   sp + 1 -------> Iteration count
    //   sp + 2 -------> Number of bytes in the item; we will
    //                   advance the view this many bytes each
    //                   iteration (except, see below).
    //   sp + 3 -------> Pointer to Container View
    //
    // We always create a view the container before calculating the
    // size and iterating on it. This will
    //
    // After calculating the size, we set $last / label$last if
    // appropriate.
    //
    // At the top of each loop, the stack should look like the above.
    // Before we test, we will, in this order:

    // 1. Store the sp in $i and/or label$i, if those variables are used.
    // 2. Compare the index variable using ZGteNoPop (lhs is the container size)
    //    This instruction is explicitly used in for loops, keeping
    //    our index variable and i
    // 3. JZ out of the loop if the comparison returns false.
    // 4. Increment the index variable (again at stack top) by 1 for
    //    the sake of the next iteration (we don't do this at the bottom
    //    so that we don't have to compilicate `continue`.
    // 5. load the item at the container view into the loop variable.
    // 6. If there are two loop variables (dict), do the same thing again.
    //
    // We use two bits of magic here:
    // 1. The GTE check auto-pops to registers.
    // 2. The ZLoadFromView instruction assumes the item size is at the
    //    top slot, and the view in the second slot. It fetches the
    //    next item and pushes it automatically.
    //    If the view object is a bitfield, the iteration count is
    //    required to be in register 1 (necessary for calculating
    //    which bit to push and when to shift the view pointer).
    //
    // Also, note that the VIEW builtin doesn't need to copy objects,
    // but it needs to give us a pointer to items, where we steal the
    // lowest 2 bits in the pointer to represent the the log base 2 of
    // the bytes in the item count.  Currently, the only allowable
    // view item sizes are therefore: 1 byte, 2 bytes, 4 bytes, 8
    // bytes.
    //
    // Currently, we don't need more than 8bytes, since anything
    // larger than 8 bytes is passed around as pointers. However, I
    // expect to eventually add 128-bit ints, which would entail a
    // double-word load.
    //
    // Therefore, views must be 8 byte aligned, because eventually we
    // will steal a third bit. But that's no problem as long as view
    // pointers start at the beginning of an allocation, since the
    // allocator always spits out aligned objects.
    //
    // We have a bit of a special case for bitfield types (right now,
    // just c4m_flags_t). There, the value of the "number of bytes"
    // field will be set to 128; When we see that, we will use
    // different code that only advances the pointer 8 bytes every 64
    // iterations.
    //
    // There are cases this test might need to be dynamic, so for ease
    // of first implementation, it will just always happen, even
    // though in most cases it should be no problem to bind to the
    // right approach statically.

    gen_tcall(ctx, C4M_BI_VIEW, NULL);
    // The length of the container is on top right now; the view is
    // underneath.  We want to store a copy to $len if appropriate,
    // and then pop to a register 2, to work on the pointer. We'll
    // push it back on top when we properly enter the loop.
    gen_len_var_init(ctx, li);
    // Move the container length out to a register for a bit.
    emit(ctx, C4M_ZPopToR2);
    emit(ctx, C4M_ZUnsteal);
    // The bit length is actually encoded by taking log base 2; here
    // were expand it back out before going into the loop.
    emit(ctx, C4M_ZShlI, c4m_kw("arg", c4m_ka(0x1)));
    // On top of this, put the iteration count, which at the start of
    // each turn through the loop, will be the second item.
    bool have_index_var = gen_index_var_init(ctx, li);
    bool have_kv_pair   = li->lvar_2 != NULL;

    if (!have_index_var) {
        gen_load_immediate(ctx, 0x0);
    }
    // Container length needs to be on the stack above the index count
    // at the start of the loop.
    emit(ctx, C4M_ZPushFromR2);

    // Top of loop actions:
    // 1. compare the top two items, without popping yet (we want to
    //    enter the loop with everything on the stack, so easiest not to
    //    pop until after the test.
    // 2. If the test is 1, it's time to exit the loop.
    // 3. Otherwise, we pop the container length into a register.
    // 4. Increment the index counter; If $i is used, store it.
    // 5. Push the index counter into a register.
    // 5. Call ZLoadFromView, which will load the top view item to the
    //    top of the stack, and advance the pointer as appropriate (leaving
    //    the previous two items in place).
    // 6. Push the top item out to its storage location.
    // 7. Restore the stack start state by putting R1 and R2 back on.
    // 8. Loop body!
    // 9. Back to top for testing.
    set_loop_entry_point(ctx, li);
    emit(ctx, C4M_ZGteNoPop);

    GEN_JNZ(emit(ctx, C4M_ZPopToR2);
            deal_with_iteration_count(ctx, li, have_index_var);
            emit(ctx, C4M_ZPopToR1);
            emit(ctx, C4M_ZLoadFromView, c4m_kw("arg", c4m_ka(have_kv_pair)));
            // Store the item(s) to the appropriate loop variable(s).
            store_view_item(ctx, li);
            emit(ctx, C4M_ZPushFromR1); // Re-groom the stack; container length.
            emit(ctx, C4M_ZPushFromR2); // Iteration count.
            gen_one_kid(ctx, ctx->cur_node->num_kids - 1);
            gen_j(ctx, &ji_top));
    // After the loop:
    // 1. backpatch.
    // 2. Move the stack down four items, popping the count, len, item size,
    //    and container.
    gen_apply_waiting_patches(ctx, &li->branch_info);
    emit(ctx, C4M_ZMoveSp, c4m_kw("arg", c4m_ka(-4)));
}

static inline void
gen_for(gen_ctx *ctx)
{
    int              expr_ix     = 0;
    c4m_loop_info_t *li          = ctx->cur_pnode->extra_info;
    int              saved_stack = ctx->current_stack_offset;

    if (gen_label(ctx, li->branch_info.label)) {
        expr_ix++;
    }

    set_stack_offset(ctx, li->lvar_1);
    if (li->lvar_2) {
        set_stack_offset(ctx, li->lvar_2);
    }
    // Load either the range or the container we're iterating over.
    gen_one_kid(ctx, expr_ix + 1);

    if (li->ranged) {
        gen_ranged_for(ctx, li);
    }
    else {
        gen_container_for(ctx, li);
    }

    ctx->current_stack_offset = saved_stack;
}

static inline void
gen_while(gen_ctx *ctx)
{
    int              saved_stack = ctx->current_stack_offset;
    int              expr_ix     = 0;
    c4m_tree_node_t *n           = ctx->cur_node;
    c4m_pnode_t     *pnode       = c4m_get_pnode(n);
    c4m_loop_info_t *li          = pnode->extra_info;
    c4m_jump_info_t  ji_top      = {
              .linked_control_structure = &li->branch_info,
              .top                      = true,
    };

    gen_index_var_init(ctx, li);
    set_loop_entry_point(ctx, li);

    if (gen_label(ctx, li->branch_info.label)) {
        expr_ix++;
    }

    gen_one_kid(ctx, expr_ix);
    pnode = c4m_get_pnode(n->children[expr_ix]);

    // Condition for a loop is always a boolean. So top of the stack
    // after the condition is evaluated will be 0 when it's time to
    // exit.
    gen_load_immediate(ctx, 0);
    emit(ctx, C4M_ZCmp);
    GEN_JZ(gen_one_kid(ctx, expr_ix + 1);
           gen_index_var_increment(ctx, li);
           gen_j(ctx, &ji_top));
    gen_apply_waiting_patches(ctx, &li->branch_info);
    gen_index_var_cleanup(ctx, li);
    ctx->current_stack_offset = saved_stack;
}

static inline void
gen_break(gen_ctx *ctx)
{
    c4m_jump_info_t *ji = ctx->cur_pnode->extra_info;
    gen_j(ctx, ji);
}

static inline void
gen_continue(gen_ctx *ctx)
{
    c4m_jump_info_t *ji = ctx->cur_pnode->extra_info;
    gen_j(ctx, ji);
}

static inline void
gen_int_binary_op(gen_ctx *ctx, c4m_operator_t op, bool sign)
{
    c4m_zop_t zop;

    switch (op) {
    case c4m_op_plus:
        zop = sign ? C4M_ZAdd : C4M_ZUAdd;
        break;
    case c4m_op_minus:
        zop = sign ? C4M_ZSub : C4M_ZUSub;
        break;
    case c4m_op_mul:
        zop = sign ? C4M_ZMul : C4M_ZUMul;
        break;
    case c4m_op_div:
    case c4m_op_fdiv:
        zop = sign ? C4M_ZDiv : C4M_ZUDiv;
        break;
    case c4m_op_mod:
        zop = sign ? C4M_ZMod : C4M_ZUMod;
        break;
    case c4m_op_shl:
        zop = C4M_ZShl;
        break;
    case c4m_op_shr:
        zop = C4M_ZShr;
        break;
    case c4m_op_bitand:
        zop = C4M_ZBAnd;
        break;
    case c4m_op_bitor:
        zop = C4M_ZBOr;
        break;
    case c4m_op_bitxor:
        zop = C4M_ZBXOr;
        break;
    case c4m_op_lt:
        zop = C4M_ZLt;
        break;
    case c4m_op_lte:
        zop = C4M_ZLte;
        break;
    case c4m_op_gt:
        zop = C4M_ZGt;
        break;
    case c4m_op_gte:
        zop = C4M_ZGte;
        break;
    case c4m_op_eq:
        zop = C4M_ZCmp;
        break;
    case c4m_op_neq:
        zop = C4M_ZNeq;
        break;
    }
    emit(ctx, zop);
}

static inline void
gen_polymorphic_binary_op(gen_ctx *ctx, c4m_operator_t op)
{
}

static inline void
gen_float_binary_op(gen_ctx *ctx, c4m_operator_t op)
{
}

static inline bool
skip_poly_call(c4m_type_t *t)
{
    if (c4m_type_is_box(t)) {
        return true;
    }

    if (c4m_type_get_base(t) == C4M_DT_KIND_primitive) {
        return true;
    }

    return false;
}

static inline void
gen_binary_op(gen_ctx *ctx)
{
    c4m_operator_t op = (c4m_operator_t)ctx->cur_pnode->extra_info;
    c4m_type_t    *t  = c4m_type_resolve(ctx->cur_pnode->type);

    gen_kids(ctx);

    if (!skip_poly_call(t)) {
        gen_polymorphic_binary_op(ctx, op);
        return;
    }

    if (c4m_type_is_box(t)) {
        t = c4m_type_unbox(t);
    }

    if (c4m_type_is_int_type(t)) {
        gen_int_binary_op(ctx, op, c4m_type_is_signed(t));
        return;
    }

    if (c4m_type_is_bool(t)) {
        gen_int_binary_op(ctx, op, false);
        return;
    }

    if (t->typeid == C4M_T_F64) {
        gen_float_binary_op(ctx, op);
        return;
    }
}

static inline void
gen_assert(gen_ctx *ctx)
{
    gen_kids(ctx);
    emit(ctx, C4M_ZAssert);
}

static inline void
gen_box_if_value_type(gen_ctx *ctx, int pos)
{
    c4m_pnode_t *pnode = c4m_get_pnode(ctx->cur_node->children[pos]);
    c4m_type_t  *t     = pnode->type;

    if (c4m_type_is_box(t)) {
        t = c4m_type_unbox(t);
    }
    else {
        if (!c4m_type_is_value_type(t)) {
            return;
        }
    }

    emit(ctx, C4M_ZBox, c4m_kw("type", c4m_ka(t)));
}

static inline void
gen_or(gen_ctx *ctx)
{
    gen_one_kid(ctx, 0);
    // If the first clause is false, we WILL need to pop it,
    // as we should only leave one value on the stack.
    GEN_JNZ_NOPOP(emit(ctx, C4M_ZPop);
                  gen_one_kid(ctx, 1););
}

static inline void
gen_and(gen_ctx *ctx)
{
    // Same as above except JZ not JNZ.
    gen_one_kid(ctx, 0);
    GEN_JZ_NOPOP(emit(ctx, C4M_ZPop);
                 gen_one_kid(ctx, 1););
}

#ifdef C4M_DEV
static inline void
gen_print(gen_ctx *ctx)
{
    gen_kids(ctx);
    gen_box_if_value_type(ctx, 0);
    emit(ctx, C4M_ZPrint);
}
#endif

static inline void
gen_literal(gen_ctx *ctx)
{
    c4m_obj_t        lit = ctx->cur_pnode->value;
    c4m_lit_info_t  *li  = (c4m_lit_info_t *)ctx->cur_pnode->extra_info;
    c4m_tree_node_t *n   = ctx->cur_node;

    if (lit != NULL) {
        c4m_obj_t   obj = ctx->cur_pnode->value;
        c4m_type_t *t   = c4m_type_resolve(c4m_get_my_type(obj));

        if (c4m_type_is_value_type(t) || c4m_type_is_box(t)) {
            gen_load_immediate(ctx, c4m_unbox(obj));
        }
        else {
            gen_load_const_obj(ctx, obj);
            // This is only true for containers, which need to be
            // copied since they are mutable, but the const version
            // is... const.
            if (li->type != NULL) {
                gen_tcall(ctx, C4M_BI_COPY, c4m_get_my_type(lit));
            }
        }

        return;
    }

    if (li->num_items == 1) {
        gen_kids(ctx);
        gen_load_immediate(ctx, n->num_kids);
        gen_tcall(ctx, C4M_BI_CONTAINER_LIT, li->type);
        return;
    }

    // We need to convert each set of items into a tuple object.
    c4m_type_t *ttype = c4m_type_tuple_from_xlist(li->type->details->items);

    for (int i = 0; i < n->num_kids;) {
        for (int j = 0; j < li->num_items; j++) {
            ctx->cur_node = n->children[i++];
            gen_one_node(ctx);
        }

        if (!ctx->lvalue) {
            gen_load_immediate(ctx, li->num_items);
            gen_tcall(ctx, C4M_BI_CONTAINER_LIT, ttype);
        }
    }

    if (!c4m_type_is_tuple(li->type)) {
        gen_load_immediate(ctx, n->num_kids / li->num_items);
        gen_tcall(ctx, C4M_BI_CONTAINER_LIT, li->type);
    }

    ctx->cur_node = n;
}

static inline bool
is_tuple_assignment(gen_ctx *ctx)
{
    c4m_tree_node_t *n = c4m_get_match_on_node(ctx->cur_node, c4m_tuple_assign);

    if (n != NULL) {
        return true;
    }

    return false;
}

static inline void
gen_assign(gen_ctx *ctx)
{
    c4m_type_t *t      = c4m_type_resolve(ctx->cur_pnode->type);
    ctx->assign_method = assign_to_mem_slot;
    ctx->lvalue        = true;
    gen_one_kid(ctx, 0);
    ctx->lvalue = false;
    gen_one_kid(ctx, 1);

    switch (ctx->assign_method) {
    case assign_to_mem_slot:

        if (is_tuple_assignment(ctx)) {
            emit(ctx, C4M_ZPopToR1);
            emit(ctx,
                 C4M_ZUnpack,
                 c4m_kw("arg", c4m_ka(c4m_type_get_num_params(t))));
        }
        else {
            emit(ctx, C4M_ZSwap);
            emit(ctx, C4M_ZAssignToLoc);
        }
        break;
    case assign_via_slice_set_call:
        gen_tcall(ctx, C4M_BI_SLICE_SET, ctx->cur_pnode->type);
        break;
    case assign_via_index_set_call:
        emit(ctx, C4M_ZSwap);
        gen_tcall(ctx, C4M_BI_INDEX_SET, ctx->cur_pnode->type);
        break;
    }
}

#define BINOP_ASSIGN_GEN(ctx, op, t)                           \
    if (c4m_type_get_base(t) == C4M_DT_KIND_primitive) {       \
        if (c4m_type_is_int_type(t)) {                         \
            gen_int_binary_op(ctx, op, c4m_type_is_signed(t)); \
        }                                                      \
                                                               \
        else {                                                 \
            if (c4m_type_is_bool(t)) {                         \
                gen_int_binary_op(ctx, op, false);             \
            }                                                  \
            else {                                             \
                if (t->typeid == C4M_T_F64) {                  \
                    gen_float_binary_op(ctx, op);              \
                }                                              \
                else {                                         \
                    gen_polymorphic_binary_op(ctx, op);        \
                }                                              \
            }                                                  \
        }                                                      \
    }

static inline void
gen_binary_assign(gen_ctx *ctx)
{
    c4m_operator_t op = (c4m_operator_t)ctx->cur_pnode->extra_info;
    c4m_type_t    *t  = c4m_type_resolve(ctx->cur_pnode->type);

    ctx->assign_method = assign_to_mem_slot;
    ctx->lvalue        = true;
    gen_one_kid(ctx, 0);
    ctx->lvalue = false;

    switch (ctx->assign_method) {
    case assign_to_mem_slot:
        emit(ctx, C4M_ZDupTop);
        emit(ctx, C4M_ZDeref);
        gen_one_kid(ctx, 1);

        BINOP_ASSIGN_GEN(ctx, op, t);

        emit(ctx, C4M_ZSwap);
        emit(ctx, C4M_ZAssignToLoc);
        break;
    case assign_via_index_set_call:
        emit(ctx, C4M_ZPopToR1);
        emit(ctx, C4M_ZPopToR2);
        //
        emit(ctx, C4M_ZPushFromR2);
        emit(ctx, C4M_ZPushFromR1);
        //
        emit(ctx, C4M_ZPushFromR2);
        emit(ctx, C4M_ZPushFromR1);

        gen_tcall(ctx, C4M_BI_INDEX_GET, ctx->cur_pnode->type);
        gen_one_kid(ctx, 1);

        BINOP_ASSIGN_GEN(ctx, op, t);

        gen_tcall(ctx, C4M_BI_INDEX_SET, ctx->cur_pnode->type);
        break;
    default:
        // TODO: disallow slice assignments and tuple assignments here..
        c4m_unreachable();
    }
}

static inline void
gen_index_or_slice(gen_ctx *ctx)
{
    // We turn of LHS tracking internally, because we don't poke
    // directly into the object's memory and don't want to generate a
    // settable ref.
    bool         lvalue = ctx->lvalue;
    c4m_pnode_t *pnode  = c4m_get_pnode(ctx->cur_node->children[1]);
    bool         slice  = pnode->kind == c4m_nt_range;

    ctx->lvalue = false;

    gen_kids(ctx);

    if (lvalue) {
        if (slice) {
            ctx->assign_method = assign_via_slice_set_call;
        }
        else {
            ctx->assign_method = assign_via_index_set_call;
        }
        ctx->lvalue = true;
        return;
    }

    if (slice) {
        gen_tcall(ctx, C4M_BI_SLICE_GET, ctx->cur_pnode->type);
    }
    else {
        gen_tcall(ctx, C4M_BI_INDEX_GET, ctx->cur_pnode->type);
    }
}

static inline void
gen_sym_decl(gen_ctx *ctx)
{
    int                last = ctx->cur_node->num_kids - 1;
    c4m_pnode_t       *kid  = c4m_get_pnode(ctx->cur_node->children[last]);
    c4m_pnode_t       *psym;
    c4m_tree_node_t   *cur = ctx->cur_node;
    c4m_symbol_t *sym;

    if (kid->kind == c4m_nt_assign) {
        psym = c4m_get_pnode(ctx->cur_node->children[last - 1]);
        if (psym->kind == c4m_nt_lit_tspec) {
            psym = c4m_get_pnode(ctx->cur_node->children[last - 2]);
        }

        sym = (c4m_symbol_t *)psym->value;

        if (sym->flags & C4M_F_DECLARED_CONST) {
            return;
        }

        ctx->cur_node = cur->children[last]->children[0];
        gen_one_node(ctx);
        ctx->cur_node = cur;
        gen_sym_load(ctx, sym, true);
        emit(ctx, C4M_ZAssignToLoc);
    }
}

static inline void
gen_unary_op(gen_ctx *ctx)
{
    // Right now, the pnode extra_info will be NULL when it's a unary
    // minus and non-null when it's a not operation.
    c4m_pnode_t *n = c4m_get_pnode(ctx->cur_node);

    gen_kids(ctx);

    if (n->extra_info != NULL) {
        gen_kids(ctx);
        emit(ctx, C4M_ZNot, c4m_kw("type", c4m_ka(c4m_type_bool())));
    }
    else {
        gen_load_immediate(ctx, -1);
        emit(ctx, C4M_ZMul);
    }
}

static inline void
gen_lock(gen_ctx *ctx)
{
    c4m_tree_node_t *saved = ctx->cur_node;
    ctx->cur_node          = saved->children[0];
    gen_one_kid(ctx, 0);
    ctx->cur_node = saved;
    emit(ctx, C4M_ZLockOnWrite);
    gen_kids(ctx);
}

static inline void
gen_use(gen_ctx *ctx)
{
    c4m_file_compile_ctx *tocall;

    tocall = (c4m_file_compile_ctx *)ctx->cur_pnode->value;
    emit(ctx, C4M_ZCallModule, c4m_kw("arg", c4m_ka(tocall->local_module_id)));
}

static void
gen_one_node(gen_ctx *ctx)
{
    ctx->cur_pnode = c4m_get_pnode(ctx->cur_node);

    switch (ctx->cur_pnode->kind) {
    case c4m_nt_module:
        gen_module(ctx);
        break;
    case c4m_nt_error:
    case c4m_nt_cast: // No syntax for this yet.
        c4m_unreachable();
    case c4m_nt_body:
    case c4m_nt_else:
        gen_kids(ctx);
        break;
    case c4m_nt_if:
        gen_if(ctx);
        break;
    case c4m_nt_elif:
        gen_elif(ctx);
        break;
    case c4m_nt_typeof:
        gen_typeof(ctx);
        break;
    case c4m_nt_switch:
        gen_switch(ctx);
        break;
    case c4m_nt_for:
        gen_for(ctx);
        break;
    case c4m_nt_while:
        gen_while(ctx);
        break;
    case c4m_nt_break:
        gen_break(ctx);
        break;
    case c4m_nt_continue:
        gen_continue(ctx);
        break;
    case c4m_nt_range:
        gen_kids(ctx);
        break;
    case c4m_nt_or:
        gen_or(ctx);
        break;
    case c4m_nt_and:
        gen_and(ctx);
        break;
    case c4m_nt_cmp:
    case c4m_nt_binary_op:
        gen_binary_op(ctx);
        break;
    case c4m_nt_member:
    case c4m_nt_identifier:
        do {
            c4m_symbol_t *sym = ctx->cur_pnode->extra_info;
            if (sym != NULL) {
                gen_sym_load(ctx, sym, ctx->lvalue);
            }
        } while (0);
        break;
    case c4m_nt_assert:
        gen_assert(ctx);
        break;
#ifdef C4M_DEV
    case c4m_nt_print:
        gen_print(ctx);
        break;
#endif
    case c4m_nt_simple_lit:
    case c4m_nt_lit_list:
    case c4m_nt_lit_dict:
    case c4m_nt_lit_set:
    case c4m_nt_lit_empty_dict_or_set:
    case c4m_nt_lit_tuple:
    case c4m_nt_lit_unquoted:
    case c4m_nt_lit_callback:
    case c4m_nt_lit_tspec:
        gen_literal(ctx);
        break;
    case c4m_nt_assign:
        gen_assign(ctx);
        break;
    case c4m_nt_binary_assign_op:
        gen_binary_assign(ctx);
        break;
    case c4m_nt_index:
        gen_index_or_slice(ctx);
        break;
    case c4m_nt_sym_decl:
        gen_sym_decl(ctx);
        break;
    case c4m_nt_unary_op:
        gen_unary_op(ctx);
        break;
    case c4m_nt_call:
        gen_call(ctx);
        break;
    case c4m_nt_return:
        gen_ret(ctx);
        break;
    case c4m_nt_attr_set_lock:
        gen_lock(ctx);
        break;
    case c4m_nt_use:
        gen_use(ctx);
        break;
        // The following list is still TODO:
    case c4m_nt_varargs_param:
        // These should always be passthrough.
    case c4m_nt_expression:
    case c4m_nt_paren_expr:
    case c4m_nt_variable_decls:
        gen_kids(ctx);
        break;
    // These nodes should NOT do any work and not descend if they're
    // hit; many of them are handled elsewhere and this should be
    // unreachable for many of them.
    //
    // Some things spec related, param related, etc will get generated
    // before the tree is walked from cached info.
    case c4m_nt_case:
    case c4m_nt_decl_qualifiers:
    case c4m_nt_label:
    case c4m_nt_config_spec:
    case c4m_nt_section_spec:
    case c4m_nt_section_prop:
    case c4m_nt_field_spec:
    case c4m_nt_field_prop:
    case c4m_nt_lit_tspec_tvar:
    case c4m_nt_lit_tspec_named_type:
    case c4m_nt_lit_tspec_parameterized_type:
    case c4m_nt_lit_tspec_func:
    case c4m_nt_lit_tspec_varargs:
    case c4m_nt_lit_tspec_return_type:
    case c4m_nt_section:
    case c4m_nt_param_block:
    case c4m_nt_param_prop:
    case c4m_nt_extern_block:
    case c4m_nt_extern_sig:
    case c4m_nt_extern_param:
    case c4m_nt_extern_local:
    case c4m_nt_extern_dll:
    case c4m_nt_extern_pure:
    case c4m_nt_extern_holds:
    case c4m_nt_extern_allocs:
    case c4m_nt_extern_return:
    case c4m_nt_enum:
    case c4m_nt_enum_item:
    case c4m_nt_global_enum:
    case c4m_nt_func_def:
    // We start this from our reference to the functions.  So when
    // it comes via the top-level of the module, ignore it.
    case c4m_nt_func_mods:
    case c4m_nt_func_mod:
    case c4m_nt_formals:
        break;
    }
}

static void
gen_set_once_memo(gen_ctx *ctx, c4m_fn_decl_t *decl)
{
    if (decl->signature_info->void_return) {
        return;
    }

    emit(ctx, C4M_ZPushFromR0);
    emit(ctx, C4M_ZPushStaticRef, c4m_kw("arg", c4m_ka(decl->sc_memo_offset)));
    emit(ctx, C4M_ZAssignToLoc);
}

static void
gen_return_once_memo(gen_ctx *ctx, c4m_fn_decl_t *decl)
{
    if (decl->signature_info->void_return) {
        return;
    }

    emit(ctx, C4M_ZPushStaticObj, c4m_kw("arg", c4m_ka(decl->sc_memo_offset)));
    emit(ctx, C4M_ZRet);
}

static void
gen_function(gen_ctx            *ctx,
             c4m_symbol_t  *sym,
             c4m_zmodule_info_t *module,
             c4m_vm_t           *vm)
{
    c4m_fn_decl_t *decl             = sym->value;
    int            n                = sym->declaration_node->num_kids;
    ctx->cur_node                   = sym->declaration_node->children[n - 1];
    c4m_zfn_info_t *fn_info_for_obj = c4m_gc_alloc(c4m_zfn_info_t);

    ctx->retsym = hatrack_dict_get(decl->signature_info->fn_scope->symbols,
                                   c4m_new_utf8("$result"),
                                   NULL);

    fn_info_for_obj->mid      = module->module_id;
    decl->module_id           = module->module_id;
    decl->offset              = ctx->instruction_counter;
    fn_info_for_obj->offset   = ctx->instruction_counter;
    fn_info_for_obj->tid      = decl->signature_info->full_type;
    fn_info_for_obj->shortdoc = decl->short_doc;
    fn_info_for_obj->longdoc  = decl->long_doc;
    fn_info_for_obj->funcname = sym->name;
    ctx->current_stack_offset = decl->frame_size;

    // In anticipation of supporting multi-threaded access, I've put a
    // write-lock around this. And once the result is cached, the lock
    // does not matter. It's really only there to avoid multiple
    // threads competing to cache the memo.
    gen_label(ctx, sym->name);
    if (decl->once) {
        fn_info_for_obj->static_lock = decl->sc_lock_offset;

        emit(ctx,
             C4M_ZPushStaticObj,
             c4m_kw("arg", c4m_ka(c4m_ka(decl->sc_bool_offset))));
        GEN_JZ(gen_return_once_memo(ctx, decl));
        emit(ctx,
             C4M_ZLockMutex,
             c4m_kw("arg", c4m_ka(decl->sc_lock_offset)));
        emit(ctx,
             C4M_ZPushStaticObj,
             c4m_kw("arg", c4m_ka(decl->sc_bool_offset)));
        // If it's not zero, we grabbed the lock, but waited while
        // someone else computed the memo.
        GEN_JZ(emit(ctx,
                    C4M_ZUnlockMutex,
                    c4m_kw("arg",
                           c4m_ka(decl->sc_lock_offset)));
               emit(ctx,
                    C4M_ZPushStaticObj,
                    c4m_kw("arg", c4m_ka(decl->sc_memo_offset)));
               emit(ctx, C4M_ZRet););
        // Set the boolean to true.
        gen_load_immediate(ctx, 1);
        emit(ctx,
             C4M_ZPushStaticRef,
             c4m_kw("arg", c4m_ka(decl->sc_bool_offset)));
        emit(ctx, C4M_ZAssignToLoc);
    }

    // Until we have full PDG analysis, always zero out the return register
    // on a call entry.
    if (!decl->signature_info->void_return) {
        emit(ctx, C4M_Z0R0c00l);
    }
    // The frame size needs to be backpatched, since the needed stack
    // space from block scopes hasn't been computed by this point. It
    // gets computed as we generate code. So stash this:
    int sp_loc = ctx->instruction_counter;
    emit(ctx, C4M_ZMoveSp);

    gen_one_node(ctx);

    if (decl->once) {
        gen_set_once_memo(ctx, decl);
        emit(ctx,
             C4M_ZUnlockMutex,
             c4m_kw("arg", c4m_ka(decl->sc_lock_offset)));
    }
    emit(ctx, C4M_ZRet);

    // The actual backpatching of the needed frame size.
    c4m_zinstruction_t *ins = c4m_list_get(module->instructions, sp_loc, NULL);
    ins->arg                = ctx->current_stack_offset;
    fn_info_for_obj->size   = ctx->current_stack_offset;

    c4m_list_append(vm->obj->func_info, fn_info_for_obj);

    // TODO: Local id is set to 1 more than its natural index because
    // 0 was taken to mean the module entry point back when I was
    // going to cache fn info per module. But since we don't handle
    // the same way, should probably make this more sane, but
    // that's why this is below the append.
    decl->local_id = c4m_list_len(vm->obj->func_info);
}

static void
gen_module_code(gen_ctx *ctx, c4m_vm_t *vm)
{
    c4m_zmodule_info_t *module = c4m_gc_alloc(c4m_zmodule_info_t);
    c4m_pnode_t        *root;

    ctx->cur_module          = module;
    ctx->fctx->module_object = module;
    ctx->cur_node            = ctx->fctx->parse_tree;
    module->instructions     = c4m_new(c4m_type_list(c4m_type_ref()));
    ctx->instructions        = module->instructions;
    module->module_id        = ctx->fctx->local_module_id;
    module->module_hash      = ctx->fctx->module_id;
    module->modname          = ctx->fctx->module;
    module->authority        = ctx->fctx->authority;
    module->path             = ctx->fctx->path;
    module->package          = ctx->fctx->package;
    module->source           = c4m_to_utf8(ctx->fctx->raw);
    root                     = c4m_get_pnode(ctx->cur_node);
    module->shortdoc         = c4m_token_raw_content(root->short_doc);
    module->longdoc          = c4m_token_raw_content(root->long_doc);

    // Still to fill in to the zmodule object (need to reshuffle to align):
    // authority/path/provided_path/package/module_id
    // Remove key / location / ext / url.
    //
    // Also need to do a bit of work aorund sym_types, codesyms, datasyms
    // and parameters.

    ctx->current_stack_offset = ctx->fctx->static_size;
    ctx->max_stack_size       = ctx->fctx->static_size;
    gen_one_node(ctx);
    emit(ctx, C4M_ZModuleRet);

    module->module_var_size = ctx->max_stack_size;
    module->init_size       = ctx->instruction_counter * sizeof(c4m_zinstruction_t);

    c4m_list_t *symlist = ctx->fctx->fn_def_syms;
    int          n       = c4m_list_len(symlist);

    if (n) {
        gen_label(ctx, c4m_new_utf8("Functions: "));
    }

    for (int i = 0; i < n; i++) {
        c4m_symbol_t *sym = c4m_list_get(symlist, i, NULL);
        gen_function(ctx, sym, module, vm);
    }

    int l = c4m_list_len(ctx->fctx->extern_decls);
    if (l != 0) {
        for (int j = 0; j < l; j++) {
            c4m_symbol_t *d    = c4m_list_get(ctx->fctx->extern_decls,
                                                 j,
                                                 NULL);
            c4m_ffi_decl_t    *decl = d->value;

            c4m_list_append(vm->obj->ffi_info, decl);
        }
    }

    // Version is not used yet.
    // Init size not done yet.
    // datasyms not set yet.
    // Parameters not done yet.
}

static inline void
backpatch_calls(gen_ctx *ctx)
{
    int                    n = c4m_list_len(ctx->call_backpatches);
    call_backpatch_info_t *info;

    for (int i = 0; i < n; i++) {
        info               = c4m_list_get(ctx->call_backpatches, i, NULL);
        info->i->arg       = info->decl->local_id;
        info->i->module_id = info->decl->module_id;

        assert(info->i->arg != 0);
    }
}

void
c4m_internal_codegen(c4m_compile_ctx *cctx, c4m_vm_t *c4m_new_vm)
{
    gen_ctx ctx = {
        .cctx             = cctx,
        .call_backpatches = c4m_new(c4m_type_list(c4m_type_ref())),
        0,
    };

    int n        = c4m_list_len(cctx->module_ordering);
    int existing = c4m_list_len(c4m_new_vm->obj->module_contents);

    for (int i = existing; i < n; i++) {
        ctx.fctx = c4m_list_get(cctx->module_ordering, i, NULL);

        if (ctx.fctx->status < c4m_compile_status_tree_typed) {
            C4M_CRAISE("Cannot call c4m_codegen with untyped modules.");
        }

        if (c4m_fatal_error_in_module(ctx.fctx)) {
            C4M_CRAISE("Cannot generate code for files with fatal errors.");
        }

        if (ctx.fctx->status >= c4m_compile_status_generated_code) {
            continue;
        }

        gen_module_code(&ctx, c4m_new_vm);

        ctx.fctx->status = c4m_compile_status_generated_code;
        c4m_add_module(c4m_new_vm->obj, ctx.fctx->module_object);
    }

    backpatch_calls(&ctx);

    c4m_new_vm->obj->num_const_objs = cctx->const_instantiation_id;
    c4m_new_vm->obj->static_data    = cctx->const_data;
}

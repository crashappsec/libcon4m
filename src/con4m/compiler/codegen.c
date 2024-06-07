#define C4M_USE_INTERNAL_API
#include "con4m.h"

// This is used in nested if/else only.
typedef struct {
    c4m_jump_info_t *targets;
    int              next;
} target_info_t;

typedef struct {
    c4m_compile_ctx      *cctx;
    c4m_file_compile_ctx *fctx;
    c4m_xlist_t          *instructions;
    c4m_tree_node_t      *cur_node;
    c4m_pnode_t          *cur_pnode;
    c4m_zmodule_info_t   *cur_module;
    target_info_t        *target_info;
    int                   instruction_counter;
    bool                  lvalue;
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

    c4m_xlist_append(ctx->instructions, instr);

    ctx->instruction_counter++;
}

#define emit(ctx, op, ...) _emit(ctx, (int32_t)op, KFUNC(__VA_ARGS__))

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
            c4m_xlist_append(ci->awaiting_patches, jmp_info);
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
    int n       = c4m_xlist_len(ci->awaiting_patches);
    ci->exit_ip = ctx->instruction_counter;

    for (int i = 0; i < n; i++) {
        c4m_jump_info_t *one = c4m_xlist_get(ci->awaiting_patches, i, NULL);
        assert(one->to_patch);
        one->to_patch->arg = ci->exit_ip;
    }
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
        emit(ctx,
             C4M_ZTCall,
             c4m_kw("arg", c4m_ka(C4M_BI_EQ), "type", c4m_ka(operand_type)));
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

// TODO: when we add refs, these should change the types written to
// indicate the item on the stack is a ref to a particular type.
//
// For now, they just indicate tspec_ref().

static inline uint32_t
gen_load_const_obj(gen_ctx *ctx, c4m_obj_t obj)
{
    uint32_t offset = c4m_layout_const_obj(ctx->cctx, obj);

    emit(ctx,
         C4M_ZPushConstObj,
         c4m_kw("arg", c4m_ka(offset), "type", c4m_ka(c4m_get_my_type(obj))));
    return offset;
}

static void
gen_load_const_by_offset(gen_ctx *ctx, uint32_t offset, c4m_type_t *type)
{
    emit(ctx,
         C4M_ZPushConstObj,
         c4m_kw("arg", c4m_ka(offset), "type", c4m_ka(type)));
}

static inline void
gen_sym_load_const(gen_ctx *ctx, c4m_scope_entry_t *sym, bool addressof)
{
    emit(ctx,
         addressof ? C4M_ZPushConstRef : C4M_ZPushConstObj,
         c4m_kw("arg", c4m_ka(sym->static_offset), "type", c4m_ka(sym->type)));
}

static inline void
gen_sym_load_attr(gen_ctx *ctx, c4m_scope_entry_t *sym, bool addressof)
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
gen_sym_load_attr_and_found(gen_ctx *ctx, c4m_scope_entry_t *sym, bool skipload)
{
    int64_t flag   = skipload ? C4M_F_ATTR_SKIP_LOAD : C4M_F_ATTR_PUSH_FOUND;
    int64_t offset = c4m_layout_string_const(ctx->cctx, sym->name);

    emit(ctx,
         C4M_ZPushConstObj,
         c4m_kw("arg", c4m_ka(offset), "type", c4m_ka(sym->type)));
    emit(ctx, C4M_ZLoadFromAttr, c4m_kw("immediate", c4m_ka(flag)));
}

static inline void
gen_sym_load_stack(gen_ctx *ctx, c4m_scope_entry_t *sym, bool addressof)
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
gen_sym_load_static(gen_ctx *ctx, c4m_scope_entry_t *sym, bool addressof)
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
        gen_load_const_by_offset(ctx, arg, c4m_tspec_utf8());

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

static void
gen_tcall(gen_ctx *ctx, c4m_builtin_type_fn arg)
{
    emit(ctx, C4M_ZTCall, c4m_kw("arg", c4m_ka(arg)));
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
    ctx->cur_node          = saved->children[n];

    gen_one_node(ctx);

    ctx->cur_node = saved;
}

// Helpers above, implementations below.
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
    GEN_JNZ(gen_load_const_obj(ctx, param->default_value);
            gen_sym_store(ctx, sym, true));
}

static inline void
gen_param_via_default_ref_type(gen_ctx                 *ctx,
                               c4m_module_param_info_t *param,
                               c4m_scope_entry_t       *sym)
{
    uint32_t    offset = c4m_layout_const_obj(ctx->cctx, param->default_value);
    c4m_type_t *t      = c4m_get_my_type(param->default_value);

    GEN_JNZ(gen_load_const_by_offset(ctx, offset, t);
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

    gen_load_const_by_offset(ctx, offset, c4m_get_my_type(cb));
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
gen_module(gen_ctx *ctx)
{
    gen_label(ctx, c4m_cstr_format("Module '{}': ", ctx->fctx->path));

    int num_params = gen_parameter_checks(ctx);

    emit(ctx, C4M_ZModuleEnter, c4m_kw("arg", c4m_ka(num_params)));
    gen_kids(ctx);
}

static inline void
gen_simple_lit(gen_ctx *ctx)
{
    c4m_obj_t   obj = ctx->cur_pnode->value;
    c4m_type_t *t   = c4m_get_my_type(obj);

    if (c4m_type_is_value_type(t)) {
        gen_load_immediate(ctx, c4m_unbox(obj));
    }
    else {
        gen_load_const_obj(ctx, obj);
    }
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
    c4m_scope_entry_t  *sym;
    c4m_tree_node_t    *n         = ctx->cur_node;
    c4m_pnode_t        *pnode     = get_pnode(n);
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

    id_node = get_match_on_node(n->children[expr_ix], c4m_id_node);
    id_pn   = get_pnode(id_node);
    sym     = id_pn->extra_info;

    gen_sym_load(ctx, sym, false);

    // pops the variable due to the arg, leaving just the type,
    // which we will dupe for each test.
    emit(ctx, C4M_ZPushObjType);

    // We could elide some dup/pop and jmp instructions here, but for
    // now, let's keep it simple.
    for (int i = expr_ix + 1; i < n->num_kids; i++) {
        c4m_tree_node_t *kid = n->children[i];
        pnode                = get_pnode(kid);

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
    c4m_pnode_t        *pnode   = get_pnode(n);
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
    pnode     = get_pnode(n->children[expr_ix]);
    expr_type = pnode->type;

    for (int i = expr_ix + 1; i < n->num_kids; i++) {
        c4m_tree_node_t *kid = n->children[i];
        pnode                = get_pnode(kid);

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
    if (li->loop_ix && c4m_xlist_len(li->loop_ix->sym_uses) > 0) {
        li->gen_ix = 1;
    }
    if (li->named_loop_ix && c4m_xlist_len(li->named_loop_ix->sym_uses) > 0) {
        li->gen_named_ix = 1;
    }

    if (!(li->gen_ix | li->gen_named_ix)) {
        return false;
    }

    gen_load_immediate(ctx, 0);

    // DO NOT POP THE COUNTER; leave it on the stack.
    if (li->gen_ix) {
        gen_sym_store(ctx, li->loop_ix, false);
    }
    if (li->gen_named_ix) {
        gen_sym_store(ctx, li->named_loop_ix, false);
    }

    return true;
}

static inline void
gen_len_var_init(gen_ctx *ctx, c4m_loop_info_t *li)
{
    if (li->loop_last && c4m_xlist_len(li->loop_last->sym_uses) > 0) {
        gen_sym_store(ctx, li->loop_last, false);
    }
    if (li->named_loop_last && c4m_xlist_len(li->named_loop_last->sym_uses) > 0) {
        gen_sym_store(ctx, li->named_loop_last, false);
    }
}

static inline void
gen_index_var_increment(gen_ctx *ctx, c4m_loop_info_t *li)
{
    if (!(li->gen_ix | li->gen_named_ix)) {
        return;
    }

    // Index var would be on the stack here; the add will clobber it,
    // making this a +=.
    gen_load_immediate(ctx, 1);
    emit(ctx, C4M_ZAdd);

    if (li->gen_ix) {
        gen_sym_store(ctx, li->loop_ix, false);
    }
    if (li->gen_named_ix) {
        gen_sym_store(ctx, li->named_loop_ix, false);
    }
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
    //   sp + 1 -------> Iteration count (stack pointer)
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

    emit(ctx, C4M_ZTCall, c4m_kw("arg", c4m_ka(C4M_BI_VIEW)));
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
    emit(ctx, C4M_ZShlI, c4m_kw("immediate", c4m_ka(0x1)));
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
    int              expr_ix = 0;
    c4m_loop_info_t *li      = ctx->cur_pnode->extra_info;

    if (gen_label(ctx, li->branch_info.label)) {
        expr_ix++;
    }

    // Load either the range or the container we're iterating over.
    gen_one_kid(ctx, expr_ix + 1);

    if (li->ranged) {
        gen_ranged_for(ctx, li);
    }
    else {
        gen_container_for(ctx, li);
    }
}

static inline void
gen_while(gen_ctx *ctx)
{
    int              expr_ix = 0;
    c4m_tree_node_t *n       = ctx->cur_node;
    c4m_pnode_t     *pnode   = get_pnode(n);
    c4m_loop_info_t *li      = pnode->extra_info;
    c4m_jump_info_t  ji_top  = {
          .linked_control_structure = &li->branch_info,
          .top                      = true,
    };

    gen_index_var_init(ctx, li);
    set_loop_entry_point(ctx, li);

    if (gen_label(ctx, li->branch_info.label)) {
        expr_ix++;
    }

    gen_one_kid(ctx, expr_ix);
    pnode = get_pnode(n->children[expr_ix]);

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

static inline void
gen_binary_op(gen_ctx *ctx)
{
    c4m_operator_t op = (c4m_operator_t)ctx->cur_pnode->extra_info;
    c4m_type_t    *t  = c4m_global_resolve_type(ctx->cur_pnode->type);

    gen_kids(ctx);

    if (c4m_tspec_get_base(t) == C4M_DT_KIND_primitive) {
        if (c4m_tspec_is_int_type(t)) {
            gen_int_binary_op(ctx, op, c4m_tspec_is_signed(t));
            return;
        }

        if (c4m_tspec_is_bool(t)) {
            gen_int_binary_op(ctx, op, false);
            return;
        }

        if (t->typeid == C4M_T_F64) {
            gen_float_binary_op(ctx, op);
            return;
        }
    }
    gen_polymorphic_binary_op(ctx, op);
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
    c4m_pnode_t *pnode = get_pnode(ctx->cur_node->children[pos]);

    if (c4m_type_is_value_type(pnode->type)) {
        emit(ctx, C4M_ZBox, c4m_kw("type", c4m_ka(pnode->type)));
    }
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

static void
gen_one_node(gen_ctx *ctx)
{
    ctx->cur_pnode = get_pnode(ctx->cur_node);

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
    case c4m_nt_simple_lit:
        gen_simple_lit(ctx);
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
            c4m_scope_entry_t *sym = ctx->cur_pnode->extra_info;
            gen_sym_load(ctx, sym, ctx->lvalue);
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
    case c4m_nt_index:
    case c4m_nt_assign:
    case c4m_nt_binary_assign_op:
    case c4m_nt_unary_op:
    case c4m_nt_enum_item:
    case c4m_nt_func_def:
    case c4m_nt_func_mods:
    case c4m_nt_func_mod:
    case c4m_nt_formals:
    case c4m_nt_varargs_param:
    case c4m_nt_call:
    case c4m_nt_paren_expr:
    case c4m_nt_variable_decls:
    case c4m_nt_use:
    case c4m_nt_expression:
    case c4m_nt_lit_list:
    case c4m_nt_lit_dict:
    case c4m_nt_lit_set:
    case c4m_nt_lit_empty_dict_or_set:
    case c4m_nt_lit_tuple:
    case c4m_nt_lit_unquoted:
    case c4m_nt_lit_callback:
    case c4m_nt_lit_tspec:
    case c4m_nt_attr_set_lock:
    case c4m_nt_return:
        // Many of the above will need their own bodies.
        gen_kids(ctx);
        break;
        // This is only partially done, which is why it's down here
        // with unfinished stuff.
    case c4m_nt_sym_decl:
        do {
            int          last = ctx->cur_node->num_kids - 1;
            c4m_pnode_t *kid  = get_pnode(ctx->cur_node->children[last]);
            if (kid->kind == c4m_nt_assign) {
                gen_one_kid(ctx, last);
            }
        } while (0);
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
    case c4m_nt_global_enum:
        break;
    }
}

static void
gen_function(gen_ctx *ctx, c4m_fn_decl_t *fn_decl)
{
}

static void
gen_module_code(gen_ctx *ctx)
{
    c4m_zmodule_info_t *module = c4m_gc_alloc(c4m_zmodule_info_t);
    c4m_pnode_t        *root;

    ctx->cur_module          = module;
    ctx->fctx->module_object = module;
    ctx->cur_node            = ctx->fctx->parse_tree;
    module->instructions     = c4m_new(c4m_tspec_xlist(c4m_tspec_ref()));
    ctx->instructions        = module->instructions;
    module->module_id        = ctx->fctx->local_module_id;
    module->module_hash      = ctx->fctx->module_id;
    module->modname          = ctx->fctx->module;
    module->authority        = ctx->fctx->authority;
    module->path             = ctx->fctx->path;
    module->package          = ctx->fctx->package;
    module->source           = c4m_to_utf8(ctx->fctx->raw);
    root                     = get_pnode(ctx->cur_node);
    module->shortdoc         = c4m_token_raw_content(root->short_doc);
    module->longdoc          = c4m_token_raw_content(root->long_doc);

    ctx->fctx->call_patch_locs = c4m_new(c4m_tspec_xlist(c4m_tspec_ref()));
    // Still to fill in to the zmodule object (need to reshuffle to align):
    // authority/path/provided_path/package/module_id
    // Remove key / location / ext / url.
    //
    // Also need to do a bit of work aorund sym_types, codesyms, datasyms
    // and parameters.

    gen_one_node(ctx);

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

    // Version is not used yet.
    // Init size not done yet.
    // datasyms not set yet.
    // Parameters not done yet.
}

void
c4m_internal_codegen(c4m_compile_ctx *cctx, c4m_vm_t *c4m_new_vm)
{
    gen_ctx ctx = {
        .cctx = cctx,
        0,
    };

    int n        = c4m_xlist_len(cctx->module_ordering);
    int existing = c4m_xlist_len(c4m_new_vm->obj->module_contents);

    for (int i = existing; i < n; i++) {
        ctx.fctx = c4m_xlist_get(cctx->module_ordering, i, NULL);

        if (ctx.fctx->status < c4m_compile_status_tree_typed) {
            C4M_CRAISE("Cannot call c4m_codegen with untyped modules.");
        }

        if (c4m_fatal_error_in_module(ctx.fctx)) {
            C4M_CRAISE("Cannot generate code for files with fatal errors.");
        }

        if (ctx.fctx->status >= c4m_compile_status_generated_code) {
            continue;
        }

        gen_module_code(&ctx);

        ctx.fctx->status = c4m_compile_status_generated_code;
        c4m_add_module(c4m_new_vm->obj, ctx.fctx->module_object);
    }

    c4m_new_vm->obj->num_const_objs = cctx->const_instantiation_id;
    c4m_new_vm->obj->static_data    = cctx->const_data;
}

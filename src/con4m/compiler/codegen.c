#define C4M_USE_INTERNAL_API
#include "con4m.h"

typedef struct jump_patch_info_t {
    int                       to_patch; // instruction index.
    c4m_tree_node_t          *target_node;
    struct jump_patch_info_t *next;
} jump_patch_info_t;

typedef struct {
    c4m_compile_ctx      *cctx;
    c4m_file_compile_ctx *fctx;
    c4m_stream_t         *outstream;
    c4m_tree_node_t      *cur_node;
    jump_patch_info_t    *backpatch_stack;
    int                   instruction_counter;
} gen_ctx;

static inline int
emit_instruction(gen_ctx *ctx, c4m_zinstruction_t *instr)
{
    c4m_stream_put_binary(ctx->outstream, instr, sizeof(c4m_zinstruction_t));
    return ctx->instruction_counter++;
}

#define INIT_STATIC_INSTRUCTION(ctx, varname, opcode, mod_id, tspec) \
    varname.op        = (opcode);                                    \
    varname.pad       = 0;                                           \
    varname.module_id = (mod_id);                                    \
    varname.line_no   = c4m_node_get_line_number(ctx->cur_node);     \
    varname.arg       = 0;                                           \
    varname.immediate = 0;                                           \
    varname.type_info = (tspec)

static inline jump_patch_info_t *
gen_jz_raw(gen_ctx *ctx, c4m_tree_node_t *target, bool pop)
{
    c4m_zinstruction_t instr;
    jump_patch_info_t *patch = c4m_gc_alloc(jump_patch_info_t);

    patch->next        = ctx->backpatch_stack;
    patch->target_node = target;

    if (!pop) {
        INIT_STATIC_INSTRUCTION(ctx, instr, C4M_ZDupTop, 0, NULL);
        emit_instruction(ctx, &instr);
    }

    INIT_STATIC_INSTRUCTION(ctx, instr, C4M_ZJz, 0, NULL);
    patch->to_patch = emit_instruction(ctx, &instr);

    ctx->backpatch_stack = patch;

    return patch;
}

static inline jump_patch_info_t *
gen_jnz_raw(gen_ctx *ctx, c4m_tree_node_t *target, bool pop)
{
    c4m_zinstruction_t instr;
    jump_patch_info_t *patch = c4m_gc_alloc(jump_patch_info_t);

    patch->next        = ctx->backpatch_stack;
    patch->target_node = target;

    if (!pop) {
        INIT_STATIC_INSTRUCTION(ctx, instr, C4M_ZDupTop, 0, NULL);
        emit_instruction(ctx, &instr);
    }

    INIT_STATIC_INSTRUCTION(ctx, instr, C4M_ZJnz, 0, NULL);
    patch->to_patch = emit_instruction(ctx, &instr);

    ctx->backpatch_stack = patch;

    return patch;
}

static inline jump_patch_info_t *
gen_j_raw(gen_ctx *ctx, c4m_tree_node_t *target)
{
    c4m_zinstruction_t instr;
    jump_patch_info_t *patch = c4m_gc_alloc(jump_patch_info_t);
}

// TODO: when we add refs, these should change the types written to
// indicate the item on the stack is a ref to a particular type.
//
// For now, they just indicate tspec_ref().

static inline void
gen_sym_load_const(gen_ctx *ctx, c4m_scope_entry_t *sym, bool addressof)
{
    c4m_zinstruction_t instr;

    INIT_STATIC_INSTRUCTION(ctx,
                            instr,
                            addressof ? C4M_ZPushConstRef : C4M_ZPushConstObj,
                            0,
                            NULL);

    instr.arg = sym->static_offset;
    emit_instruction(ctx, &instr);
}

static inline void
gen_sym_load_attr(gen_ctx *ctx, c4m_scope_entry_t *sym, bool addressof)
{
    c4m_zinstruction_t instr;

    INIT_STATIC_INSTRUCTION(ctx, instr, C4M_ZPushConstObj, 0, NULL);

    // Byte offset into the const object arena where the attribute
    // name can be found.
    instr.arg = c4m_layout_string_const(ctx->cctx, sym->name);
    emit_instruction(ctx, &instr);

    INIT_STATIC_INSTRUCTION(ctx, instr, C4M_ZLoadFromAttr, 0, NULL);

    instr.arg = (int32_t)addressof;
    emit_instruction(ctx, &instr);
}

static inline void
gen_sym_load_stack(gen_ctx *ctx, c4m_scope_entry_t *sym, bool addressof)
{
    c4m_zinstruction_t instr;

    if (addressof) {
        C4M_CRAISE("Invalid to ever store a ref that points onto the stack.");
    }

    INIT_STATIC_INSTRUCTION(ctx, instr, C4M_ZPushLocalObj, 0, sym->type);

    // This is measured in stack value slots.
    instr.arg = sym->static_offset;
    emit_instruction(ctx, &instr);
}

static inline void
gen_sym_load_static(gen_ctx *ctx, c4m_scope_entry_t *sym, bool addressof)
{
    c4m_zinstruction_t instr;

    INIT_STATIC_INSTRUCTION(ctx, instr, C4M_ZPushStaticObj, 0, sym->type);

    // This is measured in stack value slots.
    instr.arg = sym->static_offset;
    emit_instruction(ctx, &instr);
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

// Couldn't resist the variable name. For attributes, if true, the flag
// pops the value stored off the stack (that always happens for attributes),
// and for attributes, the boolean locks the attribute.

static void
gen_sym_store(gen_ctx *ctx, c4m_scope_entry_t *sym, bool pop_and_lock)
{
    c4m_zinstruction_t instr;

    switch (sym->kind) {
    case sk_attr:

        INIT_STATIC_INSTRUCTION(ctx, instr, C4M_ZPushConstObj, 0, NULL);

        // Byte offset into the const object arena where the attribute
        // name can be found.
        instr.arg = c4m_layout_string_const(ctx->cctx, sym->name);
        emit_instruction(ctx, &instr);

        INIT_STATIC_INSTRUCTION(ctx, instr, C4M_ZAssignAttr, 0, NULL);
        instr.arg = pop_and_lock;
        emit_instruction(ctx, &instr);
        return;
    case sk_variable:
    case sk_formal:
        if (!pop_and_lock) {
            INIT_STATIC_INSTRUCTION(ctx, instr, C4M_ZDupTop, 0, NULL);
            emit_instruction(ctx, &instr);
        }

        gen_sym_load(ctx, sym, true);
        INIT_STATIC_INSTRUCTION(ctx, instr, C4M_ZAssignToLoc, 0, NULL);
        emit_instruction(ctx, &instr);
        return;
    default:
        c4m_unreachable();
    }
}

static void
gen_module_code(gen_ctx *ctx)
{
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

        c4m_buf_t *buf     = c4m_buffer_empty();
        ctx.fctx->bytecode = buf;
        ctx.outstream      = c4m_buffer_outstream(buf, false);

        if (ctx.fctx->status >= c4m_compile_status_generated_code) {
            continue;
        }
        gen_module_code(&ctx);
        c4m_stream_close(ctx.outstream);

        ctx.fctx->status = c4m_compile_status_generated_code;
    }
}

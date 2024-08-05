#define C4M_USE_INTERNAL_API
#include "con4m.h"

typedef enum : int8_t {
    fmt_unused,
    fmt_const_obj,
    fmt_const_ptr,
    fmt_offset,
    fmt_bool,
    fmt_int,
    fmt_hex,
    fmt_sym_local,
    fmt_sym_static,
    fmt_load_from_attr,
    fmt_label,
    fmt_tcall,
} inst_arg_fmt_t;

typedef struct {
    char          *name;
    inst_arg_fmt_t arg_fmt;
    inst_arg_fmt_t imm_fmt;
    unsigned int   unused      : 1;
    unsigned int   show_type   : 1;
    unsigned int   show_module : 1;
} inst_info_t;

c4m_utf8_t *c4m_instr_utf8_names[256] = {
    NULL,
};

static c4m_utf8_t *bi_fn_names[C4M_BI_NUM_FUNCS];

void
show_it()
{
    c4m_utf8_t *s = c4m_instr_utf8_names[C4M_ZBox];
    char       *d = s->data;
    printf("Showing string. start = %p; data = %p\n", s, d);

    c4m_print(c4m_hex_dump(s, sizeof(c4m_str_t)));
}

static const c4m_utf8_t *
get_bool_label(c4m_zop_t op)
{
    switch (op) {
    case C4M_ZLoadFromAttr:
        return c4m_new_utf8("addressof");
    case C4M_ZAssignAttr:
        return c4m_new_utf8("lock");
    case C4M_ZLoadFromView:
        return c4m_new_utf8("load kv pair");
    case C4M_ZPushFfiPtr:
        return c4m_new_utf8("skip boxing");
    case C4M_ZRunCallback:
        return c4m_new_utf8("use return");
#ifdef C4M_VM_DEBUG
    case C4M_ZDebug:
        return c4m_new_utf8("set debug");
#endif
    default:
        c4m_unreachable();
    }
}

const inst_info_t inst_info[256] = {
    [C4M_ZPushConstObj] = {
        .name      = "ZPushConstObj",
        .arg_fmt   = fmt_const_obj,
        .show_type = true,
    },
    [C4M_ZPushConstRef] = {
        .name      = "ZPushConstRef",
        .arg_fmt   = fmt_const_ptr,
        .show_type = true,
    },
    [C4M_ZPushLocalObj] = {
        .name    = "ZPushLocalObj",
        .arg_fmt = fmt_sym_local,
    },
    [C4M_ZPushLocalRef] = {
        .name    = "ZPushLocalRef",
        .arg_fmt = fmt_sym_local,
        .unused  = true,
    },
    [C4M_ZPushStaticObj] = {
        .name        = "ZPushStaticObj",
        .show_module = true,
        .arg_fmt     = fmt_sym_static,
    },
    [C4M_ZPushStaticRef] = {
        .name        = "ZPushStaticRef",
        .arg_fmt     = fmt_sym_static,
        .show_module = true,
        .unused      = true,
    },
    [C4M_ZJ] = {
        .name    = "ZJ",
        .arg_fmt = fmt_offset,
    },
    [C4M_ZJz] = {
        .name    = "ZJz",
        .arg_fmt = fmt_offset,
    },
    [C4M_ZJnz] = {
        .name    = "ZJnz",
        .arg_fmt = fmt_offset,
    },
    [C4M_ZDupTop] = {
        .name = "ZDupTop",
    },
    [C4M_ZLoadFromAttr] = {
        .name      = "ZLoadFromAttr",
        .arg_fmt   = fmt_bool,
        .imm_fmt   = fmt_load_from_attr,
        .show_type = true,
    },
    [C4M_ZAssignAttr] = {
        .name      = "ZAssignAttr",
        .arg_fmt   = fmt_bool,
        .show_type = true,
    },
    [C4M_ZAssignToLoc] = {
        .name      = "ZAssignToLoc",
        .show_type = true,
    },
    [C4M_ZBail] = {
        .name = "ZBail",
    },
    [C4M_ZPushImm] = {
        .name    = "ZPushImm",
        .imm_fmt = fmt_hex,
    },
    [C4M_ZNop] = {
        .name    = "ZNop",
        .imm_fmt = fmt_label,
    },
    [C4M_ZTCall] = {
        .name      = "ZTCall",
        .arg_fmt   = fmt_tcall,
        .show_type = true,
    },
    [C4M_ZBAnd] = {
        .name = "ZBAnd",
    },
    [C4M_ZPushFfiPtr] = {
        .name      = "ZPushFfiPtr",
        .arg_fmt   = fmt_int,
        .imm_fmt   = fmt_bool,
        .show_type = true,
    },
    [C4M_ZPushVmPtr] = {
        .name        = "ZPushVmPtr",
        .arg_fmt     = fmt_int,
        .show_type   = true,
        .show_module = true,
    },
    [C4M_ZRunCallback] = {
        .name      = "ZRunCallback",
        .arg_fmt   = fmt_int,
        .show_type = true,
        .imm_fmt   = fmt_bool,
    },
    [C4M_ZMoveSp] = {
        .name    = "ZMoveSp",
        .arg_fmt = fmt_int,
    },
    [C4M_ZModuleEnter] = {
        .name    = "ZModuleEnter",
        .arg_fmt = fmt_int,
    },
    [C4M_ZRet] = {
        .name = "ZRet",
    },
    [C4M_ZModuleRet] = {
        .name = "ZModuleRet",
    },
    [C4M_ZPushObjType] = {
        .name = "ZPushObjType",
    },
    [C4M_ZPop] = {
        .name = "ZPop",
    },
    [C4M_ZTypeCmp] = {
        .name = "ZTypeCmp",
    },
    [C4M_ZAdd] = {
        .name = "ZAdd",
    },
    [C4M_ZUAdd] = {
        .name = "ZUAdd",
    },
    [C4M_ZFAdd] = {
        .name = "ZFAdd",
    },
    [C4M_ZSub] = {
        .name = "ZSub",
    },
    [C4M_ZUSub] = {
        .name = "ZUSub",
    },
    [C4M_ZFSub] = {
        .name = "ZFSub",
    },
    [C4M_ZMul] = {
        .name = "ZMul",
    },
    [C4M_ZUMul] = {
        .name = "ZUMul",
    },
    [C4M_ZFMul] = {
        .name = "ZFMul",
    },
    [C4M_ZDiv] = {
        .name = "ZDiv",
    },
    [C4M_ZUDiv] = {
        .name = "ZUDiv",
    },
    [C4M_ZFDiv] = {
        .name = "ZFDiv",
    },
    [C4M_ZMod] = {
        .name = "ZMod",
    },
    [C4M_ZUMod] = {
        .name = "ZUMod",
    },
    [C4M_ZShl] = {
        .name = "ZShl",
    },
    [C4M_ZShr] = {
        .name = "ZShr",
    },
    [C4M_ZBOr] = {
        .name = "ZBOr",
    },
    [C4M_ZBXOr] = {
        .name = "ZBXOr",
    },
    [C4M_ZLt] = {
        .name = "ZLt",
    },
    [C4M_ZLte] = {
        .name = "ZLte",
    },
    [C4M_ZGt] = {
        .name = "ZGt",
    },
    [C4M_ZGte] = {
        .name = "ZGte",
    },
    [C4M_ZULt] = {
        .name = "ZULt",
    },
    [C4M_ZULte] = {
        .name = "ZULte",
    },
    [C4M_ZUGt] = {
        .name = "ZUGt",
    },
    [C4M_ZUGte] = {
        .name = "ZGteU",
    },
    [C4M_ZNeq] = {
        .name = "ZNeq",
    },
    [C4M_ZCmp] = {
        .name = "ZCmp",
    },
    [C4M_ZShlI] = {
        .name    = "ZShlI",
        .arg_fmt = fmt_hex,
    },
    [C4M_ZUnsteal] = {
        .name = "ZUnsteal",
    },
    [C4M_ZSwap] = {
        .name = "ZSwap",
    },
    [C4M_ZPopToR0] = {
        .name = "ZPopToR0",
    },
    [C4M_ZPopToR1] = {
        .name = "ZPopToR1",
    },
    [C4M_ZPopToR2] = {
        .name = "ZPopToR2",
    },
    [C4M_ZPopToR3] = {
        .name = "ZPopToR3",
    },
    [C4M_Z0R0c00l] = {
        .name    = "Z0R0(c00l)",
        .arg_fmt = fmt_int,
    },
    [C4M_ZPushFromR0] = {
        .name = "ZPushFromR0",
    },
    [C4M_ZPushFromR1] = {
        .name = "ZPushFromR1",
    },
    [C4M_ZPushFromR2] = {
        .name = "ZPushFromR2",
    },
    [C4M_ZPushFromR3] = {
        .name = "ZPushFromR3",
    },
    [C4M_ZGteNoPop] = {
        .name = "ZGteNoPop",
    },
    [C4M_ZLoadFromView] = {
        .name    = "ZLoadFromView",
        .arg_fmt = fmt_bool,
    },
    [C4M_ZAssert] = {
        .name = "ZAssert",
    },
    [C4M_ZBox] = {
        .name      = "ZBox",
        .show_type = true,
    },
    [C4M_ZUnbox] = {
        .name      = "ZUnbox",
        .show_type = true,
    },
    [C4M_ZSubNoPop] = {
        .name = "ZSubNoPop",
    },
    [C4M_ZCmpNoPop] = {
        .name = "ZCmpNoPop",
    },
    [C4M_ZAbs] = {
        .name = "ZAbs",
    },
    [C4M_ZGetSign] = {
        .name = "ZGetSign",
    },
    [C4M_ZDeref] = {
        .name = "ZDeref",
    },
    [C4M_ZNot] = {
        .name = "ZNot",
    },
    [C4M_ZBNot] = {
        .name = "ZBNot",
    },
    [C4M_ZLockMutex] = {
        .name        = "ZLockMutex",
        .arg_fmt     = fmt_hex,
        .show_module = true,
    },
    [C4M_ZUnlockMutex] = {
        .name        = "ZUnLockMutex",
        .arg_fmt     = fmt_hex,
        .show_module = true,
    },
    [C4M_Z0Call] = {
        .name        = "Z0Call",
        .arg_fmt     = fmt_hex, // Should add a fmt here.
        .show_module = true,
    },
    [C4M_ZFFICall] = {
        .name        = "ZFFICall",
        .arg_fmt     = fmt_int,
        .show_module = true,
    },
    [C4M_ZLockOnWrite] = {
        .name = "ZLockOnWrite",
    },
    [C4M_ZCallModule] = {
        .name        = "ZCallModule",
        .show_module = true,
    },
    [C4M_ZUnpack] = {
        .name    = "ZUnpack",
        .arg_fmt = fmt_int,
    },
#ifdef C4M_DEV
    [C4M_ZPrint] = {
        .name = "ZPrint",
    },
    [C4M_ZDebug] = {
        .name    = "ZDebug",
        .arg_fmt = fmt_bool,
    },
#endif
};

static void
init_disasm()
{
    static bool inited = false;

    if (!inited) {
        inited = true;
        c4m_gc_register_root(c4m_instr_utf8_names, 256);
        c4m_gc_register_root(bi_fn_names, C4M_BI_NUM_FUNCS);

        for (int i = 0; i < 256; i++) {
            if (inst_info[i].name != NULL) {
                c4m_instr_utf8_names[i] = c4m_new_utf8(inst_info[i].name);
            }
        }

        bi_fn_names[C4M_BI_TO_STR]        = c4m_new_utf8("$str");
        bi_fn_names[C4M_BI_FORMAT]        = c4m_new_utf8("$format");
        bi_fn_names[C4M_BI_FINALIZER]     = c4m_new_utf8("$final");
        bi_fn_names[C4M_BI_COERCIBLE]     = c4m_new_utf8("$can_cast");
        bi_fn_names[C4M_BI_COERCE]        = c4m_new_utf8("$cast");
        bi_fn_names[C4M_BI_FROM_LITERAL]  = c4m_new_utf8("$parse_literal");
        bi_fn_names[C4M_BI_COPY]          = c4m_new_utf8("$copy");
        bi_fn_names[C4M_BI_ADD]           = c4m_new_utf8("$add");
        bi_fn_names[C4M_BI_SUB]           = c4m_new_utf8("$sub");
        bi_fn_names[C4M_BI_MUL]           = c4m_new_utf8("$mul");
        bi_fn_names[C4M_BI_DIV]           = c4m_new_utf8("$div");
        bi_fn_names[C4M_BI_MOD]           = c4m_new_utf8("$mod");
        bi_fn_names[C4M_BI_EQ]            = c4m_new_utf8("$eq");
        bi_fn_names[C4M_BI_LT]            = c4m_new_utf8("$lt");
        bi_fn_names[C4M_BI_GT]            = c4m_new_utf8("$gt");
        bi_fn_names[C4M_BI_LEN]           = c4m_new_utf8("$len");
        bi_fn_names[C4M_BI_INDEX_GET]     = c4m_new_utf8("$get_item");
        bi_fn_names[C4M_BI_INDEX_SET]     = c4m_new_utf8("$set_item");
        bi_fn_names[C4M_BI_SLICE_GET]     = c4m_new_utf8("$get_slice");
        bi_fn_names[C4M_BI_SLICE_SET]     = c4m_new_utf8("$set_slice");
        bi_fn_names[C4M_BI_VIEW]          = c4m_new_utf8("$view");
        bi_fn_names[C4M_BI_ITEM_TYPE]     = c4m_new_utf8("$item_type");
        bi_fn_names[C4M_BI_CONTAINER_LIT] = c4m_new_utf8("$parse_literal");
    }
}

static c4m_utf8_t *
fmt_builtin_fn(int64_t value)
{
    c4m_utf8_t *s = bi_fn_names[value];

    if (s == NULL) {
        s = c4m_new_utf8("???");
    }

    return c4m_cstr_format("[em]{}[/]", s);
}

static c4m_obj_t
value_to_object(c4m_vm_t *vm, uint64_t offset, c4m_type_t *t)
{
    if (t != NULL) {
        t = c4m_type_resolve(t);
    }

    if (t != NULL && c4m_type_is_value_type(t)) {
        uint64_t u = vm->obj->static_contents->items[offset].nonpointer;
        return c4m_box_obj((c4m_box_t)u, t);
    }

    return vm->obj->static_contents->items[offset].v;
}

static c4m_utf8_t *
fmt_arg_or_imm_no_syms(c4m_vm_t *vm, c4m_zinstruction_t *instr, int i, bool imm)
{
    inst_arg_fmt_t fmt;
    int64_t        value;

    if (imm) {
        fmt   = inst_info[instr->op].imm_fmt;
        value = (int64_t)instr->immediate;
    }
    else {
        fmt   = inst_info[instr->op].arg_fmt;
        value = (int64_t)(uint64_t)instr->arg;
    }

    switch (fmt) {
    case fmt_unused:
        return c4m_get_space_const();
    case fmt_const_obj:
        return c4m_cstr_format("{}\n[i]@offset: {:10x}",
                               c4m_box_i64(value),
                               c4m_box_i64(value));
    case fmt_const_ptr:
        return c4m_cstr_format("offset to ptr: {:4x}",
                               c4m_box_i64(value));
    case fmt_offset:
        do {
            int64_t *b = c4m_box_i64(value);
            return c4m_cstr_format("target @{:10x}", b);
        } while (false);
    case fmt_bool:
        return c4m_cstr_format("{}: {}",
                               get_bool_label(instr->op),
                               c4m_box_bool((bool)value));
    case fmt_int:
        return c4m_cstr_format("{}", c4m_box_i64(value));
    case fmt_hex:
        return c4m_cstr_format("{:18x}", c4m_box_i64(value));
    case fmt_sym_local:
        return c4m_cstr_format("sym stack slot offset: {}", c4m_box_i64(value));
    case fmt_sym_static:
        return c4m_cstr_format("static offset: {:4x}",
                               c4m_box_i64(value));
    case fmt_load_from_attr:
        return c4m_cstr_format("attr name @{:10x}", c4m_box_i64(value));
    case fmt_label:
        return c4m_cstr_format("[h2]{}",
                               value_to_object(vm, value, c4m_type_utf8()));
    case fmt_tcall:
        return c4m_cstr_format("builtin call of [em]{}[/]",
                               fmt_builtin_fn(value));
    default:
        c4m_unreachable();
    }
}

static c4m_utf8_t *
fmt_type_no_syms(c4m_zinstruction_t *instr)
{
    if (!inst_info[instr->op].show_type || !instr->type_info) {
        return c4m_get_space_const();
    }

    return c4m_cstr_format("{}", instr->type_info);
}

static inline c4m_utf8_t *
fmt_module_no_syms(c4m_zinstruction_t *instr)
{
    if (!inst_info[instr->op].show_module) {
        return c4m_get_space_const();
    }

    return c4m_cstr_format("{}", c4m_box_u64(instr->module_id));
}

static inline c4m_utf8_t *
fmt_addr(int64_t i)
{
    return c4m_cstr_format("{:8x}", c4m_box_u64(i));
}

c4m_utf8_t *
c4m_fmt_instr_name(c4m_zinstruction_t *instr)
{
    c4m_utf8_t *result = c4m_instr_utf8_names[instr->op];

    if (!result) {
        // This shouldn't really happen.
        return c4m_new_utf8("???");
    }

    return result;
}

static inline c4m_utf8_t *
fmt_line_no_syms(c4m_zinstruction_t *instr)
{
    return c4m_cstr_format("{}", c4m_box_u64(instr->line_no));
}

c4m_grid_t *
c4m_disasm(c4m_vm_t *vm, c4m_zmodule_info_t *m)
{
    init_disasm();

    c4m_grid_t *grid = c4m_new(c4m_type_grid(),
                               c4m_kw("start_cols",
                                      c4m_ka(7),
                                      "header_rows",
                                      c4m_ka(1),
                                      "container_tag",
                                      c4m_ka("table2"),
                                      "stripe",
                                      c4m_ka(true)));

    c4m_list_t *row = c4m_new_table_row();
    int64_t     len = c4m_list_len(m->instructions);
    c4m_list_append(row, c4m_new_utf8("Address"));
    c4m_list_append(row, c4m_new_utf8("Instruction"));
    c4m_list_append(row, c4m_new_utf8("Arg"));
    c4m_list_append(row, c4m_new_utf8("Immediate"));
    c4m_list_append(row, c4m_new_utf8("Type"));
    c4m_list_append(row, c4m_new_utf8("Module"));
    c4m_list_append(row, c4m_new_utf8("Line"));

    c4m_grid_add_row(grid, row);

    for (int64_t i = 0; i < len; i++) {
        row = c4m_new_table_row();

        c4m_zinstruction_t *ins  = c4m_list_get(m->instructions, i, NULL);
        c4m_utf8_t         *addr = fmt_addr(i);
        c4m_utf8_t         *name = c4m_fmt_instr_name(ins);
        c4m_utf8_t         *arg  = fmt_arg_or_imm_no_syms(vm, ins, i, false);
        c4m_utf8_t         *imm  = fmt_arg_or_imm_no_syms(vm, ins, i, true);
        c4m_utf8_t         *type = fmt_type_no_syms(ins);
        c4m_utf8_t         *mod  = fmt_module_no_syms(ins);
        c4m_utf8_t         *line = fmt_line_no_syms(ins);

        if (ins->op == C4M_ZNop) {
            c4m_grid_add_row(grid, row);
            c4m_renderable_t *r = c4m_to_str_renderable(imm, NULL);
            c4m_grid_add_col_span(grid, r, i + 1, 0, 7);
            continue;
        }

        c4m_list_append(row, addr);
        c4m_list_append(row, name);
        c4m_list_append(row, arg);
        c4m_list_append(row, imm);
        c4m_list_append(row, type);
        c4m_list_append(row, mod);
        c4m_list_append(row, line);
        c4m_grid_add_row(grid, row);
    }

    c4m_set_column_style(grid, 0, "snap");
    c4m_set_column_style(grid, 1, "snap");
    c4m_set_column_style(grid, 3, "snap");
    c4m_set_column_style(grid, 4, "snap");
    c4m_set_column_style(grid, 5, "snap");
    c4m_set_column_style(grid, 6, "snap");

    return grid;
}

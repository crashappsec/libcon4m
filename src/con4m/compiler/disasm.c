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

static const c4m_utf8_t *
get_bool_label(c4m_zop_t op)
{
    switch (op) {
    case C4M_ZLoadFromAttr:
        return c4m_new_utf8("addressof");
    case C4M_ZAssignAttr:
        return c4m_new_utf8("lock");
    case C4M_ZRunCallback:
        return c4m_new_utf8("currently unused");
    case C4M_ZLoadFromView:
        return c4m_new_utf8("load kv pair");
    default:
        c4m_unreachable();
    }
}

const inst_info_t inst_info[256] = {
    [C4M_ZPushConstObj] = {
        .name      = "ZPushConstObj",
        .arg_fmt   = fmt_const_obj,
        .show_type = 1,
    },
    [C4M_ZPushConstRef] = {
        .name      = "ZPushConstRef",
        .arg_fmt   = fmt_const_ptr,
        .show_type = 1,
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
        .show_module = 1,
        .arg_fmt     = fmt_sym_static,
    },
    [C4M_ZPushStaticRef] = {
        .name        = "ZPushStaticRef",
        .arg_fmt     = fmt_sym_static,
        .show_module = 1,
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
        .show_type = 1,
    },
    [C4M_ZAssignAttr] = {
        .name    = "ZAssignAttr",
        .arg_fmt = fmt_bool,
    },
    [C4M_ZAssignToLoc] = {
        .name = "ZAssignToLoc",
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
        .show_type = 1,
    },
    [C4M_ZBAnd] = {
        .name = "ZBAnd",
    },
    [C4M_ZRunCallback] = {
        .name    = "ZRunCallback",
        .arg_fmt = fmt_int,
        .imm_fmt = fmt_bool,
    },
    [C4M_ZMoveSp] = {
        .name    = "ZMoveSp",
        .arg_fmt = fmt_int,
    },
    [C4M_ZPushRes] = {
        .name = "ZPushRes",
    },
    [C4M_ZModuleEnter] = {
        .name    = "ZModuleEnter",
        .arg_fmt = fmt_int,
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
    [C4M_ZSub] = {
        .name = "ZSub",
    },
    [C4M_ZUSub] = {
        .name = "ZUSub",
    },
    [C4M_ZMul] = {
        .name = "ZMul",
    },
    [C4M_ZUMul] = {
        .name = "ZUMul",
    },
    [C4M_ZDiv] = {
        .name = "ZDiv",
    },
    [C4M_ZUDiv] = {
        .name = "ZUDiv",
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
    [C4M_ZPopToR1] = {
        .name = "ZPopToR1",
    },
    [C4M_ZPopToR2] = {
        .name = "ZPopToR2",
    },
    [C4M_ZPopToR3] = {
        .name = "ZPopToR3",
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
        .show_type = 1,
    },
    [C4M_ZUnbox] = {
        .name = "ZUnbox",
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
#ifdef C4M_DEV
    [C4M_ZPrint] = {
        .name = "ZPrint",
    },
#endif
};

static void
init_disasm()
{
    if (bi_fn_names[C4M_BI_TO_STR] == NULL) {
        for (int i = 0; i < 256; i++) {
            if (inst_info[i].name != NULL) {
                c4m_instr_utf8_names[i] = c4m_new_utf8(inst_info[i].name);
            }
        }
        c4m_gc_register_root(c4m_instr_utf8_names, 256);

        bi_fn_names[C4M_BI_TO_STR]        = c4m_new_utf8("__str__");
        bi_fn_names[C4M_BI_FORMAT]        = c4m_new_utf8("__format__");
        bi_fn_names[C4M_BI_FINALIZER]     = c4m_new_utf8("__final__");
        bi_fn_names[C4M_BI_MARSHAL]       = c4m_new_utf8("__marshal__");
        bi_fn_names[C4M_BI_UNMARSHAL]     = c4m_new_utf8("__unmarshal__");
        bi_fn_names[C4M_BI_COERCIBLE]     = c4m_new_utf8("__can_cast__");
        bi_fn_names[C4M_BI_COERCE]        = c4m_new_utf8("__cast__");
        bi_fn_names[C4M_BI_FROM_LITERAL]  = c4m_new_utf8("__parse_literal__");
        bi_fn_names[C4M_BI_COPY]          = c4m_new_utf8("__copy__");
        bi_fn_names[C4M_BI_ADD]           = c4m_new_utf8("__add__");
        bi_fn_names[C4M_BI_SUB]           = c4m_new_utf8("__sub__");
        bi_fn_names[C4M_BI_MUL]           = c4m_new_utf8("__mul__");
        bi_fn_names[C4M_BI_DIV]           = c4m_new_utf8("__div__");
        bi_fn_names[C4M_BI_MOD]           = c4m_new_utf8("__mod__");
        bi_fn_names[C4M_BI_EQ]            = c4m_new_utf8("__eq__");
        bi_fn_names[C4M_BI_LT]            = c4m_new_utf8("__lt__");
        bi_fn_names[C4M_BI_GT]            = c4m_new_utf8("__gt__");
        bi_fn_names[C4M_BI_LEN]           = c4m_new_utf8("__len__");
        bi_fn_names[C4M_BI_INDEX_GET]     = c4m_new_utf8("__get_item__");
        bi_fn_names[C4M_BI_INDEX_SET]     = c4m_new_utf8("__set_item__");
        bi_fn_names[C4M_BI_SLICE_GET]     = c4m_new_utf8("__get_slice__");
        bi_fn_names[C4M_BI_SLICE_SET]     = c4m_new_utf8("__set_slice__");
        bi_fn_names[C4M_BI_VIEW]          = c4m_new_utf8("__view__");
        bi_fn_names[C4M_BI_ITEM_TYPE]     = c4m_new_utf8("__item_type__");
        bi_fn_names[C4M_BI_CONTAINER_LIT] = c4m_new_utf8("__parse_literal__");
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
        t = c4m_global_resolve_type(t);
    }

    if (t != NULL && c4m_type_is_value_type(t)) {
        uint64_t u = (uint64_t)vm->const_pool[offset].p;
        return c4m_box_obj((c4m_box_t)u, t);
    }

    return vm->const_pool[offset].p;
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
        return c4m_cstr_format("{}\n[i]@offset: {:8x}", c4m_box_i64(value), c4m_box_i64(value));
    case fmt_const_ptr:
        return c4m_cstr_format("offset to ptr: {:8x}",
                               value_to_object(vm, value, instr->type_info),
                               c4m_box_i64(value));
    case fmt_offset:
        do {
            int64_t *b = c4m_box_i64(value * sizeof(c4m_zinstruction_t));
            return c4m_cstr_format("target @{:8x}", b);
        } while (false);
    case fmt_bool:
        return c4m_cstr_format("{}: {}",
                               get_bool_label(instr->op),
                               c4m_box_bool((bool)value));
    case fmt_int:
        return c4m_cstr_format("{}", c4m_box_i64(value));
    case fmt_hex:
        return c4m_cstr_format("{:x}", c4m_box_i64(value));
    case fmt_sym_local:
        return c4m_cstr_format("sym stack slot offset: {}", c4m_box_i64(value));
    case fmt_sym_static:
        return c4m_cstr_format("static offset: {:8x}", c4m_box_i64(value));
    case fmt_load_from_attr:
        return c4m_cstr_format("attr name @{:8x}", c4m_box_i64(value));
    case fmt_label:
        return c4m_cstr_format("[h2]{}", value_to_object(vm, value, NULL));
    case fmt_tcall:
        return c4m_cstr_format("builtin call of [em]{}[/]",
                               fmt_builtin_fn(value));
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
    return c4m_cstr_format("{:8x}",
                           c4m_box_u64(i * sizeof(c4m_zinstruction_t)));
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

    c4m_grid_t *grid = c4m_new(c4m_tspec_grid(),
                               c4m_kw("start_cols",
                                      c4m_ka(7),
                                      "header_rows",
                                      c4m_ka(1),
                                      "stripe",
                                      c4m_ka(true)));

    c4m_xlist_t *row = c4m_new_table_row();
    int64_t      len = c4m_xlist_len(m->instructions);

    c4m_xlist_append(row, c4m_new_utf8("Address"));
    c4m_xlist_append(row, c4m_new_utf8("Instruction"));
    c4m_xlist_append(row, c4m_new_utf8("Arg"));
    c4m_xlist_append(row, c4m_new_utf8("Immediate"));
    c4m_xlist_append(row, c4m_new_utf8("Type"));
    c4m_xlist_append(row, c4m_new_utf8("Module"));
    c4m_xlist_append(row, c4m_new_utf8("Line"));

    c4m_grid_add_row(grid, row);

    for (int64_t i = 0; i < len; i++) {
        row = c4m_new_table_row();

        c4m_zinstruction_t *ins  = c4m_xlist_get(m->instructions, i, NULL);
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

        c4m_xlist_append(row, addr);
        c4m_xlist_append(row, name);
        c4m_xlist_append(row, arg);
        c4m_xlist_append(row, imm);
        c4m_xlist_append(row, type);
        c4m_xlist_append(row, mod);
        c4m_xlist_append(row, line);
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

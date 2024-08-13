#define C4M_USE_INTERNAL_API
#include "con4m.h"

#undef C4M_STATIC_ASCII_STR
#define C4M_STATIC_ASCII_STR(var, contents) \
    c4m_utf8_t *var = c4m_new_utf8(contents)

#define FORMAT_NOT_IN_FN()                     \
    s = c4m_str_format(c4m_fmt_mod,            \
                       c4m_box_u64(frameno++), \
                       modname,                \
                       c4m_box_u64(lineno))
#define FORMAT_IN_FN()                                             \
    fnname = tstate->frame_stack[num_frames].targetfunc->funcname; \
    s      = c4m_str_format(c4m_fmt_fn,                            \
                       c4m_box_u64(frameno++),                \
                       fnname,                                \
                       modname,                               \
                       c4m_box_u64(lineno))
#define OUTPUT_FRAME() \
    c4m_ansi_render(s, f);

static inline c4m_list_t *
format_one_frame(c4m_vmthread_t *tstate, int n)
{
    c4m_list_t     *l = c4m_list(c4m_type_utf8());
    uint64_t       *pc;
    uint64_t       *line;
    c4m_zfn_info_t *f;
    c4m_utf8_t     *modname;
    c4m_utf8_t     *fname;

    if (n == tstate->num_frames) {
        c4m_module_t       *m = tstate->current_module;
        c4m_zinstruction_t *i = c4m_list_get(m->instructions, tstate->pc, NULL);

        pc      = c4m_box_u64(tstate->pc);
        modname = m->name;
        line    = c4m_box_u64(i->line_no);
    }
    else {
        c4m_vmframe_t *frame = &tstate->frame_stack[n];
        modname              = frame->call_module->name;
        pc                   = c4m_box_u64(frame->pc);
        line                 = c4m_box_u64(frame->calllineno);
    }

    if (n == 0) {
        f = NULL;
    }
    else {
        f = tstate->frame_stack[n - 1].targetfunc;
    }

    if (n == 1) {
        fname = c4m_new_utf8("(start of execution)");
    }
    else {
        fname = f ? f->funcname : c4m_new_utf8("(module toplevel)");
    }

    c4m_list_append(l, c4m_cstr_format("{:18x}", pc));
    c4m_list_append(l, c4m_cstr_format("{}:{}", modname, line));
    c4m_list_append(l, fname);

    return l;
}

#if defined(C4M_DEBUG)
extern int16_t *c4m_calculate_col_widths(c4m_grid_t *, int16_t, int16_t *);
#endif

c4m_grid_t *
c4m_get_backtrace(c4m_vmthread_t *tstate)
{
    if (!tstate->running) {
        return c4m_callout(c4m_new_utf8("Con4m is not running!"));
    }

    int         nframes = tstate->num_frames;
    c4m_list_t *hdr     = c4m_list(c4m_type_utf8());
    c4m_grid_t *bt      = c4m_new(c4m_type_grid(),
                             c4m_kw("start_cols",
                                    c4m_ka(3),
                                    "start_rows",
                                    c4m_ka(nframes + 1),
                                    "header_rows",
                                    c4m_ka(1),
                                    "container_tag",
                                    c4m_ka(c4m_new_utf8("table2")),
                                    "stripe",
                                    c4m_ka(true)));

    c4m_list_append(hdr, c4m_new_utf8("PC"));
    c4m_list_append(hdr, c4m_new_utf8("Location"));
    c4m_list_append(hdr, c4m_new_utf8("Function"));
    c4m_grid_add_row(bt, hdr);

    while (nframes > 0) {
        c4m_grid_add_row(bt, format_one_frame(tstate, nframes--));
    }

    // Snap columns to calculate a stand-alone with. If we have a C
    // backtrace, we will then resize columns to make both tables
    // the same width.

    c4m_snap_column(bt, 0);
    c4m_snap_column(bt, 1);
    c4m_snap_column(bt, 2);

    return bt;
}

static void
c4m_vm_exception(c4m_vmthread_t *tstate, c4m_exception_t *exc)
{
    c4m_stream_t *f      = c4m_get_stderr();
    c4m_grid_t   *bt     = c4m_get_backtrace(tstate);
    c4m_utf8_t   *to_out = c4m_rich_lit("[h2]Fatal Exception:[/] ");

    to_out = c4m_str_concat(to_out, c4m_exception_get_message(exc));
    to_out = c4m_str_concat(to_out, c4m_rich_lit("\n[h6]Con4m Trace:[/]"));

#if defined(C4M_DEBUG) && defined(C4M_BACKTRACE_SUPPORTED)
    int16_t  tmp;
    int16_t *widths1 = c4m_calculate_col_widths(exc->c_trace,
                                                C4M_GRID_TERMINAL_DIM,
                                                &tmp);
    int16_t *widths2 = c4m_calculate_col_widths(bt,
                                                C4M_GRID_TERMINAL_DIM,
                                                &tmp);

    for (int i = 0; i < 3; i++) {
        int w = c4m_max(widths1[i], widths2[i]);

        c4m_render_style_t *s = c4m_new(c4m_type_render_style(),
                                        c4m_kw("min_size",
                                               c4m_ka(w),
                                               "max_size",
                                               c4m_ka(w),
                                               "left_pad",
                                               c4m_ka(1),
                                               "right_pad",
                                               c4m_ka(1)));
        c4m_set_column_props(bt, i, s);
        c4m_set_column_props(exc->c_trace, i, s);
    }
#endif
    c4m_print(c4m_kw("stream", c4m_ka(f), "sep", c4m_ka('\n')), 2, to_out, bt);

#ifdef C4M_DEV
    c4m_stream_write_object(tstate->vm->run_state->print_stream,
                            c4m_exception_get_message(exc),
                            false);
    c4m_stream_putc(tstate->vm->run_state->print_stream, '\n');
#endif

    // This is currently just a stub that prints an rudimentary error
    // message and a stack trace. The caller handles how to
    // proceed. This will need to be changed later when a better error
    // handling framework is in place.

#if defined(C4M_DEBUG) && defined(C4M_BACKTRACE_SUPPORTED)
    if (!exc->c_trace) {
        c4m_printf("[h6]No C stack trace available.");
    }
    else {
        c4m_printf("[h6]C stack trace:");
        c4m_print(c4m_kw("stream", c4m_ka(f)), 1, exc->c_trace);
    }
#endif

    // The caller will know what to do on exception.
}

// So much of this VM implementation is based on trust that it seems silly to
// even include a stack underflow check anywhere at all, and maybe it is.
// However, if nothing else, it serves as documentation wherever the stack is
// touched to indicate how many values are expected.
//
// Note from John: in the original implementation this was just to
// help me both document for myself and help root out any codegen bugs
// with a helpful starting point instead of a crash.

#ifdef C4M_OMIT_UNDERFLOW_CHECKS
#define STACK_REQUIRE_VALUES(_n)
#else
#define STACK_REQUIRE_VALUES(_n)                              \
    if (tstate->sp > &tstate->stack[C4M_STACK_SIZE - (_n)]) { \
        C4M_CRAISE("stack underflow");                        \
    }
#endif

// Overflow checks are reasonable, however. The code generator has no way of
// knowing whether the code is generating is going to overflow the stack. It
// can only determine the maximum stack depth for a given function. But, it
// could compute the deepest stack depth and multiply by the maximum call depth
// to dynamically size the stack. Since that's not done at all, we need to check
// for stack overflow for safety.
#define STACK_REQUIRE_SLOTS(_n)              \
    if (tstate->sp - (_n) < tstate->stack) { \
        C4M_CRAISE("stack overflow");        \
    }

#define SIMPLE_COMPARE(op)                          \
    do {                                            \
        int64_t v1 = tstate->sp->sint;              \
        ++tstate->sp;                               \
        int64_t v2       = tstate->sp->sint;        \
        tstate->sp->sint = !!((int64_t)(v2 op v1)); \
    } while (0)

#define SIMPLE_COMPARE_UNSIGNED(op)                 \
    do {                                            \
        int64_t v1 = tstate->sp->uint;              \
        ++tstate->sp;                               \
        int64_t v2       = tstate->sp->uint;        \
        tstate->sp->uint = !!((int64_t)(v2 op v1)); \
    } while (0)

static c4m_obj_t
c4m_vm_variable(c4m_vmthread_t *tstate, c4m_zinstruction_t *i)
{
    return &tstate->vm->module_allocations[i->module_id][i->arg];
}

static inline bool
c4m_value_iszero(c4m_obj_t value)
{
    return 0 == value;
}

#if 0 // Will return soon

static c4m_str_t *
get_param_name(c4m_zparam_info_t *p, c4m_module_t *)
{
    if (p->attr != NULL && c4m_len(p->attr) > 0) {
        return p->attr;
    }
    return c4m_index_get(m->datasyms, (c4m_obj_t)p->offset);
}

static c4m_value_t *
get_param_value(c4m_vmthread_t *tstate, c4m_zparam_info_t *p)
{
    // if (p->userparam.type_info != NULL) {
    // return &p->userparam;
    //}
    if (p->have_default) {
        return &p->default_value;
    }

    c4m_str_t *name = get_param_name(p, tstate->current_module);

    C4M_STATIC_ASCII_STR(errstr, "parameter not set: ");
    c4m_utf8_t *msg = c4m_str_concat(errstr, name);
    C4M_STATIC_ASCII_STR(module_prefix, " (in module ");
    msg = c4m_str_concat(msg, module_prefix);
    msg = c4m_str_concat(msg, tstate->current_module->modname);
    C4M_STATIC_ASCII_STR(module_suffix, ")");
    msg = c4m_str_concat(msg, module_suffix);
    C4M_RAISE(msg);
}
#endif

static void
c4m_vm_module_enter(c4m_vmthread_t *tstate, c4m_zinstruction_t *i)
{
    // If there's already a lock, we always push ourselves to the module
    // lock stack. If there isn't, we start the stack if our module
    // has parameters.

#if 0 // Redoing parameters soon.
    int nparams = c4m_list_len(tstate->current_module->parameters);

    for (int32_t n = 0; n < nparams; ++n) {
        c4m_zparam_info_t *p = c4m_list_get(tstate->current_module->parameters,
                                            n,
                                            NULL);

        // Fill in all parameter values now. If there's a validator,
        // it will get called after this loop, along w/ a call to
        // ZParamCheck.

        if (p->attr && c4m_len(p->attr) > 0) {
            bool found;
            hatrack_dict_get(tstate->vm->attrs, p->attr, &found);
            if (!found) {
                c4m_obj_t value = get_param_value(tstate, p);
                c4m_vm_attr_set(tstate,
				p->attr,
				value,
				true,
				false,
				true);
            }
        }
    }
#endif
}

static void
c4m_vmframe_push(c4m_vmthread_t     *tstate,
                 c4m_zinstruction_t *i,
                 c4m_module_t       *call_module,
                 int32_t             call_pc,
                 c4m_module_t       *target_module,
                 c4m_zfn_info_t     *target_func,
                 int32_t             target_lineno)
{
    // TODO: Should probably recover call_pc off the stack when
    // needed, instead of adding the extra param here.

    if (C4M_MAX_CALL_DEPTH == tstate->num_frames) {
        C4M_CRAISE("maximum call depth reached");
    }

    c4m_vmframe_t *f = &tstate->frame_stack[tstate->num_frames];

    *f = (c4m_vmframe_t){
        .call_module  = call_module,
        .calllineno   = i->line_no,
        .targetline   = target_lineno,
        .targetmodule = target_module,
        .targetfunc   = target_func,
        .pc           = call_pc,
    };

    ++tstate->num_frames;
}

static void
c4m_vm_tcall(c4m_vmthread_t *tstate, c4m_zinstruction_t *i)
{
    bool        b;
    c4m_obj_t   obj;
    c4m_type_t *t;

    switch ((c4m_builtin_type_fn)i->arg) {
    case C4M_BI_TO_STR:
        STACK_REQUIRE_VALUES(1);

        obj = c4m_to_str(tstate->sp->rvalue,
                         c4m_get_my_type(tstate->sp->rvalue));

        tstate->sp->rvalue = obj;
        return;
    case C4M_BI_REPR:
        STACK_REQUIRE_VALUES(1);

        obj = c4m_repr(tstate->sp->rvalue,
                       c4m_get_my_type(tstate->sp->rvalue));

        tstate->sp->rvalue = obj;
        return;
    case C4M_BI_COERCE:
#if 0
        STACK_REQUIRE_VALUES(2);
        // srcType = i->type_info
        // dstType = tstate->sp->rvalue.type_info (should be same as .obj)
        // obj = tstate->sp[1]

        t   = tstate->sp->rvalue.type_info;
        obj = c4m_coerce(tstate->sp[1].rvalue.obj, i->type_info, t);

        ++tstate->sp;
        tstate->sp->rvalue = obj;
#endif
        return;
    case C4M_BI_FORMAT:
    case C4M_BI_ITEM_TYPE:
        C4M_CRAISE("Currently format cannot be called via tcall.");
    case C4M_BI_VIEW:
        // This is used to implement loops over objects; we pop the
        // container by pushing the view on, then also push
        // the length of the view.
        STACK_REQUIRE_VALUES(1);
        STACK_REQUIRE_SLOTS(1);
        do {
            uint64_t n;
            tstate->sp->rvalue = c4m_get_view(tstate->sp->rvalue,
                                              (int64_t *)&n);
            --tstate->sp;
            tstate->sp[0].uint = n;
        } while (0);

        return;
    case C4M_BI_COPY:
        STACK_REQUIRE_VALUES(1);

        obj = c4m_copy(tstate->sp->rvalue);

        tstate->sp->rvalue = obj;
        return;

        // clang-format off
#define BINOP_OBJ(_op, _t) \
    obj = (c4m_obj_t)(uintptr_t)((_t)(uintptr_t)tstate->sp[1].rvalue \
     _op (_t)(uintptr_t) tstate->sp[0].rvalue)
        // clang-format on

#define BINOP_INTEGERS(_op)       \
    case C4M_T_I8:                \
        BINOP_OBJ(_op, int8_t);   \
        break;                    \
    case C4M_T_I32:               \
        BINOP_OBJ(_op, int32_t);  \
        break;                    \
    case C4M_T_INT:               \
        BINOP_OBJ(_op, int64_t);  \
        break;                    \
    case C4M_T_BYTE:              \
        BINOP_OBJ(_op, uint8_t);  \
        break;                    \
    case C4M_T_CHAR:              \
        BINOP_OBJ(_op, uint32_t); \
        break;                    \
    case C4M_T_U32:               \
        BINOP_OBJ(_op, uint32_t); \
        break;                    \
    case C4M_T_UINT:              \
        BINOP_OBJ(_op, uint64_t); \
        break

#define BINOP_FLOATS(_op)       \
    case C4M_T_F32:             \
        BINOP_OBJ(_op, float);  \
        break;                  \
    case C4M_T_F64:             \
        BINOP_OBJ(_op, double); \
        break

    case C4M_BI_ADD:
        STACK_REQUIRE_VALUES(2);

        t = c4m_get_my_type(tstate->sp->rvalue);
        switch (t->typeid) {
            BINOP_INTEGERS(+);
            BINOP_FLOATS(+);
        default:
            obj = c4m_add(tstate->sp[1].rvalue, tstate->sp[0].rvalue);
        }
        ++tstate->sp;
        tstate->sp->rvalue = obj;
        return;
    case C4M_BI_SUB:
        STACK_REQUIRE_VALUES(2);
        t = c4m_get_my_type(tstate->sp->rvalue);
        switch (t->typeid) {
            BINOP_INTEGERS(-);
            BINOP_FLOATS(-);
        default:
            obj = c4m_sub(tstate->sp[1].rvalue, tstate->sp[0].rvalue);
        }
        ++tstate->sp;
        tstate->sp->rvalue = obj;
        return;
    case C4M_BI_MUL:
        STACK_REQUIRE_VALUES(2);
        t = c4m_get_my_type(tstate->sp->rvalue);
        switch (t->typeid) {
            BINOP_INTEGERS(*);
            BINOP_FLOATS(*);
        default:
            obj = c4m_mul(tstate->sp[1].rvalue, tstate->sp[0].rvalue);
        }
        ++tstate->sp;
        tstate->sp->rvalue = obj;
        return;
    case C4M_BI_DIV:
        STACK_REQUIRE_VALUES(2);
        t = c4m_get_my_type(tstate->sp->rvalue);
        switch (t->typeid) {
            BINOP_INTEGERS(/);
            BINOP_FLOATS(/);
        default:
            obj = c4m_div(tstate->sp[1].rvalue, tstate->sp[0].rvalue);
        }
        ++tstate->sp;
        tstate->sp->rvalue = obj;
        return;
    case C4M_BI_MOD:
        STACK_REQUIRE_VALUES(2);
        t = c4m_get_my_type(tstate->sp->rvalue);
        switch (t->typeid) {
            BINOP_INTEGERS(%);
        default:
            obj = c4m_mod(tstate->sp[1].rvalue, tstate->sp[0].rvalue);
        }
        ++tstate->sp;
        tstate->sp->rvalue = obj;
        return;
    case C4M_BI_EQ:
        STACK_REQUIRE_VALUES(2);

        b = c4m_eq(i->type_info,
                   tstate->sp[1].rvalue,
                   tstate->sp[0].rvalue);

        ++tstate->sp;
        tstate->sp->rvalue = (c4m_obj_t)b;
        return;
    case C4M_BI_LT:
        STACK_REQUIRE_VALUES(2);

        b = c4m_lt(i->type_info,
                   tstate->sp[1].rvalue,
                   tstate->sp[0].rvalue);

        ++tstate->sp;
        tstate->sp->rvalue = (c4m_obj_t)b;
        return;
    case C4M_BI_GT:
        STACK_REQUIRE_VALUES(2);

        b = c4m_gt(i->type_info,
                   tstate->sp[1].rvalue,
                   tstate->sp[0].rvalue);

        ++tstate->sp;
        tstate->sp->rvalue = (c4m_obj_t)b;
        return;
    case C4M_BI_LEN:
        STACK_REQUIRE_VALUES(1);
        tstate->sp->rvalue = (c4m_obj_t)c4m_len(tstate->sp->rvalue);
        return;
    case C4M_BI_INDEX_GET:
        STACK_REQUIRE_VALUES(2);
        // index = sp[0]
        // container = sp[1]
        obj = c4m_index_get(tstate->sp[1].rvalue, tstate->sp[0].rvalue);

        ++tstate->sp;
        tstate->sp->rvalue = obj;
        return;
    case C4M_BI_INDEX_SET:
        STACK_REQUIRE_VALUES(3);
        // index = sp[0]
        // container = sp[1]
        // value = sp[2]

        c4m_index_set(tstate->sp[2].rvalue,
                      tstate->sp[0].rvalue,
                      tstate->sp[1].rvalue);

        tstate->sp += 3;
        return;
    case C4M_BI_SLICE_GET:
        STACK_REQUIRE_VALUES(3);
        obj = c4m_slice_get(tstate->sp[2].rvalue,
                            (int64_t)tstate->sp[1].rvalue,
                            (int64_t)tstate->sp[0].rvalue);

        tstate->sp += 2;
        // type is already correct on the stack, since we're writing
        // over the container location and this is a slice.
        tstate->sp->rvalue = obj;
        return;
    case C4M_BI_SLICE_SET:
        STACK_REQUIRE_VALUES(4);
        // container = sp[3]
        // endIx = sp[2]
        // startIx = sp[1]
        // value = sp[0]

        c4m_slice_set(tstate->sp[3].rvalue,
                      (int64_t)tstate->sp[2].rvalue,
                      (int64_t)tstate->sp[1].rvalue,
                      tstate->sp[0].rvalue);

        tstate->sp += 4;
        return;

        // Note: we pass the full container type through the type field;
        // we assume the item type is a tuple of the items types, decaying
        // to the actual item type if only one item.
    case C4M_BI_CONTAINER_LIT:
        do {
            uint64_t    n  = tstate->sp[0].uint;
            c4m_type_t *ct = i->type_info;
            c4m_list_t *xl;

            ++tstate->sp;

            xl = c4m_new(c4m_type_list(c4m_type_ref()),
                         c4m_kw("length", c4m_ka(n)));

            while (n--) {
                c4m_list_set(xl, n, tstate->sp[0].vptr);
                ++tstate->sp;
            }

            --tstate->sp;

            tstate->sp[0].rvalue = c4m_container_literal(ct, xl, NULL);
        } while (0);
        return;

        // Nim version has others, but they're missing from c4m_builtin_type_fn
        // FIDiv, FFDiv, FShl, FShr, FBand, FBor, FBxor, FDictIndex, FAssignDIx,
        // FContainerLit
        //
        // Assumptions:
        //   * FIDiv and FFDiv combined -> FDiv
        //   * FDictIndex folded into FIndex (seems supported by dict code)
        //   * FAssignDIx folded into FAssignIx (seems supported by dict code)
        //
        // At least some of of these should be added to libcon4m as well,
        // but doing that is going to touch a lot of things, so it should be
        // done later, probably best done in a PR all on its own
    default:
        // Not implemented yet, or not called via C4M_ZTCall.
        break;
    }

    C4M_CRAISE("invalid tcall instruction");
}

static void
c4m_vm_0call(c4m_vmthread_t *tstate, c4m_zinstruction_t *i, int64_t ix)
{
    STACK_REQUIRE_SLOTS(2);

    // combine pc / module id together and push it onto the stack for recovery
    // on return
    --tstate->sp;
    tstate->sp->uint = ((uint64_t)tstate->pc << 32u) | tstate->current_module->module_id;
    --tstate->sp;

    tstate->sp->vptr = tstate->fp;
    tstate->fp       = (c4m_value_t *)&tstate->sp->vptr;

    c4m_module_t *old_module = tstate->current_module;

    c4m_zfn_info_t *fn = c4m_list_get(tstate->vm->obj->func_info,
                                      ix - 1,
                                      NULL);

    int32_t call_pc = tstate->pc;

    tstate->pc             = fn->offset;
    tstate->current_module = c4m_list_get(tstate->vm->obj->module_contents,
                                          fn->mid,
                                          NULL);

    c4m_zinstruction_t *nexti = c4m_list_get(tstate->current_module->instructions,
                                             tstate->pc,
                                             NULL);

    // push a frame onto the call stack
    c4m_vmframe_push(tstate,
                     i,
                     old_module,
                     call_pc,
                     tstate->current_module,
                     fn,
                     nexti->line_no);
}

static void
c4m_vm_call_module(c4m_vmthread_t *tstate, c4m_zinstruction_t *i)
{
    STACK_REQUIRE_SLOTS(2);

    // combine pc / module id together and push it onto the stack for recovery
    // on return
    --tstate->sp;
    tstate->sp->uint = ((uint64_t)tstate->pc << 32u) | tstate->current_module->module_id;
    --tstate->sp;

    tstate->sp->vptr = tstate->fp;
    tstate->fp       = (c4m_value_t *)&tstate->sp->vptr;

    c4m_module_t *old_module = tstate->current_module;

    int32_t call_pc        = tstate->pc;
    tstate->pc             = 0;
    tstate->current_module = c4m_list_get(tstate->vm->obj->module_contents,
                                          i->module_id,
                                          NULL);

    c4m_zinstruction_t *nexti = c4m_list_get(tstate->current_module->instructions,
                                             tstate->pc,
                                             NULL);

    // push a frame onto the call stack
    c4m_vmframe_push(tstate,
                     i,
                     old_module,
                     call_pc,
                     tstate->current_module,
                     NULL,
                     nexti->line_no);
}

static inline uint64_t
ffi_possibly_box(c4m_vmthread_t     *tstate,
                 c4m_zinstruction_t *i,
                 c4m_type_t         *dynamic_type,
                 int                 local_param)
{
    // TODO: Handle varargs.
    if (dynamic_type == NULL) {
        return tstate->sp[local_param].uint;
    }

    c4m_type_t *param = c4m_type_get_param(dynamic_type, local_param);
    param             = c4m_resolve_and_unbox(param);

    if (c4m_type_is_concrete(param)) {
        return tstate->sp[local_param].uint;
    }

    c4m_type_t *actual = c4m_type_get_param(i->type_info, local_param);

    actual = c4m_resolve_and_unbox(actual);

    if (c4m_type_is_value_type(actual)) {
        c4m_box_t box = {
            .u64 = tstate->sp[local_param].uint,
        };

        return (uint64_t)c4m_box_obj(box, actual);
    }

    return tstate->sp[local_param].uint;
}

static inline void
ffi_possible_ret_munge(c4m_vmthread_t *tstate, c4m_type_t *at, c4m_type_t *ft)
{
    // at == actual type; ft == formal type.
    at = c4m_resolve_and_unbox(at);
    ft = c4m_resolve_and_unbox(ft);

    if (!c4m_type_is_concrete(at)) {
        if (c4m_type_is_concrete(ft) && c4m_type_is_value_type(ft)) {
            tstate->r0 = c4m_box_obj((c4m_box_t){.v = tstate->r0}, ft);
        }
    }
    else {
        if (c4m_type_is_value_type(at) && !c4m_type_is_concrete(ft)) {
            tstate->r0 = c4m_unbox_obj(tstate->r0).v;
        }
    }

    return;
}

static void
c4m_vm_ffi_call(c4m_vmthread_t     *tstate,
                c4m_zinstruction_t *instr,
                int64_t             ix,
                c4m_type_t         *dynamic_type)
{
    c4m_ffi_decl_t *decl = c4m_list_get(tstate->vm->obj->ffi_info,
                                        instr->arg,
                                        NULL);

    if (decl == NULL) {
        C4M_CRAISE("Could not load external function.");
    }

    c4m_zffi_cif *ffiinfo     = &decl->cif;
    int           local_param = 0;
    void        **args;

    if (!ffiinfo->cif.nargs) {
        args = NULL;
    }
    else {
        args  = c4m_gc_array_value_alloc(void *, ffiinfo->cif.nargs);
        int n = ffiinfo->cif.nargs;

        for (unsigned int i = 0; i < ffiinfo->cif.nargs; i++) {
            // clang-format off
	    --n;

	    if (ffiinfo->str_convert &&
		n < 63 &&
		((1 << n) & ffiinfo->str_convert)) {

		c4m_utf8_t *s = (c4m_utf8_t *)tstate->sp[i].rvalue;
		s             = c4m_to_utf8(s);
		args[n]       = &s->data;
            }
            // clang-format on
            else {
                uint64_t raw;
                raw = ffi_possibly_box(tstate,
                                       instr,
                                       dynamic_type,
                                       i);

                c4m_box_t value = {.u64 = raw};

                c4m_box_t *box = c4m_new(c4m_type_box(c4m_type_i64()),
                                         value);
                args[n]        = c4m_ref_via_ffi_type(box,
                                               ffiinfo->cif.arg_types[n]);
            }

            if (n < 63 && ((1 << n) & ffiinfo->hold_info)) {
                c4m_gc_add_hold(tstate->sp[i].rvalue);
            }
        }
    }

    ffi_call(&ffiinfo->cif, ffiinfo->fptr, &tstate->r0, args);

    if (ffiinfo->str_convert & (1UL << 63)) {
        char *s    = (char *)tstate->r0;
        tstate->r0 = c4m_new_utf8(s);
    }

    if (dynamic_type != NULL) {
        ffi_possible_ret_munge(tstate,
                               c4m_type_get_param(dynamic_type, local_param),
                               c4m_type_get_param(instr->type_info,
                                                  local_param));
    }
}

static void
c4m_vm_foreign_z_call(c4m_vmthread_t *tstate, c4m_zinstruction_t *i, int64_t ix)
{
    // TODO foreign_z_call
}
c4m_zcallback_t *
c4m_new_zcallback()
{
    return c4m_new(c4m_type_callback());
}

static void
c4m_vm_run_callback(c4m_vmthread_t *tstate, c4m_zinstruction_t *i)
{
    STACK_REQUIRE_VALUES(1);

    c4m_zcallback_t *cb = tstate->sp->callback;
    ++tstate->sp;

    if (cb->ffi) {
        c4m_vm_ffi_call(tstate, i, cb->impl, cb->tid);
    }
    else if (tstate->running) {
        // The generated code will, in this branch, push the result
        // if merited.
        c4m_vm_0call(tstate, i, cb->impl);
    }
    else {
        c4m_vm_foreign_z_call(tstate, i, cb->impl);
    }
}

static void
c4m_vm_return(c4m_vmthread_t *tstate, c4m_zinstruction_t *i)
{
    if (!tstate->num_frames) {
        C4M_CRAISE("call stack underflow");
    }

    tstate->sp = tstate->fp;
    tstate->fp = tstate->sp->vptr;

    uint64_t v             = tstate->sp[1].uint;
    tstate->pc             = (v >> 32u);
    tstate->current_module = c4m_list_get(tstate->vm->obj->module_contents,
                                          (v & 0xFFFFFFFF),
                                          NULL);

    tstate->sp += 2;

    --tstate->num_frames; // pop call frame
}

static int
c4m_vm_runloop(c4m_vmthread_t *tstate_arg)
{
    // Other portions of the libcon4m API use exceptions to communicate errors,
    // and so we need to be prepared for that. We'll also use exceptions to
    // raise errors as well since already having to handle exceptions
    // complicates doing anything else, mainly because you can't use return in
    // an exception block.

    // For many ABIs, tstate_arg will be passed in a register that is not
    // preserved. Odds are pretty good that the compiler will save it on the
    // stack for its own reasons, but it doesn't have to. By declaring tstate
    // as volatile and assigning tstate_arg to it, we ensure that it's available
    // even after setjmp returns from a longjmp via tstate, but not tstate_arg!
    // This is crucial, because we need it in the except block and we need it
    // after the try block ends.
    c4m_vmthread_t *volatile const tstate = tstate_arg;
    c4m_zcallback_t *cb;
    c4m_mem_ptr     *static_mem = tstate->vm->obj->static_contents->items;

    // This temporary is used to hold popped operands during binary
    // operations.
    union {
        uint64_t uint;
        int64_t  sint;
        double   dbl;
    } rhs;

    C4M_TRY
    {
        for (;;) {
            c4m_zinstruction_t *i;

            i = c4m_list_get(tstate->current_module->instructions,
                             tstate->pc,
                             NULL);

#ifdef C4M_VM_DEBUG
            static bool  debug_on = (bool)(C4M_VM_DEBUG_DEFAULT);
            static char *debug_fmt_str =
                "[i]> {} (PC@{:x}; SP@{:x}; "
                "FP@{:x}; a = {}; i = {}; m = {})";

            if (debug_on && i->op != C4M_ZNop) {
                int num_stack_items = &tstate->stack[C4M_STACK_SIZE] - tstate->sp;
                printf("stack has %d items on it: ", num_stack_items);
                for (int i = 0; i < num_stack_items; i++) {
                    if (&tstate->sp[i] == tstate->fp) {
                        printf("\e[34m[%p]\e[0m ", tstate->sp[i].vptr);
                    }
                    else {
                        // stored program counter and module id.
                        if (&tstate->sp[i - 1] == tstate->fp) {
                            printf("\e[32m[pc: 0x%llx module: %lld]\e[0m ",
                                   tstate->sp[i].uint >> 28,
                                   tstate->sp[i].uint & 0xffffffff);
                        }
                        else {
                            if (&tstate->sp[i] > tstate->fp) {
                                // Older frames.
                                printf("\e[31m[%p]\e[0m ", tstate->sp[i].vptr);
                            }
                            else {
                                // This frame.
                                printf("\e[33m[%p]\e[0m ", tstate->sp[i].vptr);
                            }
                        }
                    }
                }
                printf("\n");
                c4m_print(
                    c4m_cstr_format(
                        debug_fmt_str,
                        c4m_fmt_instr_name(i),
                        c4m_box_u64(tstate->pc * 16),
                        c4m_box_u64((uint64_t)(void *)tstate->sp),
                        c4m_box_u64((uint64_t)(void *)tstate->fp),
                        c4m_box_i64((int64_t)i->arg),
                        c4m_box_i64((int64_t)i->immediate),
                        c4m_box_u64((uint64_t)tstate->current_module->module_id)));
            }

#endif

            switch (i->op) {
            case C4M_ZNop:
                break;
            case C4M_ZMoveSp:
                if (i->arg > 0) {
                    STACK_REQUIRE_SLOTS(i->arg);
                }
                else {
                    STACK_REQUIRE_VALUES(i->arg);
                }
                tstate->sp -= i->arg;
                break;
            case C4M_ZPushConstObj:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                *tstate->sp = (c4m_value_t){
                    .uint = static_mem[i->arg].nonpointer,
                };
                break;
            case C4M_ZPushConstRef:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                *tstate->sp = (c4m_value_t){
                    .rvalue = (void *)static_mem + i->arg,
                };
                break;
            case C4M_ZDeref:
                STACK_REQUIRE_VALUES(1);
                tstate->sp->uint = *(uint64_t *)tstate->sp->uint;
                break;
            case C4M_ZPushImm:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                *tstate->sp = (c4m_value_t){
                    .rvalue = (c4m_obj_t)i->immediate,
                };
                break;
            case C4M_ZPushLocalObj:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                tstate->sp->rvalue = tstate->fp[-i->arg].rvalue;
                break;
            case C4M_ZPushLocalRef:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                *tstate->sp = (c4m_value_t){
                    .lvalue = &tstate->fp[-i->arg].rvalue,
                };
                break;
            case C4M_ZPushStaticObj:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                *tstate->sp = (c4m_value_t){
                    .rvalue = *(c4m_obj_t *)c4m_vm_variable(tstate, i),
                };
                break;
            case C4M_ZPushStaticRef:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                *tstate->sp = (c4m_value_t){
                    .lvalue = c4m_vm_variable(tstate, i),
                };
                break;
            case C4M_ZDupTop:
                STACK_REQUIRE_VALUES(1);
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                tstate->sp[0] = tstate->sp[1];
                break;
            case C4M_ZPop:
                STACK_REQUIRE_VALUES(1);
                ++tstate->sp;
                break;
            case C4M_ZJz:
                STACK_REQUIRE_VALUES(1);
                if (c4m_value_iszero(tstate->sp->rvalue)) {
                    tstate->pc = i->arg;
                    continue;
                }
                ++tstate->sp;
                break;
            case C4M_ZJnz:
                STACK_REQUIRE_VALUES(1);
                if (!c4m_value_iszero(tstate->sp->rvalue)) {
                    tstate->pc = i->arg;
                    continue;
                }
                ++tstate->sp;
                break;
            case C4M_ZJ:
                tstate->pc = i->arg;
                continue;
            case C4M_ZAdd:
                STACK_REQUIRE_VALUES(2);
                rhs.sint = tstate->sp[0].sint;
                ++tstate->sp;
                tstate->sp[0].uint += rhs.sint;
                break;
            case C4M_ZSub:
                STACK_REQUIRE_VALUES(2);
                rhs.sint = tstate->sp[0].sint;
                ++tstate->sp;

                tstate->sp[0].sint -= rhs.sint;
                break;
            case C4M_ZSubNoPop:
                STACK_REQUIRE_VALUES(2);
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                tstate->sp[0].sint = tstate->sp[2].sint - tstate->sp[1].sint;
                break;
            case C4M_ZMul:
                STACK_REQUIRE_VALUES(2);
                rhs.sint = tstate->sp[0].sint;
                ++tstate->sp;
                tstate->sp[0].uint *= rhs.sint;
                break;
            case C4M_ZDiv:
                STACK_REQUIRE_VALUES(2);
                rhs.sint = tstate->sp[0].sint;
                ++tstate->sp;
                if (rhs.sint == 0) {
                    C4M_CRAISE("Division by zero error.");
                }
                tstate->sp[0].sint /= rhs.sint;
                break;
            case C4M_ZMod:
                STACK_REQUIRE_VALUES(2);
                rhs.sint = tstate->sp[0].sint;
                ++tstate->sp;
                tstate->sp[0].uint %= rhs.sint;
                break;
            case C4M_ZUAdd:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint += rhs.uint;
                break;
            case C4M_ZUSub:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint -= rhs.uint;
                break;
            case C4M_ZUMul:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint *= rhs.uint;
                break;
            case C4M_ZUDiv:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint /= rhs.uint;
                break;
            case C4M_ZUMod:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint %= rhs.uint;
                break;
            case C4M_ZFAdd:
                STACK_REQUIRE_VALUES(2);
                rhs.dbl = tstate->sp[0].dbl;
                ++tstate->sp;
                tstate->sp[0].dbl += rhs.dbl;
                break;
            case C4M_ZFSub:
                STACK_REQUIRE_VALUES(2);
                rhs.dbl = tstate->sp[0].dbl;
                ++tstate->sp;
                tstate->sp[0].dbl -= rhs.dbl;
                break;
            case C4M_ZFMul:
                STACK_REQUIRE_VALUES(2);
                rhs.dbl = tstate->sp[0].dbl;
                ++tstate->sp;
                tstate->sp[0].dbl *= rhs.dbl;
                break;
            case C4M_ZFDiv:
                STACK_REQUIRE_VALUES(2);
                rhs.dbl = tstate->sp[0].dbl;
                ++tstate->sp;
                tstate->sp[0].dbl /= rhs.dbl;
                break;
            case C4M_ZBOr:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint |= rhs.uint;
                break;
            case C4M_ZBAnd:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint &= rhs.uint;
                break;
            case C4M_ZShl:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint <<= rhs.uint;
                break;
            case C4M_ZShlI:
                STACK_REQUIRE_VALUES(1);
                rhs.uint           = tstate->sp[0].uint;
                tstate->sp[0].uint = i->arg << tstate->sp[0].uint;
                break;
            case C4M_ZShr:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint >>= rhs.uint;
                break;
            case C4M_ZBXOr:
                STACK_REQUIRE_VALUES(2);
                rhs.uint = tstate->sp[0].uint;
                ++tstate->sp;
                tstate->sp[0].uint ^= rhs.uint;
                break;
            case C4M_ZBNot:
                STACK_REQUIRE_VALUES(1);
                tstate->sp[0].uint = ~tstate->sp[0].uint;
                break;
            case C4M_ZNot:
                STACK_REQUIRE_VALUES(1);
                tstate->sp->uint = !tstate->sp->uint;
                break;
            case C4M_ZAbs:
                STACK_REQUIRE_VALUES(1);
                do {
                    // Done w/o a branch; since value is signed,
                    // when we shift right, if it's negative, we sign
                    // extend to all ones. Meaning, we end up with
                    // either 64 ones or 64 zeros.
                    //
                    // Then, if we DO flip the sign, we need to add back 1;
                    // if we don't, we add back in 0.
                    int64_t  value = (int64_t)tstate->sp->uint;
                    uint64_t tmp   = value >> 63;
                    value ^= tmp;
                    value += tmp & 1;
                    tstate->sp->uint = (uint64_t)value;
                } while (0);
                break;
            case C4M_ZGetSign:
                STACK_REQUIRE_VALUES(1);
                do {
                    // Here, we get tmp to the point where it's either -1
                    // or 0, then OR in a 1, which will do nothing to -1,
                    // and will turn the 0 to 1.
                    tstate->sp->sint >>= 63;
                    tstate->sp->sint |= 1;
                } while (0);
                break;
            case C4M_ZHalt:
                C4M_JUMP_TO_TRY_END();
            case C4M_ZSwap:
                STACK_REQUIRE_VALUES(2);
                do {
                    c4m_value_t tmp = tstate->sp[0];
                    tstate->sp[0]   = tstate->sp[1];
                    tstate->sp[1]   = tmp;
                } while (0);
                break;
            case C4M_ZLoadFromAttr:
                STACK_REQUIRE_VALUES(1);
                do {
                    bool        found = true;
                    c4m_utf8_t *key   = tstate->sp->vptr;
                    c4m_obj_t   val;
                    uint64_t    flag = i->immediate;

                    if (flag) {
                        val = c4m_vm_attr_get(tstate, key, &found);
                    }
                    else {
                        val = c4m_vm_attr_get(tstate, key, NULL);
                    }

                    // If we didn't pass the reference to `found`,
                    // then an exception generally gets thrown if the
                    // attr doesn't exist, which is why `found` is
                    // true by default.
                    if (found && c4m_type_is_value_type(i->type_info)) {
                        c4m_box_t box      = c4m_unbox_obj((c4m_box_t *)val);
                        tstate->sp[0].vptr = box.v;
                    }
                    else {
                        *tstate->sp = (c4m_value_t){
                            .lvalue = val,
                        };
                    }

                    // Only push the status if it was explicitly requested.
                    if (flag) {
                        *--tstate->sp = (c4m_value_t){
                            .uint = found ? 1 : 0,
                        };
                    }
                } while (0);
                break;
            case C4M_ZAssignAttr:
                STACK_REQUIRE_VALUES(2);
                do {
                    void *val = tstate->sp[0].vptr;

                    if (c4m_type_is_value_type(i->type_info)) {
                        c4m_box_t item = {
                            .v = val,
                        };

                        val = c4m_box_obj(item, i->type_info);
                    }

                    c4m_vm_attr_set(tstate,
                                    tstate->sp[1].vptr,
                                    val,
                                    i->type_info,
                                    i->arg != 0,
                                    false,
                                    false);
                    tstate->sp += 2;
                } while (0);
                break;
            case C4M_ZLockOnWrite:
                STACK_REQUIRE_VALUES(1);
                do {
                    c4m_utf8_t *key = tstate->sp->vptr;
                    c4m_vm_attr_lock(tstate, key, true);
                } while (0);
                break;
            case C4M_ZLoadFromView:
                STACK_REQUIRE_VALUES(2);
                STACK_REQUIRE_SLOTS(2); // Usually 1, except w/ dict.
                do {
                    uint64_t   obj_len   = tstate->sp->uint;
                    void     **view_slot = &(tstate->sp + 1)->rvalue;
                    char      *p         = *(char **)view_slot;
                    c4m_box_t *box       = (c4m_box_t *)p;

                    --tstate->sp;

                    switch (obj_len) {
                    case 1:
                        tstate->sp->uint = box->u8;
                        *view_slot       = (void *)(p + 1);
                        break;
                    case 2:
                        tstate->sp->uint = box->u16;
                        *view_slot       = (void *)(p + 2);
                        break;
                    case 4:
                        tstate->sp->uint = box->u32;
                        *view_slot       = (void *)(p + 4);
                        break;
                    case 8:
                        tstate->sp->uint = box->u64;
                        *view_slot       = (void *)(p + 8);
                        // This is the only size that can be a dict.
                        // Push the value on first.
                        if (i->arg) {
                            --tstate->sp;
                            p += 8;
                            box              = (c4m_box_t *)p;
                            tstate->sp->uint = box->u64;
                            *view_slot       = (p + 8);
                        }
                        break;
                    default:
                        do {
                            uint64_t count  = (uint64_t)(tstate->r1);
                            uint64_t bit_ix = (count - 1) % 64;
                            uint64_t val    = **(uint64_t **)view_slot;

                            tstate->sp->uint = val & (1 << bit_ix);

                            if (bit_ix == 63) {
                                *view_slot += 1;
                            }
                        } while (0);
                    }
                } while (0);
                break;
            case C4M_ZStoreImm:
                *(c4m_obj_t *)c4m_vm_variable(tstate, i) = (c4m_obj_t)i->immediate;
                break;
            case C4M_ZPushObjType:
                // Name is a a bit of a mis-name because it also pops
                // the object. Should be ZReplaceObjWType
                STACK_REQUIRE_SLOTS(1);
                do {
                    c4m_type_t *type = c4m_get_my_type(tstate->sp->rvalue);

                    *tstate->sp = (c4m_value_t){
                        .rvalue = type,
                    };
                } while (0);
                break;
            case C4M_ZTypeCmp:
                STACK_REQUIRE_VALUES(2);
                do {
                    c4m_type_t *t1 = tstate->sp[0].rvalue;
                    c4m_type_t *t2 = tstate->sp[1].rvalue;

                    ++tstate->sp;

                    // Does NOT check for coercible.
                    tstate->sp->uint = (uint64_t)c4m_types_are_compat(t1,
                                                                      t2,
                                                                      NULL);
                } while (0);
                break;
            case C4M_ZCmp:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE(==);
                break;
            case C4M_ZLt:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE(<);
                break;
            case C4M_ZLte:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE(<=);
                break;
            case C4M_ZGt:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE(>);
                break;
            case C4M_ZGte:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE(>=);
                break;
            case C4M_ZULt:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE_UNSIGNED(<);
                break;
            case C4M_ZULte:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE_UNSIGNED(<=);
                break;
            case C4M_ZUGt:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE_UNSIGNED(>);
                break;
            case C4M_ZUGte:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE_UNSIGNED(>=);
                break;
            case C4M_ZNeq:
                STACK_REQUIRE_VALUES(2);
                SIMPLE_COMPARE(!=);
                break;
            case C4M_ZGteNoPop:
                STACK_REQUIRE_VALUES(2);
                STACK_REQUIRE_SLOTS(1);
                do {
                    uint64_t v1 = tstate->sp->uint;
                    uint64_t v2 = (tstate->sp + 1)->uint;
                    --tstate->sp;
                    tstate->sp->uint = (uint64_t)(v2 >= v1);
                } while (0);
                break;
            case C4M_ZCmpNoPop:
                STACK_REQUIRE_VALUES(2);
                STACK_REQUIRE_SLOTS(1);
                do {
                    uint64_t v1 = tstate->sp->uint;
                    uint64_t v2 = (tstate->sp + 1)->uint;
                    --tstate->sp;
                    tstate->sp->uint = (uint64_t)(v2 == v1);
                } while (0);
                break;
            case C4M_ZUnsteal:
                STACK_REQUIRE_VALUES(1);
                STACK_REQUIRE_SLOTS(1);
                *(tstate->sp - 1) = (c4m_value_t){
                    .static_ptr = tstate->sp->static_ptr & 0x07,
                };
                tstate->sp->static_ptr &= ~(0x07ULL);
                --tstate->sp;
                break;
            case C4M_ZTCall:
                c4m_vm_tcall(tstate, i);
                break;
            case C4M_Z0Call:
                c4m_vm_0call(tstate, i, i->arg);
                break;
            case C4M_ZCallModule:
                c4m_vm_call_module(tstate, i);
                break;
            case C4M_ZRunCallback:
                c4m_vm_run_callback(tstate, i);
                break;
            case C4M_ZPushFfiPtr:;
                STACK_REQUIRE_VALUES(1);
                cb = c4m_new_zcallback();

                *cb = (c4m_zcallback_t){
                    .name       = tstate->sp->vptr,
                    .tid        = i->type_info,
                    .impl       = i->arg,
                    .ffi        = true,
                    .skip_boxes = (bool)i->immediate,
                };

                tstate->sp->vptr = cb;
                break;
            case C4M_ZPushVmPtr:;
                STACK_REQUIRE_VALUES(1);
                cb = c4m_new_zcallback();

                *cb = (c4m_zcallback_t){
                    .name = tstate->sp->vptr,
                    .tid  = i->type_info,
                    .impl = i->arg,
                    .mid  = i->module_id,
                };

                tstate->sp->vptr = cb;
                break;
            case C4M_ZRet:
                c4m_vm_return(tstate, i);
                break;
            case C4M_ZModuleEnter:
                c4m_vm_module_enter(tstate, i);
                break;
            case C4M_ZModuleRet:
                if (tstate->num_frames <= 2) {
                    C4M_JUMP_TO_TRY_END();
                }
                c4m_vm_return(tstate, i);
                break;
            case C4M_ZFFICall:
                c4m_vm_ffi_call(tstate, i, i->arg, NULL);
                break;
            case C4M_ZSObjNew:
                STACK_REQUIRE_SLOTS(1);
                do {
                    c4m_obj_t obj = static_mem[i->immediate].v;

                    if (NULL == obj) {
                        C4M_CRAISE("could not unmarshal");
                    }
                    obj = c4m_copy(obj);

                    --tstate->sp;
                    tstate->sp->rvalue = obj;
                } while (0);
                break;
            case C4M_ZAssignToLoc:
                STACK_REQUIRE_VALUES(2);
                *tstate->sp[0].lvalue = tstate->sp[1].rvalue;
                tstate->sp += 2;
                break;
            case C4M_ZAssert:
                STACK_REQUIRE_VALUES(1);
                if (!c4m_value_iszero(tstate->sp->rvalue)) {
                    ++tstate->sp;
                }
                else {
                    C4M_CRAISE("assertion failed");
                }
                break;
#ifdef C4M_DEV
            case C4M_ZDebug:
#ifdef C4M_VM_DEBUG
                debug_on = (bool)i->arg;
#endif
                break;
                // This is not threadsafe. It's just for early days.
            case C4M_ZPrint:
                STACK_REQUIRE_VALUES(1);
                c4m_print(tstate->sp->rvalue);
                c4m_stream_write_object(tstate->vm->run_state->print_stream,
                                        tstate->sp->rvalue,
                                        false);
                c4m_stream_putc(tstate->vm->run_state->print_stream, '\n');
                ++tstate->sp;
                break;
#endif
            case C4M_ZPopToR0:
                STACK_REQUIRE_VALUES(1);
                tstate->r0 = tstate->sp->rvalue;
                ++tstate->sp;
                break;
            case C4M_ZPushFromR0:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                tstate->sp->rvalue = tstate->r0;
                break;
            case C4M_Z0R0c00l:
                tstate->r0 = (void *)NULL;
                break;
            case C4M_ZPopToR1:
                STACK_REQUIRE_VALUES(1);
                tstate->r1 = tstate->sp->rvalue;
                ++tstate->sp;
                break;
            case C4M_ZPushFromR1:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                tstate->sp->rvalue = tstate->r1;
                break;
            case C4M_ZPopToR2:
                STACK_REQUIRE_VALUES(1);
                tstate->r2 = tstate->sp->rvalue;
                ++tstate->sp;
                break;
            case C4M_ZPushFromR2:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                tstate->sp->rvalue = tstate->r2;
                break;
            case C4M_ZPopToR3:
                STACK_REQUIRE_VALUES(1);
                tstate->r3 = tstate->sp->rvalue;
                ++tstate->sp;
                break;
            case C4M_ZPushFromR3:
                --tstate->sp;
                STACK_REQUIRE_SLOTS(1);
                tstate->sp->rvalue = tstate->r3;
                break;
            case C4M_ZBox:;
                STACK_REQUIRE_VALUES(1);
                c4m_box_t item = {
                    .u64 = tstate->sp->uint,
                };
                tstate->sp->rvalue = c4m_box_obj(item, i->type_info);
                break;
            case C4M_ZUnbox:
                STACK_REQUIRE_VALUES(1);
                tstate->sp->uint = c4m_unbox_obj(tstate->sp->rvalue).u64;
                break;
            case C4M_ZUnpack:
                for (int32_t x = 1; x <= i->arg; ++x) {
                    *tstate->sp[0].lvalue = c4m_tuple_get(tstate->r1,
                                                          i->arg - x);
                    ++tstate->sp;
                }
                break;
            case C4M_ZBail:
                STACK_REQUIRE_VALUES(1);
                C4M_RAISE(tstate->sp->rvalue);
                break;
            case C4M_ZLockMutex:
                STACK_REQUIRE_VALUES(1);
                pthread_mutex_lock((pthread_mutex_t *)c4m_vm_variable(tstate,
                                                                      i));
                break;
            case C4M_ZUnlockMutex:
                STACK_REQUIRE_VALUES(1);
                pthread_mutex_unlock((pthread_mutex_t *)c4m_vm_variable(tstate,
                                                                        i));
                break;
            }

            ++tstate->pc;
        }
    }
    C4M_EXCEPT
    {
        c4m_vm_exception(tstate, C4M_X_CUR());

        // not strictly needed, but it silences a compiler warning about an
        // unused label due to having C4M_FINALLY, and it otherwise does no
        // harm.
        C4M_JUMP_TO_FINALLY();
    }
    C4M_FINALLY
    {
        // we don't need the finally block, but exceptions.h says we need to
        // have it and I don't want to trace all of the logic to confirm. So
        // we have it :)
    }
    C4M_TRY_END;

    return tstate->error ? -1 : 0;
}

static thread_local c4m_vmthread_t *thread_runtime = NULL;

c4m_vmthread_t *
c4m_thread_runtime_acquire()
{
    return thread_runtime;
}

int
c4m_vmthread_run(c4m_vmthread_t *tstate)
{
    assert(!tstate->running);
    tstate->running = true;

    thread_runtime = tstate;

    c4m_zinstruction_t *i = c4m_list_get(tstate->current_module->instructions,
                                         tstate->pc,
                                         NULL);

    c4m_vmframe_push(tstate,
                     i,
                     tstate->current_module,
                     0,
                     tstate->current_module,
                     NULL,
                     0);

    int result = c4m_vm_runloop(tstate);
    --tstate->num_frames;
    tstate->running = false;

    return result;
}

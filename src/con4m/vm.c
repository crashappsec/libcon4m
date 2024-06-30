#define C4M_USE_INTERNAL_API
#include "con4m.h"

// Error handling needs to be revisited. The Nim code has a whole
// bunch of error handling infrastructure that hasn't been ported to C
// yet, so what's here is just a stopgap until that is done. It'll be
// done separately from the main VM code drop. So for now, errors are
// just raised via C4M_RAISE or C4M_CRAISE, and they're just string
// error messages, often inadequately constructed. The implementation
// of c4m_vm_exception that catches those exceptions is similarly just
// a stopgap.

// Full attribute validation and default population needs to be
// done. There's a whole lot of infrastructure there that needs to be
// built out. It'll be done separately from the main VM code drop.

// Marshalling and unmarshalling of object files needs to be
// done. That's a large body of work that'll be done separately from
// the main VM code drop.

// I seem to have broken this macro, so here's a replacement for the
// moment, but not static.

#undef C4M_STATIC_ASCII_STR
#define C4M_STATIC_ASCII_STR(var, contents) \
    c4m_utf8_t *var = c4m_new_utf8(contents)

static c4m_utf8_t *c4m_err_prefix = NULL;
static c4m_utf8_t *c4m_err_trace;
static c4m_utf8_t *c4m_fmt_mod;
static c4m_utf8_t *c4m_fmt_fn;

static inline void
c4m_init_strings()
{
    if (c4m_err_prefix == NULL) {
        c4m_err_prefix = c4m_rich_lit("[h1]Runtime Error: ");
        c4m_err_trace  = c4m_rich_lit("\n[h2]Stack Trace:\n");
        c4m_fmt_mod    = c4m_new_utf8("  [b]{}:[/] in module: [em]{}:{}[/]\n");
        c4m_fmt_fn     = c4m_new_utf8(
            "  [b]{}:[/] in func: [em]{}[/], "
                "module: [em]{}:{}[/]\n");

        c4m_gc_register_root(&c4m_err_prefix, 1);
        c4m_gc_register_root(&c4m_err_trace, 1);
        c4m_gc_register_root(&c4m_fmt_mod, 1);
        c4m_gc_register_root(&c4m_fmt_fn, 1);
    }
}

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

static void
c4m_vm_exception(c4m_vmthread_t *tstate, c4m_exception_t *exc)
{
    c4m_init_strings();
    // This is currently just a stub that prints an rudimentary error
    // message and a stack trace. The caller handles how to
    // proceed. This will need to be changed later when a better error
    // handling framework is in place.

    c4m_stream_t *f = c4m_get_stderr();
    c4m_utf8_t   *s;
    c4m_utf8_t   *fnname;
    c4m_utf8_t   *modname;
    uint64_t      frameno    = 0;
    int           num_frames = tstate->num_frames - 1;
    uint64_t      lineno;

    // instruction that triggered the error:
    c4m_zinstruction_t *i = c4m_list_get(tstate->current_module->instructions,
                                         tstate->pc,
                                         NULL);

    lineno  = i->line_no;
    modname = tstate->current_module->modname;

    if (tstate->frame_stack[num_frames].targetfunc == NULL) {
        FORMAT_NOT_IN_FN();
    }
    else {
        FORMAT_IN_FN();
    }

    c4m_ansi_render(c4m_err_prefix, f);
    c4m_ansi_render(c4m_exception_get_message(exc), f);
    c4m_ansi_render(c4m_err_trace, f);

    OUTPUT_FRAME();
    // When a frame pushes for a call, the calling module and line number are
    // recorded, but the calling function is not. The called module, function,
    // and line are also recorded. To print the frame information that we want
    // (calling function, calling module, and lineno at call site), we need to
    // use two frames. We start with num_frames - 2, because we've already
    // reported the error location, which would be the first frame and handled
    // differently because line number comes from the current instruction.
    for (int32_t n = num_frames - 1; n > 0; --n) {
        // get function and module from current frame
        c4m_vmframe_t *frame        = &tstate->frame_stack[n];
        // get lineno from called frame
        c4m_vmframe_t *called_frame = &tstate->frame_stack[n + 1];

        modname = frame->targetmodule->modname;
        lineno  = called_frame->calllineno;

        if (frame->targetfunc == NULL) {
            FORMAT_NOT_IN_FN();
        }
        else {
            FORMAT_IN_FN();
        }
        OUTPUT_FRAME();
    }

    // Nim calls quit() in its error handling, but we're a library intended to
    // be embedded, so we don't want to do that. The caller will know what to
    // do.
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
#define STACK_REQUIRE_VALUES(_n)                               \
    if (tstate->sp >= &tstate->stack[STACK_SIZE - (_n) + 1]) { \
        C4M_CRAISE("stack underflow");                         \
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

#define SIMPLE_COMPARE(op)                       \
    do {                                         \
        uint64_t v1 = tstate->sp->uint;          \
        ++tstate->sp;                            \
        uint64_t v2      = tstate->sp->uint;     \
        tstate->sp->uint = (uint64_t)(v2 op v1); \
    } while (0)

static c4m_value_t *
c4m_vm_variable(c4m_vmthread_t *tstate, c4m_zinstruction_t *i)
{
    return &tstate->vm->module_allocations[i->module_id][i->arg];
}

static inline bool
c4m_value_iszero(c4m_value_t *value)
{
    return 0 == value->obj;
}

static c4m_str_t *
get_param_name(c4m_zparam_info_t *p, c4m_zmodule_info_t *m)
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

static void
c4m_vm_module_enter(c4m_vmthread_t *tstate, c4m_zinstruction_t *i)
{
    if (!i->arg) {
        if (c4m_list_len(tstate->module_lock_stack) > 0) {
            c4m_list_append(tstate->module_lock_stack,
                            c4m_list_get(tstate->module_lock_stack,
                                         -1,
                                         NULL));
        }
        else {
            c4m_list_append(tstate->module_lock_stack, 0);
        }
        return;
    }

    for (int32_t n = 0; n < c4m_list_len(tstate->current_module->parameters); ++n) {
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
                c4m_value_t *value = get_param_value(tstate, p);
                c4m_vm_attr_set(tstate, p->attr, value, true, false, true);
            }
        }
    }

    c4m_list_append(tstate->module_lock_stack,
                    (c4m_obj_t)(uint64_t)tstate->current_module->module_id);
}

static c4m_utf8_t *
c4m_vm_attr_key(c4m_vmthread_t *tstate, uint64_t static_ptr)
{
    return (c4m_utf8_t *)(tstate->vm->obj->static_data->data + tstate->sp->static_ptr);
}

static void
c4m_vmframe_push(c4m_vmthread_t     *tstate,
                 c4m_zinstruction_t *i,
                 c4m_zmodule_info_t *call_module,
                 c4m_zmodule_info_t *target_module,
                 c4m_zfn_info_t     *target_func,
                 int32_t             target_lineno)
{
    if (MAX_CALL_DEPTH == tstate->num_frames) {
        C4M_CRAISE("maximum call depth reached");
    }

    c4m_vmframe_t *f = &tstate->frame_stack[tstate->num_frames];

    *f = (c4m_vmframe_t){
        .call_module  = call_module,
        .calllineno   = i->line_no,
        .targetline   = target_lineno,
        .targetmodule = target_module,
        .targetfunc   = target_func,
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

        obj = c4m_to_str(tstate->sp->rvalue.obj,
                         c4m_get_my_type(tstate->sp->rvalue.obj));

        tstate->sp->rvalue = (c4m_value_t){
            .obj = obj,
        };
        return;
    case C4M_BI_REPR:
        STACK_REQUIRE_VALUES(1);

        obj = c4m_repr(tstate->sp->rvalue.obj,
                       c4m_get_my_type(tstate->sp->rvalue.obj));

        tstate->sp->rvalue = (c4m_value_t){
            .obj = obj,
        };
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
        tstate->sp->rvalue = (c4m_value_t){
            .obj       = obj,
        };
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
            tstate->sp->rvalue.obj = c4m_get_view(tstate->sp->rvalue.obj,
                                                  (int64_t *)&n);
            --tstate->sp;
            tstate->sp[0].uint = n;
        } while (0);

        return;
    case C4M_BI_COPY:
        STACK_REQUIRE_VALUES(1);

        obj = c4m_copy_object(tstate->sp->rvalue.obj);

        tstate->sp->rvalue = (c4m_value_t){
            .obj = obj,
        };
        return;

        // clang-format off
#define BINOP_OBJ(_op, _t) \
    obj = (c4m_obj_t)(uintptr_t)((_t)(uintptr_t)tstate->sp[1].rvalue.obj \
     _op (_t)(uintptr_t) tstate->sp[0].rvalue.obj)
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

        t = c4m_get_my_type(tstate->sp->rvalue.obj);
        switch (t->typeid) {
            BINOP_INTEGERS(+);
            BINOP_FLOATS(+);
        default:
            obj = c4m_add(tstate->sp[1].rvalue.obj, tstate->sp[0].rvalue.obj);
        }
        ++tstate->sp;
        tstate->sp->rvalue.obj = obj;
        return;
    case C4M_BI_SUB:
        STACK_REQUIRE_VALUES(2);
        t = c4m_get_my_type(tstate->sp->rvalue.obj);
        switch (t->typeid) {
            BINOP_INTEGERS(-);
            BINOP_FLOATS(-);
        default:
            obj = c4m_sub(tstate->sp[1].rvalue.obj, tstate->sp[0].rvalue.obj);
        }
        ++tstate->sp;
        tstate->sp->rvalue.obj = obj;
        return;
    case C4M_BI_MUL:
        STACK_REQUIRE_VALUES(2);
        t = c4m_get_my_type(tstate->sp->rvalue.obj);
        switch (t->typeid) {
            BINOP_INTEGERS(*);
            BINOP_FLOATS(*);
        default:
            obj = c4m_mul(tstate->sp[1].rvalue.obj, tstate->sp[0].rvalue.obj);
        }
        ++tstate->sp;
        tstate->sp->rvalue.obj = obj;
        return;
    case C4M_BI_DIV:
        STACK_REQUIRE_VALUES(2);
        t = c4m_get_my_type(tstate->sp->rvalue.obj);
        switch (t->typeid) {
            BINOP_INTEGERS(/);
            BINOP_FLOATS(/);
        default:
            obj = c4m_div(tstate->sp[1].rvalue.obj, tstate->sp[0].rvalue.obj);
        }
        ++tstate->sp;
        tstate->sp->rvalue.obj = obj;
        return;
    case C4M_BI_MOD:
        STACK_REQUIRE_VALUES(2);
        t = c4m_get_my_type(tstate->sp->rvalue.obj);
        switch (t->typeid) {
            BINOP_INTEGERS(%);
        default:
            obj = c4m_mod(tstate->sp[1].rvalue.obj, tstate->sp[0].rvalue.obj);
        }
        ++tstate->sp;
        tstate->sp->rvalue.obj = obj;
        return;
    case C4M_BI_EQ:
        STACK_REQUIRE_VALUES(2);

        b = c4m_eq(i->type_info,
                   tstate->sp[1].rvalue.obj,
                   tstate->sp[0].rvalue.obj);

        ++tstate->sp;
        tstate->sp->rvalue = (c4m_value_t){
            .obj = (c4m_obj_t)b,
        };
        return;
    case C4M_BI_LT:
        STACK_REQUIRE_VALUES(2);

        b = c4m_lt(i->type_info,
                   tstate->sp[1].rvalue.obj,
                   tstate->sp[0].rvalue.obj);

        ++tstate->sp;
        tstate->sp->rvalue = (c4m_value_t){
            .obj = (c4m_obj_t)b,
        };
        return;
    case C4M_BI_GT:
        STACK_REQUIRE_VALUES(2);

        b = c4m_gt(i->type_info,
                   tstate->sp[1].rvalue.obj,
                   tstate->sp[0].rvalue.obj);

        ++tstate->sp;
        tstate->sp->rvalue = (c4m_value_t){
            .obj = (c4m_obj_t)b,
        };
        return;
    case C4M_BI_LEN:
        STACK_REQUIRE_VALUES(1);

        tstate->sp->rvalue = (c4m_value_t){
            .obj = (c4m_obj_t)c4m_len(tstate->sp->rvalue.obj),
        };
        return;
    case C4M_BI_INDEX_GET:
        STACK_REQUIRE_VALUES(2);
        // index = sp[0]
        // container = sp[1]

        // get the item type
        obj = tstate->sp[1].rvalue.obj;
        t   = c4m_type_get_param(c4m_get_my_type(obj), -1);
        obj = c4m_index_get(tstate->sp[1].rvalue.obj, tstate->sp[0].rvalue.obj);

        ++tstate->sp;
        tstate->sp->rvalue = (c4m_value_t){
            .obj = obj,
        };
        return;
    case C4M_BI_INDEX_SET:
        STACK_REQUIRE_VALUES(3);
        // index = sp[0]
        // container = sp[1]
        // value = sp[2]

        c4m_index_set(tstate->sp[2].rvalue.obj,
                      tstate->sp[0].rvalue.obj,
                      tstate->sp[1].rvalue.obj);

        tstate->sp += 3;
        return;
    case C4M_BI_SLICE_GET:
        STACK_REQUIRE_VALUES(3);
        obj = c4m_slice_get(tstate->sp[2].rvalue.obj,
                            (int64_t)tstate->sp[1].rvalue.obj,
                            (int64_t)tstate->sp[0].rvalue.obj);

        tstate->sp += 2;
        // type is already correct on the stack, since we're writing
        // over the container location and this is a slice.
        tstate->sp->rvalue.obj = obj;
        return;
    case C4M_BI_SLICE_SET:
        STACK_REQUIRE_VALUES(4);
        // endIx = sp[3]
        // startIx = sp[1]
        // container = sp[2]
        // value = sp[0]

        c4m_slice_set(tstate->sp[3].rvalue.obj,
                      (int64_t)tstate->sp[2].rvalue.obj,
                      (int64_t)tstate->sp[1].rvalue.obj,
                      tstate->sp[0].rvalue.obj);

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

            tstate->sp[0].rvalue.obj = c4m_container_literal(ct, xl, NULL);
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
    case C4M_BI_CONSTRUCTOR:
    case C4M_BI_MARSHAL:
    case C4M_BI_UNMARSHAL:
    case C4M_BI_COERCIBLE:
    case C4M_BI_FROM_LITERAL:
    case C4M_BI_FINALIZER:
    case C4M_BI_NUM_FUNCS:
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
    tstate->fp       = (c4m_stack_value_t *)&tstate->sp->vptr;

    c4m_zmodule_info_t *old_module = tstate->current_module;

    c4m_zfn_info_t *fn = c4m_list_get(tstate->vm->obj->func_info,
                                      ix - 1,
                                      NULL);

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
    // clang-format: off
    tstate->sp->uint = ((uint64_t)tstate->pc << 32u)
                     | tstate->current_module->module_id;
    --tstate->sp;
    // clang-format: on

    tstate->sp->fp = tstate->fp;
    --tstate->sp;
    tstate->fp = tstate->sp;

    c4m_zmodule_info_t *old_module = tstate->current_module;

    tstate->pc             = 0;
    tstate->current_module = c4m_list_get(tstate->vm->obj->module_contents,
                                          i->arg,
                                          NULL);

    c4m_zinstruction_t *nexti = c4m_list_get(tstate->current_module->instructions,
                                             tstate->pc,
                                             NULL);

    // push a frame onto the call stack
    c4m_vmframe_push(tstate,
                     i,
                     old_module,
                     tstate->current_module,
                     NULL,
                     nexti->line_no);
}

static void
c4m_vm_ffi_call(c4m_vmthread_t *tstate, c4m_zinstruction_t *i, int64_t ix)
{
    c4m_ffi_decl_t *decl = c4m_list_get(tstate->vm->obj->ffi_info,
                                        i->arg,
                                        NULL);

    if (decl == NULL) {
        fprintf(stderr, "Could not load external function.\n");
        abort();
    }

    c4m_zffi_cif *ffiinfo = &decl->cif;
    void        **args;

    if (!ffiinfo->cif.nargs) {
        args = NULL;
    }
    else {
        args  = c4m_gc_array_alloc(void *, ffiinfo->cif.nargs);
        int n = ffiinfo->cif.nargs;

        for (unsigned int i = 0; i < ffiinfo->cif.nargs; i++) {
            // clang-format off
	    --n;

	    if (ffiinfo->str_convert &&
		n < 63 &&
		((1 << n) & ffiinfo->str_convert)) {

		c4m_utf8_t *s = (c4m_utf8_t *)tstate->sp[i].rvalue.obj;
		s             = c4m_to_utf8(s);
		args[n]       = &s->data;
            }
            // clang-format on
            else {
                c4m_box_t  value = {.u64 = tstate->sp[i].uint};
                c4m_box_t *box   = c4m_new(c4m_type_box(c4m_type_ref()),
                                         value);
                args[n]          = c4m_ref_via_ffi_type(box,
                                               ffiinfo->cif.arg_types[n]);
            }

            if (n < 63 && ((1 << n) & ffiinfo->hold_info)) {
                c4m_gc_add_hold(tstate->sp[i].rvalue.obj);
            }
        }
    }

    ffi_call(&ffiinfo->cif, ffiinfo->fptr, &tstate->r0, args);

    if (ffiinfo->str_convert & (1UL << 63)) {
        char *s        = (char *)tstate->r0.obj;
        tstate->r0.obj = c4m_new_utf8(s);
    }
}

static void
c4m_vm_foreign_z_call(c4m_vmthread_t *tstate, c4m_zinstruction_t *i, int64_t ix)
{
    // TODO foreign_z_call
}

static void
c4m_vm_run_callback(c4m_vmthread_t *tstate, c4m_zinstruction_t *i)
{
    STACK_REQUIRE_VALUES(1);

    c4m_zcallback_t *cb = tstate->sp->callback;
    ++tstate->sp;

    if (cb->ffi) {
        c4m_vm_ffi_call(tstate, i, cb->impl);
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

    // This temporary is used to hold popped operands during binary
    // operations.
    union {
        uint64_t uint;
        int64_t  sint;
    } rhs;

    C4M_TRY
    {
        for (;;) {
            c4m_zinstruction_t *i;

            i = c4m_list_get(tstate->current_module->instructions,
                             tstate->pc,
                             NULL);

// #define C4M_VM_DEBUG
#ifdef C4M_VM_DEBUG
            c4m_print(
                c4m_cstr_format(
                    "[i] > {} (PC@{:x}; SP@{:x}; FP@{:x}; a = {}; m = {})",
                    c4m_fmt_instr_name(i),
                    c4m_box_u64(tstate->pc * 16),
                    c4m_box_u64((uint64_t)(void *)tstate->sp),
                    c4m_box_u64((uint64_t)(void *)tstate->fp),
                    c4m_box_i64((int64_t)i->arg),
                    c4m_box_u64((uint64_t)tstate->current_module->module_id)));
            printf("stack has %ld items on it.\n",
                   &tstate->stack[STACK_SIZE] - tstate->sp);
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
                // TODO: need to initialize const_storage_base_addr.
            case C4M_ZPushConstObj:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                *tstate->sp = (c4m_stack_value_t){
                    .uint = tstate->vm->const_pool[i->arg].u,
                };
                break;
            case C4M_ZPushConstRef:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                *tstate->sp = (c4m_stack_value_t){
                    .rvalue = (c4m_value_t){
                        .obj = (c4m_obj_t)(tstate->vm->const_pool + i->arg),
                    },
                };
                break;
            case C4M_ZDeref:
                STACK_REQUIRE_VALUES(1);
                tstate->sp->uint = *(uint64_t *)tstate->sp->uint;
                break;
            case C4M_ZPushImm:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                *tstate->sp = (c4m_stack_value_t){
                    .rvalue = (c4m_value_t){
                        .obj = (c4m_obj_t)i->immediate,
                    },
                };
                break;
            case C4M_ZPushLocalObj:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                tstate->sp->rvalue.obj = tstate->fp[-i->arg].rvalue.obj;
                break;
            case C4M_ZPushLocalRef:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                *tstate->sp = (c4m_stack_value_t){
                    .lvalue = &tstate->fp[-i->arg].rvalue,
                };
                break;
            case C4M_ZPushStaticObj:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                *tstate->sp = (c4m_stack_value_t){
                    .rvalue = *c4m_vm_variable(tstate, i),
                };
                break;
            case C4M_ZPushStaticRef:
                STACK_REQUIRE_SLOTS(1);
                --tstate->sp;
                *tstate->sp = (c4m_stack_value_t){
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
                if (c4m_value_iszero(&tstate->sp->rvalue)) {
                    tstate->pc = i->arg;
                    continue;
                }
                ++tstate->sp;
                break;
            case C4M_ZJnz:
                STACK_REQUIRE_VALUES(1);
                if (!c4m_value_iszero(&tstate->sp->rvalue)) {
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
                tstate->sp[0].uint /= rhs.sint;
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
                rhs.uint = tstate->sp[0].uint;
                tstate->sp[0].uint <<= i->arg;
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
                    c4m_stack_value_t tmp = tstate->sp[0];
                    tstate->sp[0]         = tstate->sp[1];
                    tstate->sp[1]         = tmp;
                } while (0);
                break;
            case C4M_ZLoadFromAttr:
                STACK_REQUIRE_VALUES(2);
                do {
                    bool         found = true;
                    c4m_utf8_t  *key   = c4m_vm_attr_key(tstate,
                                                      tstate->sp->static_ptr);
                    c4m_value_t *val;
                    uint64_t     flag = i->immediate;

                    if (flag) {
                        val = c4m_vm_attr_get(tstate, key, &found);
                    }
                    else {
                        val = c4m_vm_attr_get(tstate, key, NULL);
                    }

                    // If we didn't pass the reference to `found`, then
                    // an exception gets thrown if the attr doesn't exist,
                    // which is why `found` is true by default.
                    if (found && (flag != C4M_F_ATTR_SKIP_LOAD)) {
                        if (i->arg) {
                            *tstate->sp = (c4m_stack_value_t){
                                .lvalue = val,
                            };
                        }
                        else {
                            *tstate->sp = (c4m_stack_value_t){
                                .rvalue = *val,
                            };
                        }
                    }
                    // Only push the status if it was explicitly requested.
                    if (flag) {
                        *--tstate->sp = (c4m_stack_value_t){
                            .uint = found ? 1 : 0,
                        };
                    }
                } while (0);
                break;
            case C4M_ZAssignAttr:
                STACK_REQUIRE_VALUES(2);
                do {
                    c4m_utf8_t *key = c4m_vm_attr_key(tstate,
                                                      tstate->sp->static_ptr);

                    c4m_vm_attr_set(tstate,
                                    key,
                                    &tstate->sp[1].rvalue,
                                    i->arg != 0,
                                    false,
                                    false);
                    tstate->sp += 2;
                } while (0);
                break;
            case C4M_ZLockOnWrite:
                STACK_REQUIRE_VALUES(1);
                do {
                    c4m_utf8_t *key = c4m_vm_attr_key(tstate,
                                                      tstate->sp->static_ptr);
                    c4m_vm_attr_lock(tstate, key, true);
                } while (0);
                break;
            case C4M_ZLoadFromView:
                STACK_REQUIRE_VALUES(2);
                STACK_REQUIRE_SLOTS(2); // Usually 1, except w/ dict.
                do {
                    uint64_t obj_len = tstate->sp->uint;

                    union {
                        uint8_t  u8;
                        uint16_t u16;
                        uint32_t u32;
                        uint64_t u64;
                    } view_item;

                    union {
                        uint8_t  *p8;
                        uint16_t *p16;
                        uint32_t *p32;
                        uint64_t *p64;
                        c4m_obj_t obj;
                    } view_ptr;

                    view_item.u64 = 0;
                    view_ptr.obj  = (tstate->sp + 1)->rvalue.obj;

                    --tstate->sp;

                    switch (obj_len) {
                    case 1:
                        view_item.u8 = *view_ptr.p8++;
                        break;
                    case 2:
                        view_item.u16 = *view_ptr.p16++;
                        break;
                    case 4:
                        view_item.u32 = *view_ptr.p32++;
                        break;
                    case 8:
                        view_item.u64 = *view_ptr.p64++;
                        // This is the only size that can be a dict.
                        // Push the value on first.
                        if (i->arg) {
                            tstate->sp->uint = *view_ptr.p64++;
                            --tstate->sp;
                        }
                        break;
                    default:
                        do {
                            uint64_t count  = (uint64_t)(tstate->r1.obj);
                            uint64_t bit_ix = (count - 1) % 64;
                            view_item.u64   = *view_ptr.p64 & (1 << bit_ix);

                            if (bit_ix == 63) {
                                view_ptr.p64++;
                            }
                        } while (0);
                    }
                    tstate->sp->uint = view_item.u64;
                } while (0);
                break;
            case C4M_ZStoreImm:
                *c4m_vm_variable(tstate, i) = (c4m_value_t){
                    .obj = (c4m_obj_t)i->immediate,
                };
                break;
            case C4M_ZPushObjType:
                STACK_REQUIRE_SLOTS(1);
                do {
                    c4m_type_t *type = c4m_get_my_type(tstate->sp->rvalue.obj);

                    if (!i->arg) {
                        --tstate->sp;
                    }
                    *tstate->sp = (c4m_stack_value_t){
                        .rvalue = (c4m_value_t){
                            .obj = type,
                        },
                    };
                } while (0);
                break;
            case C4M_ZTypeCmp:
                STACK_REQUIRE_VALUES(2);
                do {
                    c4m_type_t *t1 = tstate->sp->rvalue.obj;
                    ++tstate->sp;
                    c4m_type_t *t2   = tstate->sp->rvalue.obj;
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
                *(tstate->sp - 1) = (c4m_stack_value_t){
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
                // pop module_lock_stack
                c4m_list_set_slice(tstate->module_lock_stack, -2, -1, NULL);
                c4m_vm_return(tstate, i);
                break;
            case C4M_ZFFICall:
                c4m_vm_ffi_call(tstate, i, i->arg);
                break;
            case C4M_ZPushFfiPtr:
                STACK_REQUIRE_SLOTS(1);
                do {
                    c4m_zcallback_t *cb = c4m_gc_raw_alloc(sizeof(c4m_zcallback_t),
                                                           GC_SCAN_ALL);

                    *cb = (c4m_zcallback_t){
                        .impl       = i->arg,
                        .nameoffset = i->immediate,
                        .tid        = i->type_info,
                        .ffi        = true,
                    };

                    --tstate->sp;
                    *tstate->sp = (c4m_stack_value_t){
                        .callback = cb,
                    };
                } while (0);
                break;
            case C4M_ZPushVmPtr:
                STACK_REQUIRE_SLOTS(1);
                do {
                    c4m_zcallback_t *cb = c4m_gc_raw_alloc(sizeof(c4m_zcallback_t),
                                                           GC_SCAN_ALL);

                    *cb = (c4m_zcallback_t){
                        .impl       = i->arg,
                        .nameoffset = i->immediate,
                        .tid        = i->type_info,
                        .ffi        = false,
                    };

                    --tstate->sp;
                    *tstate->sp = (c4m_stack_value_t){
                        .callback = cb,
                    };
                } while (0);
                break;
            case C4M_ZSObjNew:
                STACK_REQUIRE_SLOTS(1);
                do {
                    // Nim vm doesn't use the length encoded in the instruction,
                    // but codegen does include it as i->arg. We'll use it in
                    // our implementation.
                    char   *data  = &tstate->vm->obj->static_data->data[i->immediate];
                    int64_t avail = c4m_buffer_len(tstate->vm->obj->static_data)
                                  - i->immediate;
                    if (i->arg > avail) {
                        C4M_CRAISE("could not unmarshal: invalid length / offset combination");
                    }
                    c4m_buf_t *buffer = c4m_new(c4m_type_buffer(),
                                                c4m_kw("ptr",
                                                       data,
                                                       "length",
                                                       c4m_ka(i->arg)));

                    c4m_stream_t *stream = c4m_buffer_instream(buffer);
                    c4m_obj_t    *obj    = c4m_unmarshal(stream);
                    if (NULL == obj) {
                        C4M_CRAISE("could not unmarshal");
                    }

                    --tstate->sp;
                    tstate->sp->rvalue = (c4m_value_t){
                        .obj = obj,
                    };
                } while (0);
                break;
            case C4M_ZAssignToLoc:
                STACK_REQUIRE_VALUES(2);
                *tstate->sp[0].lvalue = tstate->sp[1].rvalue;
                tstate->sp += 2;
                break;
            case C4M_ZAssert:
                STACK_REQUIRE_VALUES(1);
                if (!c4m_value_iszero(&tstate->sp->rvalue)) {
                    ++tstate->sp;
                }
                else {
                    C4M_CRAISE("assertion failed");
                }
                break;
#ifdef C4M_DEV
                // This is not threadsafe. It's just for early days.
            case C4M_ZPrint:
                STACK_REQUIRE_VALUES(1);
                c4m_print(tstate->sp->rvalue.obj);
                c4m_stream_write_object(tstate->vm->print_stream,
                                        tstate->sp->rvalue.obj,
                                        false);
                c4m_stream_putc(tstate->vm->print_stream, '\n');
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
                tstate->r0.obj = (void *)NULL;
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
                tstate->sp->rvalue.obj = c4m_box_obj(item, i->type_info);
                break;
            case C4M_ZUnbox:
                STACK_REQUIRE_VALUES(1);
                tstate->sp->uint = c4m_unbox_obj(tstate->sp->rvalue.obj).u64;
                break;
            case C4M_ZUnpack:
                for (int32_t x = 1; x <= i->arg; ++x) {
                    *tstate->sp[0].lvalue = (c4m_value_t){
                        .obj = c4m_tuple_get(tstate->r1.obj, i->arg - x),
                    };
                    ++tstate->sp;
                }
                break;
            case C4M_ZBail:
                STACK_REQUIRE_VALUES(1);
                C4M_RAISE(tstate->sp->rvalue.obj);
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

static void
c4m_vm_load_const_data(c4m_vm_t *vm)
{
    int         nc    = vm->obj->num_const_objs;
    c4m_dict_t *memos = c4m_alloc_unmarshal_memos();
    c4m_buf_t  *inbuf = vm->obj->static_data;

    if (!nc) {
        return;
    }

    c4m_stream_t *s = c4m_buffer_instream(inbuf);

    c4m_buf_t *outbuf = c4m_buffer_empty();

    typedef union {
        uint64_t i;
        void    *p;
    } cout_t;

    cout_t *ptr = (cout_t *)outbuf->data;

    for (int i = 0; i < nc; i++) {
        uint8_t value_item = c4m_unmarshal_u8(s);

        if (value_item) {
            ptr[i].i = c4m_unmarshal_u64(s);
        }
        else {
            ptr[i].p = c4m_sub_unmarshal(s, memos);
        }
    }

    vm->const_pool = (void *)ptr;
}

static inline void
c4m_vm_setup_ffi(c4m_vm_t *vm)
{
    vm->ffi_info_entries = c4m_list_len(vm->obj->ffi_info);

    if (vm->ffi_info_entries == 0) {
        return;
    }

    for (int i = 0; i < vm->ffi_info_entries; i++) {
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
c4m_vm_setup_runtime(c4m_vm_t *vm)
{
    c4m_vm_load_const_data(vm);
    c4m_vm_setup_ffi(vm);

#ifdef C4M_DEV
    vm->print_buf    = c4m_buffer_empty();
    vm->print_stream = c4m_buffer_outstream(vm->print_buf, false);
#endif
}

void
c4m_vm_reset(c4m_vm_t *vm)
{
    int64_t nmodules       = c4m_list_len(vm->obj->module_contents);
    vm->module_allocations = c4m_gc_array_alloc(c4m_value_t *, nmodules);

    for (int64_t n = 0; n < nmodules; ++n) {
        c4m_zmodule_info_t *m = c4m_list_get(vm->obj->module_contents,
                                             n,
                                             NULL);

        vm->module_allocations[n] = c4m_gc_array_alloc(c4m_value_t,
                                                       m->module_var_size);
    }

    vm->attrs        = c4m_new(c4m_type_dict(c4m_type_utf8(),
                                      c4m_type_ref()));
    vm->all_sections = c4m_new(c4m_type_set(c4m_type_utf8()));
    vm->section_docs = c4m_new(c4m_type_dict(c4m_type_utf8(),
                                             c4m_type_ref()));
    vm->using_attrs  = false;
}

c4m_vmthread_t *
c4m_vmthread_new(c4m_vm_t *vm)
{
    // c4m_internal_stash_heap();
    c4m_vmthread_t *tstate = c4m_gc_alloc(c4m_vmthread_t);
    tstate->vm             = vm;

    c4m_vmthread_reset(tstate);
    // c4m_internal_unstash_heap();
    return tstate;
}

void
c4m_vmthread_reset(c4m_vmthread_t *tstate)
{
    tstate->sp                = &tstate->stack[STACK_SIZE];
    tstate->fp                = tstate->sp;
    tstate->pc                = 0;
    tstate->num_frames        = 1;
    tstate->r0                = (c4m_value_t){};
    tstate->r1                = (c4m_value_t){};
    tstate->r2                = (c4m_value_t){};
    tstate->r3                = (c4m_value_t){};
    tstate->running           = false;
    tstate->error             = false;
    tstate->module_lock_stack = c4m_list(c4m_type_i32());

    tstate->current_module = c4m_list_get(tstate->vm->obj->module_contents,
                                          tstate->vm->obj->entrypoint - 1,
                                          NULL);
}

int
c4m_vmthread_run(c4m_vmthread_t *tstate)
{
    assert(!tstate->running);
    tstate->running = true;

    c4m_zinstruction_t *i = c4m_list_get(tstate->current_module->instructions,
                                         tstate->pc,
                                         NULL);

    c4m_vmframe_push(tstate,
                     i,
                     tstate->current_module,
                     tstate->current_module,
                     NULL,
                     0);

    int result = c4m_vm_runloop(tstate);
    --tstate->num_frames;
    tstate->running = false;

    return result;
}

const c4m_vtable_t c4m_vm_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_MARSHAL]   = (c4m_vtable_entry)c4m_vm_marshal,
        [C4M_BI_UNMARSHAL] = (c4m_vtable_entry)c4m_vm_unmarshal,
    },
};

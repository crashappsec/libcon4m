#define C4M_USE_INTERNAL_API
#include "con4m.h"

typedef struct {
    c4m_scope_t            *attr_scope;
    c4m_scope_t            *global_scope;
    c4m_spec_t             *spec;
    c4m_compile_ctx        *compile;
    c4m_module_compile_ctx *module_ctx;
    // The above get initialized only once when we start processing a module.
    // Everything below this comment gets updated for each function entry too.
    c4m_scope_t            *local_scope;
    c4m_tree_node_t        *node;
    c4m_cfg_node_t         *cfg; // Current control-flow-graph node.
    c4m_cfg_node_t         *fn_exit_node;
    c4m_list_t             *func_nodes;
    // Current fn decl object when in a fn. It's NULL in a module context.
    c4m_fn_decl_t          *fn_decl;
    c4m_list_t             *current_rhs_uses;
    c4m_utf8_t             *current_section_prefix;
    // The name here is a bit of a misnomer; this is really a jump-target
    // stack for break and continue statements. That does include loop
    // nodes, but it also includes switch() and typeof() nodes, since
    // you can 'break' out of them.
    c4m_list_t             *loop_stack;
    c4m_list_t             *deferred_calls;
    c4m_list_t             *index_rechecks;
    bool                    augmented_assignment;
    __uint128_t             du_stack;
    int                     du_stack_ix;
    c4m_list_t             *simple_lits_wo_mod;
} pass2_ctx;

static void base_check_pass_dispatch(pass2_ctx *);

static void
c4m_call_resolution_gc_bits(uint64_t *bitmap, c4m_call_resolution_info_t *ci)
{
    c4m_mark_raw_to_addr(bitmap, ci, &ci->resolution);
}

static c4m_call_resolution_info_t *
c4m_new_call_resolution()
{
    return c4m_gc_alloc_mapped(c4m_call_resolution_info_t,
                               c4m_call_resolution_gc_bits);
}

static inline c4m_control_info_t *
control_init(c4m_control_info_t *ci)
{
    ci->awaiting_patches = c4m_new(c4m_type_list(c4m_type_ref()));

    return ci;
}

static inline c4m_loop_info_t *
loop_init(c4m_loop_info_t *li)
{
    control_init(&li->branch_info);
    return li;
}

static inline void
next_branch(pass2_ctx *ctx, c4m_cfg_node_t *branch_node)
{
    ctx->cfg = branch_node;
}
static inline c4m_type_t *
get_pnode_type(c4m_tree_node_t *node)
{
    c4m_pnode_t *pnode = c4m_get_pnode(node);
    return c4m_type_resolve(pnode->type);
}

static inline c4m_type_t *
merge_or_err(pass2_ctx *ctx, c4m_type_t *new_t, c4m_type_t *old_t)
{
    int warn;

    if (!new_t || !old_t) {
        c4m_add_error(ctx->module_ctx, c4m_internal_type_error, ctx->node);
        return c4m_type_error();
    }

    c4m_type_t *result = c4m_merge_types(new_t, old_t, &warn);

    if (c4m_type_is_error(result)) {
        if (!c4m_type_is_error(new_t) && !c4m_type_is_error(old_t)) {
            c4m_add_error(ctx->module_ctx,
                          c4m_err_inconsistent_infer_type,
                          ctx->node,
                          new_t,
                          old_t);
        }
    }
    return result;
}

static inline c4m_type_t *
merge_ignore_err(c4m_type_t *new_t, c4m_type_t *old_t)
{
    return c4m_merge_types(new_t, old_t, NULL);
}

static inline bool
merge_or_ret_ignore_err(c4m_type_t *new_t, c4m_type_t *old_t)
{
    return c4m_type_is_error(c4m_merge_types(new_t, old_t, NULL));
}

static inline bool
base_node_tcheck(pass2_ctx *ctx, c4m_pnode_t *pnode, c4m_type_t *type)
{
    int         warn;
    c4m_type_t *merged;

    merged = c4m_merge_types(pnode->type, type, &warn);

    return (!c4m_type_is_error(merged));
}

static inline void
set_node_type(pass2_ctx *ctx, c4m_tree_node_t *node, c4m_type_t *type)
{
    c4m_pnode_t *pnode = c4m_get_pnode(node);
    if (pnode->type == NULL) {
        pnode->type = type;
    }
    else {
        pnode->type = merge_or_err(ctx, pnode->type, type);
    }
}

static void
add_def(pass2_ctx *ctx, c4m_symbol_t *sym, bool finish_flow)
{
    ctx->cfg = c4m_cfg_add_def(ctx->cfg, ctx->node, sym, ctx->current_rhs_uses);

    if (finish_flow) {
        ctx->current_rhs_uses = NULL;
    }

    if (sym->sym_defs == NULL) {
        sym->sym_defs = c4m_new(c4m_type_list(c4m_type_ref()));
    }

    c4m_list_append(sym->sym_defs, ctx->node);
}

static void
add_use(pass2_ctx *ctx, c4m_symbol_t *sym)
{
    ctx->cfg = c4m_cfg_add_use(ctx->cfg, ctx->node, sym);
    if (ctx->current_rhs_uses) {
        c4m_list_append(ctx->current_rhs_uses, sym);
    }

    if (sym->sym_uses == NULL) {
        sym->sym_uses = c4m_new(c4m_type_list(c4m_type_ref()));
    }

    c4m_list_append(sym->sym_uses, ctx->node);
}

static inline void
start_data_flow(pass2_ctx *ctx)
{
    ctx->current_rhs_uses = c4m_new(c4m_type_list(c4m_type_ref()));
}

static inline c4m_list_t *
use_pattern(pass2_ctx *ctx, c4m_tpat_node_t *pat)
{
    return c4m_apply_pattern_on_node(ctx->node, pat);
}

static inline void
type_check_node_against_sym(pass2_ctx    *ctx,
                            c4m_pnode_t  *pnode,
                            c4m_symbol_t *sym)
{
    if (pnode->type == NULL) {
        pnode->type = c4m_new_typevar();
    }

    else {
        if (pnode->type && c4m_type_is_error(pnode->type)) {
            return; // Already errored for this node.
        }
    }

    if (!c4m_type_is_error(sym->type)) {
        if (base_node_tcheck(ctx, pnode, sym->type)) {
            return;
        }
        c4m_add_error(ctx->module_ctx,
                      c4m_err_decl_mismatch,
                      ctx->node,
                      pnode->type,
                      sym->type,
                      c4m_node_get_loc_str(sym->declaration_node));
    }
    else {
        if (base_node_tcheck(ctx, pnode, sym->type)) {
            return;
        }

        c4m_add_error(ctx->module_ctx,
                      c4m_err_inconsistent_type,
                      ctx->node,
                      pnode->type,
                      sym->name,
                      sym->type);
        // Maybe make this an option; supress further type errors for
        // this symbol.
        sym->type = c4m_type_error();
    }
}

static inline c4m_type_t *
type_check_nodes_no_err(c4m_tree_node_t *n1, c4m_tree_node_t *n2)
{
    c4m_pnode_t *pnode1 = c4m_get_pnode(n1);
    c4m_pnode_t *pnode2 = c4m_get_pnode(n2);

    return merge_ignore_err(pnode1->type, pnode2->type);
}

static inline c4m_type_t *
type_check_node_vs_type_no_err(c4m_tree_node_t *n, c4m_type_t *t)
{
    c4m_pnode_t *pnode = c4m_get_pnode(n);

    return merge_ignore_err(pnode->type, t);
}

void
c4m_fold_container(c4m_tree_node_t *n,
                   c4m_lit_info_t  *li,
                   c4m_list_t      *item_types)
{
    c4m_pnode_t *pn = c4m_get_pnode(n);

    // Don't waste if we can't fold.
    for (int i = 0; i < n->num_kids; i++) {
        c4m_pnode_t *kid_pnode = c4m_get_pnode(n->children[i]);
        if (kid_pnode->value == NULL) {
            return;
        }
    }
    c4m_list_t *items = c4m_new(c4m_type_list(c4m_type_ref()));
    c4m_list_t *tlist = item_types;
    c4m_obj_t   obj;

    if (li->num_items <= 1) {
        bool val_type = c4m_type_is_value_type(c4m_list_get(tlist, 0, NULL));

        for (int i = 0; i < n->num_kids; i++) {
            c4m_pnode_t *kid_pnode = c4m_get_pnode(n->children[i]);
            obj                    = kid_pnode->value;

            if (val_type) {
                obj = c4m_unbox_obj(obj).v;
            }
            c4m_list_append(items, obj);
        }
    }
    else {
        c4m_tuple_t *t = c4m_new(c4m_type_tuple_from_xlist(item_types));
        for (int i = 0; i < n->num_kids; i++) {
            c4m_pnode_t *kid_pnode = c4m_get_pnode(n->children[i]);
            int          ix        = i % li->num_items;
            obj                    = kid_pnode->value;

            if (c4m_type_is_value_type(c4m_list_get(tlist, ix, NULL))) {
                obj = c4m_unbox_obj(obj).v;
            }

            c4m_tuple_set(t, i % li->num_items, obj);

            if (!((i + 1) % li->num_items)) {
                c4m_list_append(items, t);
                t = c4m_new(c4m_type_tuple_from_xlist(item_types));
            }
        }
    }

    pn->value = c4m_container_literal(li->type, items, li->litmod);
}

static void
calculate_container_type(pass2_ctx *ctx, c4m_tree_node_t *n)
{
    c4m_pnode_t    *pn       = c4m_get_pnode(n);
    c4m_lit_info_t *li       = (c4m_lit_info_t *)pn->extra_info;
    bool            concrete = false;

    li->base_type = c4m_base_type_from_litmod(li->st, li->litmod);

    if (li->base_type == C4M_T_ERROR) {
        c4m_utf8_t *s;
        switch (li->st) {
        case ST_List:
            s = c4m_new_utf8("list");
            break;
        case ST_Dict:
            s = c4m_new_utf8("dict");
            break;
        case ST_Tuple:
            s = c4m_new_utf8("tuple");
            break;
        default:
            c4m_unreachable();
        }

        c4m_add_error(ctx->module_ctx,
                      c4m_err_parse_no_lit_mod_match,
                      n,
                      li->litmod,
                      s);
        return;
    }

    if (c4m_base_type_info[li->base_type].dt_kind == C4M_DT_KIND_primitive) {
        li->type = c4m_bi_types[li->base_type];
        concrete = true;
    }
    else {
        li->type = c4m_new(c4m_type_typespec(),
                           li->base_type);
    }
    pn->type = li->type;

    if (pn->kind == c4m_nt_lit_empty_dict_or_set) {
        pn->type = c4m_new_typevar();
        c4m_remove_list_options(pn->type);
        c4m_remove_tuple_options(pn->type);
        return;
    }

    c4m_list_t *items;

    if (concrete) {
        items = c4m_list(c4m_type_typespec());
        c4m_list_append(items, c4m_new_typevar());
        li->num_items = 1;
    }
    else {
        items = li->type->details->items;
        switch (li->st) {
        case ST_List:
            li->num_items = 1;
            break;

        case ST_Tuple:
            li->num_items = n->num_kids;
            break;

        case ST_Dict:
            if (pn->kind == c4m_nt_lit_set) {
                li->num_items = 1;
            }
            else {
                li->num_items = 2;
            }
            break;
        default:
            c4m_unreachable();
        }

        for (int i = 0; i < li->num_items; i++) {
            c4m_list_append(items, c4m_new_typevar());
        }
    }

    for (int i = 0; i < n->num_kids; i++) {
        c4m_pnode_t *kid_pnode = c4m_get_pnode(n->children[i]);
        c4m_type_t  *t         = c4m_list_get(items, i % li->num_items, NULL);

        ctx->node = n->children[i];
        base_check_pass_dispatch(ctx);

        if (!concrete) {
            if (merge_or_ret_ignore_err(t, kid_pnode->type)) {
                if (c4m_can_coerce(kid_pnode->type, t)) {
                    c4m_lit_info_t *li = kid_pnode->extra_info;
                    li->cast_to        = t;
                    if (kid_pnode->value != NULL) {
                        kid_pnode->value = c4m_coerce(kid_pnode->value,
                                                      kid_pnode->type,
                                                      t);
                    }
                }
                else {
                    char *p = (char *)c4m_base_type_info[li->base_type].name;

                    c4m_utf8_t *s = c4m_new_utf8(p);

                    c4m_add_error(ctx->module_ctx,
                                  c4m_err_inconsistent_item_type,
                                  n->children[i],
                                  s,
                                  t,
                                  kid_pnode->type);
                    c4m_calculate_type_hash(li->type);
                    return;
                }
            }
        }
    }

    if (!concrete) {
        c4m_calculate_type_hash(li->type);
    }

    c4m_fold_container(n, li, items);
}

// This maps names to how many arguments the function takes.  If
// there's no return type, it'll be a negative number (but a
// straight-up NOT not a properly negated number).  For now, the type
// constraints will be handled by the caller, and the # of args should
// already be checked.

static c4m_dict_t *polymorphic_fns = NULL;

static void
setup_polymorphic_fns()
{
    polymorphic_fns = c4m_new(c4m_type_dict(c4m_type_utf8(),
                                            c4m_type_ref()));

    hatrack_dict_put(polymorphic_fns, c4m_new_utf8(C4M_SLICE_FN), (void *)~3);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8(C4M_INDEX_FN), (void *)~2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8(C4M_PLUS_FN), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8(C4M_MINUS_FN), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8(C4M_MUL_FN), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8(C4M_MOD_FN), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8(C4M_DIV_FN), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8(C4M_FDIV_FN), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8(C4M_SHL_FN), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8(C4M_SHR_FN), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8(C4M_BAND_FN), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8(C4M_BOR_FN), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8(C4M_BXOR_FN), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8(C4M_CMP_FN), (void *)2);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8(C4M_SET_SLICE), (void *)~4);
    hatrack_dict_put(polymorphic_fns, c4m_new_utf8(C4M_SET_INDEX), (void *)~3);

    c4m_gc_register_root(&polymorphic_fns, 1);
}

static c4m_call_resolution_info_t *
initial_function_resolution(pass2_ctx       *ctx,
                            c4m_utf8_t      *call_name,
                            c4m_type_t      *called_type,
                            c4m_tree_node_t *call_loc)
{
    // For now, (maybe will change when we add in objects), the only
    // overloading we allow for function names is for builtin names.
    // For such things `overload` will be true. And for these, we do
    // NOT make any serious attempt to do resolution when the first
    // argument is not "concrete enough", meaning the only type
    // variables we would allow would be in *parameters* to function
    // calls.
    //
    // We'll try to statically bind at the end of processing though.
    // And if there's an ambiguity, we'll generate a run-time
    // dispatch.
    //
    // For any other function, we unify immediately, and if there is
    // no symbol at this point, we know it's a miss, since all
    // declarations have been processed by this point.
    //
    // However, if the function we've looked up hasn't been fully
    // inferenced yet, we won't have enough information yet to bind
    // the types yet (the functions params are often all going to just
    // be type variables).
    //
    // If a function hasn't been processed, comparing vs. its type sig
    // is worthless, because we don't want concrete types at call
    // sites to influence the actual type. So in those cases, we defer
    // until the end of the pass, just like w/ overloads.
    if (polymorphic_fns == NULL) {
        setup_polymorphic_fns();
    }

    c4m_call_resolution_info_t *info = c4m_new_call_resolution();

    info->name = call_name;
    info->loc  = call_loc;
    info->sig  = called_type;

    // Result will be non-zero in all cases we allow polymorphism,
    // thus the null check, even though we were putting in ints.
    if (hatrack_dict_get(polymorphic_fns, call_name, NULL) != NULL) {
        info->polymorphic = 1;
        info->deferred    = 1;

        c4m_list_append(ctx->deferred_calls, info);

        return info;
    }

    // Otherwise, at this point we must be able to statically bind
    // from our static scope, and currently the function can only live
    // in the module or global scope.
    c4m_symbol_t *sym = c4m_symbol_lookup(NULL,
                                          ctx->module_ctx->module_scope,
                                          ctx->global_scope,
                                          NULL,
                                          call_name);

    if (sym == NULL) {
        c4m_add_error(ctx->module_ctx,
                      c4m_err_fn_not_found,
                      call_loc,
                      call_name);
        return NULL;
    }

    switch (sym->kind) {
    case C4M_SK_FUNC:
    case C4M_SK_EXTERN_FUNC:

        info->resolution = sym;

        ctx->cfg = c4m_cfg_add_call(ctx->cfg,
                                    call_loc,
                                    sym,
                                    ctx->current_rhs_uses);

        int sig_params = c4m_type_get_num_params(info->sig);
        int sym_params = c4m_type_get_num_params(sym->type);

        if (sig_params != sym_params) {
            c4m_add_error(ctx->module_ctx,
                          c4m_err_num_params,
                          call_loc,
                          call_name,
                          c4m_box_u64(sig_params - 1),
                          c4m_box_u64(sym_params - 1));
            return NULL;
        }

        if (sym->kind == C4M_SK_FUNC && !(sym->flags & C4M_F_FN_PASS_DONE)) {
            info->deferred = 1;
            c4m_list_append(ctx->deferred_calls, info);
            return info;
        }

        merge_or_err(ctx, c4m_type_copy(sym->type), info->sig);
        set_node_type(ctx,
                      call_loc,
                      c4m_type_get_param(info->sig, sig_params - 1));

        return info;

    default:
        c4m_add_error(ctx->module_ctx,
                      c4m_err_calling_non_fn,
                      call_loc,
                      call_name,
                      c4m_sym_kind_name(sym));

        return NULL;
    }
}

static inline void
def_context_enter(pass2_ctx *ctx)
{
    ctx->du_stack <<= 1;
    ctx->du_stack |= 1;

    if (++ctx->du_stack_ix == 128) {
        C4M_CRAISE("Stack overflow in def/use tracker.");
    }
}

static inline void
def_use_context_exit(pass2_ctx *ctx)
{
    ctx->du_stack >>= 1;
    ctx->du_stack_ix--;
}

static inline void
use_context_enter(pass2_ctx *ctx)
{
    ctx->du_stack <<= 1;

    if (++ctx->du_stack_ix == 128) {
        C4M_CRAISE("Stack overflow in def/use tracker.");
    }
}

static void
process_children(pass2_ctx *ctx)
{
    c4m_tree_node_t *saved = ctx->node;
    int              n     = saved->num_kids;

    for (int i = 0; i < n; i++) {
        ctx->node = saved->children[i];
        base_check_pass_dispatch(ctx);
    }

    // Handle propogations automatically for unary nodes.
    // binary nodes are on their own.
    if (n == 1) {
        c4m_pnode_t *my_pnode  = c4m_get_pnode(saved);
        c4m_pnode_t *kid_pnode = c4m_get_pnode(saved->children[0]);

        my_pnode->value      = kid_pnode->value;
        my_pnode->type       = kid_pnode->type;
        my_pnode->extra_info = kid_pnode->extra_info;
    }

    ctx->node = saved;
}

static inline void
process_child(pass2_ctx *ctx, int i)
{
    c4m_tree_node_t *saved = ctx->node;
    ctx->node              = saved->children[i];
    base_check_pass_dispatch(ctx);
    ctx->node = saved;
}

static inline bool
is_def_context(pass2_ctx *ctx)
{
    return (bool)ctx->du_stack & 0x1;
}

static c4m_symbol_t *
sym_lookup(pass2_ctx *ctx, c4m_utf8_t *name)
{
    c4m_symbol_t *result;
    c4m_spec_t   *spec = ctx->spec;
    c4m_utf8_t   *dot  = c4m_new_utf8(".");

    // First, if it's specified in the spec, we consider it there,
    // even if it's not in the symbol table.

    if (spec != NULL) {
        c4m_list_t      *parts     = c4m_str_split(name, dot);
        c4m_attr_info_t *attr_info = c4m_get_attr_info(spec, parts);

        switch (attr_info->kind) {
        case c4m_attr_user_def_field:
            if (c4m_list_len(parts) == 1) {
                break;
            }
            // fallthrough
        case c4m_attr_field:

            result = hatrack_dict_get(ctx->attr_scope->symbols, name, NULL);
            if (result == NULL) {
                result             = c4m_add_inferred_symbol(ctx->module_ctx,
                                                 ctx->attr_scope,
                                                 name);
                result->other_info = attr_info;
            }
            return result;

        case c4m_attr_object_type:
            c4m_add_error(ctx->module_ctx,
                          c4m_err_spec_needs_field,
                          ctx->node,
                          name,
                          c4m_new_utf8("named section"));
            return NULL;
        case c4m_attr_singleton:
            c4m_add_error(ctx->module_ctx,
                          c4m_err_spec_needs_field,
                          ctx->node,
                          name,
                          c4m_new_utf8("singleton (unnamed) section"));
            return NULL;

        case c4m_attr_instance:
            c4m_add_error(ctx->module_ctx,
                          c4m_err_spec_needs_field,
                          ctx->node,
                          name,
                          c4m_new_utf8("section instance"));
            return NULL;

        case c4m_attr_invalid:
            if (c4m_list_len(parts) == 1) {
                // If it's not explicitly by the spec, then we
                // treat it as a variable not an attribute.
                break;
            }
            switch (attr_info->err) {
            case c4m_attr_err_sec_under_field:
                c4m_add_error(ctx->module_ctx,
                              c4m_err_field_not_spec,
                              ctx->node,
                              name,
                              attr_info->err_arg);
                break;
            case c4m_attr_err_field_not_allowed:
                c4m_add_error(ctx->module_ctx,
                              c4m_err_field_not_spec,
                              ctx->node,
                              name,
                              attr_info->err_arg);
                break;
            case c4m_attr_err_no_such_sec:
                c4m_add_error(ctx->module_ctx,
                              c4m_err_undefined_section,
                              ctx->node,
                              attr_info->err_arg);
                break;
            case c4m_attr_err_sec_not_allowed:
                c4m_add_error(ctx->module_ctx,
                              c4m_err_section_not_allowed,
                              ctx->node,
                              attr_info->err_arg);
                break;
            default:
                c4m_unreachable();
            }

            return NULL;
        }
    }
    else {
        // TODO: Object resolution.
        if (c4m_str_find(name, dot) != -1) {
            result = c4m_symbol_lookup(NULL, NULL, NULL, ctx->attr_scope, name);

            if (!result) {
                result = c4m_add_inferred_symbol(ctx->module_ctx,
                                                 ctx->attr_scope,
                                                 name);
            }

            return result;
        }
    }

    result = c4m_symbol_lookup(ctx->local_scope,
                               ctx->module_ctx->module_scope,
                               ctx->global_scope,
                               ctx->attr_scope,
                               name);

    return result;
}

static c4m_symbol_t *
lookup_or_add(pass2_ctx *ctx, c4m_utf8_t *name)
{
    c4m_symbol_t *result = sym_lookup(ctx, name);

    if (!result) {
        result = c4m_add_inferred_symbol(ctx->module_ctx,
                                         ctx->local_scope,
                                         name);
    }

    if (is_def_context(ctx)) {
        add_def(ctx, result, true);
    }
    else {
        add_use(ctx, result);
    }

    return result;
}

static void
handle_elision(pass2_ctx *ctx)
{
    c4m_pnode_t *cur    = c4m_get_pnode(ctx->node);
    c4m_pnode_t *parent = c4m_get_pnode(ctx->node->parent);

    switch (parent->kind) {
    case c4m_nt_range:
        cur->type = c4m_type_int();
        return;
    default:
        c4m_unreachable();
    }
}

static void
handle_index(pass2_ctx *ctx)
{
    c4m_pnode_t *pnode     = c4m_get_pnode(ctx->node);
    c4m_pnode_t *kid_pnode = c4m_get_pnode(ctx->node->children[1]);
    c4m_type_t  *node_type = c4m_new_typevar();
    bool         is_slice  = kid_pnode->kind == c4m_nt_range;
    c4m_type_t  *container_type;
    c4m_type_t  *ix1_type;

    process_child(ctx, 0);
    container_type = get_pnode_type(ctx->node->children[0]);

    use_context_enter(ctx);
    process_child(ctx, 1);

    ix1_type    = c4m_type_resolve(get_pnode_type(ctx->node->children[1]));
    pnode->type = node_type;

    if (c4m_type_is_box(ix1_type)) {
        ix1_type = c4m_type_unbox(ix1_type);
    }

    if (c4m_type_is_concrete(ix1_type)) {
        if (!c4m_type_is_int_type(ix1_type)) {
            if (is_slice) {
                c4m_add_error(ctx->module_ctx,
                              c4m_err_slice_on_dict,
                              ctx->node);
                return;
            }

            c4m_type_t *tmp = c4m_type_any_dict(ix1_type, NULL);
            container_type  = merge_or_err(ctx, container_type, tmp);
        }
    }
    else {
        c4m_list_append(ctx->index_rechecks, ctx->node);
    }

    c4m_call_resolution_info_t *info = NULL;

    if (is_slice) {
        merge_or_err(ctx, container_type, node_type);

        if (is_def_context(ctx) && ctx->augmented_assignment) {
            c4m_add_error(ctx->module_ctx,
                          c4m_err_augmented_assign_to_slice,
                          ctx->node);
            return;
        }

        info = initial_function_resolution(
            ctx,
            c4m_new_utf8(C4M_SLICE_FN),
            c4m_type_varargs_fn(node_type,
                                3,
                                container_type,
                                c4m_type_int(),
                                c4m_type_int()),
            ctx->node);

        pnode->type = container_type;
    }
    else {
        info = initial_function_resolution(
            ctx,
            c4m_new_utf8(C4M_INDEX_FN),
            c4m_type_varargs_fn(node_type,
                                2,
                                container_type,
                                ix1_type),
            ctx->node);
    }

    if (c4m_type_is_tvar(container_type)) {
        tv_options_t *tsi = container_type->details->tsi;

        if (tsi->value_type == NULL) {
            tsi->value_type = c4m_new_typevar();
        }

        if (!is_slice) {
            merge_or_err(ctx, node_type, tsi->value_type);
        }
    }

    else {
        int         nparams = c4m_type_get_num_params(container_type);
        c4m_type_t *tmp;

        if (!is_slice) {
            if (c4m_type_is_tuple(container_type)) {
                c4m_pnode_t *pn = c4m_get_pnode(ctx->node->children[1]);
                if (pn->value == NULL) {
                    c4m_add_error(ctx->module_ctx, c4m_err_tup_ix, ctx->node);
                    return;
                }

                int64_t v = (int64_t)c4m_unbox(pn->value);

                if (v >= c4m_type_get_num_params(container_type)) {
                    c4m_add_error(ctx->module_ctx,
                                  c4m_err_tup_ix_bounds,
                                  ctx->node,
                                  container_type);
                    return;
                }

                tmp = c4m_type_get_param(container_type, v);
            }
            else {
                tmp = c4m_type_get_param(container_type, nparams - 1);
            }
            merge_or_err(ctx, node_type, tmp);
        }
    }

    c4m_list_append(ctx->deferred_calls, info);

    def_use_context_exit(ctx);
    pnode->extra_info = info;
}

static void
handle_call(pass2_ctx *ctx)
{
    c4m_list_t *stashed_uses = ctx->current_rhs_uses;

    use_context_enter(ctx);
    start_data_flow(ctx);

    c4m_tree_node_t *saved    = ctx->node;
    int              n        = saved->num_kids;
    c4m_list_t      *argtypes = c4m_new(c4m_type_list(c4m_type_typespec()));
    c4m_utf8_t      *fname    = c4m_node_text(saved->children[0]);
    c4m_pnode_t     *pnode;

    for (int i = 1; i < n; i++) {
        ctx->node = saved->children[i];
        pnode     = c4m_get_pnode(ctx->node);
        base_check_pass_dispatch(ctx);
        c4m_list_append(argtypes, pnode->type);
    }

    c4m_type_t *fn_type;

    pnode       = c4m_get_pnode(saved);
    pnode->type = c4m_new_typevar();
    fn_type     = c4m_type_fn(pnode->type, argtypes, false);

    pnode->extra_info = initial_function_resolution(ctx, fname, fn_type, saved);

    ctx->current_rhs_uses = stashed_uses;
    def_use_context_exit(ctx);
}

static void
handle_break(pass2_ctx *ctx)
{
    c4m_tree_node_t *n     = ctx->node;
    c4m_utf8_t      *label = NULL;

    if (c4m_tree_get_number_children(n) != 0) {
        label = c4m_node_text(n->children[0]);
    }

    ctx->cfg = c4m_cfg_add_break(ctx->cfg, n, label);
    int i    = c4m_list_len(ctx->loop_stack);

    while (i--) {
        c4m_loop_info_t    *li = c4m_list_get(ctx->loop_stack, i, NULL);
        c4m_control_info_t *bi = &li->branch_info;

        if (!label || (bi->label && !strcmp(label->data, bi->label->data))) {
            c4m_pnode_t     *npnode = c4m_get_pnode(n);
            c4m_jump_info_t *ji     = npnode->extra_info;
            assert(bi);
            ji->linked_control_structure = bi;
            return;
        }
    }

    c4m_add_error(ctx->module_ctx,
                  c4m_err_label_target,
                  n,
                  label,
                  c4m_new_utf8("break"));
}

static void
handle_return(pass2_ctx *ctx)
{
    c4m_tree_node_t *n = ctx->node;

    process_children(ctx);

    if (c4m_tree_get_number_children(n) != 0) {
        c4m_symbol_t *sym = c4m_symbol_lookup(NULL,
                                              ctx->local_scope,
                                              NULL,
                                              NULL,
                                              c4m_new_utf8("$result"));
        type_check_node_against_sym(ctx,
                                    c4m_get_pnode(n->children[0]),
                                    sym);
        add_def(ctx, sym, true);

        c4m_pnode_t *pn = c4m_get_pnode(n);
        pn->extra_info  = sym;
    }

    ctx->cfg = c4m_cfg_add_return(ctx->cfg, n, ctx->fn_exit_node);
}

static void
handle_continue(pass2_ctx *ctx)
{
    c4m_tree_node_t *n     = ctx->node;
    c4m_utf8_t      *label = NULL;

    if (c4m_tree_get_number_children(n) != 0) {
        label = c4m_node_text(n->children[0]);
    }

    ctx->cfg = c4m_cfg_add_continue(ctx->cfg, n, label);
    int i    = c4m_list_len(ctx->loop_stack);

    while (i--) {
        c4m_loop_info_t    *li = c4m_list_get(ctx->loop_stack, i, NULL);
        c4m_control_info_t *bi = &li->branch_info;

        // While 'break' can be used to exit switch() and typeof()
        // cases, 'continue' cannot.
        if (bi->non_loop) {
            continue;
        }
        if (!label || (bi->label && !strcmp(label->data, bi->label->data))) {
            c4m_pnode_t     *npnode      = c4m_get_pnode(n);
            c4m_jump_info_t *ji          = npnode->extra_info;
            ji->linked_control_structure = bi;
            ji->top                      = true;
            return;
        }
    }

    c4m_add_error(ctx->module_ctx,
                  c4m_err_label_target,
                  n,
                  label,
                  c4m_new_utf8("continue"));
}

static c4m_utf8_t *ix_var_name   = NULL;
static c4m_utf8_t *last_var_name = NULL;

static void
loop_push_ix_var(pass2_ctx *ctx, c4m_loop_info_t *li)
{
    if (ix_var_name == NULL) {
        ix_var_name   = c4m_new_utf8("$i");
        last_var_name = c4m_new_utf8("$last");
        c4m_gc_register_root(&ix_var_name, 1);
        c4m_gc_register_root(&last_var_name, 1);
    }

    li->shadowed_ix = c4m_symbol_lookup(ctx->local_scope,
                                        NULL,
                                        NULL,
                                        NULL,
                                        ix_var_name);
    li->loop_ix     = c4m_add_or_replace_symbol(ctx->module_ctx,
                                            ctx->local_scope,
                                            ix_var_name);

    li->loop_ix->type = c4m_type_i64();
    li->loop_ix->flags |= C4M_F_USER_IMMUTIBLE | C4M_F_DECLARED_CONST;

    add_def(ctx, li->loop_ix, false);

    if (li->branch_info.label != NULL) {
        li->label_ix = c4m_str_concat(li->branch_info.label, ix_var_name);

        if (c4m_symbol_lookup(ctx->local_scope,
                              NULL,
                              NULL,
                              NULL,
                              li->label_ix)) {
            c4m_add_error(ctx->module_ctx,
                          c4m_err_dupe_label,
                          ctx->node,
                          li->branch_info.label);
            return;
        }

        li->named_loop_ix       = c4m_add_inferred_symbol(ctx->module_ctx,
                                                    ctx->local_scope,
                                                    li->label_ix);
        li->named_loop_ix->type = c4m_type_i64();
        li->named_loop_ix->flags |= C4M_F_USER_IMMUTIBLE | C4M_F_DECLARED_CONST;

        add_def(ctx, li->named_loop_ix, false);
    }
}

static void
loop_pop_ix_var(pass2_ctx *ctx, c4m_loop_info_t *li)
{
    hatrack_dict_remove(ctx->local_scope->symbols, ix_var_name);

    if (li->label_ix != NULL) {
        hatrack_dict_remove(ctx->local_scope->symbols, li->label_ix);
    }

    if (li->shadowed_ix != NULL) {
        hatrack_dict_put(ctx->local_scope->symbols,
                         ix_var_name,
                         li->shadowed_ix);
    }
    else {
        hatrack_dict_remove(ctx->local_scope->symbols, ix_var_name);
    }
}

// Currently looks like the merging in tree capture isn't consistent,
// resulting in an ordering problem.
// Tmp fix. The assign to elif below really wants to be:
// elifs = use_pattern(ctx, c4m_elif_branches);

static c4m_list_t *
get_elifs(c4m_tree_node_t *t)
{
    c4m_list_t *result = c4m_new(c4m_type_list(c4m_type_ref()));

    for (int i = 2; i < t->num_kids - 1; i++) {
        c4m_list_append(result, t->children[i]);
    }

    c4m_tree_node_t *last  = t->children[t->num_kids - 1];
    c4m_pnode_t     *plast = c4m_get_pnode(last);

    if (plast->kind == c4m_nt_elif) {
        c4m_list_append(result, last);
    }

    return result;
}

static void
handle_else(pass2_ctx *ctx, c4m_tree_node_t *end)
{
    c4m_cfg_node_t *enter = c4m_cfg_enter_block(ctx->cfg, end);
    ctx->cfg              = enter;

    if (end != NULL) {
        ctx->node = end;
        base_check_pass_dispatch(ctx);
    }
    ctx->cfg = c4m_cfg_exit_block(ctx->cfg, enter, end);
}

static void
handle_elifs(pass2_ctx *ctx, c4m_list_t *elifs, int ix, c4m_tree_node_t *end)
{
    c4m_cfg_node_t  *branch;
    c4m_cfg_node_t  *branch_enter;
    c4m_tree_node_t *saved = ctx->node;
    c4m_cfg_node_t  *enter = c4m_cfg_enter_block(ctx->cfg, saved);
    c4m_tree_node_t *elif  = c4m_list_get(elifs, ix, NULL);
    ctx->cfg               = enter;
    ctx->node              = elif->children[0];
    base_check_pass_dispatch(ctx);
    branch = c4m_cfg_block_new_branch_node(ctx->cfg,
                                           2,
                                           NULL,
                                           elif);
    next_branch(ctx, branch);
    branch_enter = c4m_cfg_enter_block(ctx->cfg, elif->children[1]);
    ctx->cfg     = branch_enter;
    ctx->node    = elif->children[1];
    base_check_pass_dispatch(ctx);
    c4m_cfg_exit_block(ctx->cfg, branch_enter, elif->children[1]);
    next_branch(ctx, branch);

    if (ix + 1 == c4m_list_len(elifs)) {
        handle_else(ctx, end);
    }
    else {
        handle_elifs(ctx, elifs, ix + 1, end);
    }
    c4m_cfg_exit_block(ctx->cfg, enter, saved);
}

static void
handle_if(pass2_ctx *ctx)
{
    c4m_tree_node_t *saved = ctx->node;
    c4m_cfg_node_t  *branch;
    c4m_cfg_node_t  *branch_enter;
    c4m_cfg_node_t  *enter      = c4m_cfg_enter_block(ctx->cfg, saved);
    c4m_list_t      *elses      = get_elifs(saved);
    c4m_tree_node_t *else_match = c4m_get_match_on_node(ctx->node,
                                                        c4m_else_condition);

    ctx->cfg  = enter;
    ctx->node = saved->children[0];
    base_check_pass_dispatch(ctx);
    branch = c4m_cfg_block_new_branch_node(ctx->cfg,
                                           2,
                                           NULL,
                                           saved->children[0]);
    next_branch(ctx, branch);
    branch_enter = c4m_cfg_enter_block(ctx->cfg, saved);
    ctx->cfg     = branch_enter;
    ctx->node    = saved->children[1];

    base_check_pass_dispatch(ctx);
    ctx->cfg = c4m_cfg_exit_block(ctx->cfg, branch_enter, saved);

    next_branch(ctx, branch);
    branch_enter = c4m_cfg_enter_block(ctx->cfg, NULL);
    ctx->cfg     = branch_enter;

    if (c4m_list_len(elses) != 0) {
        handle_elifs(ctx,
                     elses,
                     0,
                     else_match);
    }
    else {
        handle_else(ctx, else_match);
    }
    ctx->cfg = c4m_cfg_exit_block(ctx->cfg, branch_enter, NULL);
    ctx->cfg = c4m_cfg_exit_block(ctx->cfg, enter, saved);
}

static bool
range_runs_check(pass2_ctx *ctx, c4m_tree_node_t *n)
{
    c4m_pnode_t *pnode1 = c4m_get_pnode(n->children[0]);
    c4m_pnode_t *pnode2 = c4m_get_pnode(n->children[1]);

    if (c4m_type_is_error(get_pnode_type(n))) {
        return false;
    }

    if (!pnode1->value || !pnode2->value) {
        return false;
    }

    uint64_t v1 = c4m_unbox_obj((c4m_obj_t)pnode1->value).u64;
    uint64_t v2 = c4m_unbox_obj((c4m_obj_t)pnode2->value).u64;

    return v1 != v2;
}

static void
handle_for(pass2_ctx *ctx)
{
    c4m_pnode_t     *pnode = c4m_get_pnode(ctx->node);
    c4m_loop_info_t *li    = loop_init(pnode->extra_info);
    c4m_tree_node_t *n     = ctx->node;
    c4m_list_t      *vars  = use_pattern(ctx, c4m_loop_vars);
    c4m_cfg_node_t  *entrance;
    c4m_cfg_node_t  *exit;
    c4m_cfg_node_t  *branch;
    int              expr_ix = 0;

    start_data_flow(ctx);

    if (c4m_node_has_type(n->children[0], c4m_nt_label)) {
        expr_ix++;
        li->branch_info.label = c4m_node_text(n->children[0]);
    }

    c4m_list_append(ctx->loop_stack, li);

    // First, process either the container to unpack or the range
    // expressions. We do this before the CFG entrance node, as when
    // this isn't already a literal, we only want to do it once before
    // we loop.

    ctx->node                        = n->children[expr_ix + 1];
    c4m_tree_node_t *container_node  = ctx->node;
    c4m_pnode_t     *container_pnode = c4m_get_pnode(ctx->node);
    base_check_pass_dispatch(ctx);

    if (container_pnode->kind == c4m_nt_range) {
        li->ranged = true;
    }
    else {
        li->ranged = false;
    }

    // Now we start the loop's CFG block. The flow graph will evaluate
    // this part every time we get back to the top of the loop.  We
    // actually skip adding a 'use' here for the container (as if we
    // just moved the contents to a temporary). Instead, we need to
    // add defs for any iteration variables after we start the block,
    // but before we branch.

    entrance = c4m_cfg_enter_block(ctx->cfg, n);
    exit     = c4m_cfg_exit_node(entrance);
    ctx->cfg = entrance;

    // At this point, set up the iteration variables...  `push_ix_var`
    // sets up `$i` and `label$i` (if a label is provided), so that
    // code in the block below can reference it.
    //
    // We manually set up `$last`, which is only available in for
    // loops. It's set to the last iteration number we'll do if
    // there's no early exit. So when `$i == $last`, you are on the
    // last iteration of the loop.
    //
    // If used, `$i` gets updated each iteration with the number of
    // iterations through the loop that have successfully
    // completed. The lifetime of this variable is only the lifetime
    // of the loop; you cannot use it outside the loop.
    //
    // `$last` also cannot be used outside the loop. And it is not
    // reevaluated each loop, just once when starting the loop.
    //
    // If the variable isn't referenced, we'll just ignore the symbol
    // later.
    //
    // Note that we also track here when loops are nested, so that if
    // $i is used when nested, we can give a warning. But we only give
    // that warning if we see a use of $i once there is definitely
    // nesting.
    //
    // It might be worth making that an error, since the alternative
    // is still available: add a label and use `label$i` or `label$last`
    //
    // Additionally, I'm thinking about disallowing `$` in all
    // user-defined symbols.

    loop_push_ix_var(ctx, li);

    li->shadowed_last = c4m_symbol_lookup(ctx->local_scope,
                                          NULL,
                                          NULL,
                                          NULL,
                                          last_var_name);

    li->loop_last = c4m_add_or_replace_symbol(ctx->module_ctx,
                                              ctx->local_scope,
                                              last_var_name);
    li->loop_last->flags |= C4M_F_USER_IMMUTIBLE | C4M_F_DECLARED_CONST;
    li->loop_last->type          = c4m_type_i64();
    li->loop_last->cfg_kill_node = exit;

    add_def(ctx, li->loop_last, true);

    if (li->branch_info.label != NULL) {
        c4m_utf8_t *new_name      = c4m_str_concat(li->branch_info.label,
                                              last_var_name);
        li->label_last            = c4m_to_utf8(new_name);
        li->named_loop_last       = c4m_add_inferred_symbol(ctx->module_ctx,
                                                      ctx->local_scope,
                                                      li->label_last);
        li->named_loop_last->type = c4m_type_i64();
        li->named_loop_last->flags |= C4M_F_USER_IMMUTIBLE;
        li->named_loop_last->cfg_kill_node = exit;

        add_def(ctx, li->named_loop_last, false);
    }

    // Now, process the assignment of the variable(s) to put per-loop
    // values into.  Instead of keeping nested scopes, if we see
    // shadowing, we will stash the old symbol until the loop's
    // done. We also will warn about the shadowing.

    c4m_tree_node_t *var_node1 = c4m_list_get(vars, 0, NULL);
    c4m_tree_node_t *var_node2;
    c4m_utf8_t      *var1_name = c4m_node_text(var_node1);
    c4m_utf8_t      *var2_name = NULL;
    c4m_cfg_node_t  *bstart;

    if (c4m_list_len(vars) == 2) {
        var_node2 = c4m_list_get(vars, 1, NULL);
        var2_name = c4m_node_text(var_node2);

        if (!strcmp(var1_name->data, var2_name->data)) {
            c4m_add_error(ctx->module_ctx,
                          c4m_err_iter_name_conflict,
                          var_node2);
            return;
        }
    }

    li->shadowed_lvar_1 = c4m_symbol_lookup(ctx->local_scope,
                                            NULL,
                                            NULL,
                                            NULL,
                                            var1_name);
    li->lvar_1          = c4m_add_or_replace_symbol(ctx->module_ctx,
                                           ctx->local_scope,
                                           var1_name);

    li->lvar_1->cfg_kill_node    = exit;
    li->lvar_1->declaration_node = var_node1;
    c4m_pnode_t *v1pn            = c4m_get_pnode(var_node1);
    v1pn->type                   = li->lvar_1->type;
    li->lvar_1->flags |= C4M_F_USER_IMMUTIBLE;

    if (li->shadowed_lvar_1 != NULL) {
        c4m_add_warning(ctx->module_ctx,
                        c4m_warn_shadowed_var,
                        ctx->node,
                        var1_name,
                        c4m_sym_kind_name(li->shadowed_lvar_1),
                        c4m_sym_get_best_ref_loc(li->lvar_1));
    }

    if (var2_name) {
        li->shadowed_lvar_2          = c4m_symbol_lookup(ctx->local_scope,
                                                NULL,
                                                NULL,
                                                NULL,
                                                var2_name);
        li->lvar_2                   = c4m_add_or_replace_symbol(ctx->module_ctx,
                                               ctx->local_scope,
                                               var2_name);
        li->lvar_2->declaration_node = var_node2;
        li->lvar_2->cfg_kill_node    = exit;
        c4m_pnode_t *v2pn            = c4m_get_pnode(var_node2);
        v2pn->type                   = li->lvar_2->type;

        li->lvar_2->flags |= C4M_F_USER_IMMUTIBLE;

        if (li->shadowed_lvar_2 != NULL) {
            c4m_add_warning(ctx->module_ctx,
                            c4m_warn_shadowed_var,
                            ctx->node,
                            var2_name,
                            c4m_sym_kind_name(li->shadowed_lvar_2),
                            c4m_sym_get_best_ref_loc(li->lvar_2));
        }
    }

    add_def(ctx, li->lvar_1, true);

    if (var2_name) {
        add_def(ctx, li->lvar_2, true);
    }
    // Okay, now we need to add the branch node to the CFG.

    branch   = c4m_cfg_block_new_branch_node(ctx->cfg,
                                           2,
                                           li->branch_info.label,
                                           ctx->node);
    bstart   = c4m_cfg_enter_block(branch, n->children[expr_ix]);
    ctx->cfg = bstart;

    // Now, process that body. The body gets its own block in the CFG,
    // which makes this a bit clunkier than it needs to be.
    expr_ix += 2;
    ctx->node = n->children[expr_ix];

    base_check_pass_dispatch(ctx);
    // Note that this implicitly links to the overall exit node; we
    // don't manually add the link.
    c4m_cfg_exit_block(ctx->cfg, bstart, n->children[expr_ix]);

    // Here, we reset to the branch node for the next branch.
    ctx->cfg = branch;

    // This sets up an empty block for the code that runs when the
    // loop condition is false, but only if we can't tell that the
    // loop isn't going to run. Specifically, for a ranged loop, if
    // the items are constant integers and not the same, we will
    // always run the loop.

    if (!li->ranged || !range_runs_check(ctx, container_node)) {
        bstart   = c4m_cfg_enter_block(ctx->cfg, n);
        ctx->cfg = c4m_cfg_exit_block(bstart, bstart, n);
    }
    else {
        branch->contents.branches.num_branches--;
    }

    // Here, it's time to clean up. We need to reset the tree and the
    // loop stack, and remove our iteration variable(s) from the
    // scope.  Plus, if any variables were shadowed, we need to
    // restore them.

    c4m_list_pop(ctx->loop_stack);
    ctx->node = n;

    loop_pop_ix_var(ctx, li);

    hatrack_dict_remove(ctx->local_scope->symbols, last_var_name);

    if (li->label_last != NULL) {
        hatrack_dict_remove(ctx->local_scope->symbols, li->label_last);
    }

    hatrack_dict_remove(ctx->local_scope->symbols, var1_name);
    if (var2_name != NULL) {
        hatrack_dict_remove(ctx->local_scope->symbols, var2_name);
    }
    if (li->shadowed_last != NULL) {
        hatrack_dict_put(ctx->local_scope->symbols,
                         last_var_name,
                         li->shadowed_last);
    }
    if (li->shadowed_lvar_1 != NULL) {
        hatrack_dict_put(ctx->local_scope->symbols,
                         var1_name,
                         li->shadowed_lvar_1);
    }
    if (li->shadowed_lvar_2 != NULL) {
        hatrack_dict_put(ctx->local_scope->symbols,
                         var2_name,
                         li->shadowed_lvar_2);
    }

    // Finally, do type checking based on the type of loop. The type
    // will always be contained in the second non-label subtree of the
    // loop node. We stashed that above as `container_pnode`.

    if (li->ranged) {
        // Ranged fors are easy enough. The assigned variable is
        // always based on the range type.
        merge_or_err(ctx, li->lvar_1->type, container_pnode->type);
    }
    else {
        // If there are two variables, we can infer that the container is
        // a dictionary.
        //
        // Otherwise, if we have no info, it can be any set or list type.
        //
        // We never allow loops over tuples.

        c4m_type_t *cinfo = c4m_new_typevar();

        if (li->lvar_2 != NULL) {
            c4m_remove_list_options(cinfo);
            c4m_remove_set_options(cinfo);
            c4m_remove_tuple_options(cinfo);

            c4m_list_append(cinfo->details->items, li->lvar_1->type);
            c4m_list_append(cinfo->details->items, li->lvar_2->type);
        }
        else {
            c4m_remove_tuple_options(cinfo);
            c4m_remove_dict_options(cinfo);
            c4m_list_append(cinfo->details->items, li->lvar_1->type);
        }

        merge_or_err(ctx, cinfo, container_pnode->type);

        pnode->type = container_pnode->type;
    }

    ctx->cfg = exit;
}

static void
handle_while(pass2_ctx *ctx)
{
    int              expr_ix = 0;
    c4m_tree_node_t *n       = ctx->node;
    c4m_pnode_t     *p       = c4m_get_pnode(n);
    c4m_loop_info_t *li      = loop_init(p->extra_info);
    c4m_cfg_node_t  *entrance;
    c4m_cfg_node_t  *branch;
    c4m_cfg_node_t  *bstart;

    c4m_list_append(ctx->loop_stack, li);

    if (c4m_node_has_type(n->children[0], c4m_nt_label)) {
        expr_ix++;
        li->branch_info.label = c4m_node_text(n->children[0]);
    }

    loop_push_ix_var(ctx, li);

    entrance = c4m_cfg_enter_block(ctx->cfg, n);
    ctx->cfg = entrance;

    li->loop_ix->cfg_kill_node = c4m_cfg_exit_node(entrance);

    ctx->node = n->children[expr_ix++];
    base_check_pass_dispatch(ctx);
    branch    = c4m_cfg_block_new_branch_node(ctx->cfg,
                                           2,
                                           li->branch_info.label,
                                           ctx->node);
    bstart    = c4m_cfg_enter_block(branch, n->children[expr_ix]);
    ctx->cfg  = bstart;
    ctx->node = n->children[expr_ix];

    base_check_pass_dispatch(ctx);
    ctx->cfg = c4m_cfg_exit_block(ctx->cfg, bstart, n);

    next_branch(ctx, branch);

    bstart   = c4m_cfg_enter_block(branch, n->children[expr_ix]);
    ctx->cfg = c4m_cfg_exit_block(bstart, bstart, n);

    c4m_list_pop(ctx->loop_stack);
    ctx->node = n;
    loop_pop_ix_var(ctx, li);

    ctx->cfg = c4m_cfg_exit_node(entrance);
}

static void
handle_range(pass2_ctx *ctx)
{
    process_children(ctx);
    set_node_type(ctx,
                  ctx->node,
                  type_check_nodes_no_err(ctx->node->children[0],
                                          ctx->node->children[1]));

    if (c4m_type_is_error(
            merge_ignore_err(get_pnode_type(ctx->node), c4m_type_i64()))) {
        c4m_add_error(ctx->module_ctx,
                      c4m_err_range_type,
                      ctx->node);
    }
}

c4m_list_t *
c4m_get_case_branches(c4m_tree_node_t *n)
{
    c4m_list_t *result = c4m_list(c4m_type_ref());

    for (int i = 0; i < n->num_kids; i++) {
        c4m_pnode_t *pnode = c4m_get_pnode(n->children[i]);
        if (pnode->kind == c4m_nt_case) {
            c4m_list_append(result, n->children[i]);
        }
    }

    return result;
}

static void
handle_typeof_statement(pass2_ctx *ctx)
{
    c4m_tree_node_t    *saved    = ctx->node;
    c4m_pnode_t        *pnode    = c4m_get_pnode(saved);
    c4m_control_info_t *ci       = control_init(pnode->extra_info);
    c4m_list_t         *branches = c4m_get_case_branches(saved);
    c4m_tree_node_t    *elsenode = c4m_get_match_on_node(saved,
                                                      c4m_else_condition);
    c4m_tree_node_t    *variant  = c4m_get_match_on_node(saved,
                                                     c4m_case_cond_typeof);
    c4m_tree_node_t    *label    = c4m_get_match_on_node(saved, c4m_opt_label);
    int                 ncases   = c4m_list_len(branches);
    c4m_list_t         *prev_types;

    if (label != NULL) {
        ci->label = c4m_node_text(label);
    }

    ci->non_loop = true;
    c4m_list_append(ctx->loop_stack, ci);

    prev_types = c4m_new(c4m_type_list(c4m_type_typespec()));
    ctx->node  = variant;
    base_check_pass_dispatch(ctx);

    c4m_pnode_t    *variant_p    = c4m_get_pnode(variant);
    c4m_type_t     *type_to_test = (c4m_type_t *)variant_p->type;
    c4m_symbol_t   *sym          = variant_p->extra_info;
    c4m_cfg_node_t *entrance     = c4m_cfg_enter_block(ctx->cfg, saved);
    c4m_symbol_t   *saved_sym;
    c4m_cfg_node_t *cfgbranch;
    c4m_cfg_node_t *bstart;
    c4m_symbol_t   *tmp;

    // We don't care what scope `sym` came from; it'll get looked up
    // in the local scope first, so we will shadow it in the local scope.
    saved_sym = hatrack_dict_get(ctx->local_scope->symbols, sym->name, NULL);
    ctx->cfg  = entrance;

    if (c4m_type_is_concrete(type_to_test)) {
        c4m_add_error(ctx->module_ctx,
                      c4m_err_concrete_typeof,
                      variant,
                      type_to_test);
        return;
    }

    add_use(ctx, sym);

    cfgbranch = c4m_cfg_block_new_branch_node(ctx->cfg,
                                              ncases + 1,
                                              NULL,
                                              ctx->node);

    for (int i = 0; i < ncases; i++) {
        c4m_tree_node_t *branch   = c4m_list_get(branches, i, NULL);
        c4m_dict_t      *type_ctx = c4m_new(c4m_type_dict(c4m_type_utf8(),
                                                     c4m_type_ref()));

        for (int j = 0; j < branch->num_kids - 1; j++) {
            c4m_type_t *casetype = c4m_node_to_type(ctx->module_ctx,
                                                    branch->children[j],
                                                    type_ctx);

            c4m_pnode_t *branch_pnode = c4m_get_pnode(branch->children[j]);
            branch_pnode->value       = (c4m_obj_t)casetype;
            branch_pnode->type        = casetype;

            for (int k = 0; k < c4m_list_len(prev_types); k++) {
                c4m_type_t *oldcase = c4m_list_get(prev_types, k, NULL);

                if (c4m_types_are_compat(oldcase, casetype, NULL)) {
                    c4m_add_warning(ctx->module_ctx,
                                    c4m_warn_type_overlap,
                                    branch->children[j],
                                    casetype,
                                    oldcase);
                    break;
                }
                if (!c4m_types_are_compat(casetype, type_to_test, NULL)) {
                    c4m_add_error(ctx->module_ctx,
                                  c4m_err_dead_branch,
                                  branch->children[j],
                                  casetype);
                    break;
                }
            }

            c4m_list_append(prev_types, c4m_type_copy(casetype));

            // Alias absolutely everything except the
            // type. It's the only thing that should change.
            tmp                   = c4m_add_or_replace_symbol(ctx->module_ctx,
                                            ctx->local_scope,
                                            sym->name);
            tmp->flags            = sym->flags;
            tmp->kind             = sym->kind;
            tmp->declaration_node = sym->declaration_node;
            tmp->path             = sym->path;
            tmp->value            = sym->value;
            tmp->my_scope         = sym->my_scope;
            tmp->other_info       = sym->other_info;
            tmp->sym_defs         = sym->sym_defs;
            tmp->sym_uses         = sym->sym_uses;
            tmp->type             = casetype;
        }

        ctx->node = branch->children[branch->num_kids - 1];
        next_branch(ctx, cfgbranch);
        bstart   = c4m_cfg_enter_block(ctx->cfg, ctx->node);
        ctx->cfg = bstart;

        base_check_pass_dispatch(ctx);

        if (ctx->node->num_kids == 0) {
            c4m_add_warning(ctx->module_ctx,
                            c4m_warn_empty_case,
                            ctx->node);
        }

        ctx->cfg = c4m_cfg_exit_block(ctx->cfg, bstart, ctx->node);

        if (c4m_list_len(tmp->sym_defs) > c4m_list_len(sym->sym_defs)) {
            for (int k = c4m_list_len(sym->sym_defs);
                 k < c4m_list_len(tmp->sym_defs);
                 k++) {
                c4m_list_append(sym->sym_defs,
                                c4m_list_get(tmp->sym_defs, k, NULL));
            }
        }

        if (c4m_list_len(tmp->sym_uses) > c4m_list_len(sym->sym_uses)) {
            for (int k = c4m_list_len(sym->sym_uses);
                 k < c4m_list_len(tmp->sym_uses);
                 k++) {
                c4m_list_append(sym->sym_uses,
                                c4m_list_get(tmp->sym_uses, k, NULL));
            }
        }
        tmp->linked_symbol = sym;

        next_branch(ctx, cfgbranch);

        if (elsenode != NULL) {
            ctx->node = elsenode;
            bstart    = c4m_cfg_enter_block(ctx->cfg, ctx->node);
            ctx->cfg  = bstart;

            base_check_pass_dispatch(ctx);
            ctx->cfg = c4m_cfg_exit_block(ctx->cfg, bstart, ctx->node);
        }
        else {
            // Dummy CFG node for when we don't match any case.
            bstart = c4m_cfg_enter_block(ctx->cfg, saved);
            c4m_cfg_exit_block(bstart, bstart, saved);
        }

        ctx->cfg  = c4m_cfg_exit_node(entrance);
        ctx->node = saved;
    }

    if (saved_sym != NULL) {
        hatrack_dict_put(ctx->local_scope->symbols, sym->name, saved_sym);
    }

    c4m_list_pop(ctx->loop_stack);
}

static void
handle_switch_statement(pass2_ctx *ctx)
{
    c4m_tree_node_t    *saved        = ctx->node;
    c4m_pnode_t        *pnode        = c4m_get_pnode(saved);
    c4m_control_info_t *bi           = control_init(pnode->extra_info);
    c4m_list_t         *branches     = c4m_get_case_branches(saved);
    c4m_tree_node_t    *elsenode     = c4m_get_match_on_node(saved,
                                                      c4m_else_condition);
    c4m_tree_node_t    *variant_node = c4m_get_match_on_node(saved,
                                                          c4m_case_cond);
    c4m_tree_node_t    *label        = c4m_get_match_on_node(saved,
                                                   c4m_opt_label);
    int                 ncases       = c4m_list_len(branches);
    c4m_cfg_node_t     *entrance;
    c4m_cfg_node_t     *cfgbranch;
    c4m_cfg_node_t     *bstart;

    if (label != NULL) {
        bi->label = c4m_node_text(label);
    }

    bi->non_loop = true;

    c4m_list_append(ctx->loop_stack, bi);

    entrance  = c4m_cfg_enter_block(ctx->cfg, saved);
    ctx->cfg  = entrance;
    ctx->node = variant_node;
    base_check_pass_dispatch(ctx);

    // For the CFG's purposes, pretend we evaluate all case exprs w/o
    // short circuit, before the branch. Since we're going to
    // lazy-evaluate branches, we maybe should break this out into
    // nested ifs, like we do w/ if/else chains.
    //
    // I haven't gone through the pain though, because it is
    // impossible to define anything in the case condition, except
    // indirectly through a function call. And the worst case there
    // would be a spurious use-before-def that won't even seem
    // spurious generally.

    for (int i = 0; i < ncases; i++) {
        c4m_tree_node_t *branch    = c4m_list_get(branches, i, NULL);
        c4m_pnode_t     *pcond     = c4m_get_pnode(branch);
        // Often there will only be two kids, the test and the body.
        // In that case, n_cases will be one.
        // But you can do:
        // `case 1, 2, 3:`
        // in which case, n_cases will be 3.
        int              sub_cases = branch->num_kids - 1;

        for (int j = 0; j < sub_cases; j++) {
            ctx->node = branch->children[j];

            base_check_pass_dispatch(ctx);

            pcond->type = type_check_nodes_no_err(variant_node, ctx->node);

            if (c4m_type_is_error(pcond->type)) {
                c4m_add_error(ctx->module_ctx,
                              c4m_err_switch_case_type,
                              ctx->node,
                              get_pnode_type(ctx->node),
                              get_pnode_type(variant_node));
                return;
            }
        }
    }

    cfgbranch = c4m_cfg_block_new_branch_node(ctx->cfg,
                                              ncases + 1,
                                              NULL,
                                              ctx->node);

    for (int i = 0; i < ncases; i++) {
        next_branch(ctx, cfgbranch);
        c4m_tree_node_t *branch = c4m_list_get(branches, i, NULL);

        ctx->node = branch->children[branch->num_kids - 1];
        bstart    = c4m_cfg_enter_block(ctx->cfg, ctx->node);
        ctx->cfg  = bstart;

        if (ctx->node->num_kids == 0) {
            c4m_add_warning(ctx->module_ctx,
                            c4m_warn_empty_case,
                            ctx->node);
        }

        base_check_pass_dispatch(ctx);

        ctx->cfg = c4m_cfg_exit_block(ctx->cfg, bstart, ctx->node);
    }

    next_branch(ctx, cfgbranch);

    if (elsenode != NULL) {
        ctx->node = elsenode;
        bstart    = c4m_cfg_enter_block(ctx->cfg, ctx->node);
        ctx->cfg  = bstart;

        base_check_pass_dispatch(ctx);
        ctx->cfg = c4m_cfg_exit_block(ctx->cfg, bstart, ctx->node);
    }
    else {
        // Dummy CFG node for when we don't match any case.
        bstart   = c4m_cfg_enter_block(ctx->cfg, saved);
        ctx->cfg = c4m_cfg_exit_block(bstart, bstart, saved);
    }

    ctx->cfg  = c4m_cfg_exit_node(entrance);
    ctx->node = saved;

    c4m_list_pop(ctx->loop_stack);
}

static void
builtin_bincall(pass2_ctx *ctx)
{
    // Currently, this routine type checks based on the operation, and
    // sets the expected return type.
    //
    // Here:
    //
    // 1. Comparison ops must have the same type, and the return type is
    //    always bool.
    //
    // 2. All math ops take the same input type and returns the
    //    same output type.
    //
    // 3. Eventually we will add a float div that is a special case,
    //    but I'm still thinking about the syntax on that one, so it's
    //    not implemented yet.
    //
    // Currently, we don't bother adding the call to the CFG, since in
    // most cases this will be inlined, and not a real call.
    //
    // Once we add objects, we probably should add the call in for objects
    // (TODO).

    c4m_type_t    *tleft  = get_pnode_type(ctx->node->children[0]);
    c4m_type_t    *tright = get_pnode_type(ctx->node->children[1]);
    c4m_pnode_t   *pn     = c4m_get_pnode(ctx->node);
    c4m_operator_t op     = (c4m_operator_t)pn->extra_info;

    switch (op) {
    case c4m_op_plus:
    case c4m_op_minus:
    case c4m_op_mul:
    case c4m_op_mod:
    case c4m_op_div:
    case c4m_op_fdiv:
    case c4m_op_shl:
    case c4m_op_shr:
    case c4m_op_bitand:
    case c4m_op_bitor:
    case c4m_op_bitxor:
        set_node_type(ctx, ctx->node, merge_or_err(ctx, tleft, tright));
        return;
    case c4m_op_lt:
    case c4m_op_lte:
    case c4m_op_gt:
    case c4m_op_gte:
    case c4m_op_eq:
    case c4m_op_neq:
        merge_or_err(ctx, tleft, tright);
        set_node_type(ctx, ctx->node, c4m_type_bool());
        return;
    default:
        c4m_unreachable();
    }
}

static void
handle_binary_op(pass2_ctx *ctx)
{
    process_children(ctx);
    builtin_bincall(ctx);
}

static void
base_handle_assign(pass2_ctx *ctx, bool binop)
{
    c4m_tree_node_t *saved = ctx->node;
    c4m_pnode_t     *pnode = c4m_get_pnode(saved);

    ctx->augmented_assignment = binop;

    if (binop) {
        ctx->node = saved->children[0];
        // With binops, we process the LHS twice; once in a use context
        // and once in a def context (below, after processing the RHS).
        base_check_pass_dispatch(ctx);
    }

    ctx->node = saved->children[1];
    start_data_flow(ctx);
    base_check_pass_dispatch(ctx);

    if (binop) {
        ctx->node = saved;
        builtin_bincall(ctx);
    }

    ctx->node = saved->children[0];

    def_context_enter(ctx);
    base_check_pass_dispatch(ctx);
    def_use_context_exit(ctx);

    pnode->type = merge_or_err(ctx,
                               get_pnode_type(saved->children[0]),
                               get_pnode_type(saved->children[1]));

    ctx->node                 = saved;
    ctx->augmented_assignment = false;
}

static inline void
handle_assign(pass2_ctx *ctx)
{
    if (ctx->node->num_kids == 1) {
        process_children(ctx);
    }
    else {
        base_handle_assign(ctx, false);
    }
}

static inline void
handle_binop_assign(pass2_ctx *ctx)
{
    base_handle_assign(ctx, true);
}

static void
handle_section_decl(pass2_ctx *ctx)
{
    c4m_tree_node_t *saved      = ctx->node;
    int              n          = saved->num_kids - 1;
    c4m_utf8_t      *saved_path = ctx->current_section_prefix;

    if (saved_path == NULL) {
        ctx->current_section_prefix = c4m_node_text(saved->children[0]);
    }
    else {
        ctx->current_section_prefix = c4m_cstr_format(
            "{}.{}",
            saved_path,
            c4m_node_text(saved->children[0]));
    }

    if (n == 2) {
        ctx->current_section_prefix = c4m_cstr_format(
            "{}.{}",
            ctx->current_section_prefix,
            c4m_node_text(saved->children[1]));
    }

    ctx->node = saved->children[n];
    base_check_pass_dispatch(ctx);
    ctx->current_section_prefix = saved_path;
    ctx->node                   = saved;
}

static void
handle_identifier(pass2_ctx *ctx)
{
    c4m_pnode_t *pnode = c4m_get_pnode(ctx->node);
    c4m_utf8_t  *id    = c4m_node_text(ctx->node);

    if (ctx->current_section_prefix != NULL) {
        id = c4m_cstr_format("{}.{}", ctx->current_section_prefix, id);
    }

    c4m_symbol_t *sym = (void *)lookup_or_add(ctx, id);
    pnode->extra_info = (void *)sym;
    set_node_type(ctx, ctx->node, sym->type);
}

static inline bool
should_defer(pass2_ctx *ctx, c4m_utf8_t *litmod)
{
    if (litmod && c4m_str_codepoint_len(litmod)) {
        return false;
    }

    c4m_tree_node_t *t = ctx->node->parent;
    c4m_pnode_t     *p = c4m_get_pnode(t);

    if (p->kind != c4m_nt_expression) {
        return false;
    }

    t = t->parent;

    if (!t) {
        return false;
    }

    p = c4m_get_pnode(t);

    // Only do it for simple assignment.
    return p->kind == c4m_nt_assign;
}

static void
check_literal(pass2_ctx *ctx)
{
    //  Right now, we don't try to fold sub-items.
    c4m_pnode_t *pnode  = c4m_get_pnode(ctx->node);
    c4m_str_t   *litmod = pnode->token->literal_modifier;

    if (litmod != NULL && litmod->data) {
        litmod            = c4m_to_utf8(litmod);
        pnode->extra_info = litmod;
    }

    switch (pnode->kind) {
    case c4m_nt_simple_lit:
        pnode->value = c4m_node_simp_literal(ctx->node);

        // If there's no litmod, we want to defer adding the type until
        // if the type is concrete by the end of the pass, and the
        // type of the parsed object is not aligned, then we will
        // re-parse the type.

        if (should_defer(ctx, litmod)) {
            c4m_list_append(ctx->simple_lits_wo_mod, ctx->node);
            pnode->type = c4m_new_typevar();
        }
        else {
            pnode->type = c4m_get_my_type(pnode->value);
        }

        break;
    case c4m_nt_lit_callback:
        pnode->value = c4m_node_to_callback(ctx->module_ctx, ctx->node);
        c4m_list_append(ctx->module_ctx->callback_literals, pnode->value);
        break;
    case c4m_nt_lit_tspec:
        do {
            c4m_type_t *t;

            t = c4m_node_to_type(ctx->module_ctx,
                                 ctx->node,
                                 c4m_new(c4m_type_dict(c4m_type_utf8(),
                                                       c4m_type_ref())));

            pnode->value = (c4m_obj_t *)t;
        } while (0);
        break;
    default:
        calculate_container_type(ctx, ctx->node);
        break;
    }
}

static void
handle_member(pass2_ctx *ctx)
{
    c4m_tree_node_t **kids     = ctx->node->children;
    c4m_utf8_t       *sym_name = c4m_node_text(kids[0]);
    c4m_utf8_t       *dot      = c4m_new_utf8(".");
    c4m_pnode_t      *pnode    = c4m_get_pnode(ctx->node);

    for (int i = 1; i < ctx->node->num_kids; i++) {
        sym_name = c4m_str_concat(sym_name, dot);
        sym_name = c4m_str_concat(sym_name, c4m_node_text(kids[i]));
    }

    if (ctx->current_section_prefix != NULL) {
        sym_name = c4m_cstr_format("{}.{}", sym_name);
    }

    c4m_symbol_t *sym = (void *)lookup_or_add(ctx, sym_name);
    pnode->extra_info = (void *)sym;
    set_node_type(ctx, ctx->node, sym->type);
}

static void
handle_binary_logical_op(pass2_ctx *ctx)
{
    c4m_type_t  *btype = c4m_type_bool();
    c4m_pnode_t *kid1  = c4m_get_pnode(ctx->node->children[0]);
    c4m_pnode_t *kid2  = c4m_get_pnode(ctx->node->children[1]);
    c4m_pnode_t *pn    = c4m_get_pnode(ctx->node);

    process_children(ctx);
    // clang-format off
    if (!(base_node_tcheck(ctx, kid1, btype) &&
	  base_node_tcheck(ctx, kid2, btype))) {
        // clang-format on

        pn->type = btype;
    }
    else {
        pn->type = c4m_type_error();
    }
}

static void
handle_cmp(pass2_ctx *ctx)
{
    c4m_tree_node_t *tn = ctx->node;
    c4m_pnode_t     *pn = c4m_get_pnode(tn);

    process_children(ctx);

    if (c4m_type_is_error(
            type_check_nodes_no_err(tn->children[0], tn->children[1]))) {
        pn->type = c4m_type_error();
        c4m_add_error(ctx->module_ctx,
                      c4m_err_cannot_cmp,
                      tn,
                      get_pnode_type(tn->children[0]),
                      get_pnode_type(tn->children[1]));
    }
    else {
        pn->type = c4m_type_bool();
    }
}

static void
handle_unary_op(pass2_ctx *ctx)
{
    process_children(ctx);

    c4m_utf8_t  *text = c4m_node_text(ctx->node);
    c4m_pnode_t *pn   = c4m_get_pnode(ctx->node);

    if (!strcmp(text->data, "-")) {
        pn->extra_info = (void *)0;

        if (c4m_type_is_error(
                type_check_node_vs_type_no_err(ctx->node, c4m_type_i64()))) {
            c4m_add_error(ctx->module_ctx,
                          c4m_err_unary_minus_type,
                          ctx->node);
        }
    }
    else { // NOT operator.
        pn->type       = c4m_type_bool();
        pn->extra_info = (void *)1;
    }
}

static void
handle_var_decl(pass2_ctx *ctx)
{
    c4m_list_t *syms = c4m_apply_pattern_on_node(ctx->node, c4m_sym_decls);
    for (int i = 0; i < c4m_list_len(syms); i++) {
        c4m_tree_node_t *one_set   = c4m_list_get(syms, i, NULL);
        c4m_list_t      *var_names = c4m_apply_pattern_on_node(one_set,
                                                          c4m_sym_names);
        c4m_tree_node_t *init      = c4m_get_match_on_node(one_set, c4m_sym_init);

        if (init == NULL) {
            continue;
        }

        c4m_tree_node_t *one_name = c4m_list_get(var_names,
                                                 c4m_list_len(var_names) - 1,
                                                 NULL);
        c4m_pnode_t     *pn       = c4m_get_pnode(one_name);
        c4m_symbol_t    *sym      = (c4m_symbol_t *)pn->value;

        if (!sym) {
            return;
        }

        if (sym->flags & C4M_F_HAS_INITIALIZER) {
            ctx->node = init;
            base_check_pass_dispatch(ctx);
            ctx->node = one_name;
            add_def(ctx, sym, true);
            c4m_pnode_t *pn = c4m_get_pnode(sym->value_node);
            sym->value      = pn->value;
            type_check_node_against_sym(ctx, pn, sym);
        }
    }
}

static void
base_check_pass_dispatch(pass2_ctx *ctx)
{
    c4m_pnode_t *pnode = c4m_tree_get_contents(ctx->node);
    switch (pnode->kind) {
    case c4m_nt_global_enum:
    case c4m_nt_enum:
    case c4m_nt_func_def:
    case c4m_nt_config_spec:
    case c4m_nt_section_spec:
    case c4m_nt_param_block:
    case c4m_nt_extern_block:
    case c4m_nt_use:
        return;
    case c4m_nt_variable_decls:
        handle_var_decl(ctx);
        break;
    case c4m_nt_section:
        handle_section_decl(ctx);
        break;

    case c4m_nt_identifier:
        handle_identifier(ctx);
        break;

    case c4m_nt_member:
        handle_member(ctx);
        break;

    case c4m_nt_simple_lit:
    case c4m_nt_lit_list:
    case c4m_nt_lit_dict:
    case c4m_nt_lit_set:
    case c4m_nt_lit_empty_dict_or_set:
    case c4m_nt_lit_tuple:
    case c4m_nt_lit_unquoted:
    case c4m_nt_lit_callback:
    case c4m_nt_lit_tspec:
        check_literal(ctx);
        break;

    case c4m_nt_index:
        handle_index(ctx);
        return;

    case c4m_nt_call:
        handle_call(ctx);
        break;

    case c4m_nt_break:
        handle_break(ctx);
        break;

    case c4m_nt_continue:
        handle_continue(ctx);
        break;

    case c4m_nt_if:
        handle_if(ctx);
        break;

    case c4m_nt_for:
        handle_for(ctx);
        break;

    case c4m_nt_while:
        handle_while(ctx);
        break;

    case c4m_nt_range:
        handle_range(ctx);
        break;

    case c4m_nt_typeof:
        handle_typeof_statement(ctx);
        break;

    case c4m_nt_switch:
        handle_switch_statement(ctx);
        break;

    case c4m_nt_assign:
        handle_assign(ctx);
        break;

    case c4m_nt_binary_assign_op:
        handle_binop_assign(ctx);
        break;

    case c4m_nt_or:
    case c4m_nt_and:
        handle_binary_logical_op(ctx);
        break;

    case c4m_nt_cmp:
        handle_cmp(ctx);
        break;

    case c4m_nt_binary_op:
        handle_binary_op(ctx);
        break;

    case c4m_nt_unary_op:
        handle_unary_op(ctx);
        break;

    case c4m_nt_return:
        handle_return(ctx);
        break;

    case c4m_nt_elided:
        handle_elision(ctx);
        break;

#ifdef C4M_DEV
    case c4m_nt_print:
        c4m_list_append(ctx->module_ctx->print_nodes, ctx->node);
        process_children(ctx);
        break;
#endif
    default:
        process_children(ctx);
        break;
    }
}

static void check_pass_toplevel_dispatch(pass2_ctx *);

static inline void
process_toplevel_children(pass2_ctx *ctx)
{
    c4m_tree_node_t *saved = ctx->node;
    int              n     = saved->num_kids;

    for (int i = 0; i < n; i++) {
        ctx->node = saved->children[i];
        check_pass_toplevel_dispatch(ctx);
    }

    ctx->node = saved;
}

static void
check_pass_toplevel_dispatch(pass2_ctx *ctx)
{
    c4m_pnode_t *pnode = c4m_tree_get_contents(ctx->node);

    switch (pnode->kind) {
    case c4m_nt_module:
        process_toplevel_children(ctx);
        return;
    case c4m_nt_variable_decls:
        handle_var_decl(ctx);
        return;
    case c4m_nt_func_def:
        c4m_list_append(ctx->func_nodes, ctx->node);
        return;
    case c4m_nt_enum:
    case c4m_nt_global_enum:
    case c4m_nt_param_block:
    case c4m_nt_extern_block:
    case c4m_nt_use:
        // Absolutely 0 to do for these until we generate code.
        return;
    default:
        base_check_pass_dispatch(ctx);
        return;
    }
}

typedef struct {
    int              num_defs;
    int              num_uses;
    c4m_fn_decl_t   *decl;
    c4m_sig_info_t  *si;
    c4m_tree_node_t *node;
    pass2_ctx       *pass_ctx;
    c4m_symbol_t    *sym;
    c4m_symbol_t    *formal;
    bool             delete_result_var;
} fn_check_ctx;

static void
check_return_type(fn_check_ctx *ctx)
{
    c4m_type_t *decl_type;
    c4m_type_t *cmp_type;

    if (ctx->num_defs <= 1) {
        if (ctx->num_uses == 0) {
            ctx->delete_result_var = true;
            ctx->si->void_return   = 1;

            if (ctx->si->return_info.type != NULL) {
                c4m_add_error(ctx->pass_ctx->module_ctx,
                              c4m_err_no_ret,
                              ctx->node,
                              ctx->si->return_info.type);
                return;
            }
            else {
                ctx->si->return_info.type = c4m_type_void();
                // Drop below to set this on the fn.
            }
        }
        else {
            c4m_tree_node_t *first_use = c4m_list_get(ctx->sym->sym_uses,
                                                      0,
                                                      NULL);
            c4m_add_error(ctx->pass_ctx->module_ctx,
                          c4m_err_use_no_def,
                          first_use);
            return;
        }
    }

    if (ctx->si->return_info.type != NULL) {
        decl_type = c4m_type_copy(ctx->si->return_info.type);
        cmp_type  = merge_ignore_err(decl_type, ctx->sym->type);

        if (c4m_type_is_error(cmp_type)) {
            c4m_add_error(ctx->pass_ctx->module_ctx,
                          c4m_err_declared_incompat,
                          ctx->sym->declaration_node->children[2],
                          decl_type,
                          ctx->sym->type);
            return;
        }
        if (c4m_type_cmp_exact(decl_type, cmp_type) != c4m_type_match_exact) {
            c4m_add_error(ctx->pass_ctx->module_ctx,
                          c4m_err_too_general,
                          ctx->sym->declaration_node->children[2],
                          decl_type,
                          ctx->sym->type);
            return;
        }

        merge_ignore_err(ctx->si->return_info.type, ctx->sym->type);
    }
    else {
        // Formal was never used; just merge to be safe.
        cmp_type = merge_ignore_err(ctx->sym->type, ctx->formal->type);

        ctx->si->return_info.type = cmp_type;
        assert(!c4m_type_is_error(cmp_type));
    }
}

static void
check_formal_param(fn_check_ctx *ctx)
{
    c4m_fn_param_info_t *param   = ctx->si->param_info;
    c4m_utf8_t          *symname = ctx->sym->name;
    c4m_type_t          *decl_type;
    c4m_type_t          *cmp_type;

    if (ctx->num_uses == 0) {
        c4m_add_warning(ctx->pass_ctx->module_ctx,
                        c4m_warn_unused_param,
                        ctx->sym->declaration_node,
                        ctx->sym->name);
    }

    for (int i = 0; i < ctx->si->num_params; i++) {
        if (!strcmp(param->name->data, symname->data)) {
            break;
        }
        param++;
    }

    if (param->type != NULL) {
        decl_type = c4m_type_copy(param->type);
        cmp_type  = merge_ignore_err(decl_type, ctx->sym->type);

        if (c4m_type_is_error(cmp_type)) {
            c4m_add_error(ctx->pass_ctx->module_ctx,
                          c4m_err_declared_incompat,
                          ctx->sym->declaration_node,
                          decl_type,
                          ctx->sym->type);
            return;
        }
        if (c4m_type_cmp_exact(decl_type, cmp_type) != c4m_type_match_exact) {
            c4m_add_error(ctx->pass_ctx->module_ctx,
                          c4m_err_too_general,
                          ctx->sym->declaration_node,
                          decl_type,
                          ctx->sym->type);
            return;
        }

        merge_ignore_err(param->type, ctx->sym->type);
    }
    else {
        cmp_type = merge_ignore_err(ctx->sym->type, ctx->formal->type);
        assert(!c4m_type_is_error(cmp_type));
    }
}

static inline bool
warn_on_unused(c4m_symbol_t *sym)
{
    switch (sym->kind) {
    case C4M_SK_VARIABLE:
    case C4M_SK_FORMAL:
        return true;
    default:
        return false;
    }
}

static void
check_user_decl(fn_check_ctx *ctx)
{
    if (!ctx->num_defs && !ctx->num_uses && warn_on_unused(ctx->sym)) {
        c4m_add_warning(ctx->pass_ctx->module_ctx,
                        c4m_warn_unused_decl,
                        ctx->sym->declaration_node,
                        ctx->sym->name);
        return;
    }

    if (ctx->num_defs == 0) {
        c4m_tree_node_t *loc = ctx->sym->declaration_node;
        if (loc == NULL) {
            loc = c4m_list_get(ctx->sym->sym_uses, 0, NULL);
        }

        c4m_add_error(ctx->pass_ctx->module_ctx,
                      c4m_err_use_no_def,
                      loc,
                      ctx->sym->name);
        return;
    }

    if (ctx->num_uses == 0) {
        c4m_tree_node_t *loc = ctx->sym->declaration_node;
        if (loc == NULL) {
            loc = c4m_list_get(ctx->sym->sym_defs, 0, NULL);
        }

        c4m_add_warning(ctx->pass_ctx->module_ctx,
                        c4m_warn_def_without_use,
                        loc,
                        ctx->sym->name);
    }

    uint64_t mask  = C4M_F_DECLARED_LET | C4M_F_DECLARED_CONST;
    uint64_t flags = ctx->sym->flags & mask;

    if (ctx->num_defs > 1 && flags) {
        c4m_utf8_t *var_kind;

        if (flags & C4M_F_DECLARED_LET) {
            var_kind = c4m_new_utf8("let");
        }
        else {
            var_kind = c4m_new_utf8("const");
        }

        c4m_tree_node_t *first_def = c4m_list_get(ctx->sym->sym_defs,
                                                  0,
                                                  NULL);

        for (int i = 1; i < ctx->num_defs; i++) {
            c4m_tree_node_t *bad_def = c4m_list_get(ctx->sym->sym_defs,
                                                    1,
                                                    NULL);

            c4m_add_error(ctx->pass_ctx->module_ctx,
                          c4m_err_single_def,
                          bad_def,
                          var_kind,
                          c4m_node_get_loc_str(first_def));
        }
    }
}

static void
check_function(pass2_ctx *ctx, c4m_symbol_t *fn_sym)
{
    fn_check_ctx check_ctx = {
        .node              = ctx->node,
        .decl              = fn_sym->value,
        .pass_ctx          = ctx,
        .delete_result_var = false,
    };

    check_ctx.si = check_ctx.decl->signature_info;

    static char resname[] = "$result";
    uint64_t    num_items;
    void      **view;

    c4m_scope_t *actuals = check_ctx.si->fn_scope;
    c4m_scope_t *formals = check_ctx.si->formals;

    view = hatrack_dict_values_sort(actuals->symbols, &num_items);

    for (uint64_t i = 0; i < num_items; i++) {
        c4m_symbol_t *sym = view[i];

        check_ctx.sym      = sym;
        check_ctx.formal   = hatrack_dict_get(formals->symbols,
                                            sym->name,
                                            NULL);
        check_ctx.num_defs = c4m_list_len(sym->sym_defs);
        check_ctx.num_uses = c4m_list_len(sym->sym_uses);

        if (!check_ctx.formal) {
            check_user_decl(&check_ctx);
        }
        else {
            if (!strcmp(sym->name->data, resname)) {
                check_return_type(&check_ctx);
            }
            else {
                // We've matched the name in one scope to formals;
                // let's mark it as such so that we properly
                // capture the def in the right place.
                // Probably should move this to decl_pass.c...
                check_formal_param(&check_ctx);
                sym->kind = C4M_SK_FORMAL;
            }
        }
    }

    if (check_ctx.delete_result_var) {
        c4m_utf8_t *s = c4m_new_utf8(resname);
        hatrack_dict_remove(actuals->symbols, s);
        hatrack_dict_remove(formals->symbols, s);
    }

    // We'd previously set the fn symbol's signature, and we need
    // to update it to something more specific now.
    c4m_list_t *param_types = c4m_new(c4m_type_list(c4m_type_typespec()));

    // TODO: we need to handle varargs here and in the
    // check_formal_param bit. Right now this won't work.

    for (int i = 0; i < check_ctx.si->num_params; i++) {
        assert(check_ctx.si->param_info[i].type);
        c4m_list_append(param_types, check_ctx.si->param_info[i].type);
    }

    c4m_type_t *ret_type = check_ctx.si->return_info.type;

    check_ctx.si->full_type = c4m_type_fn(ret_type, param_types, false);

    merge_ignore_err(fn_sym->type, check_ctx.si->full_type);
}

static void
check_module_toplevel(pass2_ctx *ctx)
{
    ctx->node            = ctx->module_ctx->parse_tree;
    ctx->local_scope     = ctx->module_ctx->module_scope;
    ctx->cfg             = c4m_cfg_enter_block(NULL, ctx->node);
    ctx->module_ctx->cfg = ctx->cfg;
    ctx->func_nodes      = c4m_new(c4m_type_list(c4m_type_ref()));

    use_context_enter(ctx);
    check_pass_toplevel_dispatch(ctx);
    def_use_context_exit(ctx);

    ctx->cfg = c4m_cfg_exit_block(ctx->cfg, ctx->module_ctx->cfg, ctx->node);
}

static void
process_function_definitions(pass2_ctx *ctx)
{
    for (int i = 0; i < c4m_list_len(ctx->func_nodes); i++) {
        c4m_tree_node_t *fn_root = c4m_list_get(ctx->func_nodes, i, NULL);
        c4m_pnode_t     *pnode   = c4m_tree_get_contents(fn_root);
        c4m_symbol_t    *sym     = (c4m_symbol_t *)pnode->value;
        c4m_scope_t     *formals;
        void           **view;
        uint64_t         num_items;

        ctx->fn_decl      = sym->value;
        ctx->local_scope  = ctx->fn_decl->signature_info->fn_scope;
        ctx->cfg          = c4m_cfg_enter_block(NULL, ctx->node);
        ctx->fn_decl->cfg = ctx->cfg;
        ctx->fn_exit_node = ctx->cfg->contents.block_entrance.exit_node;
        formals           = ctx->fn_decl->signature_info->formals;

        c4m_list_append(ctx->module_ctx->fn_def_syms, sym);

        view = hatrack_dict_values_sort(ctx->local_scope->symbols, &num_items);

        for (unsigned int i = 0; i < num_items; i++) {
            c4m_symbol_t *var = view[i];

            if (hatrack_dict_get(formals->symbols, var->name, NULL)) {
                add_def(ctx, var, true);
                // This should move into decl_pass and make our lives easier.
                var->kind = C4M_SK_FORMAL;
            }
        }

        use_context_enter(ctx);

        ctx->node = fn_root->children[fn_root->num_kids - 1];
        process_children(ctx);
        def_use_context_exit(ctx);

        ctx->cfg = c4m_cfg_exit_block(ctx->cfg,
                                      ctx->fn_decl->cfg,
                                      ctx->node);
        check_function(ctx, sym);

        sym->flags = sym->flags | C4M_F_FN_PASS_DONE;
    }
}

static void
check_module_variable(c4m_module_compile_ctx *ctx, c4m_symbol_t *sym)
{
    int num_defs = c4m_list_len(sym->sym_defs);
    int num_uses = c4m_list_len(sym->sym_uses);

    if (!num_defs && !num_uses && warn_on_unused(sym)) {
        c4m_add_warning(ctx,
                        c4m_warn_unused_decl,
                        sym->declaration_node,
                        sym->name);
        return;
    }

    if (num_defs == 0) {
        c4m_tree_node_t *loc = sym->declaration_node;

        if (loc == NULL) {
            loc = c4m_list_get(sym->sym_uses, 0, NULL);
        }
        c4m_add_error(ctx,
                      c4m_err_use_no_def,
                      loc,
                      sym->name);
        return;
    }

    if (num_uses == 0) {
        c4m_tree_node_t *loc = sym->declaration_node;

        if (loc == NULL) {
            loc = c4m_list_get(sym->sym_defs, 0, NULL);
        }
        c4m_add_warning(ctx,
                        c4m_warn_def_without_use,
                        loc,
                        sym->name);
    }

    uint64_t mask  = C4M_F_DECLARED_LET | C4M_F_DECLARED_CONST;
    uint64_t flags = sym->flags & mask;

    if (num_defs > 1 && flags) {
        c4m_utf8_t *var_kind;

        if (flags & C4M_F_DECLARED_LET) {
            var_kind = c4m_new_utf8("let");
        }
        else {
            var_kind = c4m_new_utf8("const");
        }

        c4m_tree_node_t *first_def = c4m_list_get(sym->sym_defs, 0, NULL);

        for (int i = 1; i < num_defs; i++) {
            c4m_tree_node_t *bad_def = c4m_list_get(sym->sym_defs, 1, NULL);

            c4m_add_error(ctx,
                          c4m_err_single_def,
                          bad_def,
                          var_kind,
                          c4m_node_get_loc_str(first_def));
        }
    }
}

static void
check_my_global_variable(c4m_module_compile_ctx *ctx, c4m_symbol_t *sym)
{
    int num_defs = c4m_list_len(sym->sym_defs);
    int num_uses = c4m_list_len(sym->sym_uses);

    if (num_defs == 0 && num_uses != 0) {
        c4m_tree_node_t *loc = sym->declaration_node;

        if (loc == NULL) {
            loc = c4m_list_get(sym->sym_uses, 0, NULL);
        }
        c4m_add_error(ctx,
                      c4m_err_use_no_def,
                      loc,
                      sym->name);
        return;
    }

    if (num_uses == 0) {
        c4m_tree_node_t *loc = sym->declaration_node;

        if (loc == NULL) {
            loc = c4m_list_get(sym->sym_defs, 0, NULL);
        }
        c4m_add_info(ctx,
                     c4m_global_def_without_use,
                     loc,
                     sym->name);
    }

    uint64_t mask  = C4M_F_DECLARED_LET | C4M_F_DECLARED_CONST;
    uint64_t flags = sym->flags & mask;

    if (num_defs > 1 && flags) {
        c4m_utf8_t *var_kind;

        if (flags & C4M_F_DECLARED_LET) {
            var_kind = c4m_new_utf8("let");
        }
        else {
            var_kind = c4m_new_utf8("const");
        }

        c4m_tree_node_t *first_def = c4m_list_get(sym->sym_defs, 0, NULL);

        for (int i = 1; i < num_defs; i++) {
            c4m_tree_node_t *bad_def = c4m_list_get(sym->sym_defs, 1, NULL);

            c4m_add_error(ctx,
                          c4m_err_single_def,
                          bad_def,
                          var_kind,
                          c4m_node_get_loc_str(first_def));
        }
    }
}

static void
check_used_global_variable(c4m_module_compile_ctx *ctx, c4m_symbol_t *sym)
{
    int num_defs = c4m_list_len(sym->sym_defs);
    int num_uses = c4m_list_len(sym->sym_uses);

    if (!num_uses && warn_on_unused(sym)) {
        c4m_add_warning(ctx,
                        c4m_warn_unused_decl,
                        sym->declaration_node,
                        sym->name);
        return;
    }

    if (num_defs > 0) {
        if (sym->linked_symbol) {
            c4m_tree_node_t *first_node = sym->linked_symbol->declaration_node;

            c4m_add_error(ctx,
                          c4m_err_global_remote_def,
                          sym->declaration_node,
                          sym->name,
                          c4m_node_get_loc_str(first_node));
        }
        return;
    }
}

static void
validate_module_variables(c4m_module_compile_ctx *ctx)
{
    uint64_t      n;
    c4m_symbol_t *entry;
    void        **view;

    view = hatrack_dict_values_sort(ctx->module_scope->symbols, &n);

    for (uint64_t i = 0; i < n; i++) {
        entry = view[i];
        if (entry->kind == C4M_SK_VARIABLE) {
            check_module_variable(ctx, entry);
        }
    }

    view = hatrack_dict_values_sort(ctx->global_scope->symbols, &n);

    for (uint64_t i = 0; i < n; i++) {
        entry = view[i];
        if (entry->kind == C4M_SK_VARIABLE && entry->linked_symbol == NULL) {
            check_my_global_variable(ctx, entry);
        }
        else {
            check_used_global_variable(ctx, entry);
        }
    }
}

static void
perform_index_rechecks(pass2_ctx *ctx)
{
    int n = c4m_list_len(ctx->index_rechecks);

    for (int i = 0; i < n; i++) {
        c4m_tree_node_t *node  = c4m_list_get(ctx->index_rechecks, i, NULL);
        c4m_type_t      *ctype = get_pnode_type(node->children[0]);
        c4m_type_t      *t     = get_pnode_type(node->children[1]);

        if (c4m_type_is_box(t)) {
            t = c4m_type_unbox(t);
        }

        if (!c4m_type_is_concrete(t)) {
            if (c4m_types_are_compat(ctype,
                                     c4m_type_dict(c4m_new_typevar(),
                                                   c4m_new_typevar()),
                                     NULL)) {
                c4m_add_error(ctx->module_ctx,
                              c4m_err_concrete_index,
                              node);
            }
            else {
                // If it's not a dict, the index must be an int.
                merge_or_err(ctx, t, c4m_type_int());
            }
            return;
        }

        if (!c4m_type_is_int_type(t)) {
            if (c4m_types_are_compat(ctype,
                                     c4m_type_dict(c4m_new_typevar(),
                                                   c4m_new_typevar()),
                                     NULL)) {
                c4m_add_error(ctx->module_ctx,
                              c4m_err_non_dict_index_type,
                              node);
            }
        }
    }
}

static void
process_deferred_lits(pass2_ctx *ctx)
{
    int n = c4m_list_len(ctx->simple_lits_wo_mod);

    for (int i = 0; i < n; i++) {
        c4m_tree_node_t *t    = c4m_list_get(ctx->simple_lits_wo_mod, i, NULL);
        c4m_pnode_t     *p    = c4m_get_pnode(t);
        c4m_obj_t        lit  = p->value;
        c4m_token_t     *tok  = p->token;
        c4m_type_t      *type = merge_ignore_err(p->type,
                                            c4m_get_my_type(lit));

        if (!c4m_type_is_error(type)) {
            continue;
        }
        if (c4m_type_is_concrete(p->type) && c4m_fix_litmod(tok, p)) {
            continue;
        }

        // This already failed; generate the error though.
        merge_or_err(ctx, p->type, c4m_get_my_type(lit));
    }
}

static c4m_list_t *
module_check_pass(c4m_compile_ctx *cctx, c4m_module_compile_ctx *module_ctx)
{
    // This should be checked before we get here, but belt and suspenders.
    if (c4m_fatal_error_in_module(module_ctx)) {
        return NULL;
    }

    pass2_ctx ctx = {
        .attr_scope         = cctx->final_attrs,
        .global_scope       = cctx->final_globals,
        .spec               = cctx->final_spec,
        .compile            = cctx,
        .module_ctx         = module_ctx,
        .du_stack           = 0,
        .du_stack_ix        = 0,
        .loop_stack         = c4m_list(c4m_type_ref()),
        .deferred_calls     = c4m_list(c4m_type_ref()),
        .index_rechecks     = c4m_list(c4m_type_tree(c4m_type_parse_node())),
        .simple_lits_wo_mod = c4m_list(c4m_type_tree(c4m_type_parse_node())),
    };

#ifdef C4M_DEV
    module_ctx->print_nodes = c4m_list(c4m_type_tree(c4m_type_parse_node()));
#endif

    check_module_toplevel(&ctx);
    process_function_definitions(&ctx);
    perform_index_rechecks(&ctx);
    validate_module_variables(module_ctx);
    process_deferred_lits(&ctx);

    return ctx.deferred_calls;
}

typedef struct {
    c4m_module_compile_ctx *mod;
    c4m_list_t             *deferrals;
} defer_info_t;

static void
scan_for_void_symbols(c4m_module_compile_ctx *f, c4m_scope_t *scope)
{
    uint64_t n;
    void   **view = hatrack_dict_values_sort(scope->symbols, &n);

    for (uint64_t i = 0; i < n; i++) {
        c4m_symbol_t *sym = view[i];

        if (sym->kind == C4M_SK_VARIABLE || sym->kind == C4M_SK_ATTR) {
            if (c4m_type_resolve(sym->type)->typeid == C4M_T_VOID) {
                c4m_tree_node_t *def = c4m_list_get(sym->sym_defs, 0, NULL);
                c4m_add_error(f, c4m_err_assigned_void, def);
            }
        }
    }
}

static void
process_deferred_calls(c4m_compile_ctx *cctx,
                       defer_info_t    *info,
                       int              num_deferrals)
{
    for (int j = 0; j < num_deferrals; j++) {
        c4m_module_compile_ctx *f       = info->mod;
        c4m_list_t             *one_set = info->deferrals;
        int                     n       = c4m_list_len(one_set);

        for (int i = 0; i < n; i++) {
            c4m_call_resolution_info_t *info = c4m_list_get(one_set, i, NULL);

            if (info->polymorphic == 1) {
                continue;
            }

            c4m_type_t  *sym_type   = c4m_type_copy(info->resolution->type);
            c4m_pnode_t *pnode      = c4m_get_pnode(info->loc);
            c4m_type_t  *node_type  = pnode->type;
            c4m_type_t  *call_type  = info->sig;
            int          np         = c4m_type_get_num_params(call_type);
            c4m_type_t  *param_type = c4m_type_get_param(call_type, np - 1);
            c4m_type_t  *merged     = merge_ignore_err(node_type,
                                                  param_type);
            bool         err        = c4m_type_is_error(merged);

            if (!err) {
                merged = merge_ignore_err(sym_type, call_type);
                err    = c4m_type_is_error(merged);
            }

            if (err) {
                c4m_add_error(f,
                              c4m_err_call_type_err,
                              info->loc,
                              call_type,
                              sym_type);
            }
        }

        // The only problem with deferred resolution is that the return
        // value could be assigned, and in the absense of other checks,
        // some previous variable who we previously had labeled as 'type
        // unknown' could show up here as 'void' (if it were 'error' it
        // would have been caught above).
        //
        // So to finish up, we walk through all symbols the module has
        // scoped again, to look for symbols typed 'void', and if we see
        // them, complain at their def sites.
        //
        // Note that we don't hold onto loop temporaries; they are all
        // either typed to int, or based on a type restricted to indexible
        // types.

        scan_for_void_symbols(f, f->module_scope);
        scan_for_void_symbols(f, f->global_scope);
        scan_for_void_symbols(f, f->attribute_scope);

        c4m_list_t *fns = f->fn_def_syms;

        for (int i = 0; i < c4m_list_len(fns); i++) {
            c4m_symbol_t  *sym  = c4m_list_get(fns, i, NULL);
            c4m_fn_decl_t *decl = sym->value;

            scan_for_void_symbols(f, decl->signature_info->fn_scope);
        }

#ifdef C4M_DEV
        for (int i = 0; i < c4m_list_len(f->print_nodes); i++) {
            c4m_tree_node_t *n = c4m_list_get(f->print_nodes, i, NULL);
            if (c4m_type_is_void(get_pnode_type(n))) {
                c4m_add_error(f, c4m_err_void_print, n);
                continue;
            }
            // TODO: make sure there's a repr() of some sort, or add a default
            // repr.
        }
#endif
    }
}

static void
process_deferred_callbacks(c4m_compile_ctx *cctx)
{
    // Now that we have a 'whole program view', go ahead and
    // try to find a match for any callback literals used.
    //
    // Meaning, there must either be an in-scope con4m function, or
    // an extern declaration that matches the callback, as viewed from
    // the module in which the symbol was declared.

    int         n = c4m_list_len(cctx->module_ordering);
    c4m_utf8_t *s;

    for (int i = 0; i < n; i++) {
        c4m_module_compile_ctx *f = c4m_list_get(cctx->module_ordering, i, NULL);
        int                     m = c4m_list_len(f->callback_literals);
        for (int j = 0; j < m; j++) {
            c4m_callback_t *cb = c4m_list_get(f->callback_literals, j, NULL);

            c4m_symbol_t *sym = c4m_symbol_lookup(NULL,
                                                  f->module_scope,
                                                  f->global_scope,
                                                  NULL,
                                                  cb->target_symbol_name);

            if (!sym) {
                c4m_add_error(f, c4m_err_callback_no_match, cb->decl_loc);
                return;
            }

            switch (sym->kind) {
            case C4M_SK_FUNC:
                cb->binding.ffi                          = 0;
                cb->binding.implementation.ffi_interface = sym->value;
                break;
            case C4M_SK_EXTERN_FUNC:
                cb->binding.ffi                            = 1;
                cb->binding.implementation.local_interface = sym->value;
                break;
            default:;
                c4m_tree_node_t *l = sym->declaration_node;
                if (l == NULL) {
                    l = c4m_list_get(sym->sym_defs, 0, NULL);
                }
                s = c4m_node_get_loc_str(l);
                c4m_add_error(f, c4m_err_callback_bad_target, cb->decl_loc, s);
                return;
            }

            c4m_type_t *sym_type = c4m_type_copy(sym->type);
            c4m_type_t *lit_type = cb->target_type;
            c4m_type_t *merged   = merge_ignore_err(sym_type, lit_type);

            if (c4m_type_is_error(merged)) {
                s = c4m_node_get_loc_str(sym->declaration_node);
                c4m_add_error(f,
                              c4m_err_callback_type_mismatch,
                              cb->decl_loc,
                              lit_type,
                              sym_type,
                              s);
            }
        }
    }
}

static void
order_ffi_decls(c4m_compile_ctx *cctx)
{
    // TODO: when incrementally compiling we need to take into
    // acount existing FFI decl indexing.
    int n  = c4m_list_len(cctx->module_ordering);
    int ix = 0;

    for (int i = 0; i < n; i++) {
        c4m_module_compile_ctx *f = c4m_list_get(cctx->module_ordering, i, NULL);
        int                     m = c4m_list_len(f->extern_decls);

        for (int j = 0; j < m; j++) {
            c4m_symbol_t   *sym  = c4m_list_get(f->extern_decls, j, NULL);
            c4m_ffi_decl_t *decl = (c4m_ffi_decl_t *)sym->value;

            decl->global_ffi_call_ix = ix++;
        }
    }
}

void
c4m_check_pass(c4m_compile_ctx *cctx)
{
    int           n            = c4m_list_len(cctx->module_ordering);
    int           num_deferred = 0;
    defer_info_t *all_deferred = c4m_gc_array_alloc(defer_info_t, n);
    c4m_list_t   *one_deferred;

    for (int i = 0; i < n; i++) {
        c4m_module_compile_ctx *f = c4m_list_get(cctx->module_ordering, i, NULL);

        if (f->status < c4m_compile_status_code_loaded) {
            C4M_CRAISE("Cannot check files until after decl scan.");
        }

        if (f->fatal_errors) {
            C4M_CRAISE("Cannot check files that have do not properly load.");
        }

        if (f->status >= c4m_compile_status_tree_typed) {
            continue;
        }

        one_deferred = module_check_pass(cctx, f);
        c4m_module_set_status(f, c4m_compile_status_tree_typed);

        if (one_deferred == NULL) {
            continue;
        }

        if (c4m_list_len(one_deferred) != 0) {
            all_deferred[num_deferred].mod         = f;
            all_deferred[num_deferred++].deferrals = one_deferred;
        }
    }

    order_ffi_decls(cctx);
    process_deferred_calls(cctx, all_deferred, num_deferred);
    process_deferred_callbacks(cctx);

    for (int i = 0; i < n; i++) {
        c4m_module_compile_ctx *f = c4m_list_get(cctx->module_ordering, i, NULL);

        if (f->cfg != NULL) {
            c4m_cfg_analyze(f, NULL);
        }

        if (c4m_fatal_error_in_module(f)) {
            cctx->fatality = 1;
        }
        c4m_layout_module_symbols(cctx, f);
    }
}

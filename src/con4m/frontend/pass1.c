#define C4M_USE_INTERNAL_API
#include "con4m.h"

// First pass of the raw parse tree.
// Not everything we need would be resolvable in one pass, especially
// symbols in other modules.

// More consise aliases internally only.
#define tfind(x, y, ...) _c4m_tpat_find((void *)(int64_t)x, \
                                        y,                  \
                                        KFUNC(__VA_ARGS__))
#define tfind_content(x, y) c4m_tpat_content_find((void *)(int64_t)x, y)
#define tmatch(x, y, ...)   _c4m_tpat_match((void *)(int64_t)x, \
                                          y,                    \
                                          KFUNC(__VA_ARGS__))
#define tcontent(x, y) c4m_tpat_content_match((void *)(int64_t)x, y)
#define toptional(x, y, ...) \
    _c4m_tpat_opt_match((void *)(int64_t)x, y, KFUNC(__VA_ARGS__))
#define tcount(a, b, c, d, ...) \
    _c4m_tpat_n_m_match((void *)(int64_t)a, b, c, d, KFUNC(__VA_ARGS__))
#define tcount_content(a, b, c, d, ...) \
    c4m_tpat_n_m_content_match((void *)(int64_t)a, b, c, d)

#define get_pnode(x) ((x) ? c4m_tree_get_contents(x) : NULL)

// We use the null value (error) in patterns to match any type node.
#define nt_any    (c4m_nt_error)
#define max_nodes 0x7fff

typedef struct {
    c4m_tree_node_t      *cur_tnode;
    c4m_pnode_t          *cur;
    c4m_file_compile_ctx *file_ctx;
} pass_ctx;

#undef DEBUG_PATTERNS
#ifdef DEBUG_PATTERNS
static void
print_type(c4m_obj_t o)
{
    if (o == NULL) {
        printf("<null>\n");
        return;
    }
    c4m_print(c4m_get_my_type(o));
}

static c4m_utf8_t *
content_formatter(void *contents)
{
    c4m_node_kind_t kind = (c4m_node_kind_t)(uint64_t)contents;

    if (kind == nt_any) {
        return c4m_rich_lit("[h2]nt_any[]");
    }

    return c4m_cstr_format("[h2]{}[/]", c4m_node_type_name(kind));
}

static void
show_pattern(c4m_tpat_node_t *pat)
{
    c4m_tree_node_t *t = c4m_pat_repr(pat, content_formatter);
    c4m_grid_t      *g = c4m_grid_tree(t);

    c4m_print(g);
}
#endif

static bool
tcmp(int64_t kind_as_64, c4m_tree_node_t *node)
{
    c4m_node_kind_t kind  = (c4m_node_kind_t)(unsigned int)kind_as_64;
    c4m_pnode_t    *pnode = get_pnode(node);

    if (kind == nt_any) {
        return true;
    }

    bool result = kind == pnode->kind;

    return result;
}

static inline c4m_xlist_t *
apply_pattern_on_node(c4m_tree_node_t *node, c4m_tpat_node_t *pattern)
{
    c4m_xlist_t *cap = NULL;
    bool         ok  = c4m_tree_match(node,
                             pattern,
                             (c4m_cmp_fn)tcmp,
                             &cap);

    if (!ok) {
        return NULL;
    }

    return cap;
}

static inline c4m_xlist_t *
apply_pattern(pass_ctx *ctx, c4m_tpat_node_t *pattern)
{
    return apply_pattern_on_node(ctx->cur_tnode, pattern);
}

// Return the first capture if there's a match, and NULL if not.
static inline c4m_tree_node_t *
get_match_on_node(c4m_tree_node_t *node, c4m_tpat_node_t *pattern)
{
    c4m_xlist_t *cap = apply_pattern_on_node(node, pattern);

    if (cap != NULL) {
        return c4m_xlist_get(cap, 0, NULL);
    }

    return NULL;
}

static inline c4m_tree_node_t *
get_match(pass_ctx *ctx, c4m_tpat_node_t *pattern)
{
    return get_match_on_node(ctx->cur_tnode, pattern);
}

static c4m_tpat_node_t *first_kid_id = NULL;
static c4m_tpat_node_t *enum_items;
static c4m_tpat_node_t *member_prefix;
static c4m_tpat_node_t *member_last;
static c4m_tpat_node_t *use_uri;
static c4m_tpat_node_t *extern_params;
static c4m_tpat_node_t *extern_return;
static c4m_tpat_node_t *return_extract;
static c4m_tpat_node_t *find_pure;
static c4m_tpat_node_t *find_holds;
static c4m_tpat_node_t *find_allocs;
static c4m_tpat_node_t *find_extern_local;
static c4m_tpat_node_t *param_extraction;

static void
setup_pass1_patterns()
{
    if (first_kid_id != NULL) {
        return;
    }
    // Returns first child if it's an identifier, null otherwise.
    first_kid_id  = tmatch(nt_any,
                          0,
                          tmatch(c4m_nt_identifier, 1),
                          tcount_content(nt_any, 0, max_nodes, 0));
    // Skips the identifier if there, and returns all the enum items,
    // regardless of the subtree shape.
    enum_items    = tmatch(nt_any,
                        0,
                        toptional(c4m_nt_identifier, 0),
                        tcount_content(c4m_nt_enum_item,
                                       0,
                                       max_nodes,
                                       1));
    member_last   = tfind(c4m_nt_member,
                        0,
                        tcount(c4m_nt_identifier, 0, max_nodes, 0),
                        tmatch(c4m_nt_identifier, 1));
    member_prefix = tfind(c4m_nt_member,
                          0,
                          tcount(c4m_nt_identifier, 0, max_nodes, 1),
                          tmatch(c4m_nt_identifier, 0));

    extern_params    = tfind(c4m_nt_extern_sig,
                          0,
                          tcount_content(c4m_nt_extern_param, 0, max_nodes, 1),
                          tcount_content(c4m_nt_lit_tspec_return_type,
                                         0,
                                         1,
                                         0));
    extern_return    = tfind(c4m_nt_extern_sig,
                          0,
                          tcount_content(c4m_nt_extern_param, 0, max_nodes, 0),
                          tcount_content(c4m_nt_lit_tspec_return_type,
                                         0,
                                         1,
                                         1));
    return_extract   = tfind(c4m_nt_lit_tspec_return_type,
                           0,
                           tmatch(nt_any, 1));
    use_uri          = tfind(c4m_nt_simple_lit, 1);
    param_extraction = tfind(c4m_nt_formals,
                             0,
                             tcount_content(c4m_nt_var_decls, 0, max_nodes, 1));

    find_pure         = tfind_content(c4m_nt_extern_pure, 1);
    find_holds        = tfind_content(c4m_nt_extern_holds, 1);
    find_allocs       = tfind_content(c4m_nt_extern_allocs, 1);
    find_extern_local = tfind_content(c4m_nt_extern_local, 1);

    c4m_gc_register_root(&first_kid_id, 1);
    c4m_gc_register_root(&enum_items, 1);
    c4m_gc_register_root(&member_prefix, 1);
    c4m_gc_register_root(&member_last, 1);
    c4m_gc_register_root(&use_uri, 1);
    c4m_gc_register_root(&extern_params, 1);
    c4m_gc_register_root(&extern_return, 1);
    c4m_gc_register_root(&return_extract, 1);
    c4m_gc_register_root(&find_pure, 1);
    c4m_gc_register_root(&find_holds, 1);
    c4m_gc_register_root(&find_allocs, 1);
    c4m_gc_register_root(&find_extern_local, 1);
    c4m_gc_register_root(&param_extraction, 1);
}

static inline void
set_current_node(pass_ctx *ctx, c4m_tree_node_t *n)
{
    ctx->cur_tnode = n;
    ctx->cur       = c4m_tree_get_contents(n);
}

static inline bool
node_down(pass_ctx *ctx, int i)
{
    c4m_tree_node_t *n = ctx->cur_tnode;

    if (i >= n->num_kids) {
        return false;
    }

    assert(n->children[i]->parent == n);
    set_current_node(ctx, n->children[i]);

    return true;
}

static inline void
node_up(pass_ctx *ctx)
{
    set_current_node(ctx, ctx->cur_tnode->parent);
}

static void pass_dispatch(pass_ctx *ctx);

static inline void
process_children(pass_ctx *ctx)
{
    c4m_tree_node_t *n = ctx->cur_tnode;

    for (int i = 0; i < n->num_kids; i++) {
        node_down(ctx, i);
        pass_dispatch(ctx);
        node_up(ctx);
    }
}

static inline c4m_node_kind_t
cur_node_type(pass_ctx *ctx)
{
    return ctx->cur->kind;
}

static inline c4m_tree_node_t *
cur_node(pass_ctx *ctx)
{
    return ctx->cur_tnode;
}

static void
validate_str_enum_vals(pass_ctx *ctx, c4m_xlist_t *items)
{
    c4m_set_t *set = c4m_new(c4m_tspec_set(c4m_tspec_utf8()));
    int64_t    n   = c4m_xlist_len(items);

    for (int i = 0; i < n; i++) {
        c4m_tree_node_t *tnode = c4m_xlist_get(items, i, NULL);
        c4m_pnode_t     *pnode = get_pnode(tnode);

        if (c4m_tree_get_number_children(tnode) == 0) {
            pnode->value = (void *)c4m_new_utf8("error");
            continue;
        }

        c4m_utf8_t *val = (c4m_utf8_t *)pnode->value;

        if (!c4m_set_add(set, val)) {
            c4m_add_error(ctx->file_ctx, c4m_err_dupe_enum, tnode);
            return;
        }
    }
}

static c4m_type_t *
validate_int_enum_vals(pass_ctx *ctx, c4m_xlist_t *items)
{
    c4m_set_t  *set           = c4m_new(c4m_tspec_set(c4m_tspec_u64()));
    int64_t     n             = c4m_xlist_len(items);
    int         bits          = 0;
    bool        neg           = false;
    uint64_t    next_implicit = 0;
    c4m_type_t *result;

    for (int i = 0; i < n; i++) {
        c4m_tree_node_t *tnode = c4m_xlist_get(items, i, NULL);
        if (c4m_tree_get_number_children(tnode) == 0) {
            continue;
        }

        c4m_pnode_t   *pnode  = get_pnode(tnode);
        c4m_obj_t     *ref    = pnode->value;
        c4m_type_t    *ty     = c4m_get_my_type(ref);
        c4m_dt_info_t *dtinfo = c4m_tspec_get_data_type_info(ty);
        int            sz;
        uint64_t       val;

        switch (dtinfo->alloc_len) {
        case 1:
            val = (uint64_t) * (uint8_t *)ref;
            break;
        case 2:
            val = (uint64_t) * (uint16_t *)ref;
            break;
        case 4:
            val = (uint64_t) * (uint32_t *)ref;
            break;
        case 8:
            val = *(uint64_t *)ref;
            break;
        default:
            C4M_CRAISE("Invalid int size for enum item");
        }

        sz = 64 - __builtin_clzll(val);

        switch (sz) {
        case 64:
            if (dtinfo->typeid == C4M_T_INT) {
                neg = true;
                break;
            }
        case 32:
            if (dtinfo->typeid == C4M_T_I32) {
                neg = true;
                break;
            }
            break;
        case 8:
            if (dtinfo->typeid == C4M_T_I8) {
                neg = true;
                break;
            }
            break;

        default:
            break;
        }

        if (sz > bits) {
            bits = sz;
        }

        if (!c4m_set_add(set, (void *)val)) {
            c4m_add_error(ctx->file_ctx, c4m_err_dupe_enum, tnode);
        }
    }

    if (bits > 32) {
        bits = 64;
    }
    else {
        if (bits <= 8) {
            bits = 8;
        }
        else {
            bits = 32;
        }
    }

    switch (bits) {
    case 8:
        result = neg ? c4m_tspec_i8() : c4m_tspec_u8();
    case 32:
        result = neg ? c4m_tspec_i32() : c4m_tspec_u32();
    default:
        result = neg ? c4m_tspec_i64() : c4m_tspec_u64();
    }

    for (int i = 0; i < n; i++) {
        c4m_tree_node_t *tnode = c4m_xlist_get(items, i, NULL);
        c4m_pnode_t     *pnode = get_pnode(tnode);

        if (c4m_tree_get_number_children(tnode) != 0) {
            pnode->value = c4m_coerce_object(pnode->value, result);
            continue;
        }

        while (c4m_set_contains(set, (void *)next_implicit)) {
            next_implicit++;
        }

        pnode->value = c4m_coerce_object(c4m_box_u64(next_implicit++), result);
    }

    return result;
}

// For now, enum types are either going to be integer types, or they're
// going to be string types.
//
// Once we add UDTs, it will be possible to make them propert UDTs,
// so that we can do proper value checking.

static void
handle_enum_decl(pass_ctx *ctx)
{
    c4m_tree_node_t   *item;
    c4m_tree_node_t   *tnode  = get_match(ctx, first_kid_id);
    c4m_pnode_t       *id     = get_pnode(tnode);
    c4m_scope_entry_t *idsym  = NULL;
    c4m_xlist_t       *items  = apply_pattern(ctx, enum_items);
    int                n      = c4m_xlist_len(items);
    bool               is_str = false;
    c4m_scope_t       *scope;
    c4m_utf8_t        *varname;

    if (cur_node_type(ctx) == c4m_nt_enum) {
        scope = ctx->file_ctx->global_scope;
    }
    else {
        scope = ctx->file_ctx->module_scope;
    }

    if (id != NULL) {
        idsym = c4m_declare_symbol(ctx->file_ctx,
                                   scope,
                                   identifier_text(id->token),
                                   tnode,
                                   sk_enum_type,
                                   NULL,
                                   true);
    }

    for (int i = 0; i < n; i++) {
        item    = c4m_xlist_get(items, i, NULL);
        varname = node_text(item);

        if (node_num_kids(item) != 0) {
            c4m_pnode_t *pnode = get_pnode(item);

            pnode->value = node_literal(c4m_tree_get_child(item, 0));

            if (!c4m_obj_is_int_type(pnode->value)) {
                if (!c4m_obj_type_check(pnode->value, c4m_tspec_utf8())) {
                    c4m_add_error(ctx->file_ctx,
                                  c4m_err_invalid_enum_lit_type,
                                  item);
                    continue;
                }
                if (i == 0) {
                    is_str = true;
                }
                else {
                    if (!is_str) {
                        c4m_add_error(ctx->file_ctx,
                                      c4m_err_enum_str_int_mix,
                                      item);
                        continue;
                    }
                }
            }
            else {
                if (is_str) {
                    c4m_add_error(ctx->file_ctx,
                                  c4m_err_enum_str_int_mix,
                                  item);
                    continue;
                }
            }
        }
        else {
            if (is_str) {
                c4m_add_error(ctx->file_ctx,
                              c4m_err_omit_string_enum_value,
                              item);
                continue;
            }
        }

        c4m_declare_symbol(ctx->file_ctx,
                           scope,
                           varname,
                           item,
                           sk_enum_val,
                           NULL,
                           true);
    }

    if (is_str) {
        validate_str_enum_vals(ctx, items);
        if (idsym != NULL) {
            idsym->inferred_type = c4m_tspec_utf8();
        }
    }
    else {
        c4m_type_t *ty = validate_int_enum_vals(ctx, items);
        if (idsym != NULL) {
            idsym->inferred_type = ty;
        }
    }

#ifdef C4M_PASS1_UNIT_TESTS
    if (id == NULL) {
        printf("Anonymous enum (%lld kids).\n", c4m_xlist_len(items));
    }
    else {
        printf("Enum name: %s (%lld kids)\n",
               identifier_text(id->token)->data,
               c4m_xlist_len(items));
    }

    for (int i = 0; i < n; i++) {
        item               = c4m_xlist_get(items, i, NULL);
        varname            = node_text(item);
        c4m_pnode_t *pnode = get_pnode(item);
        printf("About to print an enum value:\n");
        c4m_print(c4m_cstr_format("enum name: {} value: {}",
                                  varname,
                                  pnode->value));
    }
#endif
}

static void
handle_func_decl(pass_ctx *ctx)
{
    c4m_print_parse_node(cur_node(ctx));
}

static void
handle_var_decl(pass_ctx *ctx)
{
    c4m_print_parse_node(cur_node(ctx));
}

static void
handle_config_spec(pass_ctx *ctx)
{
    c4m_print_parse_node(cur_node(ctx));
}

static void
handle_param_block(pass_ctx *ctx)
{
    c4m_print_parse_node(cur_node(ctx));
}

static c4m_type_t *
node_to_type(pass_ctx *ctx, c4m_tree_node_t *n, c4m_dict_t *type_ctx)
{
    if (type_ctx == NULL) {
        type_ctx = c4m_new(c4m_tspec_dict(c4m_tspec_utf8(), c4m_tspec_ref()));
    }

    c4m_pnode_t *pnode = get_pnode(n);
    c4m_utf8_t  *varname;
    c4m_type_t  *t;
    bool         found;
    int          numkids;

    switch (pnode->kind) {
    case c4m_nt_lit_tspec:
        return node_to_type(ctx, c4m_tree_get_child(n, 0), type_ctx);
    case c4m_nt_lit_tspec_tvar:
        varname = node_text(c4m_tree_get_child(n, 0));
        t       = hatrack_dict_get(type_ctx, varname, &found);
        if (!found) {
            t                = c4m_tspec_typevar();
            t->details->name = varname->data;
            hatrack_dict_put(type_ctx, varname, t);
        }
        return t;
    case c4m_nt_lit_tspec_named_type:
        varname = node_text(n);
        for (int i = 0; i < C4M_NUM_BUILTIN_DTS; i++) {
            if (!strcmp(varname->data, c4m_base_type_info[i].name)) {
                return c4m_bi_types[i];
            }
        }

        c4m_add_error(ctx->file_ctx, c4m_err_unk_primitive_type, n);
        return c4m_tspec_typevar();

    case c4m_nt_lit_tspec_parameterized_type:
        varname = node_text(n);
        // Need to do this more generically, but OK for now.
        if (!strcmp(varname->data, "list")) {
            return c4m_tspec_list(node_to_type(ctx,
                                               c4m_tree_get_child(n, 0),
                                               type_ctx));
        }
        if (!strcmp(varname->data, "queue")) {
            return c4m_tspec_queue(node_to_type(ctx,
                                                c4m_tree_get_child(n, 0),
                                                type_ctx));
        }
        if (!strcmp(varname->data, "ring")) {
            return c4m_tspec_queue(node_to_type(ctx,
                                                c4m_tree_get_child(n, 0),
                                                type_ctx));
        }
        if (!strcmp(varname->data, "logring")) {
            c4m_add_error(ctx->file_ctx, c4m_err_no_logring_yet, n);
            return c4m_tspec_typevar();
        }
        if (!strcmp(varname->data, "xlist")) {
            return c4m_tspec_xlist(node_to_type(ctx,
                                                c4m_tree_get_child(n, 0),
                                                type_ctx));
        }
        if (!strcmp(varname->data, "tree")) {
            return c4m_tspec_tree(node_to_type(ctx,
                                               c4m_tree_get_child(n, 0),
                                               type_ctx));
        }
        if (!strcmp(varname->data, "stack")) {
            return c4m_tspec_stack(node_to_type(ctx,
                                                c4m_tree_get_child(n, 0),
                                                type_ctx));
        }
        if (!strcmp(varname->data, "set")) {
            return c4m_tspec_set(node_to_type(ctx,
                                              c4m_tree_get_child(n, 0),
                                              type_ctx));
        }
        if (!strcmp(varname->data, "dict")) {
            return c4m_tspec_dict(node_to_type(ctx,
                                               c4m_tree_get_child(n, 0),
                                               type_ctx),
                                  node_to_type(ctx,
                                               c4m_tree_get_child(n, 1),
                                               type_ctx));
        }
        if (!strcmp(varname->data, "tuple")) {
            c4m_xlist_t *subitems;

            subitems = c4m_new(c4m_tspec_xlist(c4m_tspec_typespec()));

            for (int i = 0; i < c4m_tree_get_number_children(n); i++) {
                c4m_xlist_append(subitems,
                                 node_to_type(ctx,
                                              c4m_tree_get_child(n, i),
                                              type_ctx));
            }

            return c4m_tspec_tuple_from_xlist(subitems);
        }
        c4m_add_error(ctx->file_ctx, c4m_err_unk_param_type, n);
        return c4m_tspec_typevar();
    case c4m_nt_lit_tspec_func:
        numkids = c4m_tree_get_number_children(n);
        if (numkids == 0) {
            return c4m_tspec_varargs_fn(c4m_tspec_typevar(), 0);
        }

        c4m_xlist_t     *args = c4m_new(c4m_tspec_xlist(c4m_tspec_typespec()));
        c4m_tree_node_t *kid  = c4m_tree_get_child(n, numkids - 1);
        bool             va   = false;

        pnode = get_pnode(kid);

        if (pnode->kind == c4m_nt_lit_tspec_return_type) {
            t = node_to_type(ctx, c4m_tree_get_child(kid, 0), type_ctx);
            numkids--;
        }
        else {
            t = c4m_tspec_typevar();
        }

        for (int i = 0; i < numkids; i++) {
            kid = c4m_tree_get_child(n, i);

            if (i + 1 == numkids) {
                pnode = get_pnode(kid);

                if (pnode->kind == c4m_nt_lit_tspec_varargs) {
                    va  = true;
                    kid = c4m_tree_get_child(kid, 0);
                }
            }

            c4m_xlist_append(args, node_to_type(ctx, kid, type_ctx));
        }

        return c4m_tspec_fn(t, args, va);

    default:
        C4M_CRAISE("Reached code that should be unreachable.");
    }
}

static c4m_sig_info_t *
new_sig_info(int num_params)
{
    c4m_sig_info_t *result = c4m_gc_alloc(c4m_sig_info_t);
    result->num_params     = num_params;

    if (result->num_params > 0) {
        result->param_info = c4m_gc_array_alloc(c4m_param_info_t, num_params);
    }

    return result;
}

static c4m_sig_info_t *
extract_fn_sig_info(pass_ctx        *ctx,
                    c4m_tree_node_t *tree)
{
    c4m_xlist_t    *decls     = apply_pattern_on_node(tree, param_extraction);
    int             ndecls    = c4m_xlist_len(decls);
    int             nparams   = 0;
    int             cur_param = 0;
    c4m_xlist_t    *ptypes    = c4m_new(c4m_tspec_xlist(c4m_tspec_typespec()));
    c4m_sig_info_t *info;

    // Allocate space for parameters by counting how many variable
    // names we find.
    for (int i = 0; i < ndecls; i++) {
        c4m_tree_node_t *node  = c4m_xlist_get(decls, i, NULL);
        int              kidct = c4m_tree_get_number_children(node);

        if (kidct > 1) {
            c4m_tree_node_t *kid   = c4m_tree_get_child(node, kidct - 1);
            c4m_pnode_t     *pnode = get_pnode(kid);

            // Skip type specs.
            if (pnode->kind != c4m_nt_identifier) {
                kidct--;
            }
        }
        nparams += kidct;
    }

    info              = new_sig_info(nparams);
    info->param_scope = c4m_new_scope(ctx->file_ctx->module_scope,
                                      C4M_SCOPE_FORMALS);

    // Now, we loop through the parameter trees again. In function
    // declarations, named variables with omitted types are given a
    // type variable as a return type. Similarly, omitted return
    // values get a type variable.

    for (int i = 0; i < ndecls; i++) {
        c4m_tree_node_t *node  = c4m_xlist_get(decls, i, NULL);
        int              kidct = c4m_tree_get_number_children(node);
        c4m_type_t      *type  = NULL;

        if (kidct > 1) {
            c4m_tree_node_t *kid   = c4m_tree_get_child(node, kidct - 1);
            c4m_pnode_t     *pnode = get_pnode(kid);

            if (pnode->kind != c4m_nt_identifier) {
                type = node_to_type(ctx, kid, NULL);
                kidct--;
            }
        }

        // All but the last one in a subtree get type variables.
        for (int j = 0; j < kidct - 1; j++) {
            c4m_param_info_t *pi  = &info->param_info[cur_param++];
            c4m_tree_node_t  *kid = c4m_tree_get_child(node, j);
            pi->name              = node_text(kid);
            pi->type              = c4m_tspec_typevar();

            c4m_declare_symbol(ctx->file_ctx,
                               info->param_scope,
                               pi->name,
                               kid,
                               sk_formal,
                               NULL,
                               true);

            c4m_xlist_append(ptypes, pi->type);
        }

        // last item.
        if (!type) {
            type = c4m_tspec_typevar();
        }

        c4m_param_info_t *pi  = &info->param_info[cur_param++];
        c4m_tree_node_t  *kid = c4m_tree_get_child(node, kidct - 1);
        pi->name              = node_text(kid);
        pi->type              = type;

        c4m_declare_symbol(ctx->file_ctx,
                           info->param_scope,
                           pi->name,
                           kid,
                           sk_formal,
                           NULL,
                           true);

        c4m_xlist_append(ptypes, pi->type);
    }

    c4m_tree_node_t *retnode = get_match_on_node(tree, return_extract);

    if (!retnode) {
        info->return_info.type = c4m_tspec_void();
    }
    else {
        info->return_info.type = node_to_type(ctx,
                                              retnode,
                                              NULL);
    }

    // Now fill out the 'local_type' field of the ffi decl.
    // TODO: support varargs.
    info->full_type = c4m_tspec_fn(info->return_info.type, ptypes, false);

    return info;
}

static void
handle_extern_block(pass_ctx *ctx)
{
    c4m_ffi_decl_t  *info          = c4m_gc_alloc(c4m_ffi_info_t);
    c4m_utf8_t      *external_name = node_text(get_match(ctx, first_kid_id));
    c4m_xlist_t     *ext_params    = apply_pattern(ctx, extern_params);
    c4m_tree_node_t *ext_ret       = get_match(ctx, extern_return);
    c4m_pnode_t     *pnode         = get_pnode(cur_node(ctx));
    c4m_tree_node_t *ext_pure      = get_match(ctx, find_pure);
    c4m_tree_node_t *ext_holds     = get_match(ctx, find_holds);
    c4m_tree_node_t *ext_allocs    = get_match(ctx, find_allocs);
    c4m_tree_node_t *ext_lsig      = get_match(ctx, find_extern_local);

    if (pnode->short_doc) {
        info->short_doc = c4m_token_raw_content(pnode->short_doc);

        if (pnode->long_doc) {
            info->long_doc = c4m_token_raw_content(pnode->long_doc);
        }
    }

    if (ext_params != NULL) {
        int64_t n             = c4m_xlist_len(ext_params);
        info->num_params      = n;
        info->external_name   = external_name;
        info->external_params = c4m_gc_array_alloc(uint8_t, n);

        for (int64_t i = 0; i < n; i++) {
            c4m_tree_node_t *tnode = c4m_xlist_get(ext_params, i, NULL);
            c4m_pnode_t     *pnode = c4m_tree_get_contents(tnode);
            uint64_t         val   = (uint64_t)pnode->extra_info;

            info->external_params[i] = (uint8_t)val;
        }
    }

    if (ext_ret) {
        c4m_pnode_t *pnode = get_pnode(ext_ret);
        uint64_t     val   = (uint64_t)pnode->extra_info;

        info->external_return_type = (uint8_t)val;
    }

    info->local_params = extract_fn_sig_info(ctx, ext_lsig);

    if (ext_pure) {
        bool *pure_ptr = node_literal(c4m_tree_get_child(ext_pure, 0));

        if (pure_ptr && *pure_ptr) {
            info->local_params->pure = 1;
        }
    }

    info->local_name = node_text(get_match_on_node(ext_lsig, first_kid_id));

    if (ext_holds) {
        if (info->local_params == NULL) {
            c4m_add_error(ctx->file_ctx, c4m_err_no_params_to_hold, ext_holds);
            return;
        }

        uint64_t        bitfield  = 0;
        c4m_sig_info_t *si        = info->local_params;
        int             num_holds = c4m_tree_get_number_children(ext_holds);

        for (int i = 0; i < num_holds; i++) {
            c4m_tree_node_t *kid = c4m_tree_get_child(ext_holds, i);
            c4m_utf8_t      *txt = node_text(kid);

            for (int j = 0; j < si->num_params; j++) {
                c4m_param_info_t *param = &si->param_info[j];
                if (strcmp(txt->data, param->name->data)) {
                    continue;
                }
                param->ffi_holds = 1;
                uint64_t flag    = (uint64_t)(1 << j);
                if (bitfield & flag) {
                    c4m_add_warning(ctx->file_ctx, c4m_warn_dupe_hold, kid);
                }
                bitfield |= flag;
                goto next_i;
            }
            c4m_add_error(ctx->file_ctx, c4m_err_bad_hold_name, kid);
            break;
next_i:
    /* nothing. */;
        }
    }

    if (ext_allocs) {
        uint64_t        bitfield   = 0;
        bool            got_ret    = false;
        c4m_sig_info_t *si         = info->local_params;
        int             num_allocs = c4m_tree_get_number_children(ext_allocs);

        for (int i = 0; i < num_allocs; i++) {
            c4m_tree_node_t *kid = c4m_tree_get_child(ext_allocs, i);
            c4m_utf8_t      *txt = node_text(kid);

            if (!strcmp(txt->data, "return")) {
                if (got_ret) {
                    c4m_add_warning(ctx->file_ctx, c4m_warn_dupe_alloc, kid);
                    continue;
                }
                si->return_info.ffi_allocs = 1;
                continue;
            }

            for (int j = 0; j < si->num_params; j++) {
                c4m_param_info_t *param = &si->param_info[j];
                if (strcmp(txt->data, param->name->data)) {
                    continue;
                }
                param->ffi_allocs = 1;
                uint64_t flag     = (uint64_t)(1 << j);
                if (bitfield & flag) {
                    c4m_add_warning(ctx->file_ctx, c4m_warn_dupe_alloc, kid);
                }
                bitfield |= flag;
                goto next_alloc;
            }
            c4m_add_error(ctx->file_ctx, c4m_err_bad_alloc_name, kid);
            break;
next_alloc:
    /* nothing. */;
        }
    }

    c4m_declare_symbol(ctx->file_ctx,
                       ctx->file_ctx->module_scope,
                       info->local_name,
                       get_match(ctx, first_kid_id),
                       sk_extern_func,
                       NULL,
                       true);
}

static void
handle_use_stmt(pass_ctx *ctx)
{
    c4m_tree_node_t   *uri    = get_match(ctx, use_uri);
    c4m_tree_node_t   *member = get_match(ctx, member_last);
    c4m_xlist_t       *prefix = apply_pattern(ctx, member_prefix);
    c4m_module_info_t *mi     = c4m_gc_alloc(c4m_module_info_t);
    c4m_utf8_t        *fq     = node_text(member);
    bool               status = false;

    mi->specified_module = fq;

    if (c4m_xlist_len(prefix) != 0) {
        mi->specified_package = node_list_join(prefix,
                                               c4m_utf32_repeat('.', 1),
                                               true);
        fq                    = c4m_str_concat(mi->specified_package, fq);
    }

    if (uri) {
        mi->specified_uri = node_literal(uri);
    }

    c4m_declare_symbol(ctx->file_ctx,
                       ctx->file_ctx->imports,
                       fq,
                       cur_node(ctx),
                       sk_module,
                       &status,
                       false);
    if (!status) {
        c4m_add_info(ctx->file_ctx,
                     c4m_info_dupe_import,
                     cur_node(ctx));
    }

#ifdef C4M_PASS1_UNIT_TESTS
    c4m_utf8_t *default_txt = c4m_new_utf8("not specified");

    c4m_print(c4m_cstr_format(
        "USE: fq: {}; uri: {}\n",
        fq,
        mi->specified_uri ? mi->specified_uri : default_txt));
#endif
}

static void
pass_dispatch(pass_ctx *ctx)
{
    switch (cur_node_type(ctx)) {
    case c4m_nt_global_enum:
    case c4m_nt_enum:
        handle_enum_decl(ctx);
        break;

    case c4m_nt_func_def:
        handle_func_decl(ctx);
        break;

    case c4m_nt_var_decls:
    case c4m_nt_global_decls:
    case c4m_nt_const_var_decls:
    case c4m_nt_const_global_decls:
    case c4m_nt_const_decls:
        handle_var_decl(ctx);
        break;

    case c4m_nt_config_spec:
        handle_config_spec(ctx);
        break;

    case c4m_nt_param_block:
        handle_param_block(ctx);
        break;

    case c4m_nt_extern_block:
        handle_extern_block(ctx);
        break;

    case c4m_nt_use:
        handle_use_stmt(ctx);
        break;

    default:
        process_children(ctx);
        break;
    }
}

void
c4m_pass_1(c4m_file_compile_ctx *file_ctx)
{
    setup_pass1_patterns();

    pass_ctx ctx = {
        .file_ctx = file_ctx,
    };

    set_current_node(&ctx, file_ctx->parse_tree);
    file_ctx->global_scope    = c4m_new_scope(NULL, C4M_SCOPE_GLOBAL);
    file_ctx->module_scope    = c4m_new_scope(file_ctx->global_scope,
                                           C4M_SCOPE_MODULE);
    file_ctx->attribute_scope = c4m_new_scope(NULL, C4M_SCOPE_ATTRIBUTES);
    file_ctx->imports         = c4m_new_scope(NULL, C4M_SCOPE_IMPORTS);
    ctx.cur->static_scope     = file_ctx->module_scope;
    pass_dispatch(&ctx);
}

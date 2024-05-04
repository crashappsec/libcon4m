#define C4M_USE_INTERNAL_API
#include "con4m.h"

// First pass of the raw parse tree.
// Not everything we need would be resolvable in one pass, especially
// symbols in other modules.

// More consise aliases internally only.
#define tfind(x, y, ...) _c4m_tpat_find((void *)(int64_t)x, \
                                        y,                  \
                                        KFUNC(__VA_ARGS__))
#define tmatch(x, y, ...) _c4m_tpat_match((void *)(int64_t)x, \
                                          y,                  \
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
#define nt_any    c4m_nt_error
#define max_nodes 0x7fff

typedef struct {
    c4m_tree_node_t      *cur_tnode;
    c4m_pnode_t          *cur;
    c4m_file_compile_ctx *file_ctx;
} pass_ctx;

static bool
tcmp(int64_t kind_as_64, c4m_tree_node_t *node)
{
    c4m_node_kind_t kind  = (c4m_node_kind_t)(unsigned int)kind_as_64;
    c4m_pnode_t    *pnode = get_pnode(node);

    if (kind == nt_any) {
        return true;
    }

    return kind == pnode->kind;
}

static inline c4m_xlist_t *
apply_pattern(pass_ctx *ctx, c4m_tpat_node_t *pattern)
{
    c4m_xlist_t *cap = NULL;
    bool         ok  = c4m_tree_match(ctx->cur_tnode,
                             pattern,
                             (c4m_cmp_fn)tcmp,
                             &cap);

    if (!ok) {
        return NULL;
    }

    return cap;
}

// Return the first capture if there's a match, and NULL if not.
static inline c4m_tree_node_t *
get_match(pass_ctx *ctx, c4m_tpat_node_t *pattern)
{
    c4m_xlist_t *cap = apply_pattern(ctx, pattern);

    if (cap != NULL) {
        return c4m_xlist_get(cap, 0, NULL);
    }

    return NULL;
}

static c4m_tpat_node_t *first_kid_id = NULL;
static c4m_tpat_node_t *enum_items   = NULL;

static void
setup_pass1_patterns()
{
    if (first_kid_id != NULL) {
        return;
    }
    // Returns first child if it's an identifier, null otherwise.
    first_kid_id = tmatch(nt_any, 0, tmatch(c4m_nt_identifier, 1));
    // Skips the identifier if there, and returns all the enum items,
    // regardless of the subtree shape.
    enum_items   = tmatch(nt_any,
                        0,
                        toptional(c4m_nt_identifier, 0),
                        tcount_content(c4m_nt_enum_item,
                                       0,
                                       max_nodes,
                                       1));

    c4m_gc_register_root(&first_kid_id, 1);
    c4m_gc_register_root(&enum_items, 1);
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

static inline void *
node_info(pass_ctx *ctx)
{
    return ctx->cur->extra_info;
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

static inline c4m_pnode_t *
cur_node(pass_ctx *ctx)
{
    return ctx->cur;
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

        if (!hatrack_set_add(set, val)) {
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

        if (!hatrack_set_add(set, (void *)val)) {
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

        while (hatrack_set_contains(set, (void *)next_implicit)) {
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
                                   sk_enum_type);
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
                           sk_enum_val);
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
}

static void
handle_var_decl(pass_ctx *ctx)
{
}

static void
handle_config_spec(pass_ctx *ctx)
{
}

static void
handle_param_block(pass_ctx *ctx)
{
}

static void
handle_extern_block(pass_ctx *ctx)
{
}

static void
handle_use_stmt(pass_ctx *ctx)
{
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
    file_ctx->global_scope = c4m_new_scope(NULL, C4M_SCOPE_GLOBAL);
    file_ctx->module_scope = c4m_new_scope(file_ctx->global_scope,
                                           C4M_SCOPE_MODULE);
    ctx.cur->static_scope  = file_ctx->module_scope;
    pass_dispatch(&ctx);
}

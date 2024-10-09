#define C4M_USE_INTERNAL_API
#include "con4m.h"

static void gopt_extract_spec(c4m_gopt_extraction_ctx *);

static inline bool
is_group_node(c4m_parse_node_t *cur)
{
    return cur->group_top == true || cur->group_item == true;
}

static c4m_utf8_t *
get_node_text(c4m_tree_node_t *n)
{
    while (n->num_kids) {
        n = n->children[0];
    }

    c4m_parse_node_t *pn = c4m_tree_get_contents(n);
    return pn->info.token->value;
}

static void
handle_subcommand(c4m_gopt_extraction_ctx *ctx)
{
    // In case we decide to care about specifically tying flags to
    // where they showed up, we do ahead and save / restore
    // command state.

    c4m_tree_node_t *n = ctx->intree;
    while (n->num_kids) {
        n = n->children[0];
    }

    int               len        = n->num_kids;
    c4m_parse_node_t *cmd_node   = c4m_tree_get_contents(n);
    c4m_utf8_t       *saved_path = ctx->path;
    c4m_gopt_cspec   *cmd        = ctx->cur_cmd;
    int64_t           cmd_key;

    cmd_key      = cmd_node->info.token->tid;
    ctx->cur_cmd = hatrack_dict_get(cmd->sub_commands,
                                    (void *)cmd_key,
                                    NULL);

    c4m_utf8_t *cmd_name = ctx->cur_cmd->name;

    if (c4m_str_codepoint_len(saved_path)) {
        ctx->path = c4m_cstr_format("{}.{}", saved_path, cmd_name);
    }
    else {
        ctx->path = cmd_name;
    }

    c4m_list_t *args = c4m_list(c4m_type_ref());
    hatrack_dict_put(ctx->args, ctx->path, args);

    ctx->deepest_path = ctx->path;

    for (int i = 1; i < len; i++) {
        ctx->intree = n->children[i];
        gopt_extract_spec(ctx);
    }

    ctx->intree = n;
}

static inline void
boxed_bool_negation(bool *b)
{
    *b = !*b;
}

static bool
needs_negation(c4m_goption_t *o1, c4m_goption_t *o2)
{
    bool pos_1;
    bool pos_2;

    switch (o1->type) {
    case C4M_GOAT_BOOL_T_DEFAULT:
    case C4M_GOAT_BOOL_T_ALWAYS:
        pos_1 = true;
        break;
    default:
        pos_1 = false;
        break;
    }

    switch (o2->type) {
    case C4M_GOAT_BOOL_T_DEFAULT:
    case C4M_GOAT_BOOL_T_ALWAYS:
        pos_2 = true;
        break;
    default:
        pos_2 = false;
        break;
    }

    return pos_1 ^ pos_2;
}

static inline void
negate_bool_array(c4m_list_t *l)
{
    int n = c4m_list_len(l);

    for (int i = 0; i < n; i++) {
        bool *b = c4m_list_get(l, i, NULL);
        boxed_bool_negation(b);
    }
}

static inline void *
extract_value_from_node(c4m_gopt_extraction_ctx *ctx,
                        c4m_tree_node_t         *n,
                        bool                    *ok)
{
    c4m_parse_node_t  *cur = c4m_tree_get_contents(n);
    c4m_grammar_t     *g   = ctx->parser->grammar;
    c4m_gopt_node_type gopt_type;
    c4m_list_t        *items;
    int64_t            intval;
    double             dval;
    c4m_obj_t          val;

    if (ok) {
        *ok = true;
    }

    gopt_type = (c4m_gopt_node_type)c4m_parse_get_user_data(g, cur);
    while (!cur->token) {
        n   = n->children[0];
        cur = c4m_tree_get_contents(n);
    }

    switch (gopt_type) {
    case C4M_GTNT_FLOAT_NT:
        if ((cur->info.token->tid - C4M_START_TOK_ID) == C4M_GOTT_FLOAT) {
            if (!c4m_parse_double(cur->info.token->value, &dval)) {
                C4M_CRAISE("Todo: turn this into an error.\n");
            }
            return c4m_box_double(dval);
        }
        if (!c4m_parse_int64(cur->info.token->value, &intval)) {
            C4M_CRAISE("Todo: turn this into an error.\n");
        }
        return c4m_box_double(intval + 0.0);

    case C4M_GTNT_INT_NT:

        if (!c4m_parse_int64(cur->info.token->value, &intval)) {
            C4M_CRAISE("Todo: turn this into an error.\n");
        }

        return c4m_box_i64(intval);

    case C4M_GTNT_BOOL_NT:
        if ((cur->info.token->tid - C4M_START_TOK_ID) == C4M_GOTT_BOOL_T) {
            return c4m_box_bool(true);
        }
        return c4m_box_bool(false);
    case C4M_GTNT_WORD_NT:
        val = c4m_to_utf8(cur->info.token->value);
        if (!val && ok) {
            *ok = false;
        }

        return val;
    case C4M_GTNT_OTHER:
        if (!cur) {
            return NULL;
        }

        if (is_group_node(cur)) {
            items = c4m_list(c4m_type_ref());

            bool v;

            for (int i = 0; i < n->num_kids; i++) {
                val = extract_value_from_node(ctx, n->children[i], &v);

                if (v) {
                    c4m_list_append(items, val);
                }
            }
            switch (c4m_list_len(items)) {
            case 0:
                if (ok) {
                    *ok = NULL;
                }
                return NULL;
            case 1:
                return c4m_list_get(items, 0, NULL);
            default:
                return c4m_clean_internal_list(items);
            }
        }

        if (ok) {
            *ok = false;
        }
        return NULL;

    default:
        if (ok) {
            *ok = false;
        }
        return NULL;
    }
}

static int
get_option_arg_offset(c4m_tree_node_t *n)
{
    c4m_tree_node_t *sub = n->children[1];

    if (sub->num_kids) {
        c4m_parse_node_t *pnode = c4m_tree_get_contents(sub->children[0]);
        if ((pnode->info.token->tid - C4M_START_TOK_ID) == C4M_GOTT_ASSIGN) {
            return 2;
        }
    }

    return 1;
}

static void
handle_option_rule(c4m_gopt_extraction_ctx *ctx, c4m_tree_node_t *n)
{
    int               len              = n->num_kids;
    c4m_utf8_t       *name             = get_node_text(n);
    c4m_tree_node_t  *name_tnode       = n->children[0]->children[0];
    c4m_parse_node_t *name_pnode       = c4m_tree_get_contents(name_tnode);
    int               flag_id          = name_pnode->info.token->tid;
    c4m_list_t       *extracted_values = c4m_list(c4m_type_ref());
    int64_t           key;

    if (flag_id == (C4M_START_TOK_ID + C4M_GOTT_UNKNOWN_OPT)) {
        C4M_CRAISE("Unknown option. TODO: turn into an error.");
    }

    c4m_goption_t *cur_option = hatrack_dict_get(ctx->gctx->all_options,
                                                 name,
                                                 NULL);
    assert(cur_option);

    // when len == 1, no arg was provided.
    if (len != 1) {
        int offset = get_option_arg_offset(n);

        c4m_list_append(extracted_values,
                        extract_value_from_node(ctx,
                                                n->children[offset],
                                                NULL));

        if (len < offset + 1) {
            n   = n->children[offset + 1];
            len = n->num_kids;
            for (int i = 0; i < len; i++) {
                c4m_list_append(extracted_values,
                                extract_value_from_node(ctx,
                                                        n->children[i],
                                                        NULL));
            }
        }
    }
    else {
        c4m_list_append(extracted_values, c4m_box_bool(true));
    }
    c4m_goption_t *target = cur_option->linked_option;

    if (!target) {
        target = cur_option;
    }

    key = target->result_key;

    c4m_rt_option_t *existing = hatrack_dict_get(ctx->flags, (void *)key, NULL);

    if (!existing) {
        existing = c4m_gc_alloc_mapped(c4m_rt_option_t, C4M_GC_SCAN_ALL);
        hatrack_dict_put(ctx->flags, (void *)key, existing);
        existing->spec = target;
    }

    if (target->max_args == 1) {
        c4m_obj_t val;

        switch (cur_option->type) {
        case C4M_GOAT_BOOL_T_DEFAULT:
        case C4M_GOAT_BOOL_F_DEFAULT:
        case C4M_GOAT_BOOL_T_ALWAYS:
        case C4M_GOAT_BOOL_F_ALWAYS:

            val = c4m_list_get(extracted_values, 0, NULL);

            if (needs_negation(cur_option, target)) {
                boxed_bool_negation((bool *)val);
            }

            existing->value = val;
            existing->n     = 1;
            return;

        case C4M_GOAT_WORD:
        case C4M_GOAT_INT:
        case C4M_GOAT_FLOAT:
            val = c4m_list_get(extracted_values, 0, NULL);

            existing->value = val;
            existing->n     = 1;
            return;

            // Choices should all have more than one value.
        case C4M_GOAT_CHOICE:
        case C4M_GOAT_CHOICE_T_ALIAS:
        case C4M_GOAT_CHOICE_F_ALIAS:
        default:
            c4m_unreachable();
        }
    }

    switch (cur_option->type) {
    case C4M_GOAT_CHOICE_T_ALIAS:
        if (existing->n) {
            c4m_list_t *cur = (c4m_list_t *)existing->value;
            len             = c4m_list_len(cur);
            c4m_obj_t   us  = c4m_list_get(extracted_values, 0, NULL);
            c4m_type_t *t   = c4m_get_my_type(us);

            for (int i = 0; i < len; i++) {
                c4m_obj_t one = c4m_list_get(cur, i, NULL);
                if (c4m_eq(t, us, one)) {
                    // Nothing to do.
                    return;
                }
            }
        }
        break;
    case C4M_GOAT_CHOICE_F_ALIAS:
        if (!existing->n) {
            return;
        }
        c4m_list_t *cur = (c4m_list_t *)existing->value;
        len             = c4m_list_len(cur);
        c4m_obj_t   us  = c4m_list_get(extracted_values, 0, NULL);
        c4m_type_t *t   = c4m_get_my_type(us);
        c4m_list_t *new = c4m_list(t);

        for (int i = 0; i < len; i++) {
            c4m_obj_t one = c4m_list_get(cur, i, NULL);
            if (!c4m_eq(t, us, one)) {
                c4m_list_append(new, one);
            }
        }
        if (c4m_list_len(new) == len) {
            return;
        }

        existing->n      = 0;
        extracted_values = new;
        break;

    case C4M_GOAT_BOOL_T_DEFAULT:
    case C4M_GOAT_BOOL_F_DEFAULT:
    case C4M_GOAT_BOOL_T_ALWAYS:
    case C4M_GOAT_BOOL_F_ALWAYS:

        if (c4m_list_len(extracted_values) == 0) {
            c4m_list_append(extracted_values, c4m_box_bool(true));
        }

        if (needs_negation(cur_option, target)) {
            negate_bool_array(extracted_values);
        }
        break;
    default:
        break;
    }

    c4m_list_t *new_items;

    if (existing->n) {
        new_items = (c4m_list_t *)existing->value;
        new_items = c4m_list_plus(new_items, extracted_values);
    }
    else {
        new_items = extracted_values;
    }

    len = c4m_list_len(new_items);
    if (len > target->max_args && target->max_args) {
        len       = target->max_args;
        new_items = c4m_list_get_slice(new_items, len - target->max_args, len);
    }

    existing->value = new_items;
    existing->n     = len;

    hatrack_dict_put(ctx->flags, (void *)key, existing);
}

static void
extract_args(c4m_gopt_extraction_ctx *ctx)
{
    c4m_tree_node_t  *n   = ctx->intree;
    c4m_parse_node_t *cur = c4m_tree_get_contents(n);

    if (!c4m_parse_get_user_data(ctx->parser->grammar, cur)) {
        for (int i = 0; i < n->num_kids; i++) {
            ctx->intree = n->children[i];
            extract_args(ctx);
        }
        ctx->intree = n;
    }
    else {
        c4m_list_t *arg_info = hatrack_dict_get(ctx->args, ctx->path, NULL);
        c4m_obj_t   obj      = extract_value_from_node(ctx, n, NULL);
        c4m_list_append(arg_info, obj);
    }
}

static void
gopt_extract_spec(c4m_gopt_extraction_ctx *ctx)
{
    c4m_tree_node_t   *n   = ctx->intree;
    c4m_parse_node_t  *cur = c4m_tree_get_contents(n);
    c4m_grammar_t     *g   = ctx->parser->grammar;
    c4m_gopt_node_type t   = (c4m_gopt_node_type)c4m_parse_get_user_data(g,
                                                                       cur);
    int                len = n->num_kids;

    if (is_group_node(cur)) {
        extract_args(ctx);
        ctx->intree = n;
        return;
    }

    switch (t) {
    case C4M_GTNT_FLOAT_NT:
    case C4M_GTNT_INT_NT:
    case C4M_GTNT_BOOL_NT:
    case C4M_GTNT_WORD_NT:
    case C4M_GTNT_OPT_JUNK_MULTI:
    case C4M_GTNT_OPT_JUNK_SOLO:
    case C4M_GTNT_OPTION_RULE:
        c4m_unreachable();

    case C4M_GTNT_CMD_NAME:
        handle_subcommand(ctx);
        // fallthrough
    default:
        for (int i = 0; i < len; i++) {
            ctx->intree = n->children[i];
            gopt_extract_spec(ctx);
        }
    }
    ctx->intree = n;
}

static void
gopt_extract_top_spec(c4m_gopt_extraction_ctx *ctx)
{
    c4m_tree_node_t  *n   = ctx->intree;
    c4m_parse_node_t *cur = c4m_tree_get_contents(n);
    int               len = n->num_kids;

    ctx->cur_cmd = hatrack_dict_get(ctx->gctx->top_specs,
                                    (void *)cur->id,
                                    NULL);

    c4m_list_t *args = c4m_list(c4m_type_ref());

    hatrack_dict_put(ctx->args, ctx->path, args);

    assert(ctx->cur_cmd);

    for (int i = 0; i < len; i++) {
        ctx->intree = n->children[i];
        gopt_extract_spec(ctx);
    }
}

static void
extract_individual_flags(c4m_gopt_extraction_ctx *ctx, c4m_tree_node_t *n)
{
    c4m_parse_node_t  *np = c4m_tree_get_contents(n);
    c4m_grammar_t     *g  = ctx->parser->grammar;
    c4m_gopt_node_type tinfo;

    tinfo = (c4m_gopt_node_type)c4m_parse_get_user_data(g, np);

    if (tinfo == C4M_GTNT_OPT_JUNK_SOLO) {
        c4m_list_append(ctx->flag_nodes, n->children[0]);
        handle_option_rule(ctx, n->children[0]);
        return;
    }
    for (int i = 0; i < n->num_kids; i++) {
        extract_individual_flags(ctx, n->children[i]);
    }
}

static void
gopt_extract_flags(c4m_gopt_extraction_ctx *ctx, c4m_tree_node_t *n)
{
    int                l = n->num_kids;
    c4m_grammar_t     *g = ctx->parser->grammar;
    c4m_gopt_node_type tinfo;

    while (l--) {
        c4m_tree_node_t  *kidt = n->children[l];
        c4m_parse_node_t *kidp = c4m_tree_get_contents(kidt);

        tinfo = (c4m_gopt_node_type)c4m_parse_get_user_data(g, kidp);

        if (tinfo == C4M_GTNT_OPT_JUNK_MULTI) {
            extract_individual_flags(ctx, kidt);

            for (int i = l + 1; i < n->num_kids; i++) {
                n->children[i - 1] = n->children[i];
            }

            n->num_kids--;
        }
        else {
            gopt_extract_flags(ctx, kidt);
        }
    }
}

static c4m_gopt_result_t *
convert_parse_tree(c4m_gopt_ctx *gctx, c4m_parser_t *p, c4m_tree_node_t *node)
{
    c4m_gopt_extraction_ctx ctx = {
        .flag_nodes   = c4m_list(c4m_type_ref()),
        .flags        = c4m_dict(c4m_type_int(), c4m_type_ref()),
        .args         = c4m_dict(c4m_type_utf8(), c4m_type_ref()),
        .intree       = node,
        .gctx         = gctx,
        .path         = c4m_new_utf8(""),
        .deepest_path = c4m_new_utf8(""),
        .parser       = p,
    };

    gopt_extract_flags(&ctx, ctx.intree);

    if (gctx->show_debug) {
        for (int i = 0; i < c4m_list_len(ctx.flag_nodes); i++) {
            c4m_utf8_t *s;
            c4m_printf("[h4]Flag {}", i + 1);
            s = c4m_parse_tree_format(c4m_list_get(ctx.flag_nodes, i, NULL));
            c4m_print(s);
        }
    }

    hatrack_dict_put(ctx.args, ctx.path, c4m_list(c4m_type_ref()));

    gopt_extract_top_spec(&ctx);

    c4m_gopt_result_t *result = c4m_gc_alloc_mapped(c4m_gopt_result_t,
                                                    C4M_GC_SCAN_ALL);
    result->flags             = ctx.flags;
    result->args              = ctx.args;
    result->errors            = ctx.errors;
    result->cmd               = ctx.deepest_path;

    return result;
}

static int64_t
fix_pruned_tree(c4m_tree_node_t *t)
{
    c4m_parse_node_t *pn = c4m_tree_get_contents(t);
    // Without resetting the hash on each node, the builtin dedupe
    // won't pick up the tree changes we made (removing Opts nodes)
    pn->hv               = 0;

    if (t->num_kids != 0) {
        for (int i = 0; i < t->num_kids; i++) {
            fix_pruned_tree(t->children[i]);
            c4m_parse_node_t *cur = c4m_tree_get_contents(t->children[i]);

            // We're not actually going to use the token position indicators,
            // so this is the easiest thing to do.
            cur->start = 0;
            cur->end   = 0;
        }
    }
}

c4m_list_t *
c4m_gopt_parse(c4m_gopt_ctx *ctx, c4m_str_t *argv0, c4m_list_t *args)
{
    c4m_duration_t *start1 = c4m_now();

    if (args == NULL && argv0 == NULL) {
        argv0 = c4m_get_argv0();
    }
    if (args == NULL) {
        args = c4m_get_program_arguments();
    }

    c4m_gopt_finalize(ctx);
    c4m_gopt_tokenize(ctx, argv0, args);

    c4m_parser_t *opt_parser = c4m_new(c4m_type_parser(), ctx->grammar);

    //    opt_parser->show_debug = ctx->show_debug;

    c4m_parse_token_list(opt_parser, ctx->tokens, ctx->nt_start);
    c4m_list_t *trees   = c4m_parse_get_parses(opt_parser);
    c4m_list_t *cleaned = c4m_list(c4m_type_ref());

    c4m_list_t *results = c4m_list(c4m_type_ref());

    for (int i = 0; i < c4m_list_len(trees); i++) {
        c4m_tree_node_t   *t   = c4m_list_get(trees, i, NULL);
        c4m_gopt_result_t *res = convert_parse_tree(ctx, opt_parser, t);
        fix_pruned_tree(t);
        c4m_list_append(cleaned, t);

        int num_trees = c4m_list_len(cleaned);
        if (num_trees > 1) {
            cleaned = c4m_clean_trees(opt_parser, cleaned);
            if (c4m_list_len(cleaned) != num_trees) {
                continue;
            }
        }

        c4m_list_append(results, res);
    }

    c4m_duration_t *end1 = c4m_now();
    if (ctx->show_debug) {
        c4m_print(c4m_forest_format(cleaned));
    }
    c4m_duration_t *end2 = c4m_now();

    if (ctx->show_debug) {
        c4m_duration_t *time1 = c4m_duration_diff(start1, end1);
        c4m_duration_t *time2 = c4m_duration_diff(end1, end2);

        c4m_printf("Spent [h1]{}[/] in parser and [h1]{}[/] printing the tree.",
                   time1,
                   time2);
    }
    return results;
}

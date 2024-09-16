#define C4M_USE_INTERNAL_API
#include "con4m.h"

c4m_utf8_t *
c4m_repr_parse_node(c4m_parse_node_t *n)
{
    if (!c4m_in_heap(n)) {
        return c4m_new_utf8("??");
    }

    if (n->token) {
        if (n->id == C4M_EMPTY_STRING) {
            return c4m_cstr_format("ε ({})", n->start);
        }

        c4m_utf8_t *start = c4m_repr_token_info(n->info.token);
        c4m_utf8_t *end   = c4m_cstr_format(
            "[atomic lime]({}-{})[/] id #{}",
            n->start,
            n->end,
            n->id);
        return c4m_str_concat(start, end);
    }
    c4m_utf8_t *name    = n->info.name;
    c4m_utf8_t *penalty = c4m_new_utf8("-");
    c4m_utf8_t *empty   = c4m_new_utf8("");

    if (n->id == C4M_EMPTY_STRING) {
        return c4m_cstr_format("ε ({})", n->start);
    }

    c4m_utf8_t *rest = c4m_cstr_format(
        " [atomic lime]({}-{})[/] id #{} [em]{}{}[/]",
        n->start,
        n->end,
        n->id,
        n->penalty ? penalty : empty,
        n->penalty);

    return c4m_str_concat(name, rest);
}

c4m_utf8_t *
c4m_repr_term(c4m_grammar_t *grammar, int64_t id)
{
    if (id < C4M_START_TOK_ID) {
        if (c4m_codepoint_is_printable(id)) {
            return c4m_cstr_format("'{}'",
                                   c4m_utf8_repeat((c4m_codepoint_t)id, 1));
        }
        return c4m_cstr_format("'U+{:h}'", id);
    }

    id -= C4M_START_TOK_ID;
    c4m_terminal_t *t = c4m_list_get(grammar->named_terms, id, NULL);

    if (!t) {
        C4M_CRAISE(
            "Invalid parse token ID found (mixing "
            "rules from a different grammar?");
    }

    return c4m_cstr_format("'{}'", t->value);
}

c4m_utf8_t *
c4m_repr_nonterm(c4m_grammar_t *grammar, int64_t id, bool show)
{
    c4m_nonterm_t *nt = c4m_get_nonterm(grammar, id);

    if (!nt) {
        if (id == C4M_GID_SHOW_GROUP_LHS) {
            return c4m_rich_lit("[em]»group«  [/]");
        }
        else {
            return c4m_rich_lit("[em]»item«  [/]");
        }
    }

    c4m_utf8_t *null = (show && nt->nullable) ? c4m_new_utf8(" (∅-able)")
                                              : c4m_new_utf8("");

    if (id == grammar->default_start) {
        return c4m_cstr_format("[yellow i]{}[/]{}", nt->name, null);
    }
    return c4m_cstr_format("[em]{}[/]{}", nt->name, null);
}

c4m_utf8_t *
c4m_repr_group(c4m_grammar_t *g, c4m_rule_group_t *group)
{
    c4m_utf8_t *base;

    base = c4m_repr_rule(g, group->items, -1);

    if (group->min == 1 && group->max == 1) {
        return c4m_cstr_format("([aqua]{}[/])", base);
    }

    if (group->min == 0 && group->max == 1) {
        return c4m_cstr_format("([aqua]{}[/])?", base);
    }

    if (group->max < 1 && group->min <= 1) {
        if (group->min == 0) {
            return c4m_cstr_format("([aqua]{}[/])*", base);
        }
        else {
            return c4m_cstr_format("([aqua]{}[/])+",
                                   base);
        }
    }
    else {
        return c4m_cstr_format("({}){}{}, {}]",
                               base,
                               c4m_new_utf8("["),
                               group->min,
                               group->max);
    }
}

static c4m_utf8_t *
repr_item(c4m_grammar_t *g, c4m_pitem_t *item)
{
    switch (item->kind) {
    case C4M_P_NULL:
        return c4m_new_utf8("ε");
    case C4M_P_GROUP:
        return c4m_repr_group(g, item->contents.group);
    case C4M_P_NT:
        return c4m_repr_nonterm(g, item->contents.nonterm, false);
    case C4M_P_TERMINAL:
        return c4m_repr_term(g, item->contents.terminal);
    case C4M_P_ANY:
        return c4m_new_utf8("«Any»");
    case C4M_P_BI_CLASS:
        switch (item->contents.class) {
        case C4M_P_BIC_ID_START:
            return c4m_new_utf8("«IdStart»");
        case C4M_P_BIC_ID_CONTINUE:
            return c4m_new_utf8("«IdContinue»");
        case C4M_P_BIC_C4M_ID_START:
            return c4m_new_utf8("«C4mIdStart»");
        case C4M_P_BIC_C4M_ID_CONTINUE:
            return c4m_new_utf8("«C4mIdContinue»");
        case C4M_P_BIC_DIGIT:
            return c4m_new_utf8("«Digit»");
        case C4M_P_BIC_ANY_DIGIT:
            return c4m_new_utf8("«UDigit»");
        case C4M_P_BIC_UPPER:
            return c4m_new_utf8("«Upper»");
        case C4M_P_BIC_UPPER_ASCII:
            return c4m_new_utf8("«AsciiUpper»");
        case C4M_P_BIC_LOWER:
            return c4m_new_utf8("«Lower»");
        case C4M_P_BIC_LOWER_ASCII:
            return c4m_new_utf8("«AsciiLower»");
        case C4M_P_BIC_SPACE:
            return c4m_new_utf8("«WhiteSpace»");
        }
    case C4M_P_SET:;
        c4m_list_t *l = c4m_list(c4m_type_utf8());
        int         n = c4m_list_len(item->contents.items);

        for (int i = 0; i < n; i++) {
            c4m_list_append(l,
                            repr_item(g,
                                      c4m_list_get(item->contents.items,
                                                   i,
                                                   NULL)));
        }

        return c4m_cstr_format("{}{}]",
                               c4m_new_utf8("["),
                               c4m_str_join(l, c4m_new_utf8("|")));
    }
}

// This could be called both stand-alone when printing out a grammar, and
// when printing out Earley states. In the former case, the dot should never
// show up (nor should the iteration count, but that's handled elsewhere).
//
// To that end, if dot_location is negative, we assume we're printing
// a grammar.

c4m_utf8_t *
c4m_repr_rule(c4m_grammar_t *g, c4m_list_t *items, int dot_location)
{
    c4m_list_t *pieces = c4m_list(c4m_type_utf8());
    int         n      = c4m_list_len(items);

    for (int i = 0; i < n; i++) {
        if (i == dot_location) {
            c4m_list_append(pieces, c4m_new_utf8("•"));
        }

        c4m_pitem_t *item = c4m_list_get(items, i, NULL);
        c4m_list_append(pieces, repr_item(g, item));
    }

    if (n == dot_location) {
        c4m_list_append(pieces, c4m_new_utf8("•"));
    }

    return c4m_str_join(pieces, c4m_new_utf8(" "));
}

static inline c4m_utf8_t *
op_to_string(c4m_earley_op op)
{
    switch (op) {
    case C4M_EO_PREDICT_NT:
        return c4m_new_utf8("Predict (N)");
    case C4M_EO_PREDICT_G:
        return c4m_new_utf8("Predict (G)");
    case C4M_EO_PREDICT_I:
        return c4m_new_utf8("Predict (I)");
    case C4M_EO_SCAN_NULL:
        return c4m_new_utf8("Scan ε");
    case C4M_EO_COMPLETE_N:
        return c4m_new_utf8("Complete (N)");
    case C4M_EO_COMPLETE_I:
        return c4m_new_utf8("Complete (I)");
    case C4M_EO_COMPLETE_G:
        return c4m_new_utf8("Complete (G)");
    default:
        return c4m_new_utf8("Scan");
    }
}

static inline c4m_utf8_t *
repr_subtree_info(c4m_subtree_info_t si)
{
    switch (si) {
    case C4M_SI_NONE:
        return c4m_new_utf8(" ");
    case C4M_SI_NT_RULE_START:
        return c4m_new_utf8("⊤");
    case C4M_SI_NT_RULE_END:
        return c4m_new_utf8("⊥");
    case C4M_SI_GROUP_START:
        return c4m_new_utf8("g⊤");
    case C4M_SI_GROUP_END:
        return c4m_new_utf8("⊥g");
    case C4M_SI_GROUP_ITEM_START:
        return c4m_new_utf8("i⊤");
    case C4M_SI_GROUP_ITEM_END:
        return c4m_new_utf8("⊥i");
    }
}

// This returns a list of table elements.
c4m_list_t *
c4m_repr_earley_item(c4m_parser_t *parser, c4m_earley_item_t *cur, int id)
{
    c4m_list_t    *result = c4m_list(c4m_type_utf8());
    c4m_grammar_t *g      = parser->grammar;

    c4m_list_append(result, c4m_str_from_int(id));

    c4m_utf8_t *nt;
    c4m_utf8_t *rule;

    if (cur->double_dot) {
        nt = c4m_repr_nonterm(g, C4M_GID_SHOW_GROUP_LHS, true);
    }
    else {
        nt = c4m_repr_nonterm(g, cur->ruleset_id, true);
    }

    rule = c4m_repr_rule(g, cur->rule, cur->cursor);

    if (cur->double_dot) {
        if (cur->cursor == 0) {
            rule = c4m_str_concat(c4m_new_utf8("•"), rule);
        }
        else {
            rule = c4m_str_concat(rule, c4m_new_utf8("•"));
        }
    }

    c4m_utf8_t *full = c4m_cstr_format("{} ⟶  {}", nt, rule);

    c4m_list_append(result, full);

    if (cur->group) {
        c4m_list_append(result, c4m_str_from_int(cur->match_ct));
    }
    else {
        c4m_list_append(result, c4m_new_utf8(" "));
    }

    c4m_utf8_t         *links;
    uint64_t            n;
    c4m_earley_item_t **clist = hatrack_set_items_sort(cur->starts, &n);

    if (!n) {
        links = c4m_rich_lit(" [i]Predicted by:[/] «Root» ");
    }

    else {
        links = c4m_rich_lit(" [i]Predicted by:[/]");

        for (uint64_t i = 0; i < n; i++) {
            c4m_earley_item_t *rent = clist[i];

            c4m_utf8_t *s = c4m_cstr_format(" {}:{}",
                                            rent->estate_id,
                                            rent->eitem_index);
            links         = c4m_str_concat(links, s);
        }
    }

    n = 0;

    if (cur->completors) {
        clist = hatrack_set_items_sort(cur->completors, &n);
    }
    if (n) {
        links = c4m_str_concat(links, c4m_rich_lit(" [i]Via Completion:[/] "));

        for (uint64_t i = 0; i < n; i++) {
            c4m_earley_item_t *ei = clist[i];
            c4m_utf8_t        *s  = c4m_cstr_format(" {}:{}",
                                            ei->estate_id,
                                            ei->eitem_index);
            links                 = c4m_str_concat(links, s);
        }
    }

    clist = hatrack_set_items_sort(cur->predictions, &n);
    if (n) {
        links = c4m_str_concat(links, c4m_rich_lit(" [i]Predictions:[/] "));

        for (uint64_t i = 0; i < n; i++) {
            c4m_earley_item_t *ei = clist[i];
            c4m_utf8_t        *s  = c4m_cstr_format(" {}:{}",
                                            ei->estate_id,
                                            ei->eitem_index);
            links                 = c4m_str_concat(links, s);
        }
    }

    if (cur->group && cur->subtree_info == C4M_SI_GROUP_ITEM_START) {
        c4m_earley_item_t *prev = cur->start_item->previous_scan;
        if (prev) {
            prev = prev->start_item;
            c4m_utf8_t *pstr;

            pstr  = c4m_cstr_format(" [i]Prev item end:[/] {}:{}",
                                   prev->estate_id,
                                   prev->eitem_index);
            links = c4m_str_concat(links, pstr);
        }
    }

    if (cur->previous_scan) {
        c4m_utf8_t *scan;
        scan  = c4m_cstr_format(" [i]Prior scan:[/] {}:{}",
                               cur->previous_scan->estate_id,
                               cur->previous_scan->eitem_index);
        links = c4m_str_concat(scan, links);
    }

    if (cur->start_item) {
        links = c4m_str_concat(links,
                               c4m_cstr_format(" [i]Node Location:[/] {}:{}",
                                               cur->start_item->estate_id,
                                               cur->start_item->eitem_index));
    }

    if (cur->penalty) {
        links = c4m_str_concat(links,
                               c4m_cstr_format(" [em]-{}[/]", cur->penalty));
    }

    if (cur->group) {
        links = c4m_str_concat(links,
                               c4m_cstr_format(" (gid: {:x})",
                                               cur->ruleset_id));
    }

    links = c4m_str_strip(links);

    c4m_list_append(result, c4m_to_utf8(links));
    c4m_list_append(result, op_to_string(cur->op));
    c4m_list_append(result, repr_subtree_info(cur->subtree_info));

    return result;
}

#if defined(C4M_EARLEY_DEBUG)
c4m_grid_t *
c4m_get_parse_state(c4m_parser_t *parser, bool next)
{
    c4m_grid_t *grid = c4m_new(c4m_type_grid(),
                               c4m_kw("start_cols",
                                      c4m_ka(4),
                                      "header_rows",
                                      c4m_ka(1),
                                      "container_tag",
                                      c4m_ka(c4m_new_utf8("table2")),
                                      "stripe",
                                      c4m_ka(true)));

    c4m_utf8_t *snap = c4m_new_utf8("snap");
    c4m_utf8_t *flex = c4m_new_utf8("flex");
    c4m_list_t *hdr  = c4m_new_table_row();

    c4m_list_append(hdr, c4m_rich_lit("[th]#"));
    c4m_list_append(hdr, c4m_rich_lit("[th]Rule State"));
    c4m_list_append(hdr, c4m_rich_lit("[th]Matches"));
    c4m_list_append(hdr, c4m_rich_lit("[th]Info"));

    c4m_grid_add_row(grid, hdr);

    c4m_set_column_style(grid, 0, snap);
    c4m_set_column_style(grid, 1, flex);
    c4m_set_column_style(grid, 2, snap);
    c4m_set_column_style(grid, 3, flex);

    c4m_earley_state_t *s = next ? parser->next_state : parser->current_state;

    int n = c4m_list_len(s->items);

    for (int i = 0; i < n; i++) {
        c4m_grid_add_row(grid,
                         c4m_repr_earley_item(parser,
                                              c4m_list_get(s->items, i, NULL),
                                              i));
    }

    return grid;
}
#endif

static c4m_list_t *
repr_one_grammar_nt(c4m_grammar_t *grammar, int ix)
{
    c4m_list_t    *row;
    c4m_list_t    *res   = c4m_list(c4m_type_utf8());
    c4m_nonterm_t *nt    = c4m_get_nonterm(grammar, ix);
    int            n     = c4m_list_len(nt->rules);
    c4m_utf8_t    *lhs   = c4m_repr_nonterm(grammar, ix, true);
    c4m_utf8_t    *arrow = c4m_rich_lit("[yellow]⟶ ");
    c4m_utf8_t    *rhs;

    for (int i = 0; i < n; i++) {
        row = c4m_new_table_row();
        rhs = c4m_repr_rule(grammar, c4m_list_get(nt->rules, i, NULL), -1);
        c4m_list_append(row, lhs);
        c4m_list_append(row, arrow);
        c4m_list_append(row, rhs);
        c4m_list_append(res, row);
    }

    return res;
}

c4m_grid_t *
c4m_grammar_format(c4m_grammar_t *grammar)
{
    int32_t     n    = (int32_t)c4m_list_len(grammar->rules);
    c4m_list_t *l    = repr_one_grammar_nt(grammar, grammar->default_start);
    c4m_grid_t *grid = c4m_new(c4m_type_grid(),
                               c4m_kw("start_cols",
                                      c4m_ka(3),
                                      "header_rows",
                                      c4m_ka(0),
                                      "container_tag",
                                      c4m_ka(c4m_new_utf8("flow"))));
    c4m_utf8_t *snap = c4m_new_utf8("snap");
    c4m_set_column_style(grid, 0, snap);
    c4m_set_column_style(grid, 1, snap);
    c4m_set_column_style(grid, 2, snap);

    c4m_grid_add_rows(grid, l);

    for (int32_t i = 0; i < n; i++) {
        if (i != grammar->default_start) {
            c4m_nonterm_t *nt = c4m_get_nonterm(grammar, i);
            if (!nt->name) {
                continue;
            }
            l = repr_one_grammar_nt(grammar, i);
            c4m_grid_add_rows(grid, l);
        }
    }

    return grid;
}

#if defined(C4M_EARLEY_DEBUG)
void
c4m_print_states(c4m_parser_t *p, c4m_list_t *l)
{
    c4m_grid_t *grid = c4m_new(c4m_type_grid(),
                               c4m_kw("start_cols",
                                      c4m_ka(6),
                                      "header_rows",
                                      c4m_ka(1),
                                      "container_tag",
                                      c4m_ka(c4m_new_utf8("table2")),
                                      "stripe",
                                      c4m_ka(true)));

    c4m_utf8_t *snap = c4m_new_utf8("snap");
    c4m_utf8_t *flex = c4m_new_utf8("flex");
    c4m_list_t *hdr  = c4m_new_table_row();

    c4m_list_append(hdr, c4m_rich_lit("[th]#"));
    c4m_list_append(hdr, c4m_rich_lit("[th]Rule State"));
    c4m_list_append(hdr, c4m_rich_lit("[th]Matches"));
    c4m_list_append(hdr, c4m_rich_lit("[th]Links"));
    c4m_list_append(hdr, c4m_rich_lit("[th]Command"));
    c4m_list_append(hdr, c4m_rich_lit("[th]NI"));

    c4m_grid_add_row(grid, hdr);

    c4m_set_column_style(grid, 0, snap);
    c4m_set_column_style(grid, 1, flex);
    c4m_set_column_style(grid, 2, snap);
    c4m_set_column_style(grid, 3, flex);
    c4m_set_column_style(grid, 4, flex);
    c4m_set_column_style(grid, 5, flex);

    int n = c4m_list_len(l);
    for (int i = 0; i < n; i++) {
        c4m_earley_item_t *item = c4m_list_get(l, i, NULL);
        c4m_grid_add_row(grid, c4m_repr_earley_item(p, item, i));
    }

    c4m_print(grid);
}
#endif

c4m_utf8_t *
c4m_repr_token_info(c4m_token_info_t *tok)
{
    if (!tok) {
        return c4m_new_utf8("No token?");
    }
    if (tok->value) {
        return c4m_cstr_format(
            "[h3]{}[/] "
            "[i](tok #{}; line #{}; col #{}; tid={})[/]",
            tok->value,
            tok->index,
            tok->line,
            tok->column,
            tok->tid);
    }
    else {
        return c4m_cstr_format(
            "[h3]EOF[/] "
            "[i](tok #{}; line #{}; col #{}; tid={})[/]",
            tok->index,
            tok->line,
            tok->column,
            tok->tid);
    }
}

c4m_grid_t *
c4m_forest_format(c4m_parser_t *parser)
{
    c4m_list_t *trees     = parser->parse_trees;
    int         num_trees = c4m_list_len(trees);

    if (!num_trees) {
        return c4m_callout(c4m_new_utf8("No valid parses."));
    }

    if (num_trees == 1) {
        return c4m_parse_tree_format(c4m_list_get(trees, 0, NULL));
    }

    printf("%d\n", num_trees);

    c4m_list_t *glist = c4m_list(c4m_type_grid());
    c4m_grid_t *cur;

    cur = c4m_new_cell(c4m_cstr_format("{} trees returned.", num_trees),
                       c4m_new_utf8("h1"));

    c4m_list_append(glist, cur);

    for (int i = 0; i < num_trees; i++) {
        cur = c4m_new_cell(c4m_cstr_format("Parse #{} of {}", i, num_trees),
                           c4m_new_utf8("h4"));
        c4m_list_append(glist, cur);
        c4m_tree_node_t *t = c4m_list_get(trees,
                                          i,
                                          NULL);
        if (!t) {
            continue;
        }
        c4m_list_append(glist, c4m_parse_tree_format(t));
    }

    return c4m_grid_flow_from_list(glist);
}

c4m_grid_t *
c4m_repr_state_table(c4m_parser_t *parser, bool show_all)
{
    c4m_grid_t *grid = c4m_new(c4m_type_grid(),
                               c4m_kw("start_cols",
                                      c4m_ka(show_all ? 6 : 4),
                                      "header_rows",
                                      c4m_ka(0),
                                      "container_tag",
                                      c4m_ka(c4m_new_utf8("table2")),
                                      "stripe",
                                      c4m_ka(true)));

    c4m_utf8_t *snap = c4m_new_utf8("snap");
    c4m_utf8_t *flex = c4m_new_utf8("flex");
    c4m_list_t *hdr  = c4m_new_table_row();
    c4m_list_t *row;

    c4m_list_append(hdr, c4m_rich_lit("[th]#"));
    c4m_list_append(hdr, c4m_rich_lit("[th]Rule State"));
    c4m_list_append(hdr, c4m_rich_lit("[th]Matches"));
    c4m_list_append(hdr, c4m_rich_lit("[th]Info"));

    if (show_all) {
        c4m_list_append(hdr, c4m_rich_lit("[th]Op"));
        c4m_list_append(hdr, c4m_rich_lit("[th]NI"));
    }

    int n = c4m_list_len(parser->states) - 1;

    for (int i = 0; i < n; i++) {
        c4m_earley_state_t *s = c4m_list_get(parser->states, i, NULL);
        c4m_utf8_t         *desc;

        if (i + 1 == n) {
            desc = c4m_cstr_format("[em]State {}[/] (End)", i);
        }
        else {
            desc = c4m_cstr_format(
                "[em u]State [reverse]{}[/reverse] "
                "Input: [reverse]{}[/reverse][em u] [/]",
                i,
                s->token->value);
        }

        c4m_list_t *l
            = c4m_new_table_row();
        c4m_list_append(l, c4m_new_utf8(""));
        c4m_list_append(l, desc);

        c4m_grid_add_row(grid, l);

        int m
            = c4m_list_len(s->items);

        c4m_grid_add_row(grid, hdr);

        for (int j = 0; j < m; j++) {
            c4m_earley_item_t *item = c4m_list_get(s->items, j, NULL);

            if (show_all || c4m_list_len(item->rule) == item->cursor) {
                row = c4m_repr_earley_item(parser, item, j);
                c4m_grid_add_row(grid, row);
            }
        }
    }

    c4m_set_column_style(grid, 0, snap);
    c4m_set_column_style(grid, 1, flex);
    c4m_set_column_style(grid, 2, snap);
    c4m_set_column_style(grid, 3, flex);
    if (show_all) {
        c4m_set_column_style(grid, 4, snap);
        c4m_set_column_style(grid, 5, snap);
    }

    return grid;
}

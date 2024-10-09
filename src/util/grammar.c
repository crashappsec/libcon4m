#define C4M_USE_INTERNAL_API
#include "con4m.h"

// This is the API for setting up grammars for our parser (parsing.c).
//
// Here is also where we handle setting up the parser object, since
// they go part and parcel together.
//
// You can reuse the same parser object to parse multiple items, as
// long as you properly reset between uses.
//
//
// Everything assumes single threaded access to data structures
// in this API.
//
// There's also a little bit of code in here for automatic
// tokenization for common scenarios. If that gets expanded
// significantly, it'll get broken out.

void
c4m_parser_reset(c4m_parser_t *parser)
{
    parser->states           = c4m_list(c4m_type_ref());
    parser->position         = 0;
    parser->start            = -1; // Use default start.
    parser->current_state    = c4m_new_earley_state(0);
    parser->next_state       = NULL;
    parser->user_context     = NULL;
    parser->token_cache      = NULL;
    parser->run              = false;
    parser->current_line     = 1;
    parser->current_column   = 0;
    parser->preloaded_tokens = false;
    // Leave the tokenizer if any, and ignore_escaped_newlines.

    c4m_list_append(parser->states, parser->current_state);
}

static void
c4m_parser_init(c4m_parser_t *parser, va_list args)
{
    parser->grammar = va_arg(args, c4m_grammar_t *);
    c4m_parser_reset(parser);
}

static void
c4m_grammar_init(c4m_grammar_t *grammar, va_list args)
{
    int32_t max_penalty         = C4M_DEFAULT_MAX_PARSE_PENALTY;
    bool    detect_errors       = true;
    bool    hide_error_rewrites = false;
    bool    hide_group_rewrites = false;
    bool    long_err_prefix     = false;

    c4m_karg_va_init(args);
    c4m_kw_int32("max_penalty", max_penalty);
    c4m_kw_bool("detect_errors", detect_errors);
    c4m_kw_bool("hide_error_rewrites", hide_error_rewrites);
    c4m_kw_bool("hide_group_rewrites", hide_group_rewrites);
    c4m_kw_bool("long_err_prefix", long_err_prefix);

    grammar->named_terms           = c4m_list(c4m_type_terminal());
    grammar->rules                 = c4m_list(c4m_type_ref());
    grammar->nt_list               = c4m_list(c4m_type_ruleset());
    grammar->nt_map                = c4m_dict(c4m_type_utf8(), c4m_type_u64());
    grammar->terminal_map          = c4m_dict(c4m_type_utf8(), c4m_type_u64());
    grammar->error_rules           = detect_errors;
    grammar->max_penalty           = max_penalty;
    grammar->hide_penalty_rewrites = hide_error_rewrites;
    grammar->hide_groups           = hide_group_rewrites;
    grammar->long_err_prefix       = long_err_prefix;
}

static void
c4m_terminal_init(c4m_terminal_t *terminal, va_list args)
{
    bool found;

    c4m_grammar_t *grammar = va_arg(args, c4m_grammar_t *);
    terminal->value        = va_arg(args, c4m_utf8_t *);

    // Special-case single character terminals to their codepoint.
    if (c4m_str_codepoint_len(terminal->value) == 1) {
        terminal->id = (int64_t)c4m_index(terminal->value, 0);
        return;
    }

    terminal->id = (int64_t)hatrack_dict_get(grammar->terminal_map,
                                             terminal->value,
                                             &found);

    if (!found) {
        int64_t n = c4m_list_len(grammar->named_terms);
        c4m_list_append(grammar->named_terms, terminal);

        // If there's a race condition, we may not get the slot we
        // think we're getting. And it's slightly possible we might
        // add the same terminal twice in parallel, in which case we
        // accept there might be a redundant item in the list.
        while (true) {
            c4m_terminal_t *t = c4m_list_get(grammar->named_terms, n, NULL);

            if (t == terminal || c4m_str_eq(t->value, terminal->value)) {
                n += C4M_START_TOK_ID;
                terminal->id = n;
                hatrack_dict_add(grammar->terminal_map,
                                 terminal->value,
                                 (void *)n);
                return;
            }
            n++;
        }
    }
}

static void
c4m_nonterm_init(c4m_nonterm_t *nonterm, va_list args)
{
    // With non-terminal addition, we have a similar race condition
    // with terminals. Here, however, we are going to stick rule state in
    // the objects, so we need to error if there are dupe rule names,
    // since we cannot currently bait-and-switch the object on the
    // call to c4m_new() (though that's a good feature to add).
    //
    // Note that a name of NULL indicates an anonymous node, which we
    // don't add to the dictionary, but we do add to the list. This is
    // used for groups.

    bool           found;
    c4m_utf8_t    *err;
    int64_t        n;
    c4m_grammar_t *grammar = va_arg(args, c4m_grammar_t *);
    nonterm->name          = va_arg(args, c4m_utf8_t *);
    nonterm->rules         = c4m_list(c4m_type_ref());

    if (nonterm->name == NULL) {
        // Anonymous / inline rule.
        n = c4m_list_len(grammar->nt_list);
        c4m_list_append(grammar->nt_list, nonterm);

        while (true) {
            c4m_nonterm_t *nt = c4m_list_get(grammar->nt_list, n, NULL);

            if (nt == nonterm) {
                nonterm->id = n;
                return;
            }

            n++;
        }
    }

    hatrack_dict_get(grammar->nt_map, nonterm->name, &found);

    if (found) {
bail:
        err = c4m_cstr_format("Duplicate ruleset name: [em]{}[/]",
                              nonterm->name);
        C4M_RAISE(err);
    }

    n = c4m_list_len(grammar->nt_list);
    c4m_list_append(grammar->nt_list, nonterm);

    // If there's a race condition, we may not get the slot we
    // think we're getting. And it's slightly possible we might
    // add the same terminal twice in parallel, in which case we
    // accept there might be a redundant item in the list.
    while (true) {
        c4m_nonterm_t *nt = c4m_list_get(grammar->nt_list, n, NULL);
        if (!nt) {
            return;
        }

        if (nt == nonterm) {
            hatrack_dict_put(grammar->nt_map, nonterm->name, (void *)n);
            nonterm->id = n;

            return;
        }
        if (nt->name && c4m_str_eq(nt->name, nonterm->name)) {
            goto bail;
        }
    }
}

static bool is_nullable_rule(c4m_grammar_t *, c4m_parse_rule_t *, c4m_list_t *);

static bool
check_stack_for_nt(c4m_list_t *stack, int64_t ruleset)
{
    // Return true if 'ruleset' is in the stack, false otherwise.

    int64_t len = c4m_list_len(stack);
    for (int64_t i = 0; i < len; i++) {
        if (ruleset == (int64_t)c4m_list_get(stack, i, NULL)) {
            return true;
        }
    }

    return false;
}

static inline void
finalize_nullable(c4m_nonterm_t *nt, bool value)
{
    nt->nullable  = value;
    nt->finalized = true;

    if (nt->nullable && nt->group_nt) {
        C4M_CRAISE("Attempt to add a group where the group item can be empty.");
    }
}

static bool
is_nullable_nt(c4m_grammar_t *g, int64_t nt_id, c4m_list_t *stack)
{
    // do not allow ourselves to recurse. We simply claim
    // to be nullable in that case.
    if (stack && check_stack_for_nt(stack, nt_id)) {
        return true;
    }
    else {
        c4m_list_append(stack, (void *)nt_id);
    }

    c4m_nonterm_t *nt = c4m_get_nonterm(g, nt_id);

    if (nt->finalized) {
        return nt->nullable;
    }

    // A non-terminal is nullable if any of its individual rules are
    // nullable.
    c4m_parse_rule_t *cur_rule;
    bool              found_any_rule = false;
    int64_t           i              = 0;

    if (c4m_list_len(nt->rules) == 0) {
        finalize_nullable(nt, true);
        return true;
    }

    cur_rule = c4m_list_get(nt->rules, i++, NULL);

    while (cur_rule) {
        found_any_rule = true;
        if (is_nullable_rule(g, cur_rule, stack)) {
            finalize_nullable(nt, true);
            c4m_list_pop(stack);
            return true;
        }
        cur_rule = c4m_list_get(nt->rules, i++, NULL);
    }

    // If we got to the end of the list, either it was an empty production,
    // in which case this is nullable (and found_any_rule is true).
    //
    // Otherwise, no rules were nullable, so the rule set
    // (non-terminal) is not nullable.
    finalize_nullable(nt, !(found_any_rule));
    c4m_list_pop(stack);

    return nt->nullable;
}

// This is true if we can skip over the ENTIRE item.
// When we're looking at a rule like:
//
// A -> . B C
// We want to check to see if we can advance the dot past B, so we need
// to know that there is SOME rule that could cause us to skip B.
//
// So we need to check that B's rules are nullable, to see it we can advance
// A's rule one dot.

static bool
is_nullable_rule(c4m_grammar_t *g, c4m_parse_rule_t *rule, c4m_list_t *stack)
{
    int n = c4m_list_len(rule->contents);

    for (int i = 0; i < n; i++) {
        c4m_pitem_t *item = c4m_list_get(rule->contents, i, NULL);

        if (!c4m_is_nullable_pitem(g, item, stack)) {
            return false;
        }

        if (item->kind == C4M_P_NT) {
            if (check_stack_for_nt(stack, item->contents.nonterm)) {
                return false;
            }

            return is_nullable_nt(g, item->contents.nonterm, stack);
        }
    }

    return false;
}

bool
c4m_is_nullable_pitem(c4m_grammar_t *grammar,
                      c4m_pitem_t   *item,
                      c4m_list_t    *stack)
{
    // There's not a tremendous value in caching nullable here; it is
    // best cached in the non-terminal node. We recompute only when
    // adding rules.

    switch (item->kind) {
    case C4M_P_NULL:
        return true;
    case C4M_P_GROUP:
        // Groups MUST not be nullable, except by specifying 0 matches.
        // We assume here this constraint is checked elsewhere.
        return item->contents.group->min == 0;
    case C4M_P_NT:
        return is_nullable_nt(grammar, item->contents.nonterm, stack);
    default:
        return false;
    }
}

c4m_nonterm_t *
c4m_get_nonterm(c4m_grammar_t *grammar, int64_t id)
{
    if (id > c4m_list_len(grammar->nt_list) || id < 0) {
        return NULL;
    }

    c4m_nonterm_t *nt = c4m_list_get(grammar->nt_list, id, NULL);

    return nt;
}

c4m_nonterm_t *
c4m_pitem_get_ruleset(c4m_grammar_t *g, c4m_pitem_t *pi)
{
    return c4m_get_nonterm(g, pi->contents.nonterm);
}

// This creates an anonymous non-terminal, and adds the contained
// pitems as a rule. This allows us to easily model grouping rhs
// pieces in parentheses. For instance:
//
// x ::= dont_repeat (foo bar)+ also_no_repeats
//
// By creating an anonymous rule (say, ANON1) and rewriting the
// grammar as:
//
// x ::= dont_repeat ANON1+ also_no_repeats
// ANON1 ::= foo bar

c4m_pitem_t *
c4m_group_items(c4m_grammar_t *g, c4m_list_t *pitems, int min, int max)
{
    c4m_rule_group_t *group = c4m_gc_alloc_mapped(c4m_rule_group_t,
                                                  C4M_GC_SCAN_ALL);

    group->gid               = c4m_rand16();
    c4m_utf8_t    *tmp_name  = c4m_cstr_format("$$group_nt_{}", group->gid);
    c4m_pitem_t   *tmp_nt_pi = c4m_pitem_nonterm_raw(g, tmp_name);
    c4m_nonterm_t *nt        = c4m_pitem_get_ruleset(g, tmp_nt_pi);

    nt->group_nt = true;

    c4m_ruleset_add_rule(g, nt, pitems, 0);

    group->contents = nt;
    group->min      = min;
    group->max      = max;

    c4m_pitem_t *result    = c4m_new_pitem(C4M_P_GROUP);
    result->contents.group = group;

    return result;
}

static const char *errstr =
    "The null string and truly empty productions are not allowed. "
    "However, you can use group match operators like '?' and '*' that "
    "may accept zero items for the same effect.";

static inline bool
pitems_eq(c4m_pitem_t *p1, c4m_pitem_t *p2)
{
    if (p1->kind != p2->kind) {
        return false;
    }

    switch (p1->kind) {
    case C4M_P_NULL:
    case C4M_P_ANY:
        return true;
    case C4M_P_TERMINAL:
        return p1->contents.terminal == p2->contents.terminal;
    case C4M_P_BI_CLASS:
        return p1->contents.class == p2->contents.class;
    case C4M_P_SET:
        if (c4m_len(p1->contents.items) != c4m_len(p2->contents.items)) {
            return false;
        }
        for (int i = 0; i < c4m_list_len(p1->contents.items); i++) {
            c4m_pitem_t *sub1 = c4m_list_get(p1->contents.items, i, NULL);
            c4m_pitem_t *sub2 = c4m_list_get(p2->contents.items, i, NULL);
            if (!pitems_eq(sub1, sub2)) {
                return false;
            }
        }
        return true;
    case C4M_P_NT:
        return p1->contents.nonterm == p2->contents.nonterm;
    case C4M_P_GROUP:
        return p1->contents.group == p2->contents.group;
    }
}

static bool
rule_exists(c4m_nonterm_t *nt, c4m_parse_rule_t *new)
{
    int n = c4m_list_len(nt->rules);
    int l = c4m_list_len(new->contents);

    for (int i = 0; i < n; i++) {
        c4m_parse_rule_t *old = c4m_list_get(nt->rules, i, NULL);
        if (c4m_list_len(old->contents) != l) {
            continue;
        }
        for (int j = 0; j < l; j++) {
            c4m_pitem_t *pi_old = c4m_list_get(old->contents, j, NULL);
            c4m_pitem_t *pi_new = c4m_list_get(new->contents, j, NULL);
            if (!pitems_eq(pi_old, pi_new)) {
                goto next;
            }
        }
        return true;

next:;
    }

    return false;
}

static c4m_parse_rule_t *
ruleset_add_rule_internal(c4m_grammar_t    *g,
                          c4m_nonterm_t    *ruleset,
                          c4m_list_t       *items,
                          int               cost,
                          c4m_parse_rule_t *penalty)
{
    if (g->finalized) {
        C4M_CRAISE("Cannot modify grammar after first parse w/o a reset.");
    }

    if (!c4m_list_len(items)) {
        C4M_CRAISE(errstr);
    }

    c4m_parse_rule_t *rule = c4m_gc_alloc_mapped(c4m_parse_rule_t,
                                                 C4M_GC_SCAN_ALL);
    rule->nt               = ruleset;
    rule->contents         = items;
    rule->cost             = cost;
    rule->penalty_rule     = penalty ? true : false;
    rule->link             = penalty;

    if (rule_exists(ruleset, rule)) {
        return NULL;
    }

    c4m_list_append(ruleset->rules, rule);
    c4m_list_append(g->rules, rule);

    return rule;
}

void
c4m_ruleset_add_rule(c4m_grammar_t *g,
                     c4m_nonterm_t *nt,
                     c4m_list_t    *items,
                     int            cost)
{
    ruleset_add_rule_internal(g, nt, items, cost, NULL);
}

static inline void
create_one_error_rule_set(c4m_grammar_t *g, int rule_ix)

{
    // We're only going to handle single-token errors in these rules for now;
    // it could be useful to create all possible error rules in many cases,
    // especially with things like matching parens, but single rules with
    // dozens of tokens would pose a problem, and dealing with that would
    // over-complicate. Single-token detection is good enough.

    c4m_parse_rule_t *cur    = c4m_list_get(g->rules, rule_ix, NULL);
    int               n      = c4m_list_len(cur->contents);
    c4m_list_t       *l      = cur->contents;
    int               tok_ct = 0;
    c4m_pitem_t      *pi;

    for (int i = 0; i < n; i++) {
        pi = c4m_list_get(l, i, NULL);

        if (c4m_is_non_terminal(pi)) {
            continue;
        }
        tok_ct++;

        c4m_utf8_t    *name   = c4m_cstr_format("$term-{}-{}-{}",
                                           cur->nt->name,
                                           rule_ix,
                                           tok_ct);
        c4m_pitem_t   *pi_err = c4m_pitem_nonterm_raw(g, name);
        c4m_nonterm_t *nt_err = c4m_pitem_get_ruleset(g, pi_err);
        c4m_list_t    *r      = c4m_list(c4m_type_ref());

        c4m_list_append(r, pi);
        c4m_parse_rule_t *ok = ruleset_add_rule_internal(g, nt_err, r, 0, NULL);

        r = c4m_list(c4m_type_ref());
        c4m_list_append(r, c4m_new_pitem(C4M_P_NULL));
        ruleset_add_rule_internal(g, nt_err, r, 0, ok);

        r = c4m_list(c4m_type_ref());
        c4m_list_append(r, c4m_new_pitem(C4M_P_ANY));
        c4m_list_append(r, pi);
        ruleset_add_rule_internal(g, nt_err, r, 0, ok);

        c4m_list_set(l, i, pi_err);
    }
}

static int
cmp_rules_for_display_ordering(c4m_parse_rule_t **p1, c4m_parse_rule_t **p2)
{
    c4m_parse_rule_t *r1 = *p1;
    c4m_parse_rule_t *r2 = *p2;

    if (r1->nt && !r2->nt) {
        return -1;
    }

    if (!r1->nt && r2->nt) {
        return 1;
    }

    if (r1->nt) {
        int name_cmp = strcmp(r1->nt->name->data, r2->nt->name->data);

        if (name_cmp) {
            if (r1->nt->start_nt) {
                return -1;
            }
            if (r2->nt->start_nt) {
                return 1;
            }

            return name_cmp;
        }
    }

    if (r1->penalty_rule != r2->penalty_rule) {
        if (r1->penalty_rule) {
            return 1;
        }
        return -1;
    }

    int l1 = c4m_list_len(r1->contents);
    int l2 = c4m_list_len(r2->contents);

    if (l1 != l2) {
        return l2 - l1;
    }

    return r1->cost - r2->cost;
}

void
c4m_prep_first_parse(c4m_grammar_t *g)
{
    if (g->finalized) {
        return;
    }

    int n = c4m_list_len(g->nt_list);

    // Calculate nullability *before* we add in error rules.
    //
    // If we see 'nullable' set in a rule, we'll proactively advance
    // the cursor without expanding it or including the penalty /
    // cost, so we don't want to do this with token ellision errors.
    //
    // Those are forced to match explicitly.

    for (int i = 0; i < n; i++) {
        is_nullable_nt(g, i, c4m_list(c4m_type_u64()));
    }

    if (g->error_rules) {
        n = c4m_list_len(g->rules);

        for (int i = 0; i < n; i++) {
            create_one_error_rule_set(g, i);
        }
    }

    c4m_nonterm_t *start = c4m_get_nonterm(g, g->default_start);
    start->start_nt      = true;

    c4m_list_sort(g->rules, (c4m_sort_fn)cmp_rules_for_display_ordering);

    g->finalized = true;
}

int64_t
c4m_token_stream_codepoints(c4m_parser_t *parser, void **token_info)
{
    c4m_utf32_t     *str = (c4m_utf32_t *)parser->user_context;
    c4m_codepoint_t *p   = (c4m_codepoint_t *)str->data;

    if (parser->position >= str->codepoints) {
        return C4M_TOK_EOF;
    }

    return (int64_t)p[parser->position];
}

// In this variation, we have already done, say, a 'lex' pass, and
// have a list of string objects that we need to match against;
// we are passed strings only, and can pass user-defined info
// if needed.
int64_t
c4m_token_stream_strings(c4m_parser_t *parser, void **token_info)
{
    c4m_list_t *list  = (c4m_list_t *)parser->user_context;
    c4m_str_t  *value = c4m_list_get(list, parser->position, NULL);

    if (!value) {
        return C4M_TOK_EOF;
    }

    // If it's a one-character string, the token ID is the codepoint.
    // doesn't matter if we registered it or not.
    if (value->codepoints == 1) {
        value = c4m_to_utf32(value);

        return ((c4m_codepoint_t *)value->data)[0];
    }

    // Next, check to see if we registered the token, meaning it is an
    // explicitly named terminal symbol somewhere in the grammar.
    //
    // Since any registered tokens are non-zero, we can test for that to
    // determine if it's registered, instead of passing a bool.
    c4m_utf8_t *u8 = c4m_to_utf8(value);
    int64_t     n  = (int64_t)hatrack_dict_get(parser->grammar->terminal_map,
                                          u8,
                                          NULL);

    parser->token_cache = u8;

    if (n) {
        return (int64_t)n;
    }

    return C4M_TOK_OTHER;
}

static inline int
count_newlines(c4m_parser_t *parser, c4m_str_t *tok_value, int *last_index)
{
    c4m_utf32_t     *v      = c4m_to_utf32(tok_value);
    c4m_codepoint_t *p      = (c4m_codepoint_t *)v->data;
    int              result = 0;

    for (int i = 0; i < v->codepoints; i++) {
        switch (p[i]) {
        case '\\':
            if (parser->ignore_escaped_newlines) {
                i++;
                continue;
            }
            else {
                continue;
            }
        case '\n':
            *last_index = i;
            result++;
        }
    }

    return result;
}

void
c4m_add_debug_highlight(c4m_parser_t *parser, int32_t eid, int32_t ix)
{
    if (!parser->debug_highlights) {
        parser->debug_highlights = c4m_dict(c4m_type_int(),
                                            c4m_type_set(c4m_type_int()));
    }

    int64_t key   = eid;
    int64_t value = ix;

    c4m_set_t *s = hatrack_dict_get(parser->debug_highlights,
                                    (void *)key,
                                    NULL);

    if (!s) {
        s = c4m_set(c4m_type_int());
        hatrack_dict_put(parser->debug_highlights, (void *)key, s);
    }

    hatrack_set_put(s, (void *)value);
}

void
c4m_parser_load_token(c4m_parser_t *parser)
{
    c4m_earley_state_t *state = parser->current_state;

    // Load the token for the *current* state; do NOT advance the
    // state. The current state object is expected to be initialized.
    //
    // If we were handed a list of c4m_token_info_t objects instead of
    // a callback, then we load info from a list store in token_cache,
    // reusing those token objects, until / unless we need to add an
    // EOF.

    if (parser->preloaded_tokens) {
        c4m_list_t *toks = (c4m_list_t *)parser->token_cache;
        bool        err;

        state->token = c4m_list_get(toks, parser->position, &err);

        if (err) {
            state->token = c4m_gc_alloc_mapped(c4m_token_info_t,
                                               C4M_GC_SCAN_ALL);

            state->token->tid = C4M_TOK_EOF;
        }

        state->token->index = state->id;

        return;
    }

    // In this branch, we don't have a pre-existing list of tokens. We
    // are either processing a single string as characters, or
    // processing a list of strings.
    //
    // We get the next token generically across those via an internal
    // callback.  Unlike the above case, the token_cache is used to
    // stash the literal value when needed.

    c4m_token_info_t *tok = c4m_gc_alloc_mapped(c4m_token_info_t,
                                                C4M_GC_SCAN_ALL);

    state->token = tok;

    // Clear this to make sure we don't accidentally reuse.
    parser->token_cache = NULL;
    tok->tid            = (*parser->tokenizer)(parser, &tok->user_info);
    tok->line           = parser->current_line;
    tok->column         = parser->current_column;
    tok->index          = state->id;

    if (tok->tid == C4M_TOK_EOF) {
        return;
    }
    if (tok->tid == C4M_TOK_OTHER) {
        tok->value = parser->token_cache;
    }
    else {
        if (tok->tid < C4M_START_TOK_ID) {
            tok->value = c4m_utf8_repeat(tok->tid, 1);
        }
        else {
            c4m_terminal_t *ti = c4m_list_get(parser->grammar->named_terms,
                                              tok->tid - C4M_START_TOK_ID,
                                              0);
            if (ti->value) {
                tok->value = ti->value;
            }
            else {
                tok->value = parser->token_cache;
            }
        }
    }

    // If we have no value string associated with the token, then we
    // assume it's an invisible token of some sort, and are done.
    if (!tok->value || !tok->value->codepoints) {
        return;
    }

    // Otherwise, we assume the value should be used in advancing the
    // line / column info we keep.
    //
    // If the token has newlines in it, we advance the line count, and
    // base the column count on whatever was after the last newline.

    int last_nl_ix;
    int len      = c4m_str_codepoint_len(tok->value);
    int nl_count = count_newlines(parser, tok->value, &last_nl_ix);

    if (nl_count) {
        parser->current_line += nl_count;
        parser->current_column = len - last_nl_ix + 1;
    }
    else {
        parser->current_column += len;
    }
}

const c4m_vtable_t c4m_parser_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)c4m_parser_init,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)C4M_GC_SCAN_ALL,
    },
};
const c4m_vtable_t c4m_grammar_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)c4m_grammar_init,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)C4M_GC_SCAN_ALL,
    },
};

const c4m_vtable_t c4m_terminal_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)c4m_terminal_init,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)C4M_GC_SCAN_ALL,
    },
};

const c4m_vtable_t c4m_nonterm_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)c4m_nonterm_init,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)C4M_GC_SCAN_ALL,
    },
};

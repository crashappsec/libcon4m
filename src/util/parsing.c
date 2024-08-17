#define C4M_USE_INTERNAL_API
#define GROUP_ID 1 << 26
#include "con4m.h"

#if defined(C4M_EARLEY_DEBUG_VERBOSE) && !defined(C4M_EARLEY_DEBUG)
#define C4M_EARLEY_DEBUG
static c4m_grid_t *get_parse_state(c4m_parser_t *parser, bool next);
#endif

static c4m_utf8_t *repr_rule(c4m_grammar_t *, c4m_list_t *, int);
static c4m_list_t *repr_earley_item(c4m_parser_t      *parser,
                                    c4m_earley_item_t *cur,
                                    int                id);
static c4m_utf8_t *repr_nonterm(c4m_grammar_t *grammar, int64_t id);

static inline c4m_earley_state_t *
new_earley_state(int id)
{
    c4m_earley_state_t *result;

    result = c4m_gc_alloc_mapped(c4m_earley_state_t, C4M_GC_SCAN_ALL);

    // List of c4m_earley_item_t objects.
    result->items = c4m_list(c4m_type_ref());
    result->id    = id;

    return result;
}

static inline void
internal_reset(c4m_parser_t *parser)
{
    parser->states           = c4m_list(c4m_type_ref());
    parser->position         = 0;
    parser->start            = -1; // Use default start.
    parser->current_state    = new_earley_state(0);
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
    internal_reset(parser);
}

static void
c4m_forest_init(c4m_forest_item_t *f, va_list args)
{
    f->kind = va_arg(args, c4m_forest_node_kind);
    switch (f->kind) {
    case C4M_FOREST_ROOT:
    case C4M_FOREST_NONTERM:
    case C4M_FOREST_GROUP_TOP:
    case C4M_FOREST_GROUP_ITEM:
        f->kids = c4m_list(c4m_type_forest());
        break;
    default:
        break;
    }
}

static void
c4m_grammar_init(c4m_grammar_t *grammar, va_list args)
{
    grammar->named_terms  = c4m_list(c4m_type_terminal());
    grammar->rules        = c4m_list(c4m_type_ruleset());
    grammar->rule_map     = c4m_dict(c4m_type_utf8(), c4m_type_u64());
    grammar->terminal_map = c4m_dict(c4m_type_utf8(), c4m_type_u64());
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
                hatrack_dict_add(grammar->terminal_map, terminal->value, (void *)n);
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
    // don't add to the dictionary, but we do add to the list.

    bool           found;
    c4m_utf8_t    *err;
    int64_t        n;
    c4m_grammar_t *grammar = va_arg(args, c4m_grammar_t *);
    nonterm->name          = va_arg(args, c4m_utf8_t *);
    nonterm->rules         = c4m_list(c4m_type_ref()); // c4m_pitem_t is internal.

    if (nonterm->name == NULL) {
        // Anonymous / inline rule.
        n = c4m_list_len(grammar->rules);
        c4m_list_append(grammar->rules, nonterm);
        while (true) {
            c4m_nonterm_t *nt = c4m_list_get(grammar->rules, n, NULL);

            if (nt == nonterm) {
                nonterm->id = n;
                return;
            }

            n++;
        }
    }

    hatrack_dict_get(grammar->rule_map, nonterm->name, &found);

    if (found) {
bail:
        err = c4m_cstr_format("Duplicate ruleset name: [em]{}[/]",
                              nonterm->name);
        C4M_RAISE(err);
    }

    n = c4m_list_len(grammar->rules);
    c4m_list_append(grammar->rules, nonterm);

    // If there's a race condition, we may not get the slot we
    // think we're getting. And it's slightly possible we might
    // add the same terminal twice in parallel, in which case we
    // accept there might be a redundant item in the list.
    while (true) {
        c4m_nonterm_t *nt = c4m_list_get(grammar->rules, n, NULL);

        if (nt == nonterm) {
            hatrack_dict_add(grammar->rule_map, nonterm->name, (void *)n);
            nonterm->id = n;

            return;
        }
        if (c4m_str_eq(nt->name, nonterm->name)) {
            goto bail;
        }

        n++;
    }
}

// First, the core parsing algorithm needs to know when rules can
// match the empty string (i.e., be nullable), so we can determine if
// parse rules can match the empty string.
//
// An individual production in a rule set is nullable if every
// component in it is nullable.
//
// We explicitly skip over direct recursion, and detect indirect
// recursion.  In such cases, the correct thing to do is skip.
//
// Consider a production like:
//
// A ::= B A | 'eof'
//
// Certainly there could be a subrule that doesn't recurse may
// be nullable. But in this case, the rule basically says, "A
// is a list of B's, followed by 'eof'". For this directly
// recursive rule, the 'eof' branch is not nullable, but the
// left branch is obviously nullable iff B is
// nullable. Recursing on A does not give us any additional
// information.

static bool c4m_is_nullable_pitem(c4m_grammar_t *, c4m_pitem_t *, c4m_list_t *);
static bool
c4m_calculate_is_nullable_nt(c4m_grammar_t *,
                             int64_t,
                             c4m_list_t *);

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

static bool
c4m_is_nullable_rule(c4m_grammar_t *grammar,
                     c4m_list_t    *rule,
                     c4m_list_t    *stack)
{
    c4m_pitem_t *item = c4m_list_get(rule, 0, NULL);

    if (!c4m_is_nullable_pitem(grammar, item, stack)) {
        if (item->kind == C4M_P_NT) {
            if (check_stack_for_nt(stack, item->contents.nonterm)) {
                return false;
            }

            // If it's  a NT, we need to update judgement on the sub-item.
            return c4m_calculate_is_nullable_nt(grammar,
                                                item->contents.nonterm,
                                                stack);
        }
    }
    return false;
}

static c4m_nonterm_t *
get_nonterm(c4m_grammar_t *grammar, int64_t id)
{
    c4m_nonterm_t *nt = c4m_list_get(grammar->rules, id, NULL);

    if (!nt) {
        C4M_RAISE(c4m_cstr_format(
            "Invalid parse ruleset ID [em]{}[/] found (mixing "
            "rules from a different grammar?",
            id));
    }

    return nt;
}

c4m_nonterm_t *
c4m_pitem_get_ruleset(c4m_grammar_t *g, c4m_pitem_t *pi)
{
    return get_nonterm(g, pi->contents.nonterm);
}

// When we add a new subrule, we go ahead and re-compute nullability for
// any used non-terminals. That's not optimal, but whatever.
static bool
c4m_calculate_is_nullable_nt(c4m_grammar_t *grammar,
                             int64_t        nt_id,
                             c4m_list_t    *stack)
{
    // Per above, do not allow ourselves to recurse. We simply claim
    // to be nullable in that case.
    if (stack && check_stack_for_nt(stack, nt_id)) {
        return true;
    }
    else {
        c4m_list_append(stack, (void *)nt_id);
    }

    // A non-terminal is nullable if any of its individual rules are
    // nullable.
    c4m_list_t    *cur_rule;
    bool           found_any_rule = false;
    int64_t        i              = 0;
    c4m_nonterm_t *nt             = get_nonterm(grammar, nt_id);

    cur_rule = c4m_list_get(nt->rules, i++, NULL);

    while (cur_rule) {
        found_any_rule = true;
        if (c4m_is_nullable_rule(grammar, cur_rule, stack)) {
            nt->nullable = true;
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
    nt->nullable = !(found_any_rule);
    c4m_list_pop(stack);

    return nt->nullable;
}

static bool
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
        if (item->contents.group->min == 0) {
            return true;
        }
        return c4m_is_nullable_rule(grammar,
                                    item->contents.group->items,
                                    stack);
    case C4M_P_NT:
        return c4m_calculate_is_nullable_nt(grammar,
                                            item->contents.nonterm,
                                            stack);
    default:
        return false;
    }
}

static inline c4m_pitem_t *
new_pitem(c4m_pitem_kind kind)
{
    c4m_pitem_t *result = c4m_gc_alloc_mapped(c4m_pitem_t, C4M_GC_SCAN_ALL);

    result->kind = kind;

    return result;
}

c4m_pitem_t *
c4m_pitem_terminal_raw(c4m_grammar_t *p, c4m_utf8_t *name)
{
    c4m_pitem_t    *result    = new_pitem(C4M_P_TERMINAL);
    c4m_terminal_t *tok       = c4m_new(c4m_type_terminal(), p, name);
    result->contents.terminal = tok->id;

    return result;
}

c4m_pitem_t *
c4m_pitem_terminal_cp(c4m_codepoint_t cp)
{
    c4m_pitem_t *result       = new_pitem(C4M_P_TERMINAL);
    result->contents.terminal = cp;

    return result;
}

c4m_pitem_t *
c4m_pitem_nonterm_raw(c4m_grammar_t *p, c4m_utf8_t *name)
{
    c4m_pitem_t   *result    = new_pitem(C4M_P_NT);
    c4m_nonterm_t *nt        = c4m_new(c4m_type_ruleset(), p, name);
    result->contents.nonterm = nt->id;

    return result;
}

c4m_pitem_t *
c4m_pitem_choice_raw(c4m_grammar_t *p, c4m_list_t *choices)
{
    c4m_pitem_t *result    = new_pitem(C4M_P_SET);
    result->contents.items = choices;

    return result;
}

c4m_pitem_t *
c4m_pitem_any_terminal_raw(c4m_grammar_t *p)
{
    c4m_pitem_t *result = new_pitem(C4M_P_ANY);

    return result;
}

c4m_pitem_t *
c4m_pitem_builtin_raw(c4m_bi_class_t class)
{
    // Builtin character classes.
    c4m_pitem_t *result    = new_pitem(C4M_P_BI_CLASS);
    result->contents.class = class;

    return result;
}

void
c4m_ruleset_add_empty_rule(c4m_nonterm_t *nonterm)
{
    // Not bothering to check for dupes.
    c4m_pitem_t *item = new_pitem(C4M_P_NULL);
    c4m_list_append(nonterm->rules, item);

    // The empty rule always makes the whole production nullable, no
    // need to recurse.
    nonterm->nullable = true;
}

void
c4m_grammar_set_default_start(c4m_grammar_t *grammar, c4m_nonterm_t *nt)
{
    grammar->default_start = nt->id;
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
c4m_group_items(c4m_grammar_t *p, c4m_list_t *pitems, int min, int max)
{
    c4m_rule_group_t *group = c4m_gc_alloc_mapped(c4m_rule_group_t,
                                                  C4M_GC_SCAN_ALL);

    group->items = pitems;
    group->min   = min;
    group->max   = max;

    c4m_pitem_t *result    = new_pitem(C4M_P_GROUP);
    result->contents.group = group;

    return result;
}

void
c4m_ruleset_add_rule(c4m_grammar_t *g, c4m_nonterm_t *ruleset, c4m_list_t *items)
{
    c4m_list_append(ruleset->rules, items);

    c4m_calculate_is_nullable_nt(g, ruleset->id, c4m_list(c4m_type_u64()));
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

static void
parser_load_token(c4m_parser_t *parser)
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
        bool        found;

        state->token = c4m_list_get(toks, parser->position, &found);

        if (!found) {
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
    // line / column info we keep. This is basically calculating the
    // END position of the token, which we will insert into the next
    // token.
    int last_nl_ix;
    int nl_count = count_newlines(parser, tok->value, &last_nl_ix);

    if (nl_count) {
        parser->current_line += nl_count;
        parser->current_column = c4m_str_codepoint_len(tok->value - (last_nl_ix + 1));
    }
    else {
        parser->current_column += tok->value->codepoints;
    }
}

static inline bool
are_dupes(c4m_earley_item_t *old, c4m_earley_item_t *new)
{
    if (old->ruleset_id != new->ruleset_id) {
        return false;
    }

    if (old->rule_index != new->rule_index) {
        return false;
    }

    if (old->cursor != new->cursor) {
        return false;
    }

    if (old->match_ct != new->match_ct) {
        return false;
    }

    if (old->ginfo != new->ginfo) {
        return false;
    }

    return true;
}

static inline void
merge_parents(c4m_earley_item_t *keeper, c4m_earley_item_t *tmpone)
{
    int n = c4m_list_len(tmpone->possible_parents);

    for (int i = 0; i < n; i++) {
        c4m_earley_item_t *c = c4m_list_get(tmpone->possible_parents, i, NULL);
        if (c != keeper) {
            c4m_list_append(keeper->possible_parents, c);
        }
    }
}

static bool
add_item_if_not_dupe(c4m_parser_t       *p,
                     c4m_earley_state_t *state,
                     c4m_earley_item_t *new)
{
    int n = c4m_list_len(state->items);

    new->estate_id   = state->id;
    new->eitem_index = n;

    for (int i = 0; i < n; i++) {
        c4m_earley_item_t *old = c4m_list_get(state->items, i, NULL);

        if (are_dupes(old, new)) {
#if defined(C4M_EARLEY_DEBUG_VERBOSE)
            c4m_printf("[h5]Actually no, it's a dupe.");
#endif
            merge_parents(old, new);
            return false;
        }
    }

    c4m_list_append(state->items, new);
#if defined(C4M_EARLEY_DEBUG_VERBOSE)
    if (state == p->current_state) {
        c4m_printf("[h2]Appended to current state ({}), which is now:[/]\n",
                   p->position);
        c4m_print(get_parse_state(p, false));
    }
    else {
        c4m_printf("[h2]Appended to next state ({}), which is now:[/]\n",
                   p->position + 1);
        c4m_print(get_parse_state(p, true));
    }
#endif

    return true;
}

static c4m_earley_item_t *
new_earley_item(void)
{
    c4m_earley_item_t *res = c4m_gc_alloc_mapped(c4m_earley_item_t,
                                                 C4M_GC_SCAN_ALL);

    return res;
}

// For scans and completions we want to mostly keep the state from the
// earley item that spawned us.
static c4m_earley_item_t *
copy_earley_item(c4m_parser_t *p, c4m_earley_item_t *old, bool bump_cursor)
{
    c4m_earley_item_t *new = new_earley_item();
    new->start_item        = old->start_item;
    new->start_state       = old->start_state;
    new->rule              = old->rule;
    new->ruleset_id        = old->ruleset_id;
    new->rule_index        = old->rule_index;
    new->cursor            = old->cursor;
    new->ginfo             = old->ginfo;
    new->match_ct          = old->match_ct;

    if (bump_cursor) {
        new->cursor++;
        new->previous_scan = old;
    }
    else {
        new->previous_scan = old->previous_scan;
    }

    return new;
}

// Only returns non-null if n is 1, in which case it returns the
// earley item it created. This is because for groups only, we want to
// populate the `group` field, and it felt slightly less messy than
// the extra parameter.
static c4m_earley_item_t *
base_add_ruleset(c4m_parser_t      *p,
                 int64_t            id,
                 c4m_list_t       **rules,
                 int64_t            n,
                 c4m_earley_item_t *parent_node)
{
    c4m_earley_state_t *s = p->current_state;

    for (int64_t i = 0; i < n; i++) {
        c4m_earley_item_t *item = new_earley_item();
        item->ruleset_id        = id;
        item->rule_index        = i;
        item->rule              = rules[i];
        item->match_ct          = 0;
        item->start_state       = s;
        item->start_item        = item;
        item->possible_parents  = c4m_list(c4m_type_ref());
        item->creation_item     = parent_node;

        if (parent_node && parent_node != item) {
            c4m_list_append(item->possible_parents, parent_node);
        }

        add_item_if_not_dupe(p, s, item);
        if (n == 1) {
            // Meant for groups; non-group just ignores.
            return item;
        }
    }
    return NULL;
}

static inline void
add_ruleset_to_state(c4m_parser_t      *p,
                     c4m_nonterm_t     *nt,
                     c4m_earley_item_t *parent_node)
{
    int64_t      n;
    c4m_list_t **rules = c4m_list_view(nt->rules, (uint64_t *)&n);

    base_add_ruleset(p, nt->id, rules, n, parent_node);
}

static void
add_group_to_state(c4m_parser_t      *p,
                   c4m_rule_group_t  *group,
                   c4m_earley_item_t *from_node)
{
    c4m_earley_item_t *item;

    item        = base_add_ruleset(p, GROUP_ID, &group->items, 1, from_node);
    item->ginfo = c4m_gc_alloc_mapped(c4m_egroup_info_t, C4M_GC_SCAN_ALL);

    item->ginfo->group           = group;
    item->ginfo->prev_item_start = NULL;
    item->ginfo->prev_item_end   = NULL;
    item->ginfo->true_start      = item;
}

static void
enter_next_state(c4m_parser_t *parser)
{
    // If next_state is NULL, then we skip advancing the state, because
    // we're really going from -1 into 0.
    if (parser->next_state != NULL) {
        parser->position++;
        parser->current_state = parser->next_state;
    }
    else {
        // Load the start ruleset.
        c4m_grammar_t *grammar = parser->grammar;
        int            ix      = parser->start;
        c4m_nonterm_t *start   = get_nonterm(grammar, ix);

        parser->cur_item_index = C4M_IX_START_OF_PROGRAM;
        add_ruleset_to_state(parser, start, NULL);
    }

    parser->next_state = new_earley_state(c4m_list_len(parser->states));
    c4m_list_append(parser->states, parser->next_state);

    parser_load_token(parser);
}

static inline bool
is_nullable(c4m_parser_t *parser, int64_t nonterm_id)
{
    c4m_nonterm_t *nt = get_nonterm(parser->grammar, nonterm_id);

    return nt->nullable;
}

static void
try_nullable_prediction(c4m_parser_t      *parser,
                        c4m_earley_item_t *old,
                        c4m_pitem_t       *next)
{
    if (next->kind != C4M_P_NT) {
        return;
    }
    if (!is_nullable(parser, next->contents.nonterm)) {
        return;
    }

    c4m_earley_item_t *new = copy_earley_item(parser, old, true);
    new->creation_item     = old;
    add_item_if_not_dupe(parser, parser->current_state, new);
}

static inline bool
is_non_terminal(c4m_pitem_t *pitem)
{
    switch (pitem->kind) {
    case C4M_P_NT:
    case C4M_P_GROUP:
        return true;
    default:
        return false;
    }
}

static void
non_nullable_prediction(c4m_parser_t      *parser,
                        c4m_earley_item_t *old,
                        c4m_pitem_t       *next)
{
    if (next->kind == C4M_P_NULL) {
        return;
    }

    if (next->kind == C4M_P_GROUP) {
#if defined(C4M_EARLEY_DEBUG_VERBOSE)
        c4m_printf("[i]Predict a new group.");
#endif
        add_group_to_state(parser, next->contents.group, old);
    }

    else {
        c4m_nonterm_t *nt = get_nonterm(parser->grammar, next->contents.nonterm);
#if defined(C4M_EARLEY_DEBUG_VERBOSE)
        c4m_printf("[i]Predict {}.",
                   repr_nonterm(parser->grammar, next->contents.nonterm));
#endif
        add_ruleset_to_state(parser, nt, old);
    }
}

static bool
char_in_class(c4m_codepoint_t cp, c4m_bi_class_t class)
{
    if (!c4m_codepoint_valid(cp)) {
        return false;
    }

    switch (class) {
    case C4M_P_BIC_ID_START:
        return c4m_codepoint_is_id_start(cp);
        break;
    case C4M_P_BIC_C4M_ID_START:
        return c4m_codepoint_is_c4m_id_start(cp);
        break;
    case C4M_P_BIC_ID_CONTINUE:
        return c4m_codepoint_is_id_continue(cp);
        break;
    case C4M_P_BIC_C4M_ID_CONTINUE:
        return c4m_codepoint_is_c4m_id_continue(cp);
        break;
    case C4M_P_BIC_DIGIT:
        return c4m_codepoint_is_ascii_digit(cp);
        break;
    case C4M_P_BIC_ANY_DIGIT:
        return c4m_codepoint_is_unicode_digit(cp);
        break;
    case C4M_P_BIC_UPPER:
        return c4m_codepoint_is_unicode_upper(cp);
        break;
    case C4M_P_BIC_UPPER_ASCII:
        return c4m_codepoint_is_ascii_upper(cp);
        break;
    case C4M_P_BIC_LOWER:
        return c4m_codepoint_is_unicode_lower(cp);
        break;
    case C4M_P_BIC_LOWER_ASCII:
        return c4m_codepoint_is_ascii_lower(cp);
        break;
    case C4M_P_BIC_SPACE:
        return c4m_codepoint_is_space(cp);
        break;
    }

    c4m_unreachable();
}
static inline bool
can_scan_one(c4m_codepoint_t cp, c4m_pitem_t *to_match)
{
    switch (to_match->kind) {
    case C4M_P_ANY:
        return true;
    case C4M_P_GROUP:
        // Groups are effectively anonymous non-terminals; they

        // get predicted first, then their pieces scanned.
        return false;
    case C4M_P_TERMINAL:
        return cp == (int32_t)to_match->contents.terminal;
    case C4M_P_BI_CLASS:
        return char_in_class(cp, to_match->contents.class);
    default:
        c4m_unreachable();
    }
}

static void
scan(c4m_parser_t *parser, c4m_earley_item_t *eitem, c4m_pitem_t *to_match)
{
    c4m_pitem_t *sub;
    switch (to_match->kind) {
    case C4M_P_ANY:
        break;
    case C4M_P_TERMINAL:
    case C4M_P_BI_CLASS:
        if (can_scan_one(parser->current_state->token->tid, to_match)) {
            break;
        }
#if defined(C4M_EARLEY_DEBUG_VERBOSE)
        c4m_printf("[i]SCAN FAILED.");
#endif
        return;
    case C4M_P_GROUP:
#if defined(C4M_EARLEY_DEBUG_VERBOSE)
        c4m_printf("[i]SCAN FAILED.");
#endif
        return;
    case C4M_P_SET:
        for (int i = 0; i < c4m_list_len(to_match->contents.items); i++) {
            sub = c4m_list_get(to_match->contents.items, i, NULL);

            switch (sub->kind) {
            case C4M_P_TERMINAL:
            case C4M_P_ANY:
            case C4M_P_BI_CLASS:
                if (can_scan_one(parser->current_state->token->tid, sub)) {
                    goto success;
                }
                continue;
            default:
                C4M_CRAISE("Invalid item in custom character class.");
            }
            break;
        }
#if defined(C4M_EARLEY_DEBUG_VERBOSE)
        c4m_printf("[i]SCAN FAILED.");
#endif
        return;
    default:
        c4m_unreachable();
    }

success:;
    // Now create the successor state.
    c4m_earley_item_t *copy = copy_earley_item(parser, eitem, true);
    copy->creation_item     = eitem;

#if defined(C4M_EARLEY_DEBUG_VERBOSE)
    c4m_printf("[em]Successful scan; add successor rule.");
#endif
    add_item_if_not_dupe(parser, parser->next_state, copy);
}
static void
cascade_completions(c4m_parser_t      *parser,
                    c4m_earley_item_t *end_item,
                    c4m_earley_item_t *creator)
{
    c4m_list_t        *parents = end_item->start_item->possible_parents;
    c4m_earley_item_t *parent;
    int                n = c4m_list_len(parents);

    for (int i = 0; i < n; i++) {
        parent = c4m_list_get(parents, i, NULL);
        if (!parent) {
            continue;
        }
        // Copy the parent. Then, advance the dot and add to our current state.
        parent                = copy_earley_item(parser, parent, true);
        parent->creation_item = creator;
        add_item_if_not_dupe(parser, parser->current_state, parent);
    }
}

static void
complete_group(c4m_parser_t *parser, c4m_earley_item_t *eitem)
{
    c4m_earley_item_t *copy;
    c4m_rule_group_t  *group = eitem->ginfo->group;

    eitem->match_ct++;

    // Assuming we haven't hit the max instances for the group, we
    // predict a new successor of the group items. Later, if we've hit
    // the minimum, we will also complete the group.

    // If max is 0 / -1 it means 'no max'.
    if (group->max <= 0 || group->max > eitem->match_ct) {
        // False since we want to reset the location to the start of rule
        copy                          = copy_earley_item(parser, eitem, false);
        copy->start_item              = copy;
        copy->cursor                  = 0;
        copy->ginfo                   = c4m_gc_alloc_mapped(c4m_egroup_info_t,
                                          C4M_GC_SCAN_ALL);
        copy->ginfo->true_start       = eitem->ginfo->true_start;
        copy->ginfo->prev_item_end    = eitem;
        copy->ginfo->prev_item_start  = eitem->start_item;
        eitem->ginfo->next_item_start = copy;
        copy->ginfo->group            = group;
        copy->creation_item           = eitem;

#if defined(C4M_EARLEY_DEBUG_VERBOSE)
        c4m_printf("[em]Adding item for possible next iteration.");
#endif
        add_item_if_not_dupe(parser, parser->current_state, copy);
    }

    // If we haven't hit the minimum number of iterations for this
    // rule, then we don't have a full completion of the group,
    // just a partial one, so don't add the full completion.
    if (eitem->ginfo->group->min > eitem->match_ct) {
#if defined(C4M_EARLEY_DEBUG_VERBOSE)
        c4m_printf("[i]Cannot complete; minimum not met.");
#endif
        return;
    }

#if defined(C4M_EARLEY_DEBUG_VERBOSE)
    c4m_printf("[em]Add the group successor.");
#endif
    cascade_completions(parser, eitem->ginfo->true_start, eitem);
}

static inline void
complete(c4m_parser_t *parser, c4m_earley_item_t *eitem)
{
    // If it's a grouping where there's a min / max associated with
    // it, we want to only push the same rule if we haven't hit the
    // minimum.
    //
    // And, if haven't hit the max, we re-push our rule from
    // the beginning.

    if (eitem->ginfo != NULL) {
        complete_group(parser, eitem);
        return;
    }

    cascade_completions(parser, eitem, eitem);
}

static void
process_current_state(c4m_parser_t *parser)
{
    int                i     = 0;
    c4m_earley_item_t *eitem = c4m_list_get(parser->current_state->items,
                                            i,
                                            NULL);
#if defined(C4M_EARLEY_DEBUG)
    c4m_token_info_t *tok = parser->current_state->token;
    c4m_printf("[h4]Entering state {} w/ {} starting items; tok = [b u i]{}",
               parser->position,
               c4m_list_len(parser->current_state->items),
               tok ? tok->value : c4m_new_utf8("n/a"));
    c4m_print(get_parse_state(parser, false));
#endif

    // We expect to add to the list while processing it.
    while (eitem != NULL) {
#if defined(C4M_EARLEY_DEBUG_VERBOSE)
        c4m_printf("Process item {}: {}",
                   i,
                   repr_rule(parser->grammar,
                             eitem->rule,
                             eitem->cursor));
#endif
        parser->cur_item_index = i;

        int item_len    = c4m_list_len(eitem->rule);
        int loc_in_rule = eitem->cursor;

        if (item_len > loc_in_rule) {
            c4m_pitem_t *next = c4m_list_get(eitem->rule, loc_in_rule, NULL);
            if (is_non_terminal(next)) {
#if defined(C4M_EARLEY_DEBUG_VERBOSE)
                c4m_printf("[h5]Action is: Predict.");
#endif
                eitem->op = C4M_EO_PREDICT;
                try_nullable_prediction(parser, eitem, next);
                non_nullable_prediction(parser, eitem, next);
            }
            else {
#if defined(C4M_EARLEY_DEBUG_VERBOSE)
                c4m_printf("[h5]Action is: Scan for [em]{}",
                           parser->current_state->token->value);
#endif
                eitem->op = C4M_EO_SCAN;
                scan(parser, eitem, next);
            }
        }
        else {
#if defined(C4M_EARLEY_DEBUG_VERBOSE)
            c4m_printf("[h5]Action is: Complete.");
#endif
            eitem->op = C4M_EO_COMPLETE;
            complete(parser, eitem);
        }

        eitem = c4m_list_get(parser->current_state->items, ++i, NULL);
    }
}

static void
run_parsing_mainloop(c4m_parser_t *parser)
{
    do {
        enter_next_state(parser);
        process_current_state(parser);
#if defined(C4M_EARLEY_DEBUG)
        c4m_printf("[h4]At the end of processing this state:");
        c4m_print(get_parse_state(parser, false));
#endif
    } while (parser->current_state->token->tid != C4M_TOK_EOF);
}

static void
internal_parse(c4m_parser_t *parser, c4m_nonterm_t *start)
{
    if (!parser->grammar) {
        C4M_CRAISE("Call to parse before grammar is set.");
    }

    if (parser->run) {
        C4M_CRAISE("Attempt to re-run parser w/o resetting state.");
    }

    if (!parser->tokenizer && !parser->token_cache) {
        C4M_CRAISE("Call to parse without associating tokens.");
    }

    if (start != NULL) {
        parser->start = start->id;
    }
    else {
        parser->start = parser->grammar->default_start;
    }

    run_parsing_mainloop(parser);
}

static c4m_utf8_t *
repr_term(c4m_grammar_t *grammar, int64_t id)
{
    bool found;

    if (id < C4M_START_TOK_ID) {
        if (c4m_codepoint_is_printable(id)) {
            return c4m_cstr_format("'{}'",
                                   c4m_utf8_repeat((c4m_codepoint_t)id, 1));
        }
        return c4m_cstr_format("'U+{:h}'", id);
    }

    id -= C4M_START_TOK_ID;
    c4m_terminal_t *t = c4m_list_get(grammar->named_terms, id, &found);

    if (!found) {
        C4M_CRAISE(
            "Invalid parse token ID found (mixing "
            "rules from a different grammar?");
    }

    return c4m_cstr_format("'{}'", t->value);
}

static c4m_utf8_t *
repr_nonterm(c4m_grammar_t *grammar, int64_t id)
{
    if (id == GROUP_ID) {
        return c4m_cstr_format("[em]»group«");
    }

    c4m_nonterm_t *nt   = get_nonterm(grammar, id);
    c4m_utf8_t    *null = nt->nullable ? c4m_new_utf8("∅")
                                       : c4m_new_utf8("");

    if (id == grammar->default_start) {
        return c4m_cstr_format("{}[yellow i]{}", null, nt->name);
    }
    return c4m_cstr_format("{}[em]{}", null, nt->name);
}

static c4m_utf8_t *
repr_group(c4m_grammar_t *g, c4m_rule_group_t *group)
{
    c4m_utf8_t *base;

    base = repr_rule(g, group->items, -1);

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
        return repr_group(g, item->contents.group);
    case C4M_P_NT:
        return repr_nonterm(g, item->contents.nonterm);
    case C4M_P_TERMINAL:
        return repr_term(g, item->contents.terminal);
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

static c4m_utf8_t *
repr_rule(c4m_grammar_t *g, c4m_list_t *items, int dot_location)
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

// This returns a list of table elements.
static c4m_list_t *
repr_earley_item(c4m_parser_t *parser, c4m_earley_item_t *cur, int id)
{
    c4m_list_t    *result = c4m_list(c4m_type_utf8());
    c4m_grammar_t *g      = parser->grammar;

    c4m_list_append(result, c4m_str_from_int(id));

    c4m_utf8_t *nt   = repr_nonterm(g, cur->ruleset_id);
    c4m_utf8_t *rule = repr_rule(g, cur->rule, cur->cursor);
    c4m_utf8_t *full = c4m_cstr_format("{} ⟶  {}", nt, rule);

    c4m_list_append(result, full);

    if (cur->ginfo) {
        c4m_list_append(result, c4m_str_from_int(cur->match_ct));
    }
    else {
        c4m_list_append(result, c4m_new_utf8(" "));
    }

    c4m_utf8_t *links;

    if (cur->creation_item != NULL) {
        links = c4m_cstr_format("[i]Creator:[/] {}:{} ",
                                cur->creation_item->estate_id,
                                cur->creation_item->eitem_index);
    }
    else {
        links = c4m_rich_lit("[i]Creator:[/] «Root» ");
    }

    c4m_list_t *pp = cur->start_item->possible_parents;

    if (pp) {
        c4m_list_t *rents = c4m_list(c4m_type_utf8());
        int         n     = c4m_list_len(pp);

        for (int i = 0; i < n; i++) {
            c4m_earley_item_t *o = c4m_list_get(pp, i, NULL);
            c4m_utf8_t        *s;

            if (o == NULL) {
                s = c4m_new_utf8("«Root»");
            }
            else {
                s = c4m_cstr_format("{}:{}", o->estate_id, o->eitem_index);
            }
            c4m_list_append(rents, s);
        }

        c4m_utf8_t *rent_str;

        if (n == 0) {
            rent_str = c4m_new_utf8("«Root»");
        }
        else {
            rent_str = c4m_str_join(rents, c4m_new_utf8(", "));
        }

        rent_str = c4m_cstr_format("[i]Parents:[/] {}", rent_str);

        links = c4m_str_concat(links, rent_str);
    }

    if (cur->ginfo && cur->ginfo->prev_item_start != NULL) {
        c4m_utf8_t *prev;

        prev  = c4m_cstr_format("[i]Prev item:[/] {}:{}",
                               cur->ginfo->prev_item_start->estate_id,
                               cur->ginfo->prev_item_start->eitem_index);
        links = c4m_str_concat(links, prev);
    }

    if (cur->previous_scan) {
        c4m_utf8_t *scan;
        scan  = c4m_cstr_format("[i]Prior scan:[/] {}:{} ",
                               cur->previous_scan->estate_id,
                               cur->previous_scan->eitem_index);
        links = c4m_str_concat(scan, links);
    }

    c4m_list_append(result, c4m_to_utf8(links));

    switch (cur->op) {
    case C4M_EO_PREDICT:
        c4m_list_append(result, c4m_new_utf8("P"));
        break;
    case C4M_EO_SCAN:
        c4m_list_append(result, c4m_new_utf8("S"));
        break;

    case C4M_EO_COMPLETE:
        c4m_list_append(result, c4m_new_utf8("C"));
        break;
    }

    return result;
}

#ifdef C4M_EARLEY_DEBUG
static c4m_grid_t *
get_parse_state(c4m_parser_t *parser, bool next)
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
    c4m_list_append(hdr, c4m_rich_lit("[th]Links"));

    c4m_grid_add_row(grid, hdr);

    c4m_set_column_style(grid, 0, snap);
    c4m_set_column_style(grid, 1, flex);
    c4m_set_column_style(grid, 2, snap);
    c4m_set_column_style(grid, 3, flex);

    c4m_earley_state_t *s = next ? parser->next_state : parser->current_state;

    int n = c4m_list_len(s->items);

    for (int i = 0; i < n; i++) {
        c4m_grid_add_row(grid,
                         repr_earley_item(parser,
                                          c4m_list_get(s->items, i, NULL),
                                          i));
    }

    return grid;
}
#endif

c4m_grid_t *
c4m_parse_to_grid(c4m_parser_t *parser, bool show_all)
{
    c4m_grid_t *grid = c4m_new(c4m_type_grid(),
                               c4m_kw("start_cols",
                                      c4m_ka(show_all ? 5 : 4),
                                      "header_rows",
                                      c4m_ka(0),
                                      "container_tag",
                                      c4m_ka(c4m_new_utf8("table2")),
                                      "stripe",
                                      c4m_ka(true)));

    c4m_utf8_t *snap = c4m_new_utf8("snap");
    c4m_utf8_t *flex = c4m_new_utf8("flex");
    // c4m_list_t *row;
    c4m_list_t *hdr  = c4m_new_table_row();
    c4m_list_t *row;

    c4m_list_append(hdr, c4m_rich_lit("[th]#"));
    c4m_list_append(hdr, c4m_rich_lit("[th]Rule State"));
    c4m_list_append(hdr, c4m_rich_lit("[th]Matches"));
    c4m_list_append(hdr, c4m_rich_lit("[th]Links"));

    if (show_all) {
        c4m_list_append(hdr, c4m_rich_lit("[th]Op"));
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

        c4m_list_t *l = c4m_new_table_row();
        c4m_list_append(l, c4m_new_utf8(""));
        c4m_list_append(l, desc);

        c4m_grid_add_row(grid, l);

        int m = c4m_list_len(s->items);

        c4m_grid_add_row(grid, hdr);

        for (int j = 0; j < m; j++) {
            c4m_earley_item_t *item = c4m_list_get(s->items, j, NULL);

            if (show_all || item->op == C4M_EO_COMPLETE) {
                row = repr_earley_item(parser, item, j);
                c4m_grid_add_row(grid, row);
            }
        }
    }

    c4m_set_column_style(grid, 0, snap);
    c4m_set_column_style(grid, 1, flex);
    c4m_set_column_style(grid, 2, snap);
    c4m_set_column_style(grid, 3, flex);
    c4m_set_column_style(grid, 4, snap);

    return grid;
}

static c4m_list_t *
repr_one_grammar_nt(c4m_grammar_t *grammar, int ix)
{
    c4m_list_t    *row;
    c4m_list_t    *res   = c4m_list(c4m_type_utf8());
    c4m_nonterm_t *nt    = get_nonterm(grammar, ix);
    int            n     = c4m_list_len(nt->rules);
    c4m_utf8_t    *lhs   = repr_nonterm(grammar, ix);
    c4m_utf8_t    *arrow = c4m_rich_lit("[yellow]⟶ ");
    c4m_utf8_t    *rhs;

    for (int i = 0; i < n; i++) {
        row = c4m_new_table_row();
        rhs = repr_rule(grammar, c4m_list_get(nt->rules, i, NULL), -1);
        c4m_list_append(row, lhs);
        c4m_list_append(row, arrow);
        c4m_list_append(row, rhs);
        c4m_list_append(res, row);
    }

    return res;
}

c4m_grid_t *
c4m_grammar_to_grid(c4m_grammar_t *grammar)
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
            c4m_nonterm_t *nt = get_nonterm(grammar, i);
            if (!nt->name) {
                continue;
            }
            l = repr_one_grammar_nt(grammar, i);
            c4m_grid_add_rows(grid, l);
        }
    }

    return grid;
}

static c4m_forest_item_t *extract_group(c4m_parser_t *, c4m_earley_item_t *);
static c4m_forest_item_t *extract_nt_node(c4m_parser_t *, c4m_earley_item_t *);

static c4m_forest_item_t *
gather_last_match(c4m_parser_t *p, c4m_earley_item_t *item)
{
    c4m_forest_item_t *res;

    c4m_pitem_t *pi = c4m_list_get(item->rule, item->cursor - 1, NULL);

    switch (pi->kind) {
    case C4M_P_NULL:
        res     = c4m_new(c4m_type_forest(), C4M_FOREST_TERM);
        res->id = C4M_EMPTY_STRING_NODE;
        break;
    case C4M_P_TERMINAL:
    case C4M_P_ANY:
    case C4M_P_BI_CLASS:
    case C4M_P_SET:
        res                       = c4m_new(c4m_type_forest(), C4M_FOREST_TERM);
        c4m_earley_item_t  *scan  = item->previous_scan->creation_item;
        c4m_earley_state_t *state = c4m_list_get(p->states,
                                                 scan->estate_id,
                                                 NULL);
        res->info.token           = state->token;
        break;
    case C4M_P_GROUP:
        return extract_group(p, item->creation_item);
    case C4M_P_NT:
        return extract_nt_node(p, item->creation_item);
    }
    return res;
}

static c4m_forest_item_t *
extract_group(c4m_parser_t *p, c4m_earley_item_t *item)
{
    c4m_forest_item_t *res = c4m_new(c4m_type_forest(), C4M_FOREST_GROUP_TOP);
    c4m_forest_item_t *node;
    int                grouplen;

    grouplen       = c4m_len(item->rule);
    res->info.name = repr_group(p->grammar, item->ginfo->group);

    while (item) {
        node            = c4m_new(c4m_type_forest(), C4M_FOREST_GROUP_ITEM);
        node->info.name = c4m_cstr_format("#{}", item->match_ct);

        for (int i = 0; i < grouplen; i++) {
            c4m_list_append(node->kids, gather_last_match(p, item));
            if (i + 1 != grouplen) {
                item = item->previous_scan;
            }
        }
        c4m_list_reverse(node->kids);
        c4m_list_append(res->kids, node);

        item = item->ginfo->prev_item_end;
    }

    c4m_list_reverse(res->kids);

    return res;
}

static c4m_forest_item_t *
extract_nt_node(c4m_parser_t *p, c4m_earley_item_t *item)
{
    int                n   = c4m_len(item->rule);
    c4m_forest_item_t *res = c4m_new(c4m_type_forest(), C4M_FOREST_NONTERM);

    res->info.name = repr_nonterm(p->grammar, item->ruleset_id);
    res->id        = item->ruleset_id;

    while (n--) {
        c4m_list_append(res->kids, gather_last_match(p, item));
        item = item->previous_scan;
    }

    c4m_list_reverse(res->kids);

    return res;
}

static c4m_tree_node_t *
forest_wrap(c4m_forest_item_t *node)
{
    c4m_tree_node_t *result = c4m_new_tree_node(c4m_type_forest(), node);
    int              n      = c4m_len(node->kids);

    for (int i = 0; i < n; i++) {
        c4m_forest_item_t *kid = c4m_list_get(node->kids, i, NULL);
        c4m_tree_adopt_node(result, forest_wrap(kid));
    }

    return result;
}

c4m_tree_node_t *
c4m_parse_get_parses(c4m_parser_t *parser)
{
    c4m_forest_item_t *root = c4m_new(c4m_type_forest(), C4M_FOREST_ROOT);
    root->info.name         = c4m_new_utf8("«Valid Parses»");
    root->id                = C4M_FOREST_ROOT_NODE;

    c4m_list_t *items = parser->current_state->items;

    int n = c4m_list_len(items);

    for (int i = 0; i < n; i++) {
        c4m_earley_item_t *item = c4m_list_get(items, i, NULL);

        if (item->op != C4M_EO_COMPLETE) {
            continue;
        }
        if (item->ruleset_id != parser->start) {
            continue;
        }
        if (item->start_item->estate_id != 0) {
            continue;
        }

        c4m_list_append(root->kids, extract_nt_node(parser, item));
    }

    return forest_wrap(root);
}

static c4m_utf8_t *
repr_fnode(c4m_forest_item_t *f)
{
    switch (f->kind) {
    case C4M_FOREST_TERM:
        if (f->id == C4M_EMPTY_STRING_NODE) {
            return c4m_rich_lit("[h3]ε[/]");
        }
        c4m_token_info_t *tok = f->info.token;
        return c4m_cstr_format("[h3]{}[/] [i](tok #{}; line #{}; col #{})[/]",
                               tok->value,
                               tok->index,
                               tok->line,
                               tok->column);

    default:
        return c4m_cstr_format("[h2]{}[/] ", f->info.name);
    }
}

static inline c4m_grid_t *
format_one_parse(c4m_tree_node_t *t)
{
    return c4m_grid_tree(t, c4m_kw("converter", c4m_ka(repr_fnode)));
}

c4m_grid_t *
c4m_forest_format(c4m_tree_node_t *f)
{
    if (!f || !f->num_kids) {
        return c4m_callout(c4m_new_utf8("No valid parses."));
    }

    c4m_forest_item_t *top = c4m_tree_get_contents(f);

    if (top->id != C4M_FOREST_ROOT_NODE) {
        return format_one_parse(f);
    }

    if (f->num_kids == 1) {
        return format_one_parse(f->children[0]);
    }

    c4m_list_t *glist = c4m_list(c4m_type_grid());
    c4m_grid_t *cur;

    cur = c4m_new_cell(c4m_cstr_format("Ambiguous parse; {} trees returned.",
                                       f->num_kids),
                       c4m_new_utf8("h1"));

    c4m_list_append(glist, cur);

    for (int i = 0; i < f->num_kids; i++) {
        cur = c4m_new_cell(c4m_cstr_format("Parse #{} of {}", i, f->num_kids),
                           c4m_new_utf8("h4"));
        c4m_list_append(glist, cur);
        c4m_list_append(glist, format_one_parse(f->children[i]));
    }

    return c4m_grid_flow_from_list(glist);
}

void
c4m_parse_token_list(c4m_parser_t  *parser,
                     c4m_list_t    *toks,
                     c4m_nonterm_t *start)
{
    internal_reset(parser);
    parser->token_cache = toks;
    parser->tokenizer   = NULL;
    internal_parse(parser, start);
}

void
c4m_parse_string(c4m_parser_t *parser, c4m_str_t *s, c4m_nonterm_t *start)
{
    internal_reset(parser);
    parser->user_context = c4m_to_utf32(s);
    parser->tokenizer    = c4m_token_stream_codepoints;
    internal_parse(parser, start);
}

void
c4m_parse_string_list(c4m_parser_t  *parser,
                      c4m_list_t    *items,
                      c4m_nonterm_t *start)
{
    internal_reset(parser);
    parser->user_context = items;
    parser->tokenizer    = c4m_token_stream_strings;
    internal_parse(parser, start);
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

const c4m_vtable_t c4m_forest_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)c4m_forest_init,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)C4M_GC_SCAN_ALL,
    },
};

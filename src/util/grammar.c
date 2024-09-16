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
// Everything here assumes single threaded access to data structures
// in this API.
//
// There's also a little bit of code in here for automatic
// tokenization for common scenarios. If that gets expanded
// significantly, it'll get broken out.

void
c4m_parser_reset(c4m_parser_t *parser)
{
    parser->states           = c4m_list(c4m_type_ref());
    parser->roots            = c4m_list(c4m_type_ref());
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
    grammar->named_terms  = c4m_list(c4m_type_terminal());
    grammar->rules        = c4m_list(c4m_type_ruleset());
    grammar->rule_map     = c4m_dict(c4m_type_utf8(), c4m_type_u64());
    grammar->terminal_map = c4m_dict(c4m_type_utf8(), c4m_type_u64());
    grammar->penalty_map  = c4m_dict(c4m_type_u64(), c4m_type_u64());
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
            hatrack_dict_put(grammar->rule_map, nonterm->name, (void *)n);
            nonterm->id = n;

            return;
        }
        if (c4m_str_eq(nt->name, nonterm->name)) {
            goto bail;
        }
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

static bool c4m_is_nullable_pitem(c4m_grammar_t *,
                                  c4m_pitem_t *,
                                  c4m_list_t *);

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
    if (c4m_list_len(rule) == 0) {
        return true;
    }

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
    return true;
}

c4m_nonterm_t *
c4m_get_nonterm(c4m_grammar_t *grammar, int64_t id)
{
    if (id > c4m_list_len(grammar->rules) || id < 0) {
        return NULL;
    }

    c4m_nonterm_t *nt = c4m_list_get(grammar->rules, id, NULL);

    return nt;
}

c4m_nonterm_t *
c4m_pitem_get_ruleset(c4m_grammar_t *g, c4m_pitem_t *pi)
{
    return c4m_get_nonterm(g, pi->contents.nonterm);
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
    c4m_nonterm_t *nt             = c4m_get_nonterm(grammar, nt_id);

    if (c4m_list_len(nt->rules) == 0) {
        return true;
    }
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

    group->gid   = c4m_rand32();
    group->items = pitems;
    group->min   = min;
    group->max   = max;

    c4m_pitem_t *result    = c4m_new_pitem(C4M_P_GROUP);
    result->contents.group = group;

    return result;
}

void
c4m_ruleset_raw_add_rule(c4m_grammar_t *g,
                         c4m_nonterm_t *ruleset,
                         c4m_list_t    *items)
{
    c4m_list_append(ruleset->rules, items);

    c4m_calculate_is_nullable_nt(g, ruleset->id, c4m_list(c4m_type_u64()));
}

// We're going to automatically rewrite used terminals into
// non-terminal productions that:
//
// 1. Accept the null value with a penalty of '1'.
//
// 2. Accept prefixes of any length, where the penalty goes up by 1
//    for any letter accepted.
//
//
// Basically, we'll have a magic production
// $$penalty: $$penalty |
//
// And a version of the empty string that penalizes for matching.
//
// Any non-terminal 'n' will get re-written to a corresponding 'nt_prod':
//
// nt_prod : n | $$penalty n | $$penalty_ε
//
// That should be the short and long of it.

static c4m_pitem_t *
add_penalty_rule(c4m_grammar_t *g, int64_t key, c4m_pitem_t *term)
{
    c4m_utf8_t *name = c4m_cstr_format("$$terminal_{}", key);

    c4m_nonterm_t *nt_sub = c4m_new(c4m_type_ruleset(), g, name);
    c4m_pitem_t   *result = c4m_pitem_from_nt(nt_sub);
    c4m_list_t    *rule   = c4m_list(c4m_type_ref());

    c4m_list_append(rule, term);
    c4m_list_append(nt_sub->rules, rule);

    rule = c4m_list(c4m_type_ref());
    c4m_list_append(rule, g->penalty);
    c4m_list_append(rule, term);
    c4m_list_append(nt_sub->rules, rule);

    rule = c4m_list(c4m_type_ref());
    c4m_list_append(rule, g->penalty_empty);
    c4m_list_append(nt_sub->rules, rule);
    c4m_calculate_is_nullable_nt(g, nt_sub->id, c4m_list(c4m_type_u64()));

    hatrack_dict_put(g->penalty_map, (void *)key, result);

    return result;
}

static void
initialize_penalty(c4m_grammar_t *g)
{
    c4m_utf8_t    *name       = c4m_new_utf8("$$penalty");
    c4m_utf8_t    *name2      = c4m_new_utf8("$$penalty_ε");
    c4m_list_t    *rule       = c4m_list(c4m_type_ref());
    c4m_nonterm_t *nt_penalty = c4m_new(c4m_type_ruleset(), g, name);
    c4m_nonterm_t *nt_pempty  = c4m_new(c4m_type_ruleset(), g, name2);

    g->penalty       = c4m_pitem_from_nt(nt_penalty);
    g->penalty_empty = c4m_pitem_from_nt(nt_pempty);

    c4m_list_append(rule, g->penalty);
    c4m_list_append(rule, c4m_pitem_any_terminal_raw(g));
    c4m_list_append(nt_penalty->rules, rule);

    rule = c4m_list(c4m_type_ref());
    c4m_list_append(rule, c4m_pitem_any_terminal_raw(g));
    c4m_list_append(nt_penalty->rules, rule);

    c4m_ruleset_add_empty_rule(nt_pempty);

    nt_penalty->penalty = 1;
    nt_pempty->penalty  = 1;
    nt_pempty->nullable = true;
}

void
c4m_ruleset_add_rule(c4m_grammar_t *g,
                     c4m_nonterm_t *ruleset,
                     c4m_list_t    *items)
{
#if 0
    c4m_ruleset_raw_add_rule(g, ruleset, items);
}
void
c4m_ruleset_tmp_not_add_rule(c4m_grammar_t *g,
                             c4m_nonterm_t *ruleset,
                             c4m_list_t    *items)
{
#endif

    if (g->penalty == NULL) {
        initialize_penalty(g);
    }

    int         n     = c4m_list_len(items);
    c4m_list_t *xform = c4m_list(c4m_type_ref());

    for (int i = 0; i < n; i++) {
        c4m_pitem_t *item = c4m_list_get(items, i, NULL);
        int64_t      lookup_key;

        switch (item->kind) {
        case C4M_P_NULL:
        case C4M_P_NT:
        case C4M_P_GROUP:
            c4m_list_append(xform, item);
            goto next_iteration;
        case C4M_P_TERMINAL:
            lookup_key = item->contents.terminal;
            break;
        case C4M_P_ANY:
            lookup_key = C4M_START_TOK_ID - 1;
            break;
        case C4M_P_BI_CLASS:
            lookup_key = (C4M_START_TOK_ID / 2) + item->contents.class;
            break;
        case C4M_P_SET:
            lookup_key = c4m_rand64() & ~(1ULL << 63);
            break;
        }

        c4m_pitem_t *sub = hatrack_dict_get(g->penalty_map,
                                            (void *)lookup_key,
                                            NULL);

        if (!sub) {
            sub = add_penalty_rule(g, lookup_key, item);
        }

        c4m_list_append(xform, sub);
next_iteration:
        continue;
    }

    c4m_ruleset_raw_add_rule(g, ruleset, xform);
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

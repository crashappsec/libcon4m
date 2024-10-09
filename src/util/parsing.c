// The Earley algorithm is only a *recognizer*; it doesn't build parse
// trees; we don't use the same approach for parse trees as a lot of
// people here, because:
//
// 1. I don't like the representation for sparse, packed forests; it's
//    hard for mere mortals to work with them;
// 2. I have a 'set' abstraction and am not afraid to use it, so I can
//    keep precise track of n:m mappings between states, and do not
//    have to go back and pick all related states.
//
// Also, right now, I'm not generating nodes as I parse; it's all
// moved to post-parse; see parse_tree.c.
//
// But we do collect the relationships betweeen states here that will
// help us generate the tree.
//
// For non-terminals, we basically track nodes based on two Earley
// states. Consider A rule like:
//
//   Paren  ⟶   '(' Add ')'
//
// When we start processing the `Add` during a scan, we are processing
// an Earley state associated with the above `Paren` rule. It will
// generate one or more new states. Some or all of those states may
// eventually lead to completions. The pairing of 'Add' start states
// with the associated completion state represents a node of type Add
// (of which, in an ambiguous grammar, there could be many valid
// spans). Any such node gets parented to the 2nd item in the Paren
// rule from which it spawned. But, the Paren rule too can be
// ambiguous, so will have the potential to have multiple start / end
// items.
//
// To make our lives easier, we explicitly record all predictions made
// directly from an earley state (generally there can be only one, but
// the way I handle groups, subsequent potential items are also
// considered prodictions of the top of the group). We also keep
// backlinks in Earley states to who predicted us.
//
// All of this gets more complex when we may need to accept an empty
// match. For that reason, we do NOT allow empty rules or empty string
// matches in the grammar. The only `nullable` rules we allow are for
// groups, where we allow things to match 0 items. That constraint
// helps make null processing much easier to get correct.
//
// Valid parses must have a completion in the very final
// state, where the 'start_item' associated with it is in state 0
// somewhere (and must be our start production).
//
// So, when we're done, we can proceed as follows:
//
// 1. Identify all valid end states.
//
// 2. Follow the chain to identify parent subtrees, knowing that those
//    nodes are definitely 'good'.
//
// 3. Work backwords looking at the chain of events to find the other
//    'good' subtrees.
//
// #3 is the hard thing.
//
// However, the more proper way is to identify where we need to
// complete subtrees, and then recursively apply the algorithm. To
// that end, we keep track of the 'prior scan' state. That way, we can
// find the top of a sub-tree; the bottom of it will be found
// somewhere in the Earley state above ours. This is obvious how to do
// when we scan a token, since there's no chance for ambiguity, and no
// cascades of completions. Getting this to work does require us to be
// able to disambiguate in the face of conflicting previous scans,
// which is why it is incredibly helpful not to allow null scans.
//
// Again, building trees from the states though is actually handled in
// parse_tree.c.
//
// The group feature obviously can be directly mapped to BNF, but
// I wanted to be able to handle [m, n] matches without having to
// do crazy grammar expansion. Therefore, I made groups basically
// a first-class part of the algorithm.
//
// They mostly works like regular non-terminals, except we keep a
// little bit of extra info around on the minimum and maximum number
// of matches we accept; if we hit the maximum, we currently keep
// predicting new items, but with a penalty. Conversely, if there is a
// minimum we have not hit, then we penalize 'completing' the
// top-level group. In between, whenever we get to the end of a group
// item, we will both predict a new group item AND complete the
// group. If the penalty gets too high, we stop that branch of our
// parse state.
//
// To handle 0-length matches, the first time we predict a group, we
// also immediately complete it.
//
// The group implementation basically uses anonymous non-terminals
// The only majordifference is we do a bit of accounting per above.
//
// The grammar API can transform grammar rules to do error detection
// automatically (an algorithm by Aho). Basically here, we add in
// productions that match mistakes, but assign those mistakes a
// 'penalty'.
//
// Since the Aho approach requires adding empty-string matches when
// something is omitted, I actually end up doing something slightly
// different. For instance, Aho would convert:
//
// Paren ⟶ '(' Add ')'
//
// To something like:
//
// Paren ⟶ Term_Lparen Add Term_Rparen
// Term_Lparen ⟶ '(' | <<Any> '(' | ε
// Term_Rparen ⟶ ')' | <<Any> ')' | ε
//
// And then would assess a penalty when selecting anything other than
// the bare token. Instead, I currently do something a bit
// weaker. Basically, the idea is you can transform a grammar to
// remove empty string matches (as long as your grammar doesn't accept an
// empty input, but that can also just be special cased).
//
// For instance, something like:
//
// Add ⟶ Add Term_Plus Mul
// Term_Plus ⟶ '+' | <<Any> '+' | ε
//
// Can be rewritten as:
// Add ⟶ Add '+' Mul
// Add ⟶ Add <<Any> '+' Mul
// Add ⟶ Add Mul
//
// Where the second two items are assessed a penalty.
//
// That's effectively what I'm doing (the rewrite is done in
// grammar.c), however:
//
// 1. I do some folding for ambiguity; such a penalty rule could end
//    up having the same structure as an existing rule, in which case
//    I don't add it.
//
// 2. If you've got rules with many tokens in a single, creating the
//    set of resulting rules is exponential. While that's both
//    precomputation, and not really done in any practical grammar
//    (meaning, some insane number of tokens), I don't particularly love
//    it.
//
// There are easy ways to rectify this, like rewriting the grammar
// into a normal form, where it would contain no more than 1 terminal
// and no more than 2 items. However, I don't do that right now,
// because I want the trees that people get to match the grammar as
// they wrote it, and making that mapping is less straightforward.
//
// So for the time being, I'm weakining the matching. Aho's algorithm
// clearly would produce a tree for the 'Paren' rule above if both
// parens are accientally omitted. However, I only produce rules that
// allow for a single omission.
//
// As an example, I *could* translate the Paren rule into the following:
// Paren ⟶ '(' Add ')'
// Paren ⟶  Add ')' # 1 omission
// Paren ⟶ '(' Add # 1 omission
// Paren ⟶ <<Any>> '(' Add ')' # 1 junk item
// Paren ⟶ '(' Add <<Any>> ')' # 1 junk item
// Paren ⟶ <<Any>> '(' Add <<Any>> ')' # 2 junk items.
// Paren ⟶  Add  # 2 omissions
//
// That would be equivolent to the Aho approach, where I properly
// remove nulls from the grammar.
//
// HOWEVER, I do NOT generate the last two rules; I only generate ones
// where there is one error captured.
//
// Therefore, the error recovery here isn't quite as good. BUT, it
// does have the big advantage of avoiding a bunch of unnecessary
// Earley state explosion. Frankly, that's a real-world problem that
// led me to this compromise.
//
// I've tried to make the penalty logic as clear as possible. But the
// idea is, penalty rules (and group misses) add penalty. We consider
// the penalty both when propogating down / up, but we do not
// double-count penalties (e.g., adding for a non-terminal on the way
// down and the way up).
//
// There's also a parallel notion called 'COST' which can be used for
// precedence. This is orthogonal to penalties really, but it works
// the same way. Items with lower costs are more desirable than higher
// cost items, just like lower penalties are more desirable than
// higher ones.
//
// However, no matter how high costs get, something with no penalty is
// in some sense 'correct' and 'allowed', so a completion with 1000
// cost, and 0 penalties is accepted over an otherwise equal
// completion with 0 cost and 1 penalty.

#define C4M_USE_INTERNAL_API
#define C4M_PARSE_DEBUG

#include "con4m.h"

static inline c4m_pitem_t *
get_rule_pitem(c4m_parse_rule_t *r, int i)
{
    return c4m_list_get(r->contents, i, NULL);
}

static inline c4m_pitem_t *
get_ei_pitem(c4m_earley_item_t *ei, int i)
{
    return get_rule_pitem(ei->rule, i);
}

static inline int
ei_rule_len(c4m_earley_item_t *ei)
{
    return c4m_list_len(ei->rule->contents);
}

static inline void
set_subtree_info(c4m_parser_t *p, c4m_earley_item_t *ei)
{
    c4m_earley_item_t *start = ei->start_item;

    if (!ei->cursor) {
        if (start->group) {
            if (start->double_dot) {
                ei->subtree_info = C4M_SI_GROUP_START;
            }
            else {
                ei->subtree_info = C4M_SI_GROUP_ITEM_START;
            }
        }
        else {
            ei->subtree_info = C4M_SI_NT_RULE_START;
        }
    }

    if (ei->cursor == ei_rule_len(start)) {
        if (start->group) {
            if (start->double_dot) {
                ei->subtree_info = C4M_SI_GROUP_END;
                ei->double_dot   = true;
            }
            else {
                ei->subtree_info = C4M_SI_GROUP_ITEM_END;
            }
        }
        else {
            ei->subtree_info = C4M_SI_NT_RULE_END;
        }
    }
}

static inline void
set_next_action(c4m_parser_t *p, c4m_earley_item_t *ei)
{
    c4m_earley_item_t *start = ei->start_item;

    if (ei->cursor == ei_rule_len(start)) {
        switch (ei->subtree_info) {
        case C4M_SI_GROUP_END:
        case C4M_SI_NT_RULE_END:
            ei->op = C4M_EO_COMPLETE_N;
            return;
        default:
            ei->op = C4M_EO_ITEM_END;
            return;
        }
    }

    if (!ei->cursor && ei->double_dot) {
        ei->op = C4M_EO_FIRST_GROUP_ITEM;
        return;
    }

    c4m_pitem_t *next = get_ei_pitem(ei->start_item, ei->cursor);

    switch (next->kind) {
    case C4M_P_TERMINAL:
        ei->op = C4M_EO_SCAN_TOKEN;
        return;
    case C4M_P_ANY:
        ei->op = C4M_EO_SCAN_ANY;
        return;
    case C4M_P_BI_CLASS:
        ei->op = C4M_EO_SCAN_CLASS;
        return;
    case C4M_P_SET:
        ei->op = C4M_EO_SCAN_SET;
        return;
    case C4M_P_NT:
        if (c4m_is_nullable_pitem(p->grammar, next, c4m_list(c4m_type_ref()))) {
            ei->null_prediction = true;
        }
        ei->op = C4M_EO_PREDICT_NT;
        return;
    case C4M_P_GROUP:
        ei->op = C4M_EO_PREDICT_G;
        return;
    case C4M_P_NULL:
        ei->op = C4M_EO_SCAN_NULL;
        return;
    default:
        c4m_unreachable();
    }
}

#if 0
        printf("prop_check_ei of property " #prop " (%p vs %p)\n", \
               old->prop,                                          \
               new->prop);
#endif

#define prop_check_ei(prop)                                              \
    if (((void *)(int64_t)old->prop) != ((void *)(int64_t) new->prop)) { \
        return false;                                                    \
    }
#define prop_check_start(prop)  \
    if (os->prop != ns->prop) { \
        return false;           \
    }

static inline bool
are_dupes(c4m_earley_item_t *old, c4m_earley_item_t *new)
{
    c4m_earley_item_t *os = old->start_item;
    c4m_earley_item_t *ns = new->start_item;

    // If we're not the original prediction in a rule, then if the
    // links to the start items are different, we want separate items.

    if (os && ns && os != old) {
        if (ns != os) {
            return false;
        }
    }

    if (os && os != old) {
        prop_check_start(previous_scan);
        prop_check_start(rule);
        prop_check_start(cursor);
        prop_check_start(group);
        prop_check_start(group_top);
        prop_check_start(double_dot);
        prop_check_start(null_prediction);
    }

    prop_check_ei(previous_scan);
    prop_check_ei(cursor);
    prop_check_ei(group);
    prop_check_ei(group_top);
    prop_check_ei(double_dot);
    prop_check_ei(rule);
    prop_check_ei(null_prediction);

    return true;
}

static inline c4m_earley_item_t *
search_for_existing_state(c4m_earley_state_t *s, c4m_earley_item_t *ei, int n)
{
    for (int i = 0; i < n; i++) {
        c4m_earley_item_t *existing = c4m_list_get(s->items, i, NULL);

        if (are_dupes(existing, ei)) {
            return existing;
        }
    }

    return NULL;
}

static inline void
calculate_group_end_penalties(c4m_earley_item_t *ei)
{
    // We don't track penalties for not meeting the minimum when
    // setting items, and we don't propogate the penalty we calculated
    // when predicting the group. So we calculate the correct value
    // here.

    int match_min    = ei->group->min;
    int match_max    = ei->group->max;
    int new_match_ct = ei->match_ct;

    if (new_match_ct < match_min) {
        ei->group_penalty = new_match_ct - match_min;
    }
    else {
        if (match_max && new_match_ct > match_max) {
            ei->group_penalty = (match_max - new_match_ct);
        }
    }

    ei->penalty = ei->group_penalty + ei->my_penalty + ei->sub_penalties;
}

static void
add_item(c4m_parser_t       *p,
         c4m_earley_item_t  *from_state,
         c4m_earley_item_t **newptr,
         bool                next_state)
{
    c4m_earley_item_t *new = *newptr;

    if (new->group &&new->cursor == c4m_list_len(new->rule->contents)) {
        if (new->group->min < new->match_ct) {
            calculate_group_end_penalties(new);
        }
    }

    if (new->penalty > p->grammar->max_penalty) {
        return;
    }

    assert(new->rule);
    // The state we use for duping could also be hashed to avoid the
    // state scan if that ever were an issue.
    c4m_earley_state_t *state   = next_state ? p->next_state : p->current_state;
    int                 n       = c4m_list_len(state->items);
    new->estate_id              = state->id;
    new->eitem_index            = n;
    c4m_earley_item_t *existing = search_for_existing_state(state, new, n);

    if (existing) {
        if (new->penalty < existing->penalty) {
            existing->completors    = new->completors;
            existing->penalty       = new->penalty;
            existing->sub_penalties = new->sub_penalties;
            existing->my_penalty    = new->penalty;
            existing->group_penalty = new->group_penalty;
            existing->previous_scan = new->previous_scan;
            existing->predictions   = new->predictions;
        }
        else {
            if (new->penalty > existing->penalty) {
                // This will effectively abandon the state; the penalty is too
                // high.
                return;
            }
            existing->completors = c4m_set_union(existing->completors,
                                                 new->completors);
        }

        *newptr = existing;
        return;
    }
    c4m_list_append(state->items, new);
    set_subtree_info(p, new);
    set_next_action(p, new);
}

static inline c4m_earley_item_t *
new_earley_item(void)
{
    return c4m_gc_alloc_mapped(c4m_earley_item_t, C4M_GC_SCAN_ALL);
}

static void
terminal_scan(c4m_parser_t *p, c4m_earley_item_t *old, bool not_null)
{
    c4m_earley_item_t *new   = new_earley_item();
    c4m_earley_item_t *start = old->start_item;
    new->start_item          = start;
    new->cursor              = old->cursor + 1;
    new->parent_states       = old->parent_states;
    new->penalty             = old->penalty;
    new->sub_penalties       = old->sub_penalties;
    new->my_penalty          = old->my_penalty;
    new->previous_scan       = old;
    new->rule                = old->rule;

    old->no_reprocessing = true;
    add_item(p, NULL, &new, not_null);
}

static void
register_prediction(c4m_earley_item_t *predictor, c4m_earley_item_t *predicted)
{
    if (!predictor) {
        return;
    }
    if (!predictor->predictions) {
        predictor->predictions = c4m_set(c4m_type_ref());
    }
    if (!predicted->parent_states) {
        predicted->parent_states = c4m_set(c4m_type_ref());
    }
    hatrack_set_add(predictor->predictions, predicted);
    hatrack_set_add(predicted->parent_states, predictor);
}

static void
add_one_nt_prediction(c4m_parser_t      *p,
                      c4m_earley_item_t *predictor,
                      c4m_nonterm_t     *nt,
                      int                rule_ix)
{
    c4m_earley_item_t *ei = new_earley_item();
    ei->ruleset_id        = nt->id;
    ei->rule              = c4m_list_get(nt->rules, rule_ix, NULL);
    ei->rule_index        = rule_ix;
    ei->start_item        = ei;

    if (predictor) {
        c4m_earley_item_t *prestart = predictor->start_item;
        ei->predictor_ruleset_id    = prestart->ruleset_id;
        ei->predictor_rule_index    = prestart->rule_index;
        ei->predictor_cursor        = prestart->cursor;
    }

    if (ei->rule->penalty_rule) {
        ei->my_penalty += 1;
    }

    ei->penalty = ei->my_penalty;

    add_item(p, predictor, &ei, false);
    register_prediction(predictor, ei);
}

static c4m_earley_item_t *
add_one_group_prediction(c4m_parser_t *p, c4m_earley_item_t *predictor)
{
    // There are never penalties when predicting a group.

    c4m_earley_item_t *ps   = predictor->start_item;
    c4m_pitem_t       *next = get_ei_pitem(ps, predictor->cursor);
    c4m_rule_group_t  *g    = next->contents.group;
    c4m_earley_item_t *gei  = new_earley_item();
    gei->start_item         = gei;
    gei->cursor             = 0;
    gei->double_dot         = true;
    gei->group              = g;
    gei->ruleset_id         = g->gid;
    gei->rule               = c4m_list_get(g->contents->rules, 0, NULL);

    assert(gei->rule);

    gei->predictor_ruleset_id = ps->ruleset_id;
    gei->predictor_rule_index = ps->rule_index;
    gei->predictor_cursor     = ps->cursor;

    add_item(p, predictor, &gei, false);
    register_prediction(predictor, gei);

    return gei;
}

static void
add_first_group_item(c4m_parser_t *p, c4m_earley_item_t *gei)
{
    // We don't deal with penalties for a regex being too short until
    // completion time. So the first item doesn't need any penalty
    // accounting.
    c4m_earley_item_t *ei = new_earley_item();
    ei->start_item        = ei;
    ei->cursor            = 0;
    ei->rule              = c4m_list_get(gei->group->contents->rules, 0, NULL);
    ei->ruleset_id        = gei->group->gid;
    ei->rule_index        = 0;
    ei->match_ct          = 0;
    ei->group             = gei->group;
    ei->group_top         = gei;
    ei->double_dot        = false;

    assert(gei);

    ei->predictor_ruleset_id = ei->group_top->ruleset_id;
    ei->predictor_rule_index = ei->group_top->rule_index;
    ei->predictor_cursor     = ei->group_top->cursor;

    add_item(p, gei, &ei, false);

    register_prediction(gei, ei);
}

static void
empty_group_completion(c4m_parser_t *p, c4m_earley_item_t *gei)
{
    if (!gei->group->min) {
        c4m_earley_item_t *gend = new_earley_item();
        gend->start_item        = gei;
        gend->cursor            = c4m_list_len(gei->rule->contents);
        gend->double_dot        = true;
        gend->group             = gei->group;
        gend->ruleset_id        = gei->group->gid;
        gend->rule              = gei->rule;
        gend->match_ct          = 0;
        gend->parent_states     = gei->parent_states;

        add_item(p, gei, &gend, false);
    }

    gei->no_reprocessing = true;
}

static void
add_next_group_item(c4m_parser_t      *p,
                    c4m_earley_item_t *last_end)
{
    c4m_earley_item_t *ei         = new_earley_item();
    c4m_earley_item_t *last_start = last_end->start_item;

    last_end->no_reprocessing = true;

    ei->start_item    = ei;
    ei->cursor        = 0;
    ei->previous_scan = last_end;
    ei->rule          = last_start->rule;
    ei->ruleset_id    = last_start->ruleset_id;
    ei->parent_states = c4m_shallow(last_start->parent_states);
    ei->group_top     = last_start->group_top;
    ei->match_ct      = last_end->match_ct;
    ei->group         = last_start->group;
    ei->my_penalty    = last_end->my_penalty;
    ei->sub_penalties = last_end->sub_penalties;

    /*
    if (last_end->start_item->estate_id == p->current_state->id) {
              return;
              }
    */

    int max_items = ei->group_top->group->max;

    assert(ei->group_top);

    ei->predictor_ruleset_id = ei->group_top->ruleset_id;
    ei->predictor_rule_index = ei->group_top->rule_index;
    ei->predictor_cursor     = ei->group_top->cursor;

    // Again, items only track penalties if / when we exceed the max;
    // min is handled at completion.

    if (max_items && ei->match_ct > max_items) {
        ei->group_penalty = ei->match_ct - max_items;
    }

    // The group penalty will disappear after the lead item, and come
    // back when we complete the group.
    ei->penalty = ei->group_penalty + ei->my_penalty + ei->sub_penalties;

    add_item(p, last_end, &ei, false);

    if (!ei->completors) {
        ei->completors = c4m_set(c4m_type_ref());
    }

    hatrack_set_put(ei->completors, ei->group_top);
    register_prediction(ei->group_top, ei);
}

static void
add_one_completion(c4m_parser_t      *p,
                   c4m_earley_item_t *cur,
                   c4m_earley_item_t *parent_ei)
{
    // The node we're producing will map to the end of a single tree
    // node started by the the parent EI, but also the beginning of
    // the next node. So we advance the scan pointer.

    c4m_earley_item_t *ei           = new_earley_item();
    c4m_earley_item_t *parent_start = parent_ei->start_item;

    ei->start_item    = parent_start;
    ei->cursor        = parent_ei->cursor + 1;
    ei->previous_scan = parent_ei;
    ei->group_top     = parent_ei->group_top;
    // The new items inherits penalty data from two places; any
    // sibling in its rule (which we are calling 'parent' here because
    // it's the parent node we're bumping back up to), and the item
    // that causes the completion (cur).  Fort the completion, we
    // inherit ALL penalties ascribed to that node. The result for all
    // of those inputs goes into sub_penalties.
    // Also, we subtract out the inherited penalty at this point; it
    // *should* be the same as cur-penalty making it a waste, but
    // as I massage things I want the extra assurance.
    ei->my_penalty    = parent_ei->my_penalty;
    ei->sub_penalties = parent_ei->sub_penalties + cur->penalty;
    ei->penalty       = ei->sub_penalties + ei->my_penalty + ei->group_penalty;
    ei->rule          = parent_start->rule;

    if (ei->group_top) {
        ei->match_ct = parent_start->match_ct;
    }

    if (ei->penalty < cur->penalty) {
        ei->penalty = cur->penalty;
    }

    // Don't allow empty group items.
    if (ei->group_top && ei->start_item->estate_id == p->current_state->id) {
        return;
    }

    ei->parent_states = c4m_shallow(parent_ei->parent_states);

    add_item(p, cur, &ei, false);

    if (!ei->completors) {
        ei->completors = c4m_set(c4m_type_ref());
    }
    hatrack_set_put(ei->completors, cur);
    parent_ei->no_reprocessing = true;
}

static c4m_earley_item_t *
add_group_completion(c4m_parser_t      *p,
                     c4m_earley_item_t *cur)
{
    c4m_earley_item_t *ei     = new_earley_item();
    c4m_earley_item_t *istart = cur->start_item;
    c4m_earley_item_t *gstart = istart->group_top;

    assert(gstart);

    ei->start_item    = gstart;
    ei->rule          = gstart->rule;
    ei->cursor        = c4m_list_len(ei->rule->contents);
    ei->match_ct      = cur->match_ct;
    ei->group         = gstart->group;
    ei->double_dot    = true;
    ei->completors    = c4m_set(c4m_type_ref());
    ei->parent_states = gstart->parent_states;

    calculate_group_end_penalties(ei);
    ei->penalty += cur->my_penalty + cur->sub_penalties;

    add_item(p, cur, &ei, false);

    hatrack_set_put(ei->completors, cur);
    cur->no_reprocessing = true;
    return ei;
}

static inline void
predict_nt(c4m_parser_t *p, c4m_nonterm_t *nt, c4m_earley_item_t *ei)
{
    int n = c4m_list_len(nt->rules);

    assert(c4m_list_len(nt->rules));

    for (int64_t i = 0; i < n; i++) {
        add_one_nt_prediction(p, ei, nt, i);
    }
}

static inline void
predict_nt_via_ei(c4m_parser_t *p, c4m_earley_item_t *ei)
{
    c4m_earley_item_t *s  = ei->start_item;
    c4m_pitem_t       *nx = get_ei_pitem(s, ei->cursor);
    c4m_nonterm_t     *nt = c4m_get_nonterm(p->grammar, nx->contents.nonterm);

    if (ei->null_prediction) {
        terminal_scan(p, ei, true);
    }

    predict_nt(p, nt, ei);
}

static inline void
predict_group(c4m_parser_t *p, c4m_earley_item_t *predictor)
{
    c4m_earley_item_t *gei = add_one_group_prediction(p, predictor);
    add_first_group_item(p, gei);
}

bool
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
matches_set(c4m_parser_t *parser, c4m_codepoint_t cp, c4m_list_t *set_items)
{
    // Return true if there's a match.
    int n = c4m_list_len(set_items);

    for (int i = 0; i < n; i++) {
        c4m_pitem_t *sub = c4m_list_get(set_items, i, NULL);

        switch (sub->kind) {
        case C4M_P_ANY:
            return true;
        case C4M_P_TERMINAL:
            if (cp == (int32_t)sub->contents.terminal) {
                return true;
            }
            continue;
        case C4M_P_BI_CLASS:
            if (char_in_class(cp, sub->contents.class)) {
                return true;
            }
            continue;
        default:
            c4m_unreachable();
        }
    }

    return false;
}

static inline void
scan_class(c4m_parser_t *parser, c4m_earley_item_t *ei)
{
    c4m_codepoint_t cp   = parser->current_state->token->tid;
    c4m_pitem_t    *next = get_ei_pitem(ei->start_item, ei->cursor);

    if (char_in_class(cp, next->contents.class)) {
        terminal_scan(parser, ei, true);
    }
}

static inline void
scan_terminal(c4m_parser_t *parser, c4m_earley_item_t *ei)
{
    c4m_codepoint_t cp   = parser->current_state->token->tid;
    c4m_pitem_t    *next = get_ei_pitem(ei->start_item, ei->cursor);

    if (cp == next->contents.terminal) {
        terminal_scan(parser, ei, true);
    }
}

static inline void
scan_set(c4m_parser_t *parser, c4m_earley_item_t *ei)
{
    c4m_codepoint_t cp   = parser->current_state->token->tid;
    c4m_pitem_t    *next = get_ei_pitem(ei->start_item, ei->cursor);

    if (matches_set(parser, cp, next->contents.items)) {
        terminal_scan(parser, ei, true);
    }
}

static void
complete_group_item(c4m_parser_t *parser, c4m_earley_item_t *ei)
{
    ei->match_ct++;
    add_next_group_item(parser, ei);
}

static void
complete(c4m_parser_t *parser, c4m_earley_item_t *ei)
{
    uint64_t            n;
    c4m_set_t          *start_set = ei->start_item->parent_states;
    c4m_earley_item_t **parents   = hatrack_set_items_sort(start_set, &n);

    for (uint64_t i = 0; i < n; i++) {
        add_one_completion(parser, ei, parents[i]);
    }
}

static inline void
run_state_predictions(c4m_parser_t *parser)
{
    int                i  = 0;
    c4m_earley_item_t *ei = c4m_list_get(parser->current_state->items, i, NULL);

    while (ei != NULL) {
        if (!ei->no_reprocessing) {
            switch (ei->op) {
            case C4M_EO_PREDICT_NT:
                predict_nt_via_ei(parser, ei);
                break;
            case C4M_EO_PREDICT_G:
                predict_group(parser, ei);
                ei->no_reprocessing = true;
                break;
            case C4M_EO_FIRST_GROUP_ITEM:
                // EI should be the double dot; Generate the first item
                // under it, and possibly a completion.
                add_first_group_item(parser, ei);
                empty_group_completion(parser, ei);
                break;
            default:
                break;
            }
        }
        ei = c4m_list_get(parser->current_state->items, ++i, NULL);
    }
}

static inline void
run_state_scans(c4m_parser_t *parser)
{
    int                i  = 0;
    c4m_earley_item_t *ei = c4m_list_get(parser->current_state->items, i, NULL);

    while (ei != NULL) {
        if (!ei->no_reprocessing) {
            switch (ei->op) {
            case C4M_EO_SCAN_TOKEN:
                scan_terminal(parser, ei);
                break;
            case C4M_EO_SCAN_ANY:
                terminal_scan(parser, ei, true);
                break;
            case C4M_EO_SCAN_NULL:
                terminal_scan(parser, ei, false);
                break;
            case C4M_EO_SCAN_CLASS:
                scan_class(parser, ei);
                break;
            case C4M_EO_SCAN_SET:
                scan_set(parser, ei);
                break;
            default:
                break;
            }
        }
        ei = c4m_list_get(parser->current_state->items, ++i, NULL);
    }
}

static inline void
run_state_completions(c4m_parser_t *parser)
{
    int                i  = 0;
    c4m_earley_item_t *ei = c4m_list_get(parser->current_state->items, i, NULL);

    while (ei != NULL) {
        if (!ei->no_reprocessing) {
            switch (ei->op) {
            case C4M_EO_COMPLETE_N:
                complete(parser, ei);
                break;
            case C4M_EO_ITEM_END:
                complete_group_item(parser, ei);
                add_group_completion(parser, ei);
                break;
            default:
                break;
            }
        }
        ei = c4m_list_get(parser->current_state->items, ++i, NULL);
    }
}

static void
process_current_state(c4m_parser_t *parser)
{
    int prev = c4m_list_len(parser->current_state->items);
    int cur;

    while (true) {
        run_state_completions(parser);
        run_state_predictions(parser);
        run_state_scans(parser);
        cur = c4m_list_len(parser->current_state->items);
        if (cur == prev) {
            break;
        }

        prev = cur;
    }
}

static void
process_current_state_old(c4m_parser_t *parser)
{
    int                i  = 0;
    c4m_earley_item_t *ei = c4m_list_get(parser->current_state->items, i, NULL);

    while (ei != NULL) {
        switch (ei->op) {
        case C4M_EO_PREDICT_NT:
            predict_nt_via_ei(parser, ei);
            break;
        case C4M_EO_PREDICT_G:
            predict_group(parser, ei);
            break;
        case C4M_EO_FIRST_GROUP_ITEM:
            // EI should be the double dot; Generate the first item
            // under it, and possibly a completion.
            add_first_group_item(parser, ei);
            empty_group_completion(parser, ei);
            break;
        case C4M_EO_SCAN_TOKEN:
            scan_terminal(parser, ei);
            break;
        case C4M_EO_SCAN_ANY:
            terminal_scan(parser, ei, true);
            break;
        case C4M_EO_SCAN_NULL:
            terminal_scan(parser, ei, false);
            break;
        case C4M_EO_SCAN_CLASS:
            scan_class(parser, ei);
            break;
        case C4M_EO_SCAN_SET:
            scan_set(parser, ei);
            break;
        case C4M_EO_COMPLETE_N:
            complete(parser, ei);
            break;
        case C4M_EO_ITEM_END:
            complete_group_item(parser, ei);
            add_group_completion(parser, ei);
            break;
        }
        ei = c4m_list_get(parser->current_state->items, ++i, NULL);
    }
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
        c4m_nonterm_t *start   = c4m_get_nonterm(grammar, ix);

        predict_nt(parser, start, NULL);
    }

    assert(parser->position == c4m_list_len(parser->states) - 1);
    parser->next_state = c4m_new_earley_state(c4m_list_len(parser->states));
    c4m_list_append(parser->states, parser->next_state);

    c4m_parser_load_token(parser);
}

static void
run_parsing_mainloop(c4m_parser_t *parser)
{
    do {
        enter_next_state(parser);
        process_current_state(parser);
    } while (parser->current_state->token->tid != C4M_TOK_EOF);
}

static void
internal_parse(c4m_parser_t *parser, c4m_nonterm_t *start)
{
    if (!parser->grammar) {
        C4M_CRAISE("Call to parse before grammar is set.");
    }

    c4m_prep_first_parse(parser->grammar);

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

    if (parser->show_debug) {
        c4m_print(c4m_repr_state_table(parser, true));
    }
}

void
c4m_parse_token_list(c4m_parser_t  *parser,
                     c4m_list_t    *toks,
                     c4m_nonterm_t *start)
{
    c4m_parser_reset(parser);
    parser->preloaded_tokens = true;
    parser->token_cache      = toks;
    parser->tokenizer        = NULL;
    internal_parse(parser, start);
}

void
c4m_parse_string(c4m_parser_t *parser, c4m_str_t *s, c4m_nonterm_t *start)
{
    c4m_parser_reset(parser);
    parser->user_context = c4m_to_utf32(s);
    parser->tokenizer    = c4m_token_stream_codepoints;
    internal_parse(parser, start);
}

void
c4m_parse_string_list(c4m_parser_t  *parser,
                      c4m_list_t    *items,
                      c4m_nonterm_t *start)
{
    c4m_parser_reset(parser);
    parser->user_context = items;
    parser->tokenizer    = c4m_token_stream_strings;
    internal_parse(parser, start);
}

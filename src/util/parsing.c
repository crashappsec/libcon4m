// The Earley algorithm is only a *recognizer*; it doesn't build parse
// trees. I haven't found anything comprehensible that talks about how
// to build trees from Earley's algorthm, but here's my attempt to
// describe may algorithm.
//
// Every earley state with a dot at the front gets a node in the tree
// when we create it, corresponding to a new non-terminal node in the
// tree.
//
// As we process one rule, any state associated with that rule, at the
// same place in the parse, will have a link to the node it's working
// to fill (the `start_item` field).
//
// As we generate Earley states, we:
//
// (a) Generate new non-terminal nodes when we predict a new rule. For
//     any state where the cursor is at the beginning, we will
//     generate a node that we can find in that state. Sibling states
//     will have a link back to the node where the cursor is at the
//     front of the rule, via the 'start_item' field.
//
// (b) Link nodes associated with the rules we're processing to the
//     children they can cause to create ('predict').
//
// (c) Create nodes for tokens when we scan them.
//
// Multiple states can 'predict' the same node; we handle this by
// allowing every node to have multiple parents, and multiple links to
// multiple children.
//
// Note that, since the Earley algorithm is essentially trying every
// possible parse in parallel, a lot of the information in the graph
// that results from the above is essentially junk. For that reason,
// the links aren't kept in the actual tree during the Earley
// algorithm running, they're kept in the Early item (which is
// basically a record for each state in what is essentially a big
// state machine).
//
// In the Early record, the downward link from parent to child is
// implicit (basically self-evident once you understand the
// algorithm). Instead, we record the links back from the children in
// the graph to parents at the given Earley state. When we come back
// to draw lines in the final tree, we will need to know which child
// we are in that link. Note, if empty strings are allowed in a
// production (in lingo, parts of the rule are 'nullable'), it's
// possible we can have the same parent node from one state, through
// multiple links. In that case, we will definitely end up with
// multiple Earley states linked; the cursor position in the linked
// state tells us which edge in the graph we're supposed to follow.
//
// The thing to note here is that the cursor position is key to the
// graph action we take. If we're at the very beginning, we create a
// new node when we create the state.
//
// As we 'scan' past parts of the rule, we 'move' the cursor in rules,
// knowing that in some possible partial parse, there are going to be
// sub-nodes; either this is a token scan, or a scan of a
// non-terminal, which leads to more 'prediction'; in both cases we
// end up creating children nodes. So if the rule moves the cursor, it
// represents a link to a child node; we only add *new* nodes for
// scans when scanning a terminal (a raw token).
//
// But it's when we get to the END of a rule (completion) that we get
// the indication that these nodes we're creating actually are valid
// for at least a subtree.  Anything that doesn't end with a
// completion is garbage.
//
// In fact, valid parses must have a completion in the very final
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
// #3 is the hard thing. I've done this a number of ways; the most
// straightforward thing is to work backwards up the earley state
// chart to create all possible valid subtrees, and then come back and
// see where it makes sense to link them up. That essentially means,
// we make a second pass that gets rid of a big bunch of the junk and
// makes it easy to fix up the forest of trees in a third pass.
//
// However, the more proper way is to identify where we need to
// complete subtrees, and then recursively apply the algorithm. To
// that end, we keep track of the 'prior scan' state. That way, we can
// find the top of a sub-tree; the bottom of it will be found
// somewhere in the Earley state above ours. This is obvious how to do
// when we scan a token, since there's no chance for ambiguity, and no
// cascades of completions.
//
// But it's actually simple even for non-terminals; whenever the
// cursor advances, that's a scan, and we should reset the value when
// we tell the function that copies an earley state to move the
// cursor. (Other times, it propogates the existing value).
//
// In all cases, ambiguity is not our friend; nodes may be part of
// multiple subtrees. To that end, during this tree construction
// phase, we keep a worklist of tasks to perform.  The tree building
// actually lives in parse_tree.c though!
//
// Also, I added my own notion of groups that's a bit more higher
// level than just generating raw BNF. This mostly works like regular
// non-terminals, except we keep a little bit of extra info around on
// the minimum and maximum number of matches we accept; if we hit the
// maximum, we currently stop predicting new items. Conversely, if
// there is a minimum we have not hit, then we refuse to 'complete'
// the top-level group. In between, whenever we get to the end of a
// group item, we will both predict a new group item AND complete the
// group.
//
// The group implementation basically uses anonymous non-terminals
// (right now they get a random id every time they are expanded;
// perhaps should make them unique per grammar). The major difference
// is we do a bit of accounting per above.
//
// Another thing: the grammar API can transform grammar rules to do
// error detection automatically (an algorithm by Aho). Basically
// here, we add in productions that match mistakes, but assign those
// mistakes a 'penalty'.
//
// The parser adds the rule to states as it parses. But as we parse
// and build the the final tree, we stop working on things where the
// penalty is too high (we have to keep this low to avoid an
// exponential explosion of work).
//
// Currently, the error transformations only get applied to terminals
// within non-terminal rules.

#define C4M_USE_INTERNAL_API
#include "con4m.h"

static inline void set_next_action(c4m_parser_t *, c4m_earley_item_t *);

static inline bool
are_dupes(c4m_earley_item_t *old, c4m_earley_item_t *new)
{
    if (old->estate_id != new->estate_id) {
        return false;
    }

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

    if (old->group != new->group) {
        return false;
    }

    if (old->double_dot != new->double_dot) {
        return false;
    }

    if (old->penalty != new->penalty) {
        return false;
    }

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

static void
add_item(c4m_parser_t *p, c4m_earley_item_t **newptr, bool next_state)
{
    // The state we use for duping could also be hashed to avoid the
    // state scan if that ever were an issue.
    c4m_earley_state_t *state = next_state ? p->next_state : p->current_state;
    c4m_earley_item_t *new    = *newptr;
    int n                     = c4m_list_len(state->items);
    new->estate_id            = state->id;
    new->eitem_index          = n;

    if (new->penalty >= C4M_MAX_PARSE_PENALTY) {
        return;
    }

    c4m_earley_item_t *existing = search_for_existing_state(state, new, n);

    if (existing) {
        *newptr = existing;
        return;
    }
    c4m_list_append(state->items, new);
    set_next_action(p, new);
}

static inline c4m_earley_item_t *
new_earley_item(void)
{
    c4m_earley_item_t *res = c4m_gc_alloc_mapped(c4m_earley_item_t,
                                                 C4M_GC_SCAN_ALL);
    res->starts            = c4m_set(c4m_type_ref());
    res->predictions       = c4m_set(c4m_type_ref());

    return res;
}

// For scans and completions we want to mostly keep the state from the
// earley item that spawned us.
static c4m_earley_item_t *
copy_earley_item(c4m_parser_t *p, c4m_earley_item_t *old, bool bump_cursor)
{
    c4m_earley_item_t *new = new_earley_item();
    new->start_item        = old->start_item;
    new->rule              = old->rule;
    new->ruleset_id        = old->ruleset_id;
    new->rule_index        = old->rule_index;
    new->cursor            = old->cursor;
    new->group             = old->group;
    new->match_ct          = old->match_ct;
    new->penalty           = old->penalty;

    if (bump_cursor) {
        new->cursor        = new->cursor + 1;
        new->previous_scan = old;
    }
    else {
        new->previous_scan = old->previous_scan;
    }

    return new;
}

static inline void
set_next_action(c4m_parser_t *p, c4m_earley_item_t *ei)
{
    // At it's core, this is about the cursor position and what's to
    // the right of the cursor.
    if (ei->cursor == c4m_list_len(ei->rule)) {
        if (ei->group) {
            if (ei->start_item->double_dot) {
                ei->op = C4M_EO_COMPLETE_G;
            }
            else {
                ei->op = C4M_EO_COMPLETE_I;
            }
            return;
        }
        ei->op = C4M_EO_COMPLETE_N;
        return;
    }

    // The only other case where the double dot shows up is already
    // handled above. Since we already grabbed the group and expanded
    // it, looking at the next pitem isn't the right thing here
    // anyway.
    if (ei->double_dot) {
        ei->op = C4M_EO_PREDICT_I;
        return;
    }

    c4m_pitem_t *next = c4m_list_get(ei->rule, ei->cursor, NULL);

    switch (next->kind) {
    case C4M_P_NULL:
        ei->op = C4M_EO_SCAN_NULL;
        return;
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
        ei->op = C4M_EO_PREDICT_NT;
        return;
    case C4M_P_GROUP:
        ei->op = C4M_EO_PREDICT_G;
        return;
    default:
        c4m_unreachable();
    }
}

static void
predict_nt(c4m_parser_t      *p,
           int64_t            id,
           c4m_list_t        *rules,
           c4m_earley_item_t *predicted_by,
           int                penalty)
{
    // Since 'predicted_by' is effectively a parent link, when we are
    // predicting subsequent group items, the caller will pass in the
    // state associated with the group top. We currently don't cache
    // it; we follow the previous_scan back until there isn't one to
    // find the first match node in the group, and pass in its parent
    // (If there's more than one production that, in the face of
    // ambiguity, can generate the same group in the same place, we
    // always generate unique groups; every group prediction gets a
    // random ID toensure uniqueness.
    //
    // This is something I might go back on, because in the face of
    // ambiguity, it could amplify worst-case performance. But it also
    // makes life a lot easier until I prove I need to do it.

    int n = c4m_list_len(rules);

    for (int64_t i = 0; i < n; i++) {
        c4m_earley_item_t *ei = new_earley_item();
        ei->estate_id         = p->position;
        ei->ruleset_id        = id;
        ei->rule_index        = i;
        ei->rule              = c4m_list_get(rules, i, NULL);
        ei->start_item        = ei;
        ei->penalty           = penalty;
        ei->cursor            = 0;

        if (predicted_by) {
            ei->penalty += predicted_by->penalty;
        }
        // If this item already exists in the same state, we'll get
        // back the old item; the rest of the stuff is redundant.
        add_item(p, &ei, false);

        // Only time predicted_by doesn't exist is during the
        // setup / loading of the start non-terminal. But recursive rules
        // that directly predict themselves don't count as creating themselves.
        if (predicted_by && predicted_by != ei) {
            // The first one of these only gets set when entering a new rule.
            // The second gets set any time a state gets created.
            hatrack_set_put(ei->starts, predicted_by);
            hatrack_set_put(predicted_by->predictions, ei);
        }
    }
}

static inline void
predict_group(c4m_parser_t *p, c4m_earley_item_t *predictor)
{
    c4m_pitem_t      *next = c4m_list_get(predictor->rule,
                                     predictor->cursor,
                                     NULL);
    c4m_rule_group_t *g    = next->contents.group;

    c4m_earley_item_t *ei = new_earley_item();
    ei->estate_id         = p->position;
    ei->group             = g;
    ei->rule              = g->items;
    ei->ruleset_id        = c4m_rand32();
    ei->double_dot        = true;
    ei->cursor            = 0;
    ei->start_item        = ei;

    add_item(p, &ei, false);

    if (predictor && predictor != ei) {
        hatrack_set_put(ei->starts, predictor);
        hatrack_set_put(predictor->predictions, ei);
    }
}

static inline void
predict_group_item(c4m_parser_t      *p,
                   c4m_earley_item_t *top,
                   c4m_earley_item_t *prev,
                   int                count)
{
    // `top` is always a group top, and `prev` is the previous scan,
    // if any.
    c4m_earley_item_t *ei = copy_earley_item(p, top, false);

    // We'll know we're the first item if double_dot is set in the predictor.
    ei->double_dot    = false;
    ei->previous_scan = prev;
    ei->cursor        = 0;
    ei->start_item    = ei;
    ei->match_ct      = count;

    add_item(p, &ei, false); // Add our item prediction to the same state.
    if (top != ei) {
        hatrack_set_put(ei->starts, top);
        hatrack_set_put(top->predictions, ei);
    }
}

static inline bool
is_nullable(c4m_parser_t *parser, int64_t nonterm_id)
{
    c4m_nonterm_t *nt = c4m_get_nonterm(parser->grammar, nonterm_id);

    return nt->nullable;
}

static inline void
try_nullable_prediction(c4m_parser_t      *parser,
                        c4m_earley_item_t *ei)
{
    // This is perhaps the most unintuitive thing in all of the Earley
    // algorithm once you understand it (and even then it wasn't part
    // of the original algorithm, someone else came up with it).
    //
    // The purpose here is to more easily handle rules like x: FOO BAR
    // when FOO can accept the empty string; this basically just
    // copies the state and advances the cursor, then adds it to this
    // state, so that we'll give the current character a chance with
    // the second term in the production.
    //
    // That's why this doesn't follow the main 'predict' logic; it's a
    // shortcut that is mostly just copying.
    //
    // Note that passing 'true' to add_earley item advances the
    // cursor for us.
    //
    // The only accounting we track here is the 'creator'; the tree
    // building algorithm will be able to see when such a prediction
    // was part of a valid subtree and it can generate the empty
    // string token for us.
    //
    // We don't even need to update the state ID, since this will always
    // have been added by an item in our exact same state.

    c4m_pitem_t *next = c4m_list_get(ei->rule, ei->cursor, NULL);

    if (!is_nullable(parser, next->contents.nonterm)) {
        return;
    }
    c4m_earley_item_t *new = copy_earley_item(parser, ei, true);

    // Add the item to the CURRENT state.
    add_item(parser, &new, false);
}

static inline void
non_nullable_prediction(c4m_parser_t *parser, c4m_earley_item_t *ei)
{
    c4m_pitem_t   *next = c4m_list_get(ei->rule, ei->cursor, NULL);
    c4m_nonterm_t *nt   = c4m_get_nonterm(parser->grammar,
                                        next->contents.nonterm);

    // If a non-terminal manages to get added with no rules, and
    // we didn't add a null production up front, we're doing it now,
    // just because it's easier if we can keep the precondition of the
    // rules being fully populated with items.
    if (!c4m_list_len(nt->rules)) {
        c4m_ruleset_add_empty_rule(nt);
    }

    predict_nt(parser, nt->id, nt->rules, ei, nt->penalty);
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
handle_token_scan(c4m_parser_t *parser, c4m_earley_item_t *ei)
{
    c4m_earley_item_t *copy = copy_earley_item(parser, ei, true);
    copy->starts            = ei->starts;
    copy->penalty           = ei->penalty;
    add_item(parser, &copy, true);
}

#if 0
static inline void
scan_penalty(c4m_parser_t *parser, c4m_earley_item_t *ei)
{
    if (ei->penalty >= C4M_MAX_PARSE_PENALTY) {
        return;
    }

    c4m_earley_item_t *copy = copy_earley_item(parser, ei, false);

    add_item(parser, &copy, true);
    copy->penalty++;

    if (copy->cursor == 0) {
        copy->start_item = copy;
	copy->starts            = old->starts;

        c4m_list_t *starts = c4m_set_to_xlist(ei->starts);

        for (int i = 0; i < c4m_list_len(starts); i++) {
            c4m_earley_item_t *ei = c4m_list_get(starts, i, NULL);
            hatrack_set_add(ei->predictions, copy);
        }
    }

    copy->previous_scan = ei->previous_scan;
}
#endif

static inline void
scan_null(c4m_parser_t *parser, c4m_earley_item_t *ei)
{
    // Every other scan advances the state when it adds an item; this
    // one does not.
    c4m_earley_item_t *copy = copy_earley_item(parser, ei, true);
    copy->starts            = ei->starts;

    add_item(parser, &copy, false);
}

static inline void
scan_class(c4m_parser_t *parser, c4m_earley_item_t *ei)
{
    c4m_codepoint_t cp   = parser->current_state->token->tid;
    c4m_pitem_t    *next = c4m_list_get(ei->rule, ei->cursor, NULL);

    if (char_in_class(cp, next->contents.class)) {
        handle_token_scan(parser, ei);
    }
}

static inline void
scan_terminal(c4m_parser_t *parser, c4m_earley_item_t *ei)
{
    c4m_codepoint_t cp   = parser->current_state->token->tid;
    c4m_pitem_t    *next = c4m_list_get(ei->rule, ei->cursor, NULL);

    if (cp == next->contents.terminal) {
        handle_token_scan(parser, ei);
    }
}

static inline void
scan_set(c4m_parser_t *parser, c4m_earley_item_t *ei)
{
    c4m_codepoint_t cp   = parser->current_state->token->tid;
    c4m_pitem_t    *next = c4m_list_get(ei->rule, ei->cursor, NULL);

    if (matches_set(parser, cp, next->contents.items)) {
        handle_token_scan(parser, ei);
    }
}

static void
complete_base(c4m_parser_t      *parser,
              c4m_earley_item_t *ei,
              bool               dbl_dot,
              int                match_value,
              int                demerits)
{
    uint64_t            n;
    c4m_earley_item_t **parents = hatrack_set_items_sort(ei->starts, &n);
    c4m_earley_item_t  *parent;
    c4m_earley_item_t  *copy;

    // If multiple nodes would have created this one, do the same for
    // all of them.
    for (uint64_t i = 0; i < n; i++) {
        // Copy the parent. Then, advance the dot and add to our
        // current state.  The default for setting the parent is wrong
        // for this use of copy (every other use is copying a
        // sibling), so we need to reset it too.
        parent           = parents[i];
        copy             = copy_earley_item(parser, parent, true);
        copy->starts     = parent->starts;
        copy->penalty    = ei->penalty + parent->start_item->penalty + demerits;
        copy->start_item = parent->start_item;
        copy->completors = c4m_set(c4m_type_ref());

        if (dbl_dot) {
            copy->double_dot = true;
            copy->cursor     = c4m_list_len(copy->rule);
        }

        if (copy->group) {
            copy->match_ct = match_value;
        }

        add_item(parser, &copy, false);
        if (parent == copy) {
            return;
        }

        if (match_value) {
            copy->double_dot = true;
        }

        if (!copy->completors) {
            copy->completors = c4m_set(c4m_type_ref());
        }
        hatrack_set_add(copy->completors, ei);
        hatrack_set_put(copy->starts, parent);
    }
}

static inline void
complete_nt_or_group(c4m_parser_t *parser, c4m_earley_item_t *ei)
{
    complete_base(parser, ei, false, 0, 0);
}

static inline void
complete_group_item(c4m_parser_t *parser, c4m_earley_item_t *ei)
{
    // The group item completing should add a state to complete the
    // group start as one might suspect, but it also should predict
    // the next group item.
    //
    // Note that if we've already predicted enough items that the
    // penalty exceeds our max, we skip predicting altogether. The
    // same can be said for the completion of the group itself; when
    // we hit that limit, we do not predict completion. The same thing
    // happens if we haven't come close enough to a minimum number of
    // matches.
    //
    // Note that, before we reach the minimum threshold, items get no
    // penalty, only completions.

    bool predict_item       = true;
    bool complete_group     = true;
    int  match_min          = ei->group->min;
    int  match_max          = ei->group->max;
    int  new_match_ct       = ++ei->match_ct;
    int  completion_penalty = 0;
    int  prediction_penalty = 0;

    if (new_match_ct < match_min) {
        completion_penalty = match_min - new_match_ct;
        if (completion_penalty > C4M_MAX_PARSE_PENALTY) {
            complete_group = false;
        }
    }
    else {
        if (match_max && new_match_ct > match_max) {
            prediction_penalty = new_match_ct - match_max;

            if (prediction_penalty >= C4M_MAX_PARSE_PENALTY) {
                // We still complete the group this one last time.
                predict_item = false;
            }
        }
    }

    if (predict_item) {
        c4m_list_t *starts = c4m_set_to_xlist(ei->starts);

        for (int i = 0; i < c4m_list_len(starts); i++) {
            c4m_earley_item_t *start = c4m_list_get(starts, i, NULL);
            predict_group_item(parser, start, ei, new_match_ct);
        }
    }

    if (complete_group) {
        // Propogates up the new match count to the group completion
        // for the sake of convenience.
        complete_base(parser, ei, true, new_match_ct, completion_penalty);
    }
}

static inline void
add_subtree_info(c4m_earley_item_t *ei)
{
    if (!ei->cursor) {
        if (!ei->group) {
            ei->subtree_info = C4M_SI_NT_RULE_START;
            return;
        }
        if (ei->double_dot) {
            ei->subtree_info = C4M_SI_GROUP_START;
            return;
        }
        ei->subtree_info = C4M_SI_GROUP_ITEM_START;
        return;
    }

    if (ei->cursor != c4m_list_len(ei->rule)) {
        return;
    }

    if (!ei->group) {
        ei->subtree_info = C4M_SI_NT_RULE_END;
        return;
    }
    if (ei->double_dot) {
        ei->subtree_info = C4M_SI_GROUP_END;
        return;
    }
    ei->subtree_info = C4M_SI_GROUP_ITEM_END;
    return;
}

static void
process_current_state(c4m_parser_t *parser)
{
    int                i  = 0;
    c4m_earley_item_t *ei = c4m_list_get(parser->current_state->items, i, NULL);

    while (ei != NULL) {
        parser->cur_item_index = i;

        add_subtree_info(ei);

        switch (ei->op) {
        case C4M_EO_PREDICT_NT:
            try_nullable_prediction(parser, ei);
            non_nullable_prediction(parser, ei);
            break;
        // Called for the first group item.
        case C4M_EO_PREDICT_G:
            predict_group(parser, ei);
            break;
        case C4M_EO_PREDICT_I:
            predict_group_item(parser, ei, NULL, 0);
            break;
        case C4M_EO_SCAN_TOKEN:
            scan_terminal(parser, ei);
            break;
        case C4M_EO_SCAN_NULL:
            scan_null(parser, ei);
            break;
        case C4M_EO_SCAN_ANY:
            handle_token_scan(parser, ei);
            break;
        case C4M_EO_SCAN_CLASS:
            scan_class(parser, ei);
            break;
        case C4M_EO_SCAN_SET:
            scan_set(parser, ei);
            break;
        case C4M_EO_COMPLETE_N:
            complete_nt_or_group(parser, ei);
            break;
        case C4M_EO_COMPLETE_G:
            complete_nt_or_group(parser, ei);
            break;
        case C4M_EO_COMPLETE_I:
            complete_group_item(parser, ei);
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

        parser->cur_item_index = C4M_IX_START_OF_PROGRAM;
        predict_nt(parser, start->id, start->rules, NULL, 0);
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
    c4m_print(c4m_repr_state_table(parser, true));

    c4m_list_t *parses  = c4m_find_all_trees(parser);
    parser->parse_trees = parses;
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
    c4m_utf8_t *dbg = c4m_cstr_format("Test input: {}", s);
    c4m_print(c4m_callout(dbg));
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

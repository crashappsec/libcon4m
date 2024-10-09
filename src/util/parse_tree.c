#define C4M_USE_INTERNAL_API
#include "con4m.h"

typedef struct nb_info_t {
    // Could union a lot of this, but gets confusing.
    c4m_parse_node_t  *pnode;
    c4m_list_t        *opts;  // A set of all possible subtrees for this node.
    c4m_list_t        *slots; // Pre-muxed data.
    int                num_kids;
    // Realisticially this is more diagnostic.
    c4m_earley_item_t *bottom_item;
    c4m_earley_item_t *top_item;
    // This tracks when the subtree has been built but not post-processed.
    // Post-processing ignores it.
    bool               cached;
    bool               visiting;
} nb_info_t;

static nb_info_t *
populate_subtree(c4m_parser_t *p, c4m_earley_item_t *end);

#define C4M_EARLEY_DEBUG
#ifdef C4M_EARLEY_DEBUG
static inline void
ptree(void *v, char *txt)
{
    if (c4m_type_is_list(c4m_get_my_type(v))) {
        return;
    }

    c4m_tree_node_t *t = (c4m_tree_node_t *)v;
    c4m_printf("[h2]{}", c4m_new_utf8(txt));

    c4m_print(c4m_grid_tree_new(t,
                                c4m_kw("callback",
                                       c4m_ka(c4m_repr_parse_node))));
}

static inline c4m_utf8_t *
dei_base(c4m_parser_t *p, c4m_earley_item_t *ei)
{
    assert(ei);
    c4m_list_t *repr = c4m_repr_earley_item(p, ei, 0);

    return c4m_cstr_format(" [em]{}:{}[/] {} ({})  [reverse]{}[/]",
                           ei->estate_id,
                           ei->eitem_index,
                           c4m_list_get(repr, 1, NULL),
                           c4m_list_get(repr, 4, NULL),
                           c4m_list_get(repr, 5, NULL));
}

static void
_dei(c4m_parser_t *p, c4m_earley_item_t *start, c4m_earley_item_t *end, char *s)
{
    c4m_utf8_t *custom = s ? c4m_rich_lit(s) : c4m_rich_lit("[h2]dei:[/] ");
    c4m_utf8_t *s1     = dei_base(p, start);
    c4m_utf8_t *s2     = NULL;
    c4m_utf8_t *to_print;

    if (end) {
        s2 = dei_base(p, end);

        to_print = c4m_str_concat(custom, c4m_rich_lit(" [h6]Top: "));
        to_print = c4m_str_concat(to_print, s1);
        to_print = c4m_str_concat(to_print, c4m_rich_lit(" [h6]End: "));
        to_print = c4m_str_concat(to_print, s2);
    }
    else {
        to_print = c4m_str_concat(custom, s1);
    }

    c4m_print(to_print);
}

#define dei(x, y, z) _dei(p, x, y, z)
#define dni(x, y)    _dei(p, x->top_item, x->bottom_item, y)
#else
#define dei(x, y, z)
#define dni(x, y)
#define ptree(x, y)
#endif

uint64_t
c4m_parse_node_hash(c4m_tree_node_t *t)
{
    c4m_parse_node_t *pn = c4m_tree_get_contents(t);

    if (pn->hv) {
        return pn->hv;
    }

    c4m_sha_t *sha    = c4m_new(c4m_type_hash());
    uint64_t   bounds = (uint64_t)pn->start;
    uint64_t   other  = (uint64_t)pn->penalty;

    bounds <<= 32;
    bounds |= (uint64_t)pn->end;

    if (pn->token) {
        other |= (1ULL << 33);
    }
    else {
        if (pn->group_item) {
            other |= (1ULL << 34);
        }
        else {
            if (pn->group_top) {
                other |= (1ULL << 35);
            }
        }
    }

    c4m_sha_int_update(sha, pn->id);
    c4m_sha_int_update(sha, bounds);
    c4m_sha_int_update(sha, other);

    if (pn->token) {
        uint64_t tinfo = (uint64_t)pn->info.token->tid;
        tinfo <<= 32;
        tinfo |= (uint64_t)pn->info.token->index;
        c4m_sha_int_update(sha, tinfo);

        c4m_utf8_t *s = pn->info.token->value;
        if (s) {
            c4m_sha_string_update(sha, s);
        }
        else {
            c4m_sha_int_update(sha, 0ULL);
        }
    }
    else {
        c4m_sha_int_update(sha, t->num_kids);
        c4m_sha_string_update(sha, pn->info.name);

        for (int i = 0; i < t->num_kids; i++) {
            c4m_sha_int_update(sha, c4m_parse_node_hash(t->children[i]));
        }
    }

    c4m_buf_t *buf = c4m_sha_finish(sha);
    pn->hv         = ((uint64_t *)buf->data)[0];

    return pn->hv;
}

static void
add_penalty_info(c4m_grammar_t    *g,
                 c4m_parse_node_t *pn,
                 c4m_parse_rule_t *bad_rule)
{
    c4m_pitem_t *pi = c4m_list_get(bad_rule->contents,
                                   pn->penalty_location,
                                   NULL);

    if (pi->kind == C4M_P_NULL) {
        pn->missing = true;
    }
    else {
        pn->bad_prefix = true;
    }

    if (bad_rule->penalty_rule) {
        pn->info.name = c4m_repr_rule(g, bad_rule->link->contents, -1);
    }
}

static void
add_penalty_annotation(c4m_parse_node_t *pn)
{
    c4m_utf8_t *s;

    if (pn->missing) {
        s = c4m_cstr_format(" [em i](Missing token before position {})",
                            (uint64_t)pn->penalty_location);
    }
    else {
        s = c4m_cstr_format(" [em i](Unexpected token at position {})",
                            (uint64_t)pn->penalty_location);
    }

    pn->info.name = c4m_str_concat(pn->info.name, s);
}

c4m_list_t *
c4m_clean_trees(c4m_parser_t *p, c4m_list_t *l)
{
    c4m_set_t  *hashes = c4m_set(c4m_type_int());
    c4m_list_t *result = c4m_list(c4m_type_ref());

    int n = c4m_list_len(l);

    if (n < 1) {
        return l;
    }

    for (int i = 0; i < n; i++) {
        c4m_tree_node_t *t  = c4m_list_get(l, i, NULL);
        uint64_t         hv = c4m_parse_node_hash(t);

        if (hatrack_set_contains(hashes, (void *)hv)) {
            continue;
        }
        hatrack_set_put(hashes, (void *)hv);
        c4m_list_append(result, t);
    }

    return result;
}

static void
add_option(nb_info_t *parent, int i, nb_info_t *kid)
{
    c4m_list_t *options = c4m_list_get(parent->slots, i, NULL);
    if (!options) {
        options = c4m_list(c4m_type_ref());
        c4m_list_set(parent->slots, i, options);
    }

    c4m_list_append(options, kid);
}

static nb_info_t *
get_node(c4m_parser_t *p, c4m_earley_item_t *b)
{
    c4m_earley_item_t *top = b->start_item;

    // When we pass non-terminals to get their node, the top (t)
    // should be the first prediction involving the subtree, and the
    // bottom (b) should be the state that completes the non-terminal.
    c4m_dict_t *cache = top->cache;

    if (!cache) {
        // We're going to map a cache of any token to null, so use int
        // not ref so we don't try to cache the hash value and crash.
        cache      = c4m_dict(c4m_type_int(), c4m_type_ref());
        top->cache = cache;
    }

    nb_info_t *result = hatrack_dict_get(cache, b, NULL);

    if (result) {
        return result;
    }

    c4m_parse_node_t *pn = c4m_gc_alloc_mapped(c4m_parse_node_t,
                                               C4M_GC_SCAN_ALL);

    result              = c4m_gc_alloc_mapped(nb_info_t, C4M_GC_SCAN_ALL);
    result->pnode       = pn;
    result->bottom_item = b;
    result->top_item    = top;
    result->slots       = c4m_list(c4m_type_ref());

    pn->id      = top->ruleset_id;
    pn->start   = top->estate_id;
    pn->end     = b->estate_id;
    // Will add in kid penalties too, later.
    pn->penalty = b->penalty;

    hatrack_dict_put(cache, b, result);

    c4m_parse_rule_t *rule = top->rule;

    // For group nodes in the tree, the number of kids is determined
    // by the match count. Everything else that might get down here is
    // a non-terminal, and the number of kids is determined by the
    // rule length
    c4m_utf8_t *s1;

    if (b->subtree_info == C4M_SI_GROUP_END) {
        s1            = c4m_repr_nonterm(p->grammar,
                              C4M_GID_SHOW_GROUP_LHS,
                              false);
        pn->group_top = true;
    }
    else {
        result->num_kids = c4m_list_len(rule->contents);
        if (top->group_top) {
            s1             = c4m_repr_group(p->grammar, top->group_top->group);
            pn->group_item = true;
        }
        else {
            s1 = c4m_repr_nonterm(p->grammar, top->ruleset_id, false);
        }
    }

    for (int i = 0; i < result->num_kids; i++) {
        c4m_list_append(result->slots, NULL);
    }

    c4m_utf8_t *s2;

    if (top->group) {
        s2 = c4m_repr_group(p->grammar, top->group);
    }
    else {
        s2 = c4m_repr_rule(p->grammar, rule->contents, -1);
    }

    pn->info.name = c4m_cstr_format("{}  [yellow]⟶  [/]{}", s1, s2);

    if (rule->penalty_rule) {
        add_penalty_info(p->grammar, pn, rule);
        add_penalty_annotation(pn); // Expect to make this an option.
    }

    return result;
}

static c4m_list_t *postprocess_subtree(c4m_parser_t *p, nb_info_t *);

static inline c4m_list_t *
process_slot(c4m_parser_t *p, c4m_list_t *ni_options)
{
    c4m_list_t *result = c4m_list(c4m_type_ref());
    int         n      = c4m_list_len(ni_options);

    for (int i = 0; i < n; i++) {
        nb_info_t *sub = c4m_list_get(ni_options, i, NULL);

        c4m_list_plus_eq(result, postprocess_subtree(p, sub));
    }

    return result;
}

static inline c4m_list_t *
score_filter(c4m_list_t *opts)
{
    uint32_t    penalty = ~0;
    int         n       = c4m_list_len(opts);
    c4m_list_t *results = c4m_list(c4m_type_ref());

    // First, calculate the lowest penalty we see.

    for (int i = 0; i < n; i++) {
        c4m_tree_node_t  *t = c4m_list_get(opts, i, NULL);
        c4m_parse_node_t *p = c4m_tree_get_contents(t);
        if (p->penalty < penalty) {
            penalty = p->penalty;
        }
    }

    for (int i = 0; i < n; i++) {
        c4m_tree_node_t  *t = c4m_list_get(opts, i, NULL);
        c4m_parse_node_t *p = c4m_tree_get_contents(t);

        if (p->penalty == penalty) {
            c4m_list_append(results, t);
        }
    }

    return results;
}

static inline c4m_parse_node_t *
copy_parse_node(c4m_parse_node_t *in)
{
    c4m_parse_node_t *out = c4m_gc_alloc_mapped(c4m_parse_node_t,
                                                C4M_GC_SCAN_ALL);

    memcpy(out, in, sizeof(c4m_parse_node_t));

    return out;
}

static inline c4m_list_t *
package_single_slot_options(nb_info_t *ni, c4m_list_t *t_opts)
{
    c4m_list_t       *output_opts = c4m_list(c4m_type_ref());
    int               n           = c4m_list_len(t_opts);
    c4m_parse_node_t *pn;

    for (int i = 0; i < n; i++) {
        c4m_tree_node_t *kid_t = c4m_list_get(t_opts, i, NULL);

        pn                       = copy_parse_node(ni->pnode);
        c4m_parse_node_t *kid_pn = c4m_tree_get_contents(kid_t);
        c4m_tree_node_t  *t      = c4m_new_tree_node(c4m_type_ref(), pn);

        if (kid_pn->penalty > pn->penalty) {
            pn->penalty = kid_pn->penalty;
        }

        c4m_tree_adopt_node(t, kid_t);
        c4m_list_append(output_opts, t);
    }

    ni->opts = output_opts;

    return score_filter(c4m_clean_trees(NULL, output_opts));
}

static inline c4m_list_t *
parse_node_zipper(c4m_list_t *kid_sets, c4m_list_t *options)
{
    // If there's only one option in the slot, it should be valid;
    // we can just append it and move on.
    int n              = c4m_list_len(options);
    int num_start_sets = c4m_list_len(kid_sets);

    if (n == 1) {
        c4m_tree_node_t *t = c4m_list_get(options, 0, NULL);

        for (int i = 0; i < num_start_sets; i++) {
            c4m_list_t *kid_set = c4m_list_get(kid_sets, i, NULL);
            c4m_list_append(kid_set, t);
        }

        return kid_sets;
    }

    c4m_list_t *results = c4m_list(c4m_type_ref());

    for (int i = 0; i < n; i++) {
        c4m_tree_node_t  *option_t = c4m_list_get(options, i, NULL);
        c4m_parse_node_t *opt_pn   = c4m_tree_get_contents(option_t);

        for (int j = 0; j < num_start_sets; j++) {
            c4m_list_t      *s1     = c4m_list_get(kid_sets, j, NULL);
            int              slen   = c4m_list_len(s1);
            c4m_tree_node_t *prev_t = c4m_list_get(s1, slen - 1, NULL);
            if (!prev_t) {
                continue;
            }
            c4m_parse_node_t *prev_pn = c4m_tree_get_contents(prev_t);

            if (prev_pn->end != opt_pn->start) {
                continue;
            }

            c4m_list_t *new_set = c4m_shallow(s1);
            assert(option_t);
            c4m_list_append(new_set, option_t);
            c4m_list_append(results, new_set);
        }
    }

    return results;
}

static inline c4m_parse_node_t *
new_epsilon_node(void)
{
    c4m_parse_node_t *pn = c4m_gc_alloc_mapped(c4m_parse_node_t,
                                               C4M_GC_SCAN_ALL);
    pn->info.name        = c4m_new_utf8("ε");
    pn->id               = C4M_EMPTY_STRING;

    return pn;
}

static inline void
package_kid_sets(nb_info_t *ni, c4m_list_t *kid_sets)
{
    int n_sets = c4m_list_len(kid_sets);
    ni->opts   = c4m_list(c4m_type_ref());

    for (int i = 0; i < n_sets; i++) {
        c4m_list_t       *a_set       = c4m_list_get(kid_sets, i, NULL);
        c4m_parse_node_t *copy        = copy_parse_node(ni->pnode);
        c4m_tree_node_t  *t           = c4m_new_tree_node(c4m_type_ref(), copy);
        c4m_parse_node_t *kpn         = NULL;
        int               old_penalty = copy->penalty;

        copy->penalty = 0;

        for (int j = 0; j < ni->num_kids; j++) {
            c4m_tree_node_t *kid_t = c4m_list_get(a_set, j, NULL);

            if (!kid_t) {
                continue;
            }

            kpn = c4m_tree_get_contents(kid_t);

            if (!kpn) {
                kpn = new_epsilon_node();
                c4m_tree_replace_contents(kid_t, kpn);
            }
            else {
                copy->penalty += kpn->penalty;
            }

            c4m_tree_adopt_node(t, kid_t);
        }

        c4m_list_append(ni->opts, t);

        if (copy->penalty < old_penalty) {
            copy->penalty = old_penalty;
        }
    }

    ni->opts = score_filter(c4m_clean_trees(NULL, ni->opts));
}

static c4m_list_t *
postprocess_subtree(c4m_parser_t *p, nb_info_t *ni)
{
    c4m_list_t      *slot_options;
    c4m_list_t      *klist;
    c4m_tree_node_t *t;

    if (ni->opts) {
        return ni->opts;
    }

    if (ni->visiting) {
        return c4m_list(c4m_type_ref());
    }

    ni->visiting = true;

    // Leaf nodes just need to be wrapped.
    if (!ni->num_kids) {
        ni->opts = c4m_list(c4m_type_ref());
        t        = c4m_new_tree_node(c4m_type_ref(), ni->pnode);

        c4m_list_append(ni->opts, t);

        return ni->opts;
    }

    // Go ahead and fully process the FIRST slot.

    slot_options = process_slot(p, c4m_list_get(ni->slots, 0, NULL));

    // If it's the ONLY slot, score and wrap. No need to zipper.
    if (ni->num_kids == 1) {
        c4m_list_t *result = package_single_slot_options(ni, slot_options);
        ni->visiting       = false;
        return result;
    }

    // Otherwise, we're going to incrementally 'zipper' options from
    // left to right. We start by turning our one column with n options
    // for the first node into a list of n possible incremental sets
    // of kids.

    c4m_list_t *kid_sets = c4m_list(c4m_type_ref());
    int         n        = c4m_list_len(slot_options);

    for (int i = 0; i < n; i++) {
        t     = c4m_list_get(slot_options, i, NULL);
        klist = c4m_list(c4m_type_ref());

        c4m_list_append(klist, t);
        c4m_list_append(kid_sets, klist);
    }

    // Basically, for each slot / column, we postprocess down, and
    // create a 'new' column that will be populated with tnodes, not
    // node_info structures. Once we create that column, we zipper it
    // into the kid_sets if possible.
    for (int i = 1; i < ni->num_kids; i++) {
        slot_options = process_slot(p, c4m_list_get(ni->slots, i, NULL));
        kid_sets     = parse_node_zipper(kid_sets, slot_options);
    }

    package_kid_sets(ni, kid_sets); // sets ni-opts.

    ni->visiting = false;
    return ni->opts;
}

// This is a primitive to extract the raw Earley items we care about
// from the final state, which are ones that are completions of a
// proper non-terminal that matches our start id. Grammars always
// start w/ non-terminals; they can never start w/ a group or token.
//
// Here we do not filter them out by start state (yet).
static inline c4m_list_t *
search_for_end_items(c4m_parser_t *p)
{
    c4m_earley_state_t *state   = p->current_state;
    c4m_list_t         *items   = state->items;
    c4m_list_t         *results = c4m_list(c4m_type_ref());
    int                 n       = c4m_list_len(items);

    if (!c4m_list_len(items)) {
        return results;
    }

    while (--n) {
        c4m_earley_item_t *ei  = c4m_list_get(items, n, NULL);
        c4m_earley_item_t *top = ei->start_item;

        if (top->subtree_info != C4M_SI_NT_RULE_START) {
            continue;
        }

        if (top->ruleset_id != p->start) {
            continue;
        }

        if (ei->cursor != c4m_len(top->rule->contents)) {
            continue;
        }

        c4m_list_append(results, ei);
    }

    return results;
}

static void
scan_group_items(c4m_parser_t *p, nb_info_t *group_ni, c4m_earley_item_t *end)
{
    // We could have multiple group items ending the group with the same
    // number of matches, if we have specific enough ambiguity.
    //
    // So follow each path back seprately.

    uint64_t            n;
    c4m_earley_item_t **clist  = hatrack_set_items_sort(end->completors, &n);
    uint32_t            minp   = ~0;
    uint32_t            nitems = ~0;

    for (uint64_t i = 0; i < n; i++) {
        c4m_earley_item_t *cur = clist[i];
        if (cur->penalty < minp) {
            minp   = cur->penalty;
            nitems = cur->match_ct;
        }
        if (cur->penalty == minp && cur->match_ct < nitems) {
            nitems = cur->match_ct;
        }
    }

    for (uint64_t i = 0; i < n; i++) {
        c4m_earley_item_t *cur = clist[i];

        if (cur->penalty != minp || cur->match_ct != nitems) {
            continue;
        }

        int ix             = cur->match_ct;
        group_ni->num_kids = cur->match_ct;

        while (cur && ix-- > 0) {
            nb_info_t         *possible_node = populate_subtree(p, cur);
            c4m_earley_item_t *start         = cur->start_item;
            add_option(group_ni, ix, possible_node);
            cur = start->previous_scan;
        }

        break;
    }
}

static void
add_token_node(c4m_parser_t *p, nb_info_t *node, c4m_earley_item_t *ei)
{
    // For the moment, we wrap the parse node in a dummy nb_info_t.
    nb_info_t          *ni = c4m_gc_alloc_mapped(nb_info_t, C4M_GC_SCAN_ALL);
    c4m_parse_node_t   *pn = c4m_gc_alloc_mapped(c4m_parse_node_t,
                                               C4M_GC_SCAN_ALL);
    c4m_earley_state_t *s  = c4m_list_get(p->states, ei->estate_id, NULL);

    pn->start       = s->id;
    pn->end         = s->id + 1;
    pn->info.token  = s->token;
    pn->id          = s->token->tid;
    pn->token       = true;
    ni->top_item    = ei;
    ni->bottom_item = ei;
    ni->pnode       = pn;

    add_option(node, ei->cursor, ni);
}

static void
add_epsilon_node(nb_info_t *node, c4m_earley_item_t *ei)
{
    // For the moment, we wrap the parse node in a dummy nb_info_t.
    nb_info_t        *ni = c4m_gc_alloc_mapped(nb_info_t, C4M_GC_SCAN_ALL);
    c4m_parse_node_t *pn = new_epsilon_node();

    pn->start       = ei->estate_id;
    pn->end         = ei->estate_id;
    pn->info.name   = c4m_new_utf8("ε");
    pn->id          = C4M_EMPTY_STRING;
    ni->top_item    = ei;
    ni->bottom_item = ei;
    ni->pnode       = pn;

    add_option(node, ei->cursor, ni);
}

static void
scan_rule_items(c4m_parser_t *p, nb_info_t *parent_ni, c4m_earley_item_t *end)
{
    c4m_earley_item_t *start = end->start_item;
    c4m_earley_item_t *cur   = end;
    c4m_earley_item_t *prev  = end;

    for (int i = 0; i < c4m_list_len(start->rule->contents); i++) {
        cur        = cur->previous_scan;
        int cursor = cur->cursor;

        assert(prev->cursor == cur->cursor + 1);

        c4m_pitem_t        *pi = c4m_list_get(start->rule->contents,
                                       cur->cursor,
                                       NULL);
        uint64_t            n_bottoms;
        c4m_earley_item_t **bottoms;

        switch (pi->kind) {
        case C4M_P_NULL:
            add_epsilon_node(parent_ni, cur);
            prev = cur;
            break;
        case C4M_P_TERMINAL:
        case C4M_P_ANY:
        case C4M_P_BI_CLASS:
        case C4M_P_SET:
            add_token_node(p, parent_ni, cur);
            prev = cur;
            break;
        case C4M_P_GROUP:
            // fallthrough
        default:
            // We piece together subtrees by looking at the each
            // subtree that resulted in the right-hand EI being
            // generated; if the start state for that subtree was
            // predicted by the LEFT-HAND item, then we are golden
            // (and I believe it always should be; I am checking to be
            // safe).
            bottoms = hatrack_set_items(prev->completors, &n_bottoms);

            for (uint64_t i = 0; i < n_bottoms; i++) {
                c4m_earley_item_t *subtree_end = bottoms[i];

                // The tops of the subtrees should have been predicted
                // by the LEFT ei.
                nb_info_t *subnode = populate_subtree(p, subtree_end);

                // Add the option to *any* possible parent, not just the one
                // that called us.
                add_option(parent_ni, cursor, subnode);
            }
        }
        prev = cur;
    }
}

static nb_info_t *
populate_subtree(c4m_parser_t *p, c4m_earley_item_t *end)
{
    nb_info_t *ni = get_node(p, end);

    if (ni->cached) {
        return ni;
    }

    if (ni->visiting) {
        return ni;
    }
    ni->visiting = true;

    // We handle populating subnodes in a group differently from
    // populating group items and other non-terminals.
    if (end->subtree_info == C4M_SI_GROUP_END) {
        scan_group_items(p, ni, end);
    }
    else {
        scan_rule_items(p, ni, end);
    }

    ni->cached   = true;
    ni->visiting = false;

    return ni;
}

c4m_list_t *
c4m_build_forest(c4m_parser_t *p)
{
    c4m_list_t *results = NULL;
    c4m_list_t *roots   = c4m_list(c4m_type_ref());
    c4m_list_t *ends    = search_for_end_items(p);
    int         n       = c4m_list_len(ends);

    p->grammar->suspend_penalty_hiding++;

    for (int i = 0; i < n; i++) {
        c4m_earley_item_t *end = c4m_list_get(ends, i, NULL);

        if (end->start_item->estate_id != 0) {
            continue;
        }

        nb_info_t *res = populate_subtree(p, end);

        if (!res) {
            continue;
        }
        c4m_list_append(roots, res);
    }

    n = c4m_list_len(roots);

    // We've built the content of the subtrees, but with the exception
    // of tokens, we've not created proper tree nodes. Plus, we can
    // easily have have ambiguous subtrees, too.

    for (int i = 0; i < n; i++) {
        nb_info_t  *ni        = c4m_list_get(roots, i, NULL);
        c4m_list_t *possibles = postprocess_subtree(p, ni);

        results = c4m_list_plus(results, possibles);
    }

    p->grammar->suspend_penalty_hiding--;
    return results;
}

c4m_list_t *
c4m_parse_get_parses(c4m_parser_t *p)
{
    // This one provides ambiguous trees, but stops at the maximum penalty.
    c4m_list_t *results = score_filter(c4m_build_forest(p));

    results = c4m_clean_trees(p, results);

    return results;
}

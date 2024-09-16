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

static inline void
ptree(void *v, char *txt)
{
    if (c4m_type_is_list(c4m_get_my_type(v))) {
        c4m_list_t *l = (c4m_list_t *)v;
        int         n = c4m_list_len(l);

        for (int i = 0; i < n; i++) {
            c4m_printf("[h2]{} #{}", c4m_new_utf8(txt), i);
            c4m_print(c4m_grid_tree_new(c4m_list_get(l, i, NULL),
                                        c4m_kw("callback", c4m_ka(c4m_repr_parse_node))));
        }
        return;
    }

    c4m_tree_node_t *t = (c4m_tree_node_t *)v;
    c4m_printf("[h2]{}", c4m_new_utf8(txt));

    c4m_print(c4m_grid_tree_new(t,
                                c4m_kw("callback", c4m_ka(c4m_repr_parse_node))));
}

static inline c4m_utf8_t *
dei_base(c4m_parser_t *p, c4m_earley_item_t *ei)
{
    if (!ei) {
        return c4m_new_utf8(" NULL eitem :(");
    }
    else {
        c4m_list_t *repr = c4m_repr_earley_item(p, ei, 0);

        return c4m_cstr_format(" [em]{}:{}[/] {} ({})  [reverse]{}[/]",
                               ei->estate_id,
                               ei->eitem_index,
                               c4m_list_get(repr, 1, NULL),
                               c4m_list_get(repr, 4, NULL),
                               c4m_list_get(repr, 5, NULL));
    }
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

#define dei2(x, y)   _dei(p, x, NULL, y)
#define dei(x, y, z) _dei(p, x, y, z)
#define dni(x, y)    _dei(p, x->top_item, x->bottom_item, y)

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

        for (int i = 0; i < t->num_kids; i++) {
            c4m_sha_int_update(sha, c4m_parse_node_hash(t->children[i]));
        }
    }

    c4m_buf_t *buf = c4m_sha_finish(sha);
    pn->hv         = ((uint64_t *)buf->data)[0];

    return pn->hv;
}

static c4m_list_t *
clean_trees(c4m_list_t *l)
{
    c4m_set_t  *hashes = c4m_set(c4m_type_int());
    c4m_list_t *result = c4m_list(c4m_type_ref());

    int n = c4m_list_len(l);

    if (n <= 1) {
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

#define compat_bounds(t, b)                 \
    assert(t->subtree_info != C4M_SI_NONE); \
    assert(b->subtree_info != C4M_SI_NONE); \
    assert(((int)t->subtree_info) % 2);     \
    assert(((int)b->subtree_info) == ((int)t->subtree_info) + 1);

static nb_info_t *
get_node(c4m_parser_t *p, c4m_earley_item_t *b)
{
    // When we pass non-terminals to get their node, the top (t)
    // should be the first prediction involving the subtree, and the
    // bottom (b) should be the state that completes the non-terminal.
    c4m_dict_t *cache = b->start_item->cache;

    if (!cache) {
        // We're going to map a cache of any token to null, so use int
        // not ref so we don't try to cache the hash value and crash.
        cache                = c4m_dict(c4m_type_int(), c4m_type_ref());
        b->start_item->cache = cache;
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
    result->top_item    = b->start_item;
    result->slots       = c4m_list(c4m_type_ref());

    pn->id      = b->ruleset_id;
    pn->start   = b->start_item->estate_id;
    pn->end     = b->estate_id;
    // Will add in kid penalties too, later.
    pn->penalty = b->start_item->penalty;

    hatrack_dict_put(cache, b, result);

    // For group nodes in the tree, the number of kids is determined
    // by the match count. Everything else that might get down here is
    // a non-terminal, and the number of kids is determined by the
    // rule length
    c4m_utf8_t *s1;

    if (b->subtree_info == C4M_SI_GROUP_END) {
        s1               = c4m_repr_nonterm(p->grammar,
                              C4M_GID_SHOW_GROUP_LHS,
                              false);
        result->num_kids = b->match_ct;
        pn->group_top    = true;
    }
    else {
        result->num_kids = c4m_list_len(b->rule);
        s1               = c4m_repr_nonterm(p->grammar, b->ruleset_id, false);
        if (b->subtree_info == C4M_SI_GROUP_ITEM_END) {
            pn->group_item = true;
        }
    }

    for (int i = 0; i < result->num_kids; i++) {
        c4m_list_append(result->slots, NULL);
    }

    c4m_utf8_t *s2 = c4m_repr_rule(p->grammar, b->rule, -1);

    pn->info.name = c4m_cstr_format("{}  [yellow]⟶  [/]{}", s1, s2);

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

static inline c4m_parse_node_t *
copy_pnode(c4m_parse_node_t *pn)
{
    c4m_parse_node_t *result = c4m_gc_alloc_mapped(c4m_parse_node_t,
                                                   C4M_GC_SCAN_ALL);
    memcpy(result, pn, sizeof(c4m_parse_node_t));

    return result;
}

static inline c4m_list_t *
package_single_slot_options(nb_info_t *ni, c4m_list_t *t_opts)
{
    c4m_list_t       *output_opts = c4m_list(c4m_type_ref());
    int               n           = c4m_list_len(t_opts);
    c4m_parse_node_t *pn;

    for (int i = 0; i < n; i++) {
        c4m_tree_node_t  *kid_t  = c4m_list_get(t_opts, i, NULL);
        c4m_parse_node_t *kid_pn = c4m_tree_get_contents(kid_t);

        if (kid_pn->penalty) {
            pn = copy_pnode(ni->pnode);
            pn->penalty += kid_pn->penalty;
        }
        else {
            pn = ni->pnode;
        }

        c4m_tree_node_t *t = c4m_new_tree_node(c4m_type_ref(), pn);

        c4m_tree_adopt_node(t, kid_t);
        c4m_list_append(output_opts, t);
    }

    ni->opts = output_opts;
    return clean_trees(output_opts);
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

    printf("Len of zipper so far: %d\n", c4m_list_len(results));
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
        c4m_list_t      *one_set = c4m_list_get(kid_sets, i, NULL);
        int              penalty = 0;
        c4m_tree_node_t *t       = c4m_new_tree_node(c4m_type_ref(), ni->pnode);

        c4m_printf("Kid set #{} / {}", i, n_sets);

        for (int j = 0; j < ni->num_kids; j++) {
            c4m_tree_node_t *kid_t = c4m_list_get(one_set, j, NULL);
            ptree(kid_t, "One option");
            c4m_print(c4m_get_my_type(kid_t));
        }
        for (int j = 0; j < ni->num_kids; j++) {
            c4m_tree_node_t *kid_t = c4m_list_get(one_set, j, NULL);

            if (!kid_t) {
                continue;
            }

            c4m_parse_node_t *kid_pn = c4m_tree_get_contents(kid_t);

            if (kid_pn) {
                penalty += kid_pn->penalty;
            }
            else {
                kid_pn = new_epsilon_node();
                c4m_tree_replace_contents(kid_t, kid_pn);
            }

            c4m_tree_adopt_node(t, kid_t);
        }

        if (penalty) {
            c4m_parse_node_t *pn_copy = copy_pnode(ni->pnode);
            pn_copy->penalty += penalty;

            c4m_tree_replace_contents(t, pn_copy);
        }

        c4m_list_append(ni->opts, t);
    }

    ptree(ni->opts, "Zippered");
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

    dni(ni, "[h4]Postprocess Subtree");

    // Leaf nodes just need to be wrapped.
    if (!ni->num_kids) {
        ni->opts = c4m_list(c4m_type_ref());
        t        = c4m_new_tree_node(c4m_type_ref(), ni->pnode);

        ptree(t, "Leaf node");

        c4m_list_append(ni->opts, t);

        dni(ni, "[h6]Finished subtree");
        return ni->opts;
    }

    // Go ahead and fully process the FIRST slot.

    slot_options = process_slot(p, c4m_list_get(ni->slots, 0, NULL));

    // If it's the ONLY slot, score and wrap. No need to zipper.
    if (ni->num_kids == 1) {
        c4m_list_t *result = package_single_slot_options(ni, slot_options);
        ptree(result, "No zipper");
        dni(ni, "[h6]Finished subtree");
        ni->visiting = false;
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
    dni(ni, "[h6]Finished subtree");
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
        c4m_earley_item_t *ei = c4m_list_get(items, n, NULL);

        if (ei->cursor != c4m_len(ei->rule)) {
            continue;
        }

        if (ei->ruleset_id != p->start) {
            continue;
        }

        c4m_list_append(results, ei);
    }

    return results;
}

static void
create_all_subtrees(c4m_parser_t *p)
{
    // This doesn't try to parent anything; it just goes ahead and
    // pre-creates nodes of possible subtrees, so that we can KISS.

    int n = p->position + 1;

    while (n--) {
        c4m_earley_state_t *s       = c4m_list_get(p->states, n, NULL);
        int                 num_eis = c4m_list_len(s->items);

        for (int i = 0; i < num_eis; i++) {
            c4m_earley_item_t *ei = c4m_list_get(s->items, i, NULL);
            switch (ei->subtree_info) {
            case C4M_SI_NT_RULE_END:
            case C4M_SI_GROUP_END:
            case C4M_SI_GROUP_ITEM_END:;
                nb_info_t *info = get_node(p, ei);
                continue;
            default:
                continue;
            }
        }
    }
}

static nb_info_t *populate_subtree(c4m_parser_t *, c4m_earley_item_t *);

static void
scan_group_items(c4m_parser_t *p, nb_info_t *group_ni, c4m_earley_item_t *end)
{
    // We could have multiple group items ending the group with the same
    // number of matches, if we have specific enough ambiguity.
    //
    // So follow each path back seprately.
    int ix = group_ni->num_kids;

    uint64_t            n;
    c4m_earley_item_t **clist = hatrack_set_items_sort(end->completors, &n);

    dni(group_ni, "Scanning this group");

    for (uint64_t i = 0; i < n; i++) {
        c4m_earley_item_t *cur = clist[i];

        dei(cur, cur->start_item, NULL);

        while (cur) {
            nb_info_t         *possible_node = populate_subtree(p, cur);
            c4m_earley_item_t *start         = cur->start_item;

            dei(start, cur, "One EI");
            dni(possible_node, "How did we do?");
            add_option(group_ni, --ix, possible_node);
            cur = start->previous_scan;
        }
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
    // Unlike with a group, we are already where we need to start our
    // scan chain. But each link of the chain, we have to do some
    // logic :( Thinking is hard.

    c4m_earley_item_t *start = end->start_item;
    c4m_earley_item_t *cur   = end;
    c4m_earley_item_t *prev  = end;

    // dei(start, end, "Scan start location");

    assert(end->cursor == c4m_list_len(end->rule));

    for (int i = 0; i < c4m_list_len(end->rule); i++) {
        cur        = cur->previous_scan;
        int cursor = cur->cursor;

        assert(prev->cursor == cur->cursor + 1);
        // dei(cur, prev, "Scan item start");

        if (cur->estate_id == prev->estate_id) {
            add_epsilon_node(parent_ni, cur);
            prev = cur;
            continue;
        }

        c4m_pitem_t        *pi = c4m_list_get(cur->rule, cur->cursor, NULL);
        uint64_t            n_bottoms;
        c4m_earley_item_t **bottoms;

        switch (pi->kind) {
        case C4M_P_NULL:
            c4m_unreachable();
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
                c4m_earley_item_t *subtree_top = subtree_end->start_item;

                // There's a bug in copying predictors. Need to fix it
                // properly, but this is the practical consequence, so
                // not a huge hurry.
                if (subtree_top == start) {
                    continue;
                }

                // The tops of the subtrees should have been predicted
                // by the LEFT ei.
                dei(cur, prev, "Scan state.");
                dei(subtree_top, subtree_end, "Subtree.");
                //                assert(hatrack_set_contains(cur->predictions, subtree_top));
                assert(subtree_end != subtree_top);

                //  dei(subtree_top, subtree_end, "Scan subtree");

                nb_info_t *subnode = populate_subtree(p, subtree_end);

                add_option(parent_ni, cursor, subnode);

                // dni(subnode, "Done scanning subtree");
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
        printf("IN A GROUP\n");
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
c4m_find_all_trees(c4m_parser_t *p)
{
    c4m_list_t *results = NULL;
    c4m_list_t *roots   = c4m_list(c4m_type_ref());
    c4m_list_t *ends    = search_for_end_items(p);
    int         n       = c4m_list_len(ends);

    create_all_subtrees(p);

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

    printf("Collected %d results pre-cleaning.\n", c4m_list_len(results));
    results = clean_trees(results);
    c4m_printf("[h2]Exiting with [em]{}[/] results.", c4m_list_len(results));
    for (int i = 0; i < c4m_list_len(results); i++) {
        ptree(c4m_list_get(results, i, NULL), "Here's one.");
    }

    return results;
}

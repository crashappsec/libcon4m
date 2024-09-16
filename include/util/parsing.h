#pragma once
#include "con4m.h"

// This implements Earley's algorithm with the Aycock / Horspool
// trick (adding empty string predictions, which requires checking
// nullability of non-terminals).
//
// I have not yet implemented the Joop Leo optimization for right
// recursive grammars.
//
// I also add a couple of very minor enhancements to the core algorithm:
//
// 1. I support anonymous non-terminals, which maps to parenthesized
//    grouping of items in an EBNF grammar.
//
// 2. I support groups tracking min / max consecutive instances in a
//     rule.
//
// These two things combined make it easier to directly map common
// regexp operators like * (any), ? (optional), + (one or more), or
// 'n to m' instances.

typedef enum {
    C4M_P_NULL,     // A terminal value-- the empty string.
    C4M_P_NT,       // A non-terminal ruleset definition.
    C4M_P_TERMINAL, // A single terminal.
    C4M_P_ANY,      // Match for 'any terminal'.
    C4M_P_BI_CLASS, // Builtin character classes; these are effectively
                    // a 'choice' of one character terminals.
    C4M_P_SET,      // A specific set of terminals can
                    // match. Generally for use when the built-in
                    // classes don't meet your needs.
    C4M_P_GROUP,    // A group of items, which can have a min or max.
} c4m_pitem_kind;

typedef enum {
    C4M_P_BIC_ID_START,
    C4M_P_BIC_ID_CONTINUE,
    C4M_P_BIC_C4M_ID_START,
    C4M_P_BIC_C4M_ID_CONTINUE,
    C4M_P_BIC_DIGIT,
    C4M_P_BIC_ANY_DIGIT,
    C4M_P_BIC_UPPER,
    C4M_P_BIC_UPPER_ASCII,
    C4M_P_BIC_LOWER,
    C4M_P_BIC_LOWER_ASCII,
    C4M_P_BIC_SPACE,
} c4m_bi_class_t;

typedef enum {
    C4M_EO_PREDICT_NT,
    C4M_EO_PREDICT_G,
    C4M_EO_PREDICT_I,
    C4M_EO_SCAN_NULL,
    C4M_EO_SCAN_TOKEN,
    C4M_EO_SCAN_ANY,
    C4M_EO_SCAN_CLASS,
    C4M_EO_SCAN_SET,
    C4M_EO_COMPLETE_N,
    C4M_EO_COMPLETE_I,
    C4M_EO_COMPLETE_G,
} c4m_earley_op;

// For my own sanity, this will map individual earley items to the
// kind of node info we have for it.
//
// I've explicitly numbered them to what they'd be assigned anyway, just
// because I check the relationships rn and want to make it explicit.
typedef enum {
    C4M_SI_NONE             = 0,
    C4M_SI_NT_RULE_START    = 1,
    C4M_SI_NT_RULE_END      = 2,
    C4M_SI_GROUP_START      = 3,
    C4M_SI_GROUP_END        = 4,
    C4M_SI_GROUP_ITEM_START = 5,
    C4M_SI_GROUP_ITEM_END   = 6,
} c4m_subtree_info_t;

typedef struct c4m_terminal_t     c4m_terminal_t;
typedef struct c4m_nonterm_t      c4m_nonterm_t;
typedef struct c4m_rule_group_t   c4m_rule_group_t;
typedef struct c4m_pitem_t        c4m_pitem_t;
typedef struct c4m_token_info_t   c4m_token_info_t;
typedef struct c4m_parser_t       c4m_parser_t;
typedef struct c4m_egroup_info_t  c4m_egroup_info_t;
typedef struct c4m_earley_item_t  c4m_earley_item_t;
typedef struct c4m_grammar_t      c4m_grammar_t;
typedef struct c4m_earley_state_t c4m_earley_state_t;
typedef struct c4m_parse_node_t   c4m_parse_node_t;

struct c4m_terminal_t {
    c4m_utf8_t *value;
    void       *user_data;
    int64_t     id;
};

struct c4m_nonterm_t {
    c4m_utf8_t *name;
    c4m_list_t *rules; // A list of c4m_prule_t objects;
    void       *user_data;
    int64_t     id;
    int         penalty;
    bool        nullable;
};

struct c4m_rule_group_t {
    c4m_list_t *items;
    int32_t     min;
    int32_t     max;
    int32_t     gid;
};

struct c4m_pitem_t {
    c4m_utf8_t   *s;
    c4m_parser_t *parser;

    union {
        c4m_list_t       *items; // c4m_pitem_t's
        c4m_rule_group_t *group;
        c4m_bi_class_t class;
        int64_t nonterm;
        int64_t terminal; // An index into a table of terminals.
    } contents;
    c4m_pitem_kind kind;
};

struct c4m_token_info_t {
    void       *user_info;
    c4m_utf8_t *value;
    int32_t     tid;
    int32_t     index;
    // These capture the start location.
    uint32_t    line;   // 1-indexed.
    uint32_t    column; // 0-indexed.
    uint32_t    endcol;
};

struct c4m_parse_node_t {
    int64_t  id;
    int64_t  hv;
    int32_t  start;
    int32_t  end;
    uint32_t penalty;
    bool     token;
    bool     group_item;
    bool     group_top;

    union {
        c4m_token_info_t *token;
        c4m_utf8_t       *name;
    } info;
};

struct c4m_earley_item_t {
    // The predicted item in our cur rule.
    c4m_earley_item_t *start_item;
    // This is where we really are keeping track of the previous
    // sibling in the same chain of rules. This will never need to
    // hold more than one item when pointing in this direction, part
    // of why it's easier to build the graph backwards.
    c4m_earley_item_t *previous_scan;
    // This one only gets set for states that are starts of rules.  It
    // holds a cache that holds in-progress subtrees during the
    // tree-building process.
    c4m_dict_t        *tree_cache;
    // This basically represents all potential parent nodes in the tree
    // above the node we're currently processing.
    c4m_set_t         *starts;
    // Any item we predict, if we predicted. This basically tracks
    // subgraph starts, and the next one tracks subgraph ends.
    c4m_set_t         *predictions;
    // Earley items that, (like Jerry McGuire?), complete us. This is
    // used in conjunction with the prior one to identify all matching
    // derrivations for a non-terminal when we have an ambiguous
    // grammar.
    c4m_set_t         *completors;
    // If we predict a null, we want to track that for tree-building,
    // to make sure we add an empty string as an option at this spot.
    // A pointer to the actual rule contents we represent.
    c4m_list_t        *rule;
    // Static info about the current group.
    c4m_rule_group_t  *group;
    // Used to cache info related to the state during tree-building.
    c4m_dict_t        *cache;
    // The `rule_id` is the non-terminal we pulled the rule from.
    // The non-terminal can contain multiple rules though.
    int32_t            ruleset_id;
    // This is the field that distinguishes which rule in the ruleset
    // we pulled. This isn't all that useful given we have the pointer
    // to the rule, but I keep it around for debuggability.
    int32_t            rule_index;
    // Which item in the rule are we to process (the 'dot')?
    int32_t            cursor;
    // If we're a group item, how many have we matched?
    int32_t            match_ct;
    // Thhe next two fields is the recognizer's state machine
    // location, mainly useful for I/O, but since we have it, we use
    // it over the state arrays in the parser.
    //
    // Basically, each 'state' in the Early algorithm processes one
    // token of input stream, and generates a bunch of so-called Early
    // Items in that state. You *could* think about them as sub-states
    // for sure. But I stick with the algorithm's lingo, so we track
    // here both the state this item was created in, as well as the
    // index into that state in which we live.
    int32_t            estate_id;
    int32_t            eitem_index;
    // This tracks any penalty the grammar associates with this rule, so
    // that we can apply it during graph building.
    int                penalty;

    // In the classic Earley algorithm, this operation to perform on a
    // state is not selected when we generate the state; it's
    // generated by looking at the item when we get to it for
    // processing.
    //
    // However, we absolutely *do* have all the information we need to
    // determine the operation when we create the state, which is what
    // I am doing. I do it this way because, with the addition of
    // groups, it is the best, most intuitive mechanism in my view for
    // create the extra helper state we need that represents the
    // difference between starting a group node in the tree, and
    // starting the first group item. For that reason alone, it's
    // easier for us to just set the operation when we generate the
    // state, so we do that for everything now.
    //
    // To that end, whereas the raw Earley algorithm only uses
    // 'predict', 'scan' and 'complete', we basically split out
    // prediction into 'predict non-terminal', 'predict group start',
    // and 'predict group item'. You can think of 'scan' as 'predict
    // terminal'; that's exactly what it is, as we do add the state
    // when we see a terminal, even before we've checked to see if it
    // matches the input (we don't move the cursor to the right of
    // terminals unless the scan operation can succeed).
    //
    // In some ways, that would be more regular, but I continue to
    // call it 'scan' because it's aligned with the classic algorithm.
    c4m_earley_op      op;
    // When we predict a group we seprately predict items. So for
    // groups it would be natural for to write Earley states like:
    //
    // >>anonymous_group<< := • (<<Digit>>)
    //
    // But to make tree building easy, we really need to keep groups
    // separate from their items.  So Right now, we bascially use this
    // to denote that a state is associated with the top node, not the
    // items.
    //
    // We'll represent it like this:
    // >>group<< := •• (<<Digit>> <<Any>>)+
    //
    // And the items as:
    // >>item<< := • (<<Digit>> <<Any>>)+
    //
    // And the completion versions:
    // >>item<< := (<<Digit>> <<Any>>)+ •
    // >>group<< := (<<Digit>> <<Any>>)+ ••
    //
    // The individual item nodes don't need to keep count of matches;
    // we do for convenience, but it can lead to more Earley states
    // than necessary for sure.
    //
    // Also note that any time a group is first predicted, it's
    // randomly assigned an ID right now. I'll probably change that and
    // assign them hidden non-terminal IDs statically.
    bool               double_dot;
    // This strictly isn't necessary, but it's been a nice helper for
    // me.  The basic idea is that subtrees span from a start item to
    // an end item. The start item will result from a prediction (in
    // practice, the cursor will be at 0), and the end item will be
    // where the cursor is at the end.
    //
    // In ambiguous grammars, this can resilt in an n:m mapping of
    // valid subtrees. So when tree-building, we cache subtrees in the
    // earley item associated with the start, using a dictionary where
    // the key is the END early item, and the payload is the node info
    // for that particular possible subtree.
    //
    // This helper allows us to look at the raw state machine output
    // and be able to tell whether the EI is the start or end of a
    // subtree, and if so, whether the subtree constitutes a group, an
    // item in a group, or a vanilla non-terminal.
    c4m_subtree_info_t subtree_info;
};

struct c4m_grammar_t {
    c4m_list_t  *named_terms;
    c4m_list_t  *rules;
    c4m_dict_t  *rule_map;
    c4m_dict_t  *terminal_map;
    c4m_dict_t  *penalty_map;
    c4m_pitem_t *penalty;
    c4m_pitem_t *penalty_empty;
    int32_t      default_start;
};

struct c4m_earley_state_t {
    // The token associated with the current state;
    c4m_token_info_t *token;
    // When tree-building, any ambiguous parse can share the leaf.
    c4m_tree_node_t  *cache;
    c4m_list_t       *items;
    int               id;
};

// Token iterators get passed the parse context, and can assign to the
// second parameter a memory address associated with the yielded
// token.  This gets put into the 'info' field of the token we
// produce.
//
// The callback should return the special C4M_TOK_EOF when there are
// no more tokens. Note, if the token is not in the parser's set,
// there will not be an explicit error directly-- the token might be a
// unicode codepoint that match a builtin class, or else will still
// match when the rule specifies the C4M_P_ANY terminal.
typedef int64_t (*c4m_tokenizer_fn)(c4m_parser_t *, void **);

struct c4m_parser_t {
    c4m_grammar_t      *grammar;
    c4m_list_t         *states;
    c4m_list_t         *roots;
    c4m_earley_state_t *current_state;
    c4m_earley_state_t *next_state;
    void               *user_context;
    // The tokenization callback should set this if the token ID
    // returned isn't recognized, or if the value wasn't otherwise set
    // when initializing the corresponding non-terminal.
    c4m_list_t         *parse_trees;
    void               *token_cache;
    c4m_tokenizer_fn    tokenizer;
    bool                run;
    int32_t             start;
    int32_t             position;       // Absolute pos in tok stream
    int32_t             current_line;   // 1-indexed.
    int32_t             current_column; // 0-indexed.
    int32_t             cur_item_index; // Current ix in inner loop.
    int32_t             fnode_count;    // Next unique node ID
    bool                ignore_escaped_newlines;
    bool                preloaded_tokens;
};

// Any registered terminals that we give IDs will be strings. If the
// string is only one character, we reuse the unicode code point, and
// don't bother registering. All terminals of more than one character
// start with this prefix.
//
// Ensuring that single-digit terminals match their code point makes
// it easier to provide a simple API for cases where we want to
// tokenize a string into single codepoints... we don't have to
// register or translate anything.

#define C4M_START_TOK_ID       0x40000000
#define C4M_TOK_OTHER          -3
#define C4M_TOK_IGNORED        -2 // Passthrough tokens, like whitespace.
#define C4M_TOK_EOF            -1
#define C4M_EMPTY_STRING       -126
#define C4M_GID_SHOW_GROUP_LHS -255

#define C4M_IX_START_OF_PROGRAM -1

extern c4m_pitem_t *c4m_group_items(c4m_grammar_t *p,
                                    c4m_list_t    *pitems,
                                    int            min,
                                    int            max);
// Does not try to do automatic error recovery.
extern void         c4m_ruleset_raw_add_rule(c4m_grammar_t *,
                                             c4m_nonterm_t *,
                                             c4m_list_t *);
extern void         c4m_ruleset_add_rule(c4m_grammar_t *,
                                         c4m_nonterm_t *,
                                         c4m_list_t *);
extern void         c4m_parser_set_default_start(c4m_grammar_t *,
                                                 c4m_nonterm_t *);
extern void         c4m_internal_parse(c4m_parser_t *, c4m_nonterm_t *);

extern void           c4m_parse_token_list(c4m_parser_t *,
                                           c4m_list_t *,
                                           c4m_nonterm_t *);
extern void           c4m_parse_string(c4m_parser_t *,
                                       c4m_str_t *,
                                       c4m_nonterm_t *);
extern void           c4m_parse_string_list(c4m_parser_t *,
                                            c4m_list_t *,
                                            c4m_nonterm_t *);
extern c4m_nonterm_t *c4m_pitem_get_ruleset(c4m_grammar_t *, c4m_pitem_t *);

extern c4m_grid_t *c4m_grammar_to_grid(c4m_grammar_t *);
extern c4m_grid_t *c4m_parse_state_format(c4m_parser_t *, bool);
extern c4m_grid_t *c4m_forest_format(c4m_parser_t *);
extern c4m_utf8_t *c4m_repr_token_info(c4m_token_info_t *);
extern int64_t     c4m_token_stream_codepoints(c4m_parser_t *, void **);
extern int64_t     c4m_token_stream_strings(c4m_parser_t *, void **);
extern c4m_grid_t *c4m_get_parse_state(c4m_parser_t *, bool);
extern c4m_grid_t *c4m_format_parse_state(c4m_parser_t *, bool);
extern c4m_grid_t *c4m_grammar_format(c4m_grammar_t *);
extern void        c4m_parser_reset(c4m_parser_t *);
extern c4m_utf8_t *c4m_repr_parse_node(c4m_parse_node_t *);

static inline c4m_pitem_t *
c4m_new_pitem(c4m_pitem_kind kind)
{
    c4m_pitem_t *result = c4m_gc_alloc_mapped(c4m_pitem_t, C4M_GC_SCAN_ALL);

    result->kind = kind;

    return result;
}

static inline c4m_pitem_t *
c4m_pitem_terminal_from_int(c4m_grammar_t *g, int64_t n)
{
    c4m_pitem_t    *result = c4m_new_pitem(C4M_P_TERMINAL);
    c4m_terminal_t *term;
    int             ix;

    result->contents.terminal = n;
    ix                        = n - C4M_START_TOK_ID;
    term                      = c4m_list_get(g->named_terms, ix, NULL);
    result->s                 = term->value;

    return result;
}

static inline c4m_pitem_t *
c4m_pitem_term_raw(c4m_grammar_t *p, c4m_utf8_t *name)
{
    c4m_pitem_t    *result    = c4m_new_pitem(C4M_P_TERMINAL);
    c4m_terminal_t *tok       = c4m_new(c4m_type_terminal(), p, name);
    result->contents.terminal = tok->id;

    return result;
}

static inline c4m_pitem_t *
c4m_pitem_from_nt(c4m_nonterm_t *nt)
{
    c4m_pitem_t *result      = c4m_new_pitem(C4M_P_NT);
    result->contents.nonterm = nt->id;
    result->s                = nt->name;

    return result;
}

static inline int64_t
c4m_grammar_add_term(c4m_grammar_t *g, c4m_utf8_t *s)
{
    c4m_pitem_t *pi = c4m_pitem_term_raw(g, s);
    return pi->contents.terminal;
}

static inline c4m_pitem_t *
c4m_pitem_terminal_cp(c4m_codepoint_t cp)
{
    c4m_pitem_t *result       = c4m_new_pitem(C4M_P_TERMINAL);
    result->contents.terminal = cp;

    return result;
}

static inline c4m_pitem_t *
c4m_pitem_nonterm_raw(c4m_grammar_t *p, c4m_utf8_t *name)
{
    c4m_pitem_t   *result    = c4m_new_pitem(C4M_P_NT);
    c4m_nonterm_t *nt        = c4m_new(c4m_type_ruleset(), p, name);
    result->contents.nonterm = nt->id;

    return result;
}

static inline c4m_pitem_t *
c4m_pitem_choice_raw(c4m_grammar_t *p, c4m_list_t *choices)
{
    c4m_pitem_t *result    = c4m_new_pitem(C4M_P_SET);
    result->contents.items = choices;

    return result;
}

static inline c4m_pitem_t *
c4m_pitem_any_terminal_raw(c4m_grammar_t *p)
{
    c4m_pitem_t *result = c4m_new_pitem(C4M_P_ANY);

    return result;
}

static inline c4m_pitem_t *
c4m_pitem_builtin_raw(c4m_bi_class_t class)
{
    // Builtin character classes.
    c4m_pitem_t *result    = c4m_new_pitem(C4M_P_BI_CLASS);
    result->contents.class = class;

    return result;
}

static inline void
c4m_ruleset_add_empty_rule(c4m_nonterm_t *nonterm)
{
    // Not bothering to check for dupes.
    c4m_pitem_t *item = c4m_new_pitem(C4M_P_NULL);
    c4m_list_t  *rule = c4m_list(c4m_type_ref());
    c4m_list_append(rule, item);
    c4m_list_append(nonterm->rules, rule);

    // The empty rule always makes the whole production nullable, no
    // need to recurse.
    nonterm->nullable = true;
}

static inline void
c4m_grammar_set_default_start(c4m_grammar_t *grammar, c4m_nonterm_t *nt)
{
    grammar->default_start = nt->id;
}

static inline void
c4m_ruleset_set_user_data(c4m_nonterm_t *nonterm, void *data)
{
    nonterm->user_data = data;
}

static inline void
c4m_terminal_set_user_data(c4m_terminal_t *term, void *data)
{
    term->user_data = data;
}

static inline c4m_list_t *
c4m_parse_get_parses(c4m_parser_t *parser)
{
    return parser->parse_trees;
}

static inline c4m_grid_t *
c4m_parse_tree_format(c4m_tree_node_t *t)
{
    return c4m_grid_tree_new(t,
                             c4m_kw("callback", c4m_ka(c4m_repr_parse_node)));
}

static inline void *
c4m_parse_get_user_data(c4m_grammar_t *g, c4m_parse_node_t *node)
{
    int64_t id;

    if (!node || node->group_item) {
        return NULL;
    }

    if (node->token) {
        id = node->info.token->tid;
        if (id < C4M_START_TOK_ID) {
            return NULL;
        }

        id -= C4M_START_TOK_ID;
        if (id >= c4m_list_len(g->named_terms)) {
            return NULL;
        }

        c4m_terminal_t *t = c4m_list_get(g->named_terms, id, NULL);

        return t->user_data;
    }

    id = node->id;

    c4m_nonterm_t *nt = c4m_list_get(g->rules, id, NULL);
    return nt->user_data;
}

#ifdef C4M_USE_INTERNAL_API
#define C4M_GROUP_ID 1 << 28

#define C4M_MAX_PARSE_PENALTY 2

extern c4m_nonterm_t *c4m_get_nonterm(c4m_grammar_t *, int64_t);
extern c4m_utf8_t    *c4m_repr_rule(c4m_grammar_t *, c4m_list_t *, int);
extern c4m_list_t    *c4m_repr_earley_item(c4m_parser_t *,
                                           c4m_earley_item_t *,
                                           int);
extern c4m_utf8_t    *c4m_repr_nonterm(c4m_grammar_t *, int64_t, bool);
extern c4m_utf8_t    *c4m_repr_group(c4m_grammar_t *, c4m_rule_group_t *);
extern c4m_utf8_t    *c4m_repr_term(c4m_grammar_t *, int64_t);
extern c4m_utf8_t    *c4m_repr_rule(c4m_grammar_t *, c4m_list_t *, int);
extern c4m_utf8_t    *c4m_repr_token_info(c4m_token_info_t *);
extern c4m_grid_t    *c4m_repr_state_table(c4m_parser_t *, bool);
extern c4m_list_t    *c4m_find_all_trees(c4m_parser_t *);
extern void           c4m_parser_load_token(c4m_parser_t *);
extern c4m_grid_t    *c4m_get_parse_state(c4m_parser_t *, bool);
extern void           c4m_print_states(c4m_parser_t *, c4m_list_t *);

static inline bool
c4m_is_non_terminal(c4m_pitem_t *pitem)
{
    switch (pitem->kind) {
    case C4M_P_NT:
    case C4M_P_GROUP:
        return true;
    default:
        return false;
    }
}

static inline c4m_earley_state_t *
c4m_new_earley_state(int id)
{
    c4m_earley_state_t *result;

    result = c4m_gc_alloc_mapped(c4m_earley_state_t, C4M_GC_SCAN_ALL);

    // List of c4m_earley_item_t objects.
    result->items = c4m_list(c4m_type_ref());
    result->id    = id;

    return result;
}

#endif

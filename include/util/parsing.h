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
    C4M_EO_PREDICT,
    C4M_EO_SCAN,
    C4M_EO_COMPLETE,
} c4m_earley_op;

// Earley's algorithm is a recognizer, and doesn't produce parse
// trees (or forests of trees) directly.
//
// Currently taking what is probably too simple an approach, but will
// dig into literature if necessary.
typedef enum : int64_t {
    C4M_FOREST_ROOT,
    C4M_FOREST_TERM,
    C4M_FOREST_GROUP_TOP,
    C4M_FOREST_GROUP_ITEM,
    C4M_FOREST_NONTERM,
} c4m_forest_node_kind;

typedef struct c4m_terminal_t    c4m_terminal_t;
typedef struct c4m_nonterm_t     c4m_nonterm_t;
typedef struct c4m_rule_group_t  c4m_rule_group_t;
typedef struct c4m_pitem_t       c4m_pitem_t;
typedef struct c4m_token_info_t  c4m_token_info_t;
typedef struct c4m_parser_t      c4m_parser_t;
typedef struct c4m_forest_item_t c4m_forest_item_t;
typedef struct c4m_egroup_info_t c4m_egroup_info_t;
typedef struct c4m_earley_item_t c4m_earley_item_t;
typedef struct c4m_grammar_t     c4m_grammar_t;
typedef struct c4m_early_state_t c4m_earley_state_t;

struct c4m_terminal_t {
    c4m_utf8_t *value;
    int64_t     id;
};

struct c4m_nonterm_t {
    c4m_utf8_t *name;
    c4m_list_t *rules; // A list of c4m_prule_t objects;
    int64_t     id;
    bool        nullable;
};

struct c4m_rule_group_t {
    c4m_list_t *items;
    int         min;
    int         max;
};

struct c4m_pitem_t {
    c4m_utf8_t *s;

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
};

struct c4m_forest_item_t {
    c4m_list_t *kids;
    union {
        c4m_token_info_t *token;
        c4m_utf8_t       *name;
    } info;
    int64_t              id;
    c4m_forest_node_kind kind;
};

struct c4m_egroup_info_t {
    c4m_earley_item_t *prev_item_end;
    c4m_earley_item_t *prev_item_start;
    c4m_earley_item_t *next_item_start;
    c4m_earley_item_t *true_start;
    c4m_rule_group_t  *group;
};

struct c4m_earley_item_t {
    // The predicted item in our cur rule.
    c4m_earley_item_t  *start_item;
    c4m_earley_state_t *start_state;
    c4m_egroup_info_t  *ginfo;
    // This one is just for debugability, and only gets
    // set for scans and completes via copy_earley_item(),
    // since prediction parents are stored in start_item.
    c4m_earley_item_t  *creation_item;
    c4m_earley_item_t  *previous_scan;
    c4m_list_t         *rule;
    c4m_list_t         *possible_parents;
    int32_t             ruleset_id;
    // Which item in the rule (the 'dot')?
    int32_t             cursor;
    // This should prob move into the group object.
    int32_t             match_ct;
    int32_t             estate_id;
    int32_t             eitem_index;
    // Which rule in the ruleset?
    int32_t             rule_index;
    c4m_earley_op       op;
    bool                root_item;
};

struct c4m_grammar_t {
    c4m_list_t *named_terms;
    c4m_list_t *rules;
    c4m_dict_t *rule_map;
    c4m_dict_t *terminal_map;
    int32_t     default_start;
};

struct c4m_early_state_t {
    // The token associated with the current state;
    c4m_token_info_t *token;
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
    c4m_earley_state_t *current_state;
    c4m_earley_state_t *next_state;
    void               *user_context;
    // The tokenization callback should set this if the token ID
    // returned isn't recognized, or if the value wasn't otherwise set
    // when initializing the corresponding non-terminal.
    void               *token_cache;
    c4m_tokenizer_fn    tokenizer;
    bool                run;
    int32_t             start;
    int32_t             position;       // Absolute pos in tok stream
    int32_t             current_line;   // 1-indexed.
    int32_t             current_column; // 0-indexed.
    int32_t             cur_item_index; // Current ix in inner loop.
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

#define C4M_START_TOK_ID      0x40000000
#define C4M_TOK_OTHER         -3
#define C4M_TOK_IGNORED       -2 // Passthrough tokens, like whitespace.
#define C4M_TOK_EOF           -1
#define C4M_FOREST_ROOT_NODE  -127
#define C4M_EMPTY_STRING_NODE -126

#define C4M_IX_START_OF_PROGRAM -1

extern void
                    c4m_ruleset_add_empty_rule(c4m_nonterm_t *);
extern c4m_pitem_t *c4m_pitem_terminal_raw(c4m_grammar_t *,
                                           c4m_utf8_t *);
extern c4m_pitem_t *c4m_pitem_nonterm_raw(c4m_grammar_t *, c4m_utf8_t *);
extern c4m_pitem_t *c4m_pitem_choice_raw(c4m_grammar_t *, c4m_list_t *);
extern c4m_pitem_t *c4m_pitem_any_terminal_raw(c4m_grammar_t *);
extern c4m_pitem_t *c4m_pitem_builtin_raw(c4m_bi_class_t);
extern c4m_pitem_t *c4m_pitem_terminal_cp(c4m_codepoint_t);
extern c4m_pitem_t *c4m_group_items(c4m_grammar_t *p,
                                    c4m_list_t    *pitems,
                                    int            min,
                                    int            max);
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

extern c4m_grid_t      *c4m_grammar_to_grid(c4m_grammar_t *);
extern c4m_grid_t      *c4m_parse_to_grid(c4m_parser_t *, bool);
extern c4m_tree_node_t *c4m_parse_get_parses(c4m_parser_t *);
extern c4m_grid_t      *c4m_forest_format(c4m_tree_node_t *);

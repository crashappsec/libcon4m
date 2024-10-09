#include "con4m.h"
#pragma once

typedef enum {
    C4M_GOPT_FLAG_OFF          = 0,
    // By default we are lax w/ the equals sign.  If you really want
    // the usuability nightmare of disallowing spaces around it, then
    // okay... this generates a warning if an equals sign is at the start of
    // a token, and then tokenizes the whole thing as a word.
    //
    // Handled when lexing.
    C4M_GOPT_NO_EQ_SPACING     = 0x0001,
    // --flag foo is normally accepted if the flag takes any sort
    // of argument.  But with this on, if the argument is
    // optional, the = is required.
    //
    // If the argument is not optional, we still allow no equals;
    // Perhaps could be talked into an option to change that.
    //
    // Handled when parsing.
    C4M_GOPT_ALWAYS_REQUIRE_EQ = 0x0002,
    // When on, -dbar is treated as "-d bar" if -d takes an argument,
    // or -d -b -a -r if they do not. The first flag name seen that
    // takes an argument triggers the rest of the word being treated
    // as an argument.
    //
    // When off, -dbar and --dbar are treated interchangably, and we only
    // generate the flag_prefix_1 token for flags.
    //
    // Handled in lexing.
    C4M_GOPT_RESPECT_DASH_LEN  = 0x0004,
    // By default a solo -- will cause the rest of the arguments to be
    // passed through as non-flag tokens (the words still get tag
    // types, and get comma separated).
    //
    // Turn this on to disable that behavior; -- will be a flag with
    // no name (generally an error).
    //
    // Handled in lexing.
    C4M_GOPT_NO_DOUBLE_DASH    = 0x0008,
    // By default, a solo - is passed through as a word, since it
    // usually implies stdin where a file name is expected. But if you
    // don't want that hassle, turn this on, then it'll be treated as
    // a badly formed flag (one with a null name).
    //
    // Handled in lexing.
    C4M_GOPT_SINGLE_DASH_ERR   = 0x0010,
    // Options on what we consider a boolean value.  If they're
    // all off, boolean flags will be errors.
    //
    // This is handled the same across the entire parse for
    // consistency, and is not a per-flag or per-command option.
    // Handled in the lexer.
    C4M_GOPT_BOOL_NO_TF        = 0x0020,
    C4M_GOPT_BOOL_NO_YN        = 0x0040,
    // If the same flag 'key' is provided multiple times, we usually allow it,
    // assuming that the last one we see overrides previous instances, as is
    // often the case in unix. If this gets turned on, if you do not
    // explicitly mark something as 'multi', then dupes will be an
    // error.
    // To handle in a lex post-processing step.
    C4M_GOPT_ERR_ON_DUPES      = 0x080,
    // By default, we are case INsensitive with flags; but if you need
    // case sensitivity, here you go. Note that this is partially
    // implemented by transforming strings when you register flags and
    // subcommands, so set it before you start setting up stuff.
    //
    // Note that I don't think it's good form when you do allow case
    // sensitivity for flags to allow flag names that only differ in
    // capitalization.
    //
    // Handled in lexer.
    C4M_CASE_SENSITIVE         = 0x0100,
    // If true, the lexer accepts /flag per Windows; it accepts both,
    // but can warn if you mix/match.
    //
    // Handled in lex.
    C4M_ALLOW_WINDOWS_SYNTAX   = 0x0200,
    // If true, the colon is also allowed where an equals sign may
    // appear. We only support this because Chalk is in Nim, and want
    // to follow their tropes; the equals sign cannot currently be
    // disabled, and I have no intention of doing so.
    // Handled in lex.
    C4M_ALLOW_COLON_SEPARATOR  = 0x0400,
    //
    // When this is true, the top-level 'command' is taken from the
    // command name, in which case if the command name is not a known
    // command, a special token OTHER_COMMAND_NAME is generated.
    // Handled in lex.
    C4M_TOPLEVEL_IS_ARGV0      = 0x0800,
    //
    // This option causes subcommand names to be treated as plain old
    // words inside any sub-command where the word doesn't constitute
    // a sub-command itself. Eventually, we might also add it all
    // places BEFORE the command's last nullable piece, when possible.
    //
    // For now though, this is not implemented at all yet, and
    // subcommands are subcommands, even if they're in the wrong
    // place.
    //
    // So this is just TODO.
    C4M_ACCEPT_SUBS_AS_WORDS   = 0x1000,
    //
    //
    C4M_BAD_GOPT_WARN          = 0x2000,
} c4m_gopt_global_flags;

// These token types are always available; flag names and command names
// get their own token IDs as well, which are handed out in the order
// they're registered.
//
// These tokens are really meant to be internal to the generated
// parser.  Most of them are elided when we produce output to the
// user.
typedef enum {
    C4M_GOTT_ASSIGN             = 0,
    C4M_GOTT_COMMA              = 1, // separate items for multi-words.
    C4M_GOTT_INT                = 2,
    C4M_GOTT_FLOAT              = 3,
    C4M_GOTT_BOOL_T             = 4,
    C4M_GOTT_BOOL_F             = 5,
    C4M_GOTT_WORD               = 6,
    // You can differentiate behavior based on argv[0];
    C4M_GOTT_OTHER_COMMAND_NAME = 7,
    C4M_GOTT_UNKNOWN_OPT        = 8,
    C4M_GOTT_LAST_BI            = 9,
} c4m_gopt_bi_ttype;

// The above values show up in *tokens* we generate.  However, when
// specifying a 'rule' for a command or subcommand, before we convert
// to an earley grammar, the api has the user pass a list of either
// the below IDs, or the id associated with commands.
//
// Note that any sub-command always has to be the last thing in any
// given specification, if it exists. You then specify the rules for
// that subcommand seprately.
//
// Similarly, nothing can appear after RAW; it acts like a '--' at the
// command line, and passes absolutely everything through.
//
// Parenthesis have to be nested, and OPT, STAR and PLUS modify
// whatever they follow (use the parens where needed).

typedef enum : int64_t {
    C4M_GOG_NONE,
    C4M_GOG_WORD = 1,
    C4M_GOG_INT,
    C4M_GOG_FLOAT,
    C4M_GOG_BOOL,
    C4M_GOG_RAW,
    C4M_GOG_LPAREN,
    C4M_GOG_RPAREN,
    C4M_GOG_OPT,
    C4M_GOG_STAR,
    C4M_GOG_PLUS,
    C4M_GOG_NUM_GRAMMAR_CONSTS,
} c4m_gopt_grammar_consts;

// For any flag type, you can make arguments optional, or accept
// multiple arguments.
//
// Internally, we deal with 'any number of args' by looking for the
// max value being negative.
typedef enum {
    C4M_GOAT_NONE, // No option is allowed.
    // For boolean flags, T_DEFAULT means the flag name is assumed
    // to be true if no argument is given, but can be overridden with
    // a boolean argument.
    C4M_GOAT_BOOL_T_DEFAULT,
    C4M_GOAT_BOOL_F_DEFAULT,
    C4M_GOAT_BOOL_T_ALWAYS,
    C4M_GOAT_BOOL_F_ALWAYS,
    C4M_GOAT_WORD, // Any value as a string; could be an INT, FLOAT or bool.
    C4M_GOAT_INT,
    C4M_GOAT_FLOAT,
    C4M_GOAT_CHOICE,         // User-supplied list of options
    C4M_GOAT_CHOICE_T_ALIAS, // Boolean flag that invokes a particular choice.
    C4M_GOAT_CHOICE_F_ALIAS, // Boolean flag that clears a particular choice.
    C4M_GOAT_LAST,
} c4m_flag_arg_type;

// This enum is used to categorize forest nonterms to help us more
// easily walk the graph. The non-terminal's 'user data' field, when
// set, becomes available through the forest node, if we keep the
// parse ctx around.
typedef enum : int64_t {
    C4M_GTNT_OTHER,
    C4M_GTNT_OPT_JUNK_MULTI,
    C4M_GTNT_OPT_JUNK_SOLO,
    C4M_GTNT_CMD_NAME,
    C4M_GTNT_CMD_RULE,
    C4M_GTNT_OPTION_RULE,
    C4M_GTNT_FLOAT_NT,
    C4M_GTNT_INT_NT,
    C4M_GTNT_BOOL_NT,
    C4M_GTNT_WORD_NT,
} c4m_gopt_node_type;

typedef struct c4m_goption_t {
    c4m_utf8_t            *name;
    c4m_utf8_t            *normalized;
    c4m_utf8_t            *short_doc;
    c4m_utf8_t            *long_doc;
    c4m_list_t            *choices;
    c4m_nonterm_t         *my_nonterm;
    // Token id is the unique ID associated with the specific flag name.
    int64_t                token_id;
    // If linked to a command, the flag will only be allowed
    // in cases where the command is supplied (or inferred).
    struct c4m_gopt_cspec *linked_command;
    struct c4m_goption_t  *linked_option;
    // Parse results come back from the raw API in a dictionary keyed by
    // result key.
    int64_t                result_key;
    int16_t                min_args;
    int16_t                max_args;
    // If multiple flags w/ the same result key, this is the main one for the
    // purposes of documentation.
    bool                   primary;
    c4m_flag_arg_type      type;
} c4m_goption_t;

typedef struct c4m_gopt_cspec {
    struct c4m_gopt_ctx   *context;
    struct c4m_gopt_cspec *parent;
    c4m_nonterm_t         *name_nt;
    c4m_nonterm_t         *rule_nt;
    c4m_utf8_t            *name;
    int64_t                token_id;
    uint32_t               seq; // Used for tracking valid flags per rule.
    c4m_utf8_t            *short_doc;
    c4m_utf8_t            *long_doc;
    c4m_dict_t            *sub_commands;
    c4m_list_t            *aliases;
    c4m_list_t            *owned_opts;
    // By default, command opts must match known names.  If this is
    // on, it will pass through bad flag names (inapropriate to the
    // context or totally unknown), treating them as arguments (of
    // type WORD).
    bool                   bad_opt_passthru;
    // This causes warnings to be printed for bad opts, as opposed to
    // them being fatal errors. If ignore_bad_opts is also on, then
    // bad opts both produce warnings and get converted to words.
    // Otherwise, (if bad_opt_passthrough is off, and this is on), bad
    // opts warn but are dropped from the input.
    bool                   bad_opt_warn;
    // Whether the subcommand exists in an explicit parent rule.
    bool                   explicit_parent_rule;
} c4m_gopt_cspec;

typedef struct c4m_gopt_ctx {
    c4m_grammar_t *grammar;
    c4m_dict_t    *all_options;
    c4m_dict_t    *sub_names;
    c4m_dict_t    *primary_options;
    c4m_dict_t    *top_specs;
    c4m_list_t    *tokens;
    c4m_list_t    *terminal_info;
    c4m_nonterm_t *nt_start;
    c4m_nonterm_t *nt_float;
    c4m_nonterm_t *nt_int;
    c4m_nonterm_t *nt_word;
    c4m_nonterm_t *nt_bool;
    c4m_nonterm_t *nt_eq;
    c4m_nonterm_t *nt_opts;
    c4m_nonterm_t *nt_1opt;
    int64_t        default_command;
    uint64_t       counter;
    // See the global flags array above.
    uint32_t       options;
    bool           finalized;
    bool           show_debug;
} c4m_gopt_ctx;

typedef struct c4m_gopt_cmd_rule {
    c4m_list_t *items;
    c4m_list_t *all_sub_toks;
} c4m_gopt_cmd_rule;

typedef struct {
    c4m_gopt_ctx *gctx;
    c4m_utf8_t   *command_name;
    c4m_utf8_t   *normalized_name;
    c4m_list_t   *words;
    c4m_utf8_t   *word;
    c4m_utf32_t  *raw_word;
    int           num_words;
    int           word_id;
    int           cur_wordlen;
    int           cur_word_position;
    bool          force_word;
    bool          all_words;
} c4m_gopt_lex_state;

typedef struct {
    c4m_utf8_t *cmd;
    c4m_dict_t *flags;
    c4m_dict_t *args;
    c4m_list_t *errors;
} c4m_gopt_result_t;

typedef struct {
    c4m_list_t      *flag_nodes;
    c4m_gopt_cspec  *cur_cmd;
    c4m_dict_t      *flags;
    c4m_dict_t      *args;
    c4m_tree_node_t *intree;
    c4m_gopt_ctx    *gctx;
    c4m_utf8_t      *path;
    c4m_utf8_t      *deepest_path;
    c4m_list_t      *errors;
    c4m_parser_t    *parser;
} c4m_gopt_extraction_ctx;

typedef struct {
    c4m_utf8_t    *cmd;
    c4m_goption_t *spec;
    c4m_obj_t      value;
    int            n; // Number of items stored.
} c4m_rt_option_t;

extern c4m_list_t *c4m_gopt_parse(c4m_gopt_ctx *, c4m_str_t *, c4m_list_t *);
extern void        c4m_gopt_command_add_rule(c4m_gopt_cspec *,
                                             c4m_list_t *);
extern c4m_list_t *_c4m_gopt_rule(int64_t, ...);
#define c4m_gopt_rule(s, ...) _c4m_gopt_rule(s, C4M_VA(__VA_ARGS__))

void c4m_gopt_add_subcommand(c4m_gopt_cspec *, c4m_utf8_t *);

static inline c4m_list_t *
c4m_gopt_rule_any_word()
{
    return c4m_gopt_rule(C4M_GOG_LPAREN,
                         C4M_GOG_WORD,
                         C4M_GOG_RPAREN,
                         C4M_GOG_STAR);
}

static inline c4m_list_t *
c4m_gopt_rule_optional_word()
{
    return c4m_gopt_rule(C4M_GOG_LPAREN,
                         C4M_GOG_WORD,
                         C4M_GOG_RPAREN,
                         C4M_GOG_OPT);
}
#ifdef C4M_USE_INTERNAL_API
void c4m_gopt_tokenize(c4m_gopt_ctx *, c4m_utf8_t *, c4m_list_t *);
void c4m_gopt_finalize(c4m_gopt_ctx *);

static inline bool
c4m_gctx_gflag_is_set(c4m_gopt_ctx *ctx, uint32_t flag)
{
    return (bool)ctx->options & flag;
}

static inline bool
c4m_gopt_gflag_is_set(c4m_gopt_lex_state *state, uint32_t flag)
{
    return c4m_gctx_gflag_is_set(state->gctx, flag);
}
#endif

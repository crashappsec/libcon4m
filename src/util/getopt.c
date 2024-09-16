// The API is set up so that you should set things up as so:
//
// 1. Create the getopt context.
// 2. Add your commands, and any rules.
// 3. Add flags, optionally attaching them to particular commands
//    (they're always available below the command until there
//     is a command that passes through everything).
// 4. Finalize the parser (automatically happens if needed when using
//    it).
//
//
// Each flag generates a parse rule just for that flag. Each
// subcommand generate a parse rule for the set of flags specific to
// it, that it might accept.
//
// in-scope flag rules will get dropped in between every RHS item
// that makes sense.
//
//
// Sub-commands that are added should be named in rules, but if you
// don't, we automatically add a rule that takes no argument except
// the subcommand.
//
//
// Let's take `chalk` as an example. The `inspect` subcommand itself
// can take a list of items as arguments, or it can take subcommands
// that themselves may or may not take commands. For instance, you can
// do 'chalk inspect containers`, or `chalk inspect all`, and I think
// you can optionally add a list of containers.  The `chalk inspect`
// command, when passing the grammar, only specifies its arguments and
// any sub-commands.
//
// So essentially, you'd add rules like (in EBNF form):
//
// inspect ::= (ARG)*
// inspect ::= containers
// inspect ::= all
//
// You *can* add arguments before the sub-command.
//
// Note that when you add a subcommand like 'containers' to the
// grammar, it becomes its own token, and is not accepted within the
// current command that has it as a subcommand. As such, it will not
// matche 'ARG' for that sub-command (it will be accepted in other
// parts of the tree that do not have 'command' as a name).  In our
// rule scheme, if you wanted to accept the word 'containers', even
// though it might end up ambiguous, you are currently out of luck.
// I might provide a way to specify an exception in the future, but
// I am trying to keep it simple.
//
// Another artifical limitation is that the sub-command, if any, can
// only appear LAST right now. Sure, it's not unreasonable to want to
// have sub-command rules in the middle of the grammar the user
// provides.
//
// But for the moment, I'm going to air on the side of what I think is
// most inutitive, which is not to treat any of it like the user
// understands BNF.
//
// By the way, in-scope flags will end up grouped based on their
// command and dropped between any symbols in the grammar.
//
// For instance, if we have inspect-specific flags attached to the top
// 'inspect' command, then the 'inspect' rules above will
// translate to something like:
//
// toplevel_flags: ('--someflag', WORD | ... | BAD_FLAG) *
// iflags_1: (toplevel_flags | inspect_flags)*
// inspect_rule_1: iflags_1 'inspect' iflags1 (ARG iflags1)* iflags_1
// iflags_2: (iflags_1 | containers_flags)*
// inspect_rule_2: iflags_2 'inspect' iflags2 containers_rule iflags_2
//
// And so on.
//
// The grammar rules for the flags are added automatically as we add
// flags.
//
// No information on flags whatsoever is accepted on the spec the user
// gives.  Note that the 'finalization' stage is important, because we
// don't actually add grammar productions for COMMAND bodies until
// this stage (even though we do add rules for flags incrementally).

// We wait to add command rules, so that we can look at the whole
// graph and place command flag groupings everywhere
// appropriate. Additionally, any subcommand names can be added into
// all commands that don't have the name as a sub-command, if you set
// C4M_ACCEPT_SUBS_AS_WORDS

#include "con4m.h"

static void c4m_gopt_comma(c4m_gopt_lex_state *);

static inline c4m_utf8_t *
c4m_gopt_normalize(c4m_gopt_lex_state *state, c4m_utf8_t *s)
{
    if (state->gctx->options & C4M_CASE_SENSITIVE) {
        return c4m_to_utf8(s);
    }

    return c4m_to_utf8(c4m_str_lower(s));
}

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

static int64_t
lookup_bi_id(c4m_gopt_lex_state *state, c4m_gopt_bi_ttype i)
{
    return (int64_t)c4m_list_get(state->gctx->terminal_info, i, NULL);
}

static void
c4m_gopt_tok_emit(c4m_gopt_lex_state *state,
                  int64_t             id,
                  int                 word,
                  int                 start,
                  int                 end,
                  c4m_utf8_t         *contents)
{
    if (id >= 0 && id < C4M_GOTT_LAST_BI) {
        id = lookup_bi_id(state, id);
    }

    c4m_token_info_t *tok = c4m_gc_alloc_mapped(c4m_token_info_t,
                                                C4M_GC_SCAN_ALL);
    tok->value            = contents;
    tok->tid              = id,
    tok->index            = c4m_list_len(state->gctx->tokens);
    tok->line             = word;
    tok->column           = start;
    tok->endcol           = end;

    c4m_list_append(state->gctx->tokens, tok);
}

static inline void
c4m_gopt_tok_command_name(c4m_gopt_lex_state *state)
{
    if (!c4m_gopt_gflag_is_set(state, C4M_TOPLEVEL_IS_ARGV0)) {
        return;
    }

    if (!state->command_name) {
        c4m_gopt_tok_emit(state,
                          lookup_bi_id(state, C4M_GOTT_OTHER_COMMAND_NAME),
                          0,
                          0,
                          0,
                          c4m_new_utf8("??"));
        return;
    }

    c4m_utf8_t *s     = c4m_to_utf8(state->command_name);
    int64_t     tokid = (int64_t)hatrack_dict_get(state->gctx->sub_names,
                                              s,
                                              NULL);

    if (!tokid) {
        tokid = C4M_GOTT_OTHER_COMMAND_NAME;
    }

    c4m_gopt_tok_emit(state, tokid, 0, 0, 0, s);
}

static void
c4m_gopt_tok_word_or_bool(c4m_gopt_lex_state *state)
{
    // At this point, we are either going to generate BOOL or WORD
    // for whatever is left. So slice it out, compare the normalized
    // version against our boolean values, and be done.

    c4m_str_t  *raw_contents;
    c4m_utf8_t *s;
    c4m_utf8_t *truthstr;

    if (state->cur_word_position == 0) {
        raw_contents = c4m_to_utf8(state->raw_word);
    }
    else {
        raw_contents = c4m_str_slice(state->raw_word,
                                     state->cur_word_position,
                                     state->cur_wordlen);
    }

    if (c4m_len(raw_contents) == 0) {
        return; // Nothing to yield.
    }

    raw_contents       = c4m_to_utf32(raw_contents);
    c4m_codepoint_t *p = (c4m_codepoint_t *)raw_contents->data;

    switch (*p) {
    case 't':
    case 'T':
        if (c4m_gopt_gflag_is_set(state, C4M_GOPT_BOOL_NO_TF)) {
            break;
        }
        // Even if it isn't normalized, force it...
        s = c4m_to_utf8(c4m_str_lower(raw_contents));
        if (c4m_str_eq(s, c4m_new_utf8("t")) || c4m_str_eq(s, c4m_new_utf8("true"))) {
            c4m_gopt_tok_emit(state,
                              C4M_GOTT_BOOL_T,
                              state->word_id,
                              state->cur_word_position,
                              state->cur_wordlen,
                              c4m_new_utf8("true"));
            return;
        }
        break;
    case 'f':
    case 'F':
        if (c4m_gopt_gflag_is_set(state, C4M_GOPT_BOOL_NO_TF)) {
            break;
        }
        // Even if it isn't normalized, force it...
        s = c4m_to_utf8(c4m_str_lower(raw_contents));
        if (c4m_str_eq(s, c4m_new_utf8("f")) || c4m_str_eq(s, c4m_new_utf8("false"))) {
            c4m_gopt_tok_emit(state,
                              C4M_GOTT_BOOL_F,
                              state->word_id,
                              state->cur_word_position,
                              state->cur_wordlen,
                              c4m_new_utf8("false"));
            return;
        }
        break;
    case 'y':
    case 'Y':
        if (c4m_gopt_gflag_is_set(state, C4M_GOPT_BOOL_NO_YN)) {
            break;
        }

        if (c4m_gopt_gflag_is_set(state, C4M_GOPT_BOOL_NO_TF)) {
            truthstr = c4m_new_utf8("yes");
        }
        else {
            truthstr = c4m_new_utf8("true");
        }
        // Even if it isn't normalized, force it...
        s = c4m_to_utf8(c4m_str_lower(raw_contents));
        if (c4m_str_eq(s, c4m_new_utf8("y")) || c4m_str_eq(s, c4m_new_utf8("yes"))) {
            c4m_gopt_tok_emit(state,
                              C4M_GOTT_BOOL_T,
                              state->word_id,
                              state->cur_word_position,
                              state->cur_wordlen,
                              truthstr);
            return;
        }
        break;
    case 'n':
    case 'N':
        if (c4m_gopt_gflag_is_set(state, C4M_GOPT_BOOL_NO_YN)) {
            break;
        }

        if (c4m_gopt_gflag_is_set(state, C4M_GOPT_BOOL_NO_TF)) {
            truthstr = c4m_new_utf8("no");
        }
        else {
            truthstr = c4m_new_utf8("false");
        }
        // Even if it isn't normalized, force it...
        s = c4m_to_utf8(c4m_str_lower(raw_contents));
        if (c4m_str_eq(s, c4m_new_utf8("n")) || c4m_str_eq(s, c4m_new_utf8("no"))) {
            c4m_gopt_tok_emit(state,
                              C4M_GOTT_BOOL_F,
                              state->word_id,
                              state->cur_word_position,
                              state->cur_wordlen,
                              truthstr);
            return;
        }
        break;
    }

    int64_t ix = c4m_str_find(raw_contents, c4m_new_utf8(","));
    int64_t tokid;

    if (ix == -1) {
        // If it's a command name, use the specific token
        // for the command name; otherwise, use the token
        // for generic word.

        tokid = (int64_t)hatrack_dict_get(state->gctx->sub_names,
                                          c4m_to_utf8(raw_contents),
                                          NULL);

        if (!tokid) {
            tokid = C4M_GOTT_WORD;
        }

        c4m_gopt_tok_emit(state,
                          tokid,
                          state->word_id,
                          state->cur_word_position,
                          state->cur_wordlen,
                          raw_contents);
        return;
    }

    raw_contents = c4m_str_slice(raw_contents, 0, ix);

    tokid = (int64_t)hatrack_dict_get(state->gctx->sub_names,
                                      c4m_to_utf8(raw_contents),
                                      NULL);

    if (!tokid) {
        tokid = C4M_GOTT_WORD;
    }

    c4m_gopt_tok_emit(state,
                      tokid,
                      state->word_id,
                      state->cur_word_position,
                      state->cur_word_position + ix,
                      raw_contents);
    state->cur_word_position += ix;
    c4m_gopt_comma(state);
    return;
}

static inline void
c4m_gopt_tok_possible_number(c4m_gopt_lex_state *state)
{
    c4m_codepoint_t *start = (c4m_codepoint_t *)state->raw_word->data;
    c4m_codepoint_t *p     = start + state->cur_word_position;
    c4m_codepoint_t *end   = p + state->cur_wordlen;
    bool             dot   = false;

    while (p < end) {
        switch (*p++) {
        case '.':
            if (dot) {
                c4m_gopt_tok_word_or_bool(state);
                return;
            }
            dot = true;
            continue;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            continue;
        case ',':;
            int ix = (p - start) + state->cur_word_position;

            c4m_utf8_t *s = c4m_to_utf8(c4m_str_slice(state->raw_word,
                                                      state->cur_word_position,
                                                      ix));

            c4m_gopt_tok_emit(state,
                              dot ? C4M_GOTT_FLOAT : C4M_GOTT_INT,
                              state->word_id,
                              state->cur_word_position,
                              end - start,
                              s);

            state->cur_word_position = (end - start);
            c4m_gopt_comma(state);
            return;
        default:
            c4m_gopt_tok_word_or_bool(state);
            return;
        }
    }

    c4m_utf8_t *s = c4m_to_utf8(c4m_str_slice(state->raw_word,
                                              state->cur_word_position,
                                              state->cur_wordlen));
    c4m_gopt_tok_emit(state,
                      dot ? C4M_GOTT_FLOAT : C4M_GOTT_INT,
                      state->word_id,
                      state->cur_word_position,
                      state->cur_wordlen,
                      s); // Have emit take the slice.
}

static inline void
c4m_gopt_tok_num_bool_or_word(c4m_gopt_lex_state *state)
{
    c4m_codepoint_t *p = (c4m_codepoint_t *)state->raw_word->data;
    switch (p[state->cur_word_position]) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '.':
        c4m_gopt_tok_possible_number(state);
        break;
    default:
        c4m_gopt_tok_word_or_bool(state);
    }
}

static void
c4m_gopt_comma(c4m_gopt_lex_state *state)
{
    c4m_gopt_tok_emit(state,
                      C4M_GOTT_COMMA,
                      state->word_id,
                      state->cur_word_position,
                      state->cur_word_position + 1,
                      c4m_new_utf8(","));
    state->cur_word_position++;

    if (state->cur_word_position != state->cur_wordlen) {
        c4m_gopt_tok_num_bool_or_word(state);
    }
}

void
c4m_peek_and_disallow_assign(c4m_gopt_lex_state *state)
{
    // Here, we know the start of the next word cannot be an
    // assignment operator, so we set a flag to ensure that, if the
    // next word does start with one, it's treated as a letter in a
    // word, and does not generate a token.

    if (state->word_id + 1 == state->num_words) {
        return;
    }

    c4m_str_t *s = c4m_list_get(state->words, state->word_id + 1, NULL);
    if (c4m_str_codepoint_len(s) == 0) {
        return;
    }

    s = c4m_to_utf8(s);
    switch (s->data[0]) {
    case '=':
        state->force_word = true;
        return;
    case ':':
        if (c4m_gopt_gflag_is_set(state, C4M_ALLOW_COLON_SEPARATOR)) {
            state->force_word = true;
        }
        return;
    default:
        return;
    }
}

static inline void
c4m_gopt_tok_argument_separator(c4m_gopt_lex_state *state, c4m_codepoint_t cp)
{
    if (!state->cur_word_position) {
        if (c4m_gopt_gflag_is_set(state, C4M_GOPT_NO_EQ_SPACING)) {
            // TODO: add a warning here.
            c4m_gopt_tok_word_or_bool(state);
            return;
        }
    }

    c4m_gopt_tok_emit(state,
                      C4M_GOTT_ASSIGN,
                      state->word_id,
                      state->cur_word_position,
                      state->cur_word_position + 1,
                      c4m_utf8_repeat(cp, 1));

    state->cur_word_position++;

    if (state->cur_word_position != state->cur_wordlen) {
        c4m_gopt_tok_num_bool_or_word(state);
        return;
    }
    if (state->word_id + 1 == state->num_words) {
        return;
    }

    c4m_peek_and_disallow_assign(state);
}

static void
c4m_emit_proper_flag(c4m_gopt_lex_state *state, int end_ix)
{
    c4m_str_t *s = c4m_str_slice(state->word,
                                 state->cur_word_position,
                                 end_ix);
    int64_t    tok_id;

    s                   = c4m_to_utf8(s);
    c4m_goption_t *info = hatrack_dict_get(state->gctx->all_options,
                                           s,
                                           NULL);
    if (!info) {
        tok_id = C4M_GOTT_UNKNOWN_OPT;
    }
    else {
        tok_id = info->token_id;
    }

    s = c4m_str_slice(state->raw_word,
                      state->cur_word_position,
                      end_ix);
    s = c4m_to_utf8(s);

    c4m_gopt_tok_emit(state,
                      tok_id,
                      state->word_id,
                      state->cur_word_position,
                      end_ix,
                      s);
}

static inline void
c4m_gopt_tok_gnu_short_opts(c4m_gopt_lex_state *state)
{
    // Generate a token for every flag we see from the beginning.
    // If we find a letter that isn't a flag, we generate an
    // UNKNOWN_OPT token, but keep going.
    //
    // As soon as we find an argument that can take an option at all,
    // we treat the rest of the word as an argument.

    c4m_codepoint_t *start   = (c4m_codepoint_t *)state->raw_word->data;
    c4m_codepoint_t *p       = start + state->cur_word_position;
    c4m_codepoint_t *end     = start + state->cur_wordlen;
    bool             got_arg = false;
    c4m_goption_t   *info;
    c4m_utf8_t      *flag;
    int64_t          tok_id;

    while (p < end) {
        flag = c4m_utf8_repeat(*p, 1);
        info = hatrack_dict_get(state->gctx->all_options, flag, NULL);

        if (info == NULL) {
            tok_id = C4M_GOTT_UNKNOWN_OPT;
        }
        else {
            tok_id = info->token_id;
            switch (info->type) {
            case C4M_GOAT_BOOL_T_ALWAYS:
            case C4M_GOAT_BOOL_F_ALWAYS:
            case C4M_GOAT_CHOICE_T_ALIAS:
            case C4M_GOAT_CHOICE_F_ALIAS:
                break;
            default:
                got_arg = true;
                break;
            }
        }

        c4m_gopt_tok_emit(state,
                          tok_id,
                          state->word_id,
                          state->cur_word_position,
                          state->cur_word_position + 1,
                          flag);
        p++;
        state->cur_word_position++;

        if (got_arg && state->cur_word_position != state->cur_wordlen) {
            c4m_gopt_tok_num_bool_or_word(state);
            return;
        }
    }
}

static inline void
c4m_gopt_tok_longform_opt(c4m_gopt_lex_state *state)
{
    c4m_codepoint_t *start = (c4m_codepoint_t *)state->raw_word->data;
    c4m_codepoint_t *p     = start + state->cur_word_position;
    c4m_codepoint_t *end   = start + state->cur_wordlen;

    while (p < end) {
        switch (*p) {
        case ':':
            if (c4m_gopt_gflag_is_set(state, C4M_ALLOW_COLON_SEPARATOR)) {
                goto got_attached_arg;
            }
            break;
        case '=':
            goto got_attached_arg;
        default:
            break;
        }
        p++;
    }

    c4m_emit_proper_flag(state, state->cur_wordlen);
    if (c4m_gopt_gflag_is_set(state, C4M_GOPT_NO_EQ_SPACING)) {
        c4m_peek_and_disallow_assign(state);
    }

    return;

got_attached_arg:
    c4m_emit_proper_flag(state, p - start);
    state->cur_word_position = p - start;
    c4m_gopt_tok_argument_separator(state, *p);
}

static inline void
c4m_gopt_tok_unix_flag(c4m_gopt_lex_state *state)
{
    if (!c4m_gopt_gflag_is_set(state, C4M_GOPT_SINGLE_DASH_ERR)) {
        if (c4m_str_eq(state->word, c4m_new_utf8("-"))) {
            c4m_gopt_tok_word_or_bool(state);
            return;
        }
    }
    if (!c4m_gopt_gflag_is_set(state, C4M_GOPT_NO_DOUBLE_DASH)) {
        if (c4m_str_eq(state->word, c4m_new_utf8("--"))) {
            state->all_words = true;
        }
    }

    state->cur_word_position++;

    if (c4m_str_starts_with(state->word, c4m_new_utf8("--"))) {
        state->cur_word_position++;
        c4m_gopt_tok_longform_opt(state);
        return;
    }

    if (c4m_gopt_gflag_is_set(state, C4M_GOPT_RESPECT_DASH_LEN)) {
        c4m_gopt_tok_gnu_short_opts(state);
        return;
    }

    c4m_gopt_tok_longform_opt(state);
}

static inline void
c4m_gopt_tok_windows_flag(c4m_gopt_lex_state *state)
{
    state->cur_word_position++;
    c4m_gopt_tok_longform_opt(state);
}

static inline void
c4m_gopt_tok_default_state(c4m_gopt_lex_state *state)
{
    state->cur_word_position = 0;

    if (!state->cur_wordlen) {
        return;
    }

    c4m_codepoint_t cp = ((c4m_codepoint_t *)state->raw_word->data)[0];
    switch (cp) {
    case ':':
        if (!c4m_gopt_gflag_is_set(state, C4M_ALLOW_COLON_SEPARATOR)) {
            c4m_gopt_tok_word_or_bool(state);
            return;
        }
        // fallthrough
    case '=':
        // This will process the argument if there's anything after
        // the mark.  Note that the flag check to see how to parse
        // this happens inside this func.
        c4m_gopt_tok_argument_separator(state, cp);
        return;
    case '-':
        // Again, the arg check happens in the function.
        c4m_gopt_tok_unix_flag(state);
        return;
    case '/':
        if (c4m_gopt_gflag_is_set(state, C4M_ALLOW_WINDOWS_SYNTAX)) {
            c4m_gopt_tok_windows_flag(state);
            return;
        }
        c4m_gopt_tok_word_or_bool(state);
        return;
    case ',':
        c4m_gopt_comma(state);
        return;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '.':
        c4m_gopt_tok_possible_number(state);
        return;
    default:
        c4m_gopt_tok_word_or_bool(state);
        return;
    }
}

static inline void
c4m_gopt_tok_main_loop(c4m_gopt_lex_state *state)
{
    int n = c4m_len(state->words);

    for (int i = 0; i < n; i++) {
        state->word_id     = i;
        state->raw_word    = c4m_to_utf32(c4m_list_get(state->words, i, NULL));
        state->word        = c4m_gopt_normalize(state, state->raw_word);
        state->cur_wordlen = c4m_str_codepoint_len(state->raw_word);

        if (state->force_word) {
            c4m_gopt_tok_word_or_bool(state);
            state->force_word = false;
            continue;
        }
        if (state->all_words) {
            continue;
        }
        c4m_gopt_tok_default_state(state);
    }

    c4m_gopt_tok_emit(state, C4M_TOK_EOF, 0, 0, 0, NULL);
}

// Words must be a list of strings. We turn them into a list of tokens
// that the Earley parser expects to see. We could pass all this to
// the Earley parser ourselves, but it's easier for me to not bother.
static void
c4m_gopt_tokenize(c4m_gopt_ctx *ctx,
                  c4m_utf8_t   *command_name,
                  c4m_list_t   *words)
{
    c4m_gopt_lex_state *state = c4m_gc_alloc_mapped(c4m_gopt_lex_state,
                                                    C4M_GC_SCAN_ALL);

    state->gctx         = ctx;
    state->command_name = command_name;
    state->words        = words;
    state->num_words    = c4m_list_len(words);
    ctx->tokens         = c4m_list(c4m_type_ref());

    c4m_gopt_tok_command_name(state);
    c4m_gopt_tok_main_loop(state);
}

static inline c4m_pitem_t *
get_bi_pitem(c4m_gopt_ctx *ctx, c4m_gopt_bi_ttype ix)
{
    bool found;

    int64_t termid = (int64_t)c4m_list_get(ctx->terminal_info, ix, &found);

    return c4m_pitem_terminal_from_int(ctx->grammar, termid);
}

#define ADD_BI(ctx, name)                                      \
    c4m_list_append(ctx->terminal_info,                        \
                    (void *)c4m_grammar_add_term(ctx->grammar, \
                                                 c4m_new_utf8(name)))
void
c4m_gopt_init(c4m_gopt_ctx *ctx, va_list args)
{
    ctx->options         = va_arg(args, uint32_t);
    ctx->grammar         = c4m_new(c4m_type_grammar());
    ctx->all_options     = c4m_dict(c4m_type_utf8(), c4m_type_ref());
    ctx->primary_options = c4m_dict(c4m_type_i64(), c4m_type_ref());
    ctx->top_specs       = c4m_dict(c4m_type_i64(), c4m_type_ref());
    ctx->sub_names       = c4m_dict(c4m_type_utf8(), c4m_type_i64());
    ctx->terminal_info   = c4m_list(c4m_type_ref());

    // Add in builtin token types.
    ADD_BI(ctx, "«Assign»");
    ADD_BI(ctx, "«Comma»");
    ADD_BI(ctx, "«Int»");
    ADD_BI(ctx, "«Float»");
    ADD_BI(ctx, "«True»");
    ADD_BI(ctx, "«False»");
    ADD_BI(ctx, "«Word»");
    ADD_BI(ctx, "«Unknown Cmd»");
    ADD_BI(ctx, "«Unknown Opt»");
    ADD_BI(ctx, "«Error»");

    // Add non-terminals and rules we're likely to need.
    // These will be numbered in the order we add them, since they're
    // the first non-terminals added, and the first will be the default
    // start rule.

    c4m_pitem_t *tmp;
    c4m_list_t  *rule;
    c4m_list_t  *items;

    // Start rule.
    tmp           = c4m_pitem_nonterm_raw(ctx->grammar,
                                c4m_new_utf8("Start"));
    ctx->nt_start = c4m_pitem_get_ruleset(ctx->grammar, tmp);
    // The rule that matches one opt (but does not check their positioning; that
    // is done at the end of parsing).
    tmp           = c4m_pitem_nonterm_raw(ctx->grammar,
                                c4m_new_utf8("Opt"));
    ctx->nt_1opt  = c4m_pitem_get_ruleset(ctx->grammar, tmp);
    tmp           = c4m_pitem_nonterm_raw(ctx->grammar,
                                c4m_new_utf8("Opts"));
    ctx->nt_opts  = c4m_pitem_get_ruleset(ctx->grammar, tmp);
    tmp           = c4m_pitem_nonterm_raw(ctx->grammar,
                                c4m_new_utf8("FlNT"));
    ctx->nt_float = c4m_pitem_get_ruleset(ctx->grammar, tmp);
    tmp           = c4m_pitem_nonterm_raw(ctx->grammar,
                                c4m_new_utf8("IntNT"));
    ctx->nt_int   = c4m_pitem_get_ruleset(ctx->grammar, tmp);
    tmp           = c4m_pitem_nonterm_raw(ctx->grammar,
                                c4m_new_utf8("BoolNT"));
    ctx->nt_bool  = c4m_pitem_get_ruleset(ctx->grammar, tmp);
    tmp           = c4m_pitem_nonterm_raw(ctx->grammar,
                                c4m_new_utf8("WordNT"));
    ctx->nt_word  = c4m_pitem_get_ruleset(ctx->grammar, tmp);
    tmp           = c4m_pitem_nonterm_raw(ctx->grammar,
                                c4m_new_utf8("EQ"));
    ctx->nt_eq    = c4m_pitem_get_ruleset(ctx->grammar, tmp);
    tmp           = c4m_pitem_nonterm_raw(ctx->grammar,
                                c4m_new_utf8("EQ2"));
    ctx->nt_oeq   = c4m_pitem_get_ruleset(ctx->grammar, tmp);

    c4m_ruleset_set_user_data(ctx->nt_1opt, (void *)C4M_GTNT_OPT_JUNK_SOLO);
    c4m_ruleset_set_user_data(ctx->nt_opts, (void *)C4M_GTNT_OPT_JUNK_MULTI);
    c4m_ruleset_set_user_data(ctx->nt_float, (void *)C4M_GTNT_FLOAT_NT);
    c4m_ruleset_set_user_data(ctx->nt_int, (void *)C4M_GTNT_INT_NT);
    c4m_ruleset_set_user_data(ctx->nt_bool, (void *)C4M_GTNT_BOOL_NT);
    c4m_ruleset_set_user_data(ctx->nt_word, (void *)C4M_GTNT_WORD_NT);

    // Opts matches any number of consecutive options. Done w/o a group
    // for simplicity.
    rule = c4m_list(c4m_type_ref());
    c4m_list_append(rule, c4m_pitem_from_nt(ctx->nt_1opt));
    c4m_list_append(rule, c4m_pitem_from_nt(ctx->nt_opts));
    c4m_ruleset_raw_add_rule(ctx->grammar, ctx->nt_opts, rule);
    rule = c4m_list(c4m_type_ref());
    c4m_ruleset_raw_add_rule(ctx->grammar, ctx->nt_opts, rule);

    // Args that take floats may accept ints.
    items = c4m_list(c4m_type_ref());
    rule  = c4m_list(c4m_type_ref());
    c4m_list_append(items, get_bi_pitem(ctx, C4M_GOTT_FLOAT));
    c4m_list_append(items, get_bi_pitem(ctx, C4M_GOTT_INT));
    c4m_list_append(rule, c4m_pitem_choice_raw(ctx->grammar, items));
    c4m_ruleset_raw_add_rule(ctx->grammar, ctx->nt_float, rule);

    rule = c4m_list(c4m_type_ref());
    c4m_list_append(rule, get_bi_pitem(ctx, C4M_GOTT_INT));
    c4m_ruleset_raw_add_rule(ctx->grammar, ctx->nt_int, rule);

    items = c4m_list(c4m_type_ref());
    rule  = c4m_list(c4m_type_ref());
    c4m_list_append(items, get_bi_pitem(ctx, C4M_GOTT_BOOL_T));
    c4m_list_append(items, get_bi_pitem(ctx, C4M_GOTT_BOOL_F));
    c4m_list_append(rule, c4m_pitem_choice_raw(ctx->grammar, items));
    c4m_ruleset_raw_add_rule(ctx->grammar, ctx->nt_bool, rule);

    // Args that take words can accept int, float or bool,
    // for better or worse.
    //
    // Note that if a parse is ambiguous, we probably should prefer
    // the more specific type. This is not done yet.
    items = c4m_list(c4m_type_ref());
    rule  = c4m_list(c4m_type_ref());
    c4m_list_append(items, get_bi_pitem(ctx, C4M_GOTT_FLOAT));
    c4m_list_append(items, get_bi_pitem(ctx, C4M_GOTT_INT));
    c4m_list_append(items, get_bi_pitem(ctx, C4M_GOTT_BOOL_T));
    c4m_list_append(items, get_bi_pitem(ctx, C4M_GOTT_BOOL_F));
    c4m_list_append(items, get_bi_pitem(ctx, C4M_GOTT_WORD));
    c4m_list_append(rule, c4m_pitem_choice_raw(ctx->grammar, items));
    c4m_ruleset_raw_add_rule(ctx->grammar, ctx->nt_word, rule);

    // We won't set up comma-separated lists of items unless we find a
    // flag than needs them.
    //
    // But we *will* set up the assign non-terminal, where the
    // assign can be omitted, unless the flag's argument is optional and
    // C4M_GOPT_ALWAYS_REQUIRE_EQ is set.
    //
    // The 'o' version is the 'other' version, which is used if the
    // above flag is set, and also in some error productions.
    //
    // Basically we're setting up:
    // EQ  ::= '=' | ε
    // OEQ ::= '='

    rule = c4m_list(c4m_type_ref());
    c4m_list_append(rule, get_bi_pitem(ctx, C4M_GOTT_ASSIGN));
    c4m_ruleset_raw_add_rule(ctx->grammar, ctx->nt_eq, rule);
    c4m_ruleset_raw_add_rule(ctx->grammar, ctx->nt_oeq, rule);

    rule = c4m_list(c4m_type_ref());
    c4m_ruleset_raw_add_rule(ctx->grammar, ctx->nt_eq, rule);
}

void
c4m_gcommand_init(c4m_gopt_cspec *cmd_spec, va_list args)
{
    c4m_utf8_t     *name             = NULL;
    c4m_list_t     *aliases          = NULL;
    c4m_gopt_ctx   *context          = NULL;
    c4m_utf8_t     *short_doc        = NULL;
    c4m_utf8_t     *long_doc         = NULL;
    c4m_gopt_cspec *parent           = NULL;
    bool            bad_opt_passthru = false;

    c4m_karg_va_init(args);
    c4m_kw_ptr("name", name);
    c4m_kw_ptr("context", context);
    c4m_kw_ptr("aliases", aliases);
    c4m_kw_ptr("short_doc", short_doc);
    c4m_kw_ptr("long_doc", short_doc);
    c4m_kw_ptr("parent", parent);
    c4m_kw_bool("bad_opt_passthru", bad_opt_passthru);

    if (!context) {
        C4M_CRAISE(
            "Must provide a getopt context in the 'context' field");
    }

    if (name) {
        cmd_spec->token_id = c4m_grammar_add_term(context->grammar,
                                                  name);
    }
    else {
        if (parent) {
            C4M_CRAISE(
                "getopt commands that are not the top-level command "
                "must have the 'name' field set.");
        }
        if (context->default_command != 0) {
            C4M_CRAISE(
                "There is already a default command set; new "
                "top-level commands require a name.");
        }
        if (aliases != NULL) {
            C4M_CRAISE("Cannot alias an unnamed (default) command.");
        }
        cmd_spec->token_id = 0;
    }

    if (!parent) {
        if (!hatrack_dict_add(context->top_specs,
                              (void *)cmd_spec->token_id,
                              cmd_spec)) {
dupe_error:
            C4M_CRAISE(
                "Attempt to add two commands at the same "
                "level with the same name.");
        }
    }
    else {
        if (!hatrack_dict_add(parent->sub_commands,
                              (void *)cmd_spec->token_id,
                              cmd_spec)) {
            goto dupe_error;
        }
    }

    cmd_spec->context          = context;
    cmd_spec->name             = name;
    cmd_spec->short_doc        = short_doc;
    cmd_spec->long_doc         = long_doc;
    cmd_spec->sub_commands     = c4m_dict(c4m_type_int(),
                                      c4m_type_ref());
    cmd_spec->aliases          = aliases;
    cmd_spec->bad_opt_passthru = bad_opt_passthru;
    cmd_spec->parent           = parent;
    cmd_spec->owned_opts       = c4m_list(c4m_type_ref());

    // Add a non-terminal rule to the grammar for any rules associated
    // with our command, and one for any flags.
    if (!name) {
        name = c4m_new_utf8("$$");
    }

    // To support the same sub-command appearing multiple places in
    // the grammar we generate, add a rule counter to
    // the non-terminal name.
    c4m_utf8_t *nt_name = c4m_cstr_format("{}_{}",
                                          name,
                                          context->counter++);

    // Add a map from the name to the token ID if needed.
    hatrack_dict_add(context->sub_names,
                     name,
                     (void *)cmd_spec->token_id);

    // 'top_nt' is used to handle any aliases for this command.
    // Rules get added to 'rule_nt' without the command name in it.
    cmd_spec->top_nt  = c4m_new(c4m_type_ruleset(), context->grammar, nt_name);
    nt_name           = c4m_cstr_format("{}_rules", nt_name);
    cmd_spec->rule_nt = c4m_new(c4m_type_ruleset(), context->grammar, nt_name);

    c4m_ruleset_set_user_data(cmd_spec->top_nt, (void *)C4M_GTNT_CMD_TOP);
    c4m_ruleset_set_user_data(cmd_spec->rule_nt, (void *)C4M_GTNT_CMD_RULE);
    // For the main name and any alias, let's add in a rule for each
    // that matches the name as a terminal, and then the rule non-terminal.
    // We'll do this with a 'choice'.
    c4m_list_t *items = c4m_list(c4m_type_ref());
    c4m_list_t *rule  = c4m_list(c4m_type_ref());

    if (cmd_spec->token_id >= C4M_START_TOK_ID) {
        c4m_list_append(items,
                        c4m_pitem_terminal_from_int(cmd_spec->context->grammar,
                                                    cmd_spec->token_id));
    }
    else {
        c4m_list_append(rule, c4m_pitem_from_nt(cmd_spec->rule_nt));
        c4m_ruleset_raw_add_rule(context->grammar, cmd_spec->top_nt, rule);
        return;
    }

    if (aliases != NULL) {
        int     n = c4m_list_len(aliases);
        int64_t id;

        for (int i = 0; i < n; i++) {
            c4m_str_t *one = c4m_to_utf8(c4m_list_get(aliases, i, NULL));
            id             = c4m_grammar_add_term(context->grammar, one);
            c4m_list_append(items,
                            c4m_pitem_terminal_from_int(context->grammar, id));
        }
    }
    c4m_list_append(rule, c4m_pitem_choice_raw(context->grammar, items));
    c4m_list_append(rule, c4m_pitem_from_nt(cmd_spec->rule_nt));

    c4m_ruleset_raw_add_rule(context->grammar, cmd_spec->top_nt, rule);
}

typedef struct gopt_rgen_ctx {
    c4m_list_t     *inrule;
    c4m_list_t     *outrule;
    c4m_gopt_cspec *cmd;
    int             inrule_index;
    int             inrule_len;
    int             nesting;
    c4m_gopt_ctx   *gctx;
} gopt_rgen_ctx;

static inline void
opts_pitem(gopt_rgen_ctx *ctx, c4m_list_t *out)
{
    c4m_pitem_t *pi = c4m_pitem_from_nt(ctx->gctx->nt_opts);
    c4m_list_append(out, pi);
}

static c4m_pitem_t *
opts_raw(gopt_rgen_ctx *ctx)
{
    c4m_list_t *items = c4m_list(c4m_type_ref());
    c4m_list_append(items, c4m_pitem_any_terminal_raw(ctx->gctx->grammar));
    return c4m_group_items(ctx->gctx->grammar, items, 0, 0);
}

static void
translate_gopt_rule(gopt_rgen_ctx *ctx)
{
    c4m_list_t *out = ctx->outrule;

    while (ctx->inrule_index < ctx->inrule_len) {
        int64_t item = (int64_t)c4m_list_get(ctx->inrule,
                                             ctx->inrule_index++,
                                             NULL);
        switch (item) {
        case C4M_GOG_WORD:
            opts_pitem(ctx, out);
            c4m_list_append(out, c4m_pitem_from_nt(ctx->gctx->nt_word));
            continue;
        case C4M_GOG_INT:
            opts_pitem(ctx, out);
            c4m_list_append(out, c4m_pitem_from_nt(ctx->gctx->nt_int));
            continue;
        case C4M_GOG_FLOAT:
            opts_pitem(ctx, out);
            c4m_list_append(out, c4m_pitem_from_nt(ctx->gctx->nt_float));
            continue;
        case C4M_GOG_BOOL:
            opts_pitem(ctx, out);
            c4m_list_append(out, c4m_pitem_from_nt(ctx->gctx->nt_bool));
            continue;
        case C4M_GOG_RAW:
            c4m_list_append(out, opts_raw(ctx));
            if (ctx->inrule_index != ctx->inrule_len) {
                C4M_CRAISE("RAW must be the last item specified.");
            }
            return;
        case C4M_GOG_LPAREN:;
            c4m_list_t *out_rule = ctx->outrule;
            ctx->outrule         = c4m_list(c4m_type_ref());
            int old_nest         = ctx->nesting++;
            translate_gopt_rule(ctx);

            if (ctx->nesting != old_nest) {
                C4M_CRAISE("Left paren is unmatched.");
            }
            c4m_list_t  *group = ctx->outrule;
            c4m_pitem_t *g;
            ctx->outrule = out_rule;
            // Check the postfix; these are defaults if none.
            int imin     = 1;
            int imax     = 1;
            if (ctx->inrule_index != ctx->inrule_len) {
                item = (int64_t)c4m_list_get(ctx->inrule,
                                             ctx->inrule_index,
                                             NULL);
                switch (item) {
                case C4M_GOG_OPT:
                    imin = 0;
                    ctx->inrule_index++;
                    break;
                case C4M_GOG_STAR:
                    imin = 0;
                    imax = 0;
                    ctx->inrule_index++;
                    break;
                case C4M_GOG_PLUS:
                    imax = 0;
                    ctx->inrule_index++;
                    break;
                default:
                    break;
                }
            }

            g = c4m_group_items(ctx->gctx->grammar, group, imin, imax);

            c4m_list_append(ctx->outrule, g);
            break;
        case C4M_GOG_RPAREN:
            if (--ctx->nesting < 0) {
                C4M_CRAISE("Right paren without matching left paren.");
            }
            return;
        case C4M_GOG_OPT:
        case C4M_GOG_STAR:
        case C4M_GOG_PLUS:
            C4M_CRAISE("Operator may only appear after a right paren.");
        default:;
            c4m_gopt_cspec *sub;

            sub = hatrack_dict_get(ctx->cmd->sub_commands,
                                   (void *)item,
                                   NULL);

            if (!sub) {
                C4M_CRAISE(
                    "Token is not a primitive, and is not a "
                    "sub-command name that's been registed for this command.");
            }
            if (ctx->inrule_index != ctx->inrule_len) {
                C4M_CRAISE("Subcommand must be the last item in a rule.");
            }
            sub->explicit_parent_rule = true;
            opts_pitem(ctx, out);
            c4m_list_append(out, c4m_pitem_from_nt(sub->top_nt));
            return;
        }
    }

    opts_pitem(ctx, out);
}

c4m_list_t *
_c4m_gopt_rule(int64_t start, ...)
{
    int64_t     next;
    c4m_list_t *result = c4m_list(c4m_type_int());
    va_list     vargs;

    c4m_list_append(result, (void *)start);
    va_start(vargs, start);

    while (true) {
        next = va_arg(vargs, int64_t);
        if (!next) {
            break;
        }
        c4m_list_append(result, (void *)next);
    }
    va_end(vargs);
    return result;
}

void
c4m_gopt_command_add_rule(c4m_gopt_cspec *cmd,
                          c4m_list_t     *items)
{
    // The only items allowed are the values in c4m_gopt_grammar_consts
    // and the specific token id for any command that is actually
    // one of our sub-commands (though, that can only be in the
    // very last slot).
    int         n;
    c4m_list_t *rule = c4m_list(c4m_type_ref());

    if (!items || (n = c4m_len(items)) == 0) {
        c4m_pitem_t *pi = c4m_pitem_from_nt(cmd->context->nt_opts);
        c4m_list_append(rule, pi);
        c4m_ruleset_raw_add_rule(cmd->context->grammar, cmd->rule_nt, rule);
        return;
    }

    gopt_rgen_ctx ctx = {
        .inrule       = items,
        .outrule      = rule,
        .cmd          = cmd,
        .inrule_index = 0,
        .inrule_len   = n,
        .nesting      = 0,
        .gctx         = cmd->context,

    };

    translate_gopt_rule(&ctx);
    c4m_ruleset_raw_add_rule(cmd->context->grammar, cmd->rule_nt, rule);
}

static void
c4m_gopt_finalize(c4m_gopt_ctx *gctx)
{
    if (gctx->finalized) {
        return;
    }
    // This sets up the productions of the 'start' rule and makes sure
    // all sub-commands are properly linked and have some sub-rule.
    //
    // When people add sub-commands, we assume that, if they didn't add
    // a rule to the parent, then the parent should take no
    // arguments of its own when the sub-command is used. Here, we
    // detect those cases and add rules at the 11th hour.
    //
    // This is breadth first.
    c4m_list_t      *stack = c4m_list(c4m_type_ref());
    uint64_t         n;
    c4m_gopt_cspec **tops = (void *)hatrack_dict_values(gctx->top_specs, &n);
    if (!n) {
        C4M_CRAISE("No commands added to the getopt environment.");
    }
    for (uint64_t i = 0; i < n; i++) {
        c4m_list_t *rule = c4m_list(c4m_type_ref());

        c4m_list_append(rule, c4m_pitem_from_nt(tops[i]->top_nt));
        c4m_ruleset_raw_add_rule(gctx->grammar, gctx->nt_start, rule);

        c4m_list_append(stack, tops[i]);
    }

    while (c4m_len(stack) != 0) {
        c4m_gopt_cspec *one = c4m_list_pop(stack);
        if (!one->context) {
            one->context = gctx;
        }

        if (!c4m_list_len(one->rule_nt->rules)) {
            c4m_gopt_command_add_rule(one, NULL);
        }

        tops = (void *)hatrack_dict_values(one->sub_commands,
                                           &n);
        if (!n) {
            continue;
        }
        for (uint64_t i = 0; i < n; i++) {
            if (!tops[i]->explicit_parent_rule) {
                c4m_list_t *items = c4m_list(c4m_type_int());
                c4m_list_append(items, (void *)tops[i]->token_id);
                c4m_gopt_command_add_rule(one, items);
            }

            c4m_list_append(stack, tops[i]);
        }
    }
    gctx->finalized = true;
}

static inline c4m_pitem_t *
flag_pitem(c4m_gopt_ctx *ctx, c4m_goption_t *option)
{
    return c4m_pitem_terminal_from_int(ctx->grammar, option->token_id);
}

static inline c4m_pitem_t *
from_nt(c4m_nonterm_t *nt)
{
    return c4m_pitem_from_nt(nt);
}

static void
add_noarg_rules(c4m_gopt_ctx *ctx, c4m_goption_t *option)
{
    // We will add a dummy rule that looks for a '=' WORD after; if
    // it matches, then we know there was an error.
    c4m_list_t  *good_rule = c4m_list(c4m_type_ref());
    c4m_pitem_t *pi        = flag_pitem(ctx, option);

    c4m_list_append(good_rule, pi);
    c4m_ruleset_raw_add_rule(ctx->grammar, option->my_nonterm, good_rule);

    c4m_list_t *bad_rule = c4m_list(c4m_type_ref());

    c4m_list_append(bad_rule, pi);

    c4m_list_append(bad_rule, from_nt(ctx->nt_oeq));
    c4m_list_append(bad_rule, from_nt(ctx->nt_word));
    c4m_ruleset_raw_add_rule(ctx->grammar, option->my_nonterm, bad_rule);
}

static inline void
add_proper_eq(c4m_gopt_ctx *ctx, c4m_list_t *rule)
{
    // Adds a rule that either requires an EQ token, or allows it to be
    // optional.
    if (ctx->options & C4M_GOPT_ALWAYS_REQUIRE_EQ) {
        c4m_list_append(rule, from_nt(ctx->nt_oeq));
    }
    else {
        c4m_list_append(rule, from_nt(ctx->nt_eq));
    }
}

static void
add_bool_rules(c4m_gopt_ctx *ctx, c4m_goption_t *option)
{
    c4m_list_t  *rule = c4m_list(c4m_type_ref());
    c4m_pitem_t *pi   = flag_pitem(ctx, option);

    c4m_list_append(rule, pi);

    if (option->min_args == 0) {
        c4m_ruleset_raw_add_rule(ctx->grammar, option->my_nonterm, rule);
        rule = c4m_list(c4m_type_ref());
        c4m_list_append(rule, pi);
    }

    add_proper_eq(ctx, rule);
    c4m_list_append(rule, from_nt(ctx->nt_bool));

    if (option->max_args == 1) {
        c4m_ruleset_raw_add_rule(ctx->grammar, option->my_nonterm, rule);
        return;
    }

    // We don't enforce min / max here. Instead, we wait till the parse
    // is done to make it easier to count totals.
    // This accepts any number of comma separated values.
    c4m_list_t *group = c4m_list(c4m_type_ref());
    c4m_list_append(group, get_bi_pitem(ctx, C4M_GOTT_COMMA));
    c4m_list_append(group, from_nt(ctx->nt_bool));
    c4m_list_append(rule, c4m_group_items(ctx->grammar, group, 0, 0));
    c4m_ruleset_raw_add_rule(ctx->grammar, option->my_nonterm, rule);
}

static void
add_int_rules(c4m_gopt_ctx *ctx, c4m_goption_t *option)
{
    c4m_list_t  *rule = c4m_list(c4m_type_ref());
    c4m_pitem_t *pi   = flag_pitem(ctx, option);

    c4m_list_append(rule, pi);
    if (option->min_args == 0) {
        c4m_ruleset_raw_add_rule(ctx->grammar, option->my_nonterm, rule);
        rule = c4m_list(c4m_type_ref());
        c4m_list_append(rule, pi);
    }

    add_proper_eq(ctx, rule);
    c4m_list_append(rule, from_nt(ctx->nt_int));

    if (option->max_args == 1) {
        c4m_ruleset_raw_add_rule(ctx->grammar, option->my_nonterm, rule);
        return;
    }

    // We don't enforce min / max here. Instead, we wait till the parse
    // is done to make it easier to count totals.
    // This accepts any number of comma separated values.
    c4m_list_t *group = c4m_list(c4m_type_ref());
    c4m_list_append(group, get_bi_pitem(ctx, C4M_GOTT_COMMA));
    c4m_list_append(group, from_nt(ctx->nt_int));
    c4m_list_append(rule, c4m_group_items(ctx->grammar, group, 0, 0));
    c4m_ruleset_raw_add_rule(ctx->grammar, option->my_nonterm, rule);
}

static void
add_float_rules(c4m_gopt_ctx *ctx, c4m_goption_t *option)
{
    c4m_list_t  *rule = c4m_list(c4m_type_ref());
    c4m_pitem_t *pi   = flag_pitem(ctx, option);

    c4m_list_append(rule, pi);
    if (option->min_args == 0) {
        c4m_ruleset_raw_add_rule(ctx->grammar, option->my_nonterm, rule);
        rule = c4m_list(c4m_type_ref());
        c4m_list_append(rule, pi);
    }

    add_proper_eq(ctx, rule);
    c4m_list_append(rule, from_nt(ctx->nt_float));

    if (option->max_args == 1) {
        c4m_ruleset_raw_add_rule(ctx->grammar, option->my_nonterm, rule);
        return;
    }

    c4m_list_t *group = c4m_list(c4m_type_ref());
    c4m_list_append(group, get_bi_pitem(ctx, C4M_GOTT_COMMA));
    c4m_list_append(group, from_nt(ctx->nt_float));
    c4m_list_append(rule, c4m_group_items(ctx->grammar, group, 0, 0));
    c4m_ruleset_raw_add_rule(ctx->grammar, option->my_nonterm, rule);
}

static void
add_word_rules(c4m_gopt_ctx *ctx, c4m_goption_t *option)
{
    c4m_list_t  *rule = c4m_list(c4m_type_ref());
    c4m_pitem_t *pi   = flag_pitem(ctx, option);

    c4m_list_append(rule, pi);
    if (option->min_args == 0) {
        c4m_ruleset_raw_add_rule(ctx->grammar, option->my_nonterm, rule);
        rule = c4m_list(c4m_type_ref());
        c4m_list_append(rule, pi);
    }

    add_proper_eq(ctx, rule);
    c4m_list_append(rule, from_nt(ctx->nt_word));

    if (option->max_args == 1) {
        c4m_ruleset_raw_add_rule(ctx->grammar, option->my_nonterm, rule);
        return;
    }

    c4m_list_t *group = c4m_list(c4m_type_ref());
    c4m_list_append(group, get_bi_pitem(ctx, C4M_GOTT_COMMA));
    c4m_list_append(group, from_nt(ctx->nt_word));
    c4m_list_append(rule, c4m_group_items(ctx->grammar, group, 0, 0));
    c4m_ruleset_raw_add_rule(ctx->grammar, option->my_nonterm, rule);
}

void
c4m_goption_init(c4m_goption_t *option, va_list args)
{
    c4m_utf8_t     *name           = NULL;
    c4m_gopt_ctx   *context        = NULL;
    c4m_utf8_t     *short_doc      = NULL;
    c4m_utf8_t     *long_doc       = NULL;
    c4m_list_t     *choices        = NULL;
    c4m_gopt_cspec *linked_command = NULL;
    int64_t         key            = 0;
    int32_t         opt_type       = C4M_GOAT_NONE;
    int32_t         min_args       = 1;
    int32_t         max_args       = 1;

    c4m_karg_va_init(args);
    c4m_kw_ptr("name", name);
    c4m_kw_ptr("context", context);
    c4m_kw_ptr("short_doc", short_doc);
    c4m_kw_ptr("long_doc", short_doc);
    c4m_kw_ptr("choices", choices);
    c4m_kw_ptr("linked_command", linked_command);
    c4m_kw_int64("key", key);
    c4m_kw_int32("opt_type", opt_type);
    c4m_kw_int32("min_args", min_args);
    c4m_kw_int32("max_args", max_args);

    if (!context && linked_command) {
        context = linked_command->context;
    }

    if (!context) {
        C4M_CRAISE(
            "Must provide a getopt context in the 'context' field, "
            "or linked_command to a command that has an associated context.");
    }

    if (!name) {
        C4M_CRAISE("getopt options must have the 'name' field set.");
    }
    // clang-format off
    if (c4m_str_find(name, c4m_new_utf8("=")) != -1 ||
	c4m_str_find(name, c4m_new_utf8(",")) != -1) {
        C4M_CRAISE("Options may not contain '=' or ',' in the name.");
    }
    // clang-format on

    c4m_goption_t *linked_option = NULL;

    switch (opt_type) {
    case C4M_GOAT_BOOL_T_DEFAULT:
    case C4M_GOAT_BOOL_F_DEFAULT:
        min_args = 0;
        max_args = 1;
        break;
    case C4M_GOAT_BOOL_T_ALWAYS:
    case C4M_GOAT_BOOL_F_ALWAYS:
        min_args = 0;
        max_args = 0;
        break;
    case C4M_GOAT_WORD:
    case C4M_GOAT_INT:
    case C4M_GOAT_FLOAT:
        break;
    case C4M_GOAT_CHOICE: // User-supplied list of options
        if (!choices || c4m_list_len(choices) <= 1) {
            C4M_CRAISE(
                "Choice alias options cannot be the primary value"
                "for a key; they must link to a pre-existing"
                "choice field.");
        }
        break;
    case C4M_GOAT_CHOICE_T_ALIAS:
    case C4M_GOAT_CHOICE_F_ALIAS:
        if (!key) {
            C4M_CRAISE("Must provide a linked option.");
        }
        linked_option = hatrack_dict_get(context->primary_options,
                                         (void *)key,
                                         NULL);

        goto check_link;
        break;
    default:
        C4M_CRAISE(
            "Must provide a valid option type in the "
            "'opt_type' field.");
    }

    if (key && linked_option) {
        linked_option = hatrack_dict_get(context->primary_options,
                                         (void *)key,
                                         NULL);
check_link:
        if (!linked_option) {
            C4M_CRAISE("Linked key for choice alias must already exist.");
        }
    }

    if (!linked_option) {
        option->primary = true;
        if (!key) {
            key = c4m_rand64();
        }
    }
    else {
        if (!linked_option->primary) {
            C4M_CRAISE(
                "Linked option is not the primary option for the "
                "key provided; you must link to the primary option.");
        }
    }

    if (!linked_command) {
        linked_command = hatrack_dict_get(context->primary_options,
                                          (void *)context->default_command,
                                          NULL);
    }

    if (linked_option) {
        bool compat = false;

        if (linked_option->linked_command != linked_command) {
            C4M_CRAISE("Linked options must be part of the same command.");
        }

        switch (linked_option->type) {
        case C4M_GOAT_BOOL_T_DEFAULT:
        case C4M_GOAT_BOOL_T_ALWAYS:
        case C4M_GOAT_BOOL_F_ALWAYS:
            switch (opt_type) {
            case C4M_GOAT_BOOL_T_DEFAULT:
            case C4M_GOAT_BOOL_T_ALWAYS:
            case C4M_GOAT_BOOL_F_ALWAYS:
                compat = true;
                break;
            }
            break;
        case C4M_GOAT_WORD:
            if (opt_type == C4M_GOAT_WORD) {
                compat = true;
                break;
            }
        case C4M_GOAT_INT:
            if (opt_type == C4M_GOAT_INT) {
                compat = true;
                break;
            }
        case C4M_GOAT_FLOAT:
            if (opt_type == C4M_GOAT_FLOAT) {
                compat = true;
                break;
            }
        case C4M_GOAT_CHOICE:
            switch (opt_type) {
            case C4M_GOAT_CHOICE:
                if (choices != NULL) {
                    C4M_CRAISE(
                        "Choice fields that alias another choice "
                        "field currently are not allowed to provide their "
                        "own options.");
                }
                compat = true;
                break;
            case C4M_GOAT_CHOICE_T_ALIAS:
            case C4M_GOAT_CHOICE_F_ALIAS:
                compat = true;
                break;
            default:
                break;
            }
        default:
            c4m_unreachable();
        }
        if (!compat) {
            C4M_CRAISE(
                "Type of linked option is not compatible with "
                "the declared type of the current option.");
        }
    }
    option->name           = c4m_to_utf8(name);
    option->short_doc      = short_doc;
    option->long_doc       = long_doc;
    option->linked_option  = linked_option;
    option->result_key     = key;
    option->min_args       = min_args;
    option->max_args       = max_args;
    option->type           = opt_type;
    option->choices        = choices;
    option->linked_command = linked_command;

    c4m_list_append(linked_command->owned_opts, option);

    if (c4m_gctx_gflag_is_set(context, C4M_CASE_SENSITIVE)) {
        option->normalized = option->name;
    }
    else {
        option->normalized = c4m_to_utf8(c4m_str_lower(name));
    }

    option->token_id = c4m_grammar_add_term(context->grammar,
                                            option->normalized);

    if (!hatrack_dict_add(context->all_options,
                          (void *)option->normalized,
                          option)) {
        C4M_CRAISE("Cannot add option that has already been added.");
    }

    if (!link) {
        hatrack_dict_put(context->primary_options,
                         (void *)key,
                         option);
    }

    uint64_t    rand    = c4m_rand64();
    c4m_utf8_t *nt_name = c4m_cstr_format("opt_{}_{}",
                                          option->name,
                                          rand);
    option->my_nonterm  = c4m_new(c4m_type_ruleset(),
                                 context->grammar,
                                 nt_name);

    c4m_ruleset_set_user_data(option->my_nonterm, (void *)C4M_GTNT_OPTION_RULE);
    // When flags are required to have arguments, we will add a rule
    // for a null argument, knowing if the final parse matches it, it
    // will be invalid.
    //
    // Similarly, if a flag takes no argument, we will add a rule for
    // '=', and error if the final parse accepts it. At some point, we
    // should more generally automate such error detection /
    // correction in the Earley parser directly.

    switch (option->type) {
    case C4M_GOAT_BOOL_T_ALWAYS:
    case C4M_GOAT_BOOL_F_ALWAYS:
    case C4M_GOAT_CHOICE_T_ALIAS:
    case C4M_GOAT_CHOICE_F_ALIAS:
        add_noarg_rules(context, option);
        break;
    case C4M_GOAT_BOOL_T_DEFAULT:
    case C4M_GOAT_BOOL_F_DEFAULT:
        add_bool_rules(context, option);
        break;
    case C4M_GOAT_INT:
        add_int_rules(context, option);
        break;
    case C4M_GOAT_FLOAT:
        add_float_rules(context, option);
        break;
    case C4M_GOAT_CHOICE:
    case C4M_GOAT_WORD:
        add_word_rules(context, option);
        break;
    default:
        c4m_unreachable();
    }

    // Create a rule that consists only of the non-terminal we created for
    // ourselves, and add it to the list of flags in the grammar (nt_1opt)
    c4m_list_t *rule = c4m_list(c4m_type_ref());
    c4m_list_append(rule, c4m_pitem_from_nt(option->my_nonterm));
    c4m_ruleset_raw_add_rule(context->grammar, context->nt_1opt, rule);
}

#ifdef C4M_DEBUG_GETOPT
static inline void
c4m_print_token_stream(c4m_gopt_ctx *ctx)
{
    int n = c4m_list_len(ctx->tokens);

    for (int i = 0; i < n; i++) {
        c4m_token_info_t *tok = c4m_list_get(ctx->tokens, i, NULL);
        c4m_print(c4m_repr_token_info(tok));
    }
}
#endif

c4m_list_t *
c4m_gopt_parse(c4m_gopt_ctx *ctx, c4m_str_t *argv0, c4m_list_t *args)
{
    if (args == NULL && argv0 == NULL) {
        argv0 = c4m_get_argv0();
    }
    if (args == NULL) {
        args = c4m_get_program_arguments();
    }

    c4m_gopt_finalize(ctx);
    c4m_gopt_tokenize(ctx, argv0, args);

    c4m_parser_t *opt_parser = c4m_new(c4m_type_parser(),
                                       ctx->grammar);
    c4m_parse_token_list(opt_parser, ctx->tokens, ctx->nt_start);
    return c4m_parse_get_parses(opt_parser);
}

static void gopt_extract_spec(c4m_gopt_extraction_ctx *);
static void handle_option_rule(c4m_gopt_extraction_ctx *);

static inline bool
is_group_node(c4m_parse_node_t *cur)
{
    return cur->group_top == true || cur->group_item == true;
}

static void
handle_subcommand(c4m_gopt_extraction_ctx *ctx)
{
    // In case we decide to care about specifically tying flags to
    // where they showed up, we do ahead and save / restore
    // command state.

    c4m_tree_node_t  *n          = ctx->intree;
    int               len        = n->num_kids;
    c4m_parse_node_t *cmd_node   = c4m_tree_get_contents(n->children[0]);
    c4m_utf8_t       *saved_path = ctx->path;
    c4m_gopt_cspec   *cmd        = ctx->cur_cmd;
    int64_t           cmd_key;

    // clang-format off
    if (!cmd_node->token || !(cmd_key = cmd_node->info.token->tid)) {


        for (int i = 0; i < len; i++) {
            ctx->intree = n->children[i];
            gopt_extract_spec(ctx);
        }

        ctx->intree = n;
        return;
    }
    // clang-format on
    ctx->cur_cmd = hatrack_dict_get(cmd->sub_commands,
                                    (void *)cmd_key,
                                    NULL);

    c4m_utf8_t *cmd_name = ctx->cur_cmd->name;

    if (c4m_str_codepoint_len(saved_path)) {
        ctx->path = c4m_cstr_format("{}.{}", saved_path, cmd_name);
    }
    else {
        ctx->path = cmd_name;
    }

    c4m_list_t *args = c4m_list(c4m_type_ref());
    hatrack_dict_put(ctx->args, ctx->path, args);

    ctx->deepest_path = ctx->path;

    for (int i = 1; i < len; i++) {
        ctx->intree = n->children[i];
        gopt_extract_spec(ctx);
    }

    ctx->cur_cmd = cmd;
    ctx->intree  = n;
    ctx->path    = saved_path;
}

static inline void
boxed_bool_negation(bool *b)
{
    *b = !*b;
}

static bool
needs_negation(c4m_goption_t *o1, c4m_goption_t *o2)
{
    bool pos_1;
    bool pos_2;

    switch (o1->type) {
    case C4M_GOAT_BOOL_T_DEFAULT:
    case C4M_GOAT_BOOL_T_ALWAYS:
        pos_1 = true;
        break;
    default:
        pos_1 = false;
        break;
    }

    switch (o2->type) {
    case C4M_GOAT_BOOL_T_DEFAULT:
    case C4M_GOAT_BOOL_T_ALWAYS:
        pos_2 = true;
        break;
    default:
        pos_2 = false;
        break;
    }

    return pos_1 ^ pos_2;
}

static inline void
negate_bool_array(c4m_list_t *l)
{
    int n = c4m_list_len(l);

    for (int i = 0; i < n; i++) {
        bool *b = c4m_list_get(l, i, NULL);
        boxed_bool_negation(b);
    }
}

static inline void *
nt_get_value(c4m_gopt_extraction_ctx *ctx, c4m_tree_node_t *n, bool *ok)
{
    c4m_parse_node_t  *cur = c4m_tree_get_contents(n);
    c4m_grammar_t     *g   = ctx->parser->grammar;
    c4m_gopt_node_type gopt_type;
    c4m_list_t        *items;
    int64_t            intval;
    double             dval;
    c4m_obj_t          val;

    if (ok) {
        *ok = true;
    }

    gopt_type = (c4m_gopt_node_type)c4m_parse_get_user_data(g, cur);

    switch (gopt_type) {
    case C4M_GTNT_FLOAT_NT:
        cur = c4m_tree_get_contents(n->children[0]);
        if ((cur->info.token->tid - C4M_START_TOK_ID) == C4M_GOTT_FLOAT) {
            if (!c4m_parse_double(cur->info.token->value, &dval)) {
                C4M_CRAISE("Todo: turn this into an error.\n");
            }
            return c4m_box_double(dval);
        }
        if (!c4m_parse_int64(cur->info.token->value, &intval)) {
            C4M_CRAISE("Todo: turn this into an error.\n");
        }
        return c4m_box_double(intval + 0.0);

    case C4M_GTNT_INT_NT:
        cur = c4m_tree_get_contents(n->children[0]);

        if (!c4m_parse_int64(cur->info.token->value, &intval)) {
            C4M_CRAISE("Todo: turn this into an error.\n");
        }

        return c4m_box_i64(intval);

    case C4M_GTNT_BOOL_NT:
        cur = c4m_tree_get_contents(n->children[0]);
        if ((cur->info.token->tid - C4M_START_TOK_ID) == C4M_GOTT_BOOL_T) {
            return c4m_box_bool(true);
        }
        return c4m_box_bool(false);
    case C4M_GTNT_WORD_NT:
        cur = c4m_tree_get_contents(n->children[0]);
        val = c4m_to_utf8(cur->info.token->value);
        if (!val && ok) {
            *ok = false;
        }

        return val;
    case C4M_GTNT_OTHER:
        if (cur->token) {
            if (ok) {
                *ok = false;
            }
            return NULL;
        }

        if (is_group_node(cur)) {
            items = c4m_list(c4m_type_ref());

            bool v;

            for (int i = 0; i < n->num_kids; i++) {
                val = nt_get_value(ctx, n->children[i], &v);

                if (v) {
                    c4m_list_append(items, val);
                }
            }
            switch (c4m_list_len(items)) {
            case 0:
                if (ok) {
                    *ok = NULL;
                }
                return NULL;
            case 1:
                return c4m_list_get(items, 0, NULL);
            default:
                return c4m_clean_internal_list(items);
            }
        }

        c4m_unreachable();
    case C4M_GTNT_OPT_JUNK_MULTI:
    case C4M_GTNT_OPT_JUNK_SOLO:
        if (ok) {
            *ok = false;
        }
        c4m_tree_node_t *saved = ctx->intree;
        ctx->intree            = n;
        gopt_extract_spec(ctx);
        ctx->intree = saved;
        return NULL;

    case C4M_GTNT_CMD_TOP:
        if (ok) {
            *ok = false;
        }
        saved       = ctx->intree;
        ctx->intree = n;
        handle_subcommand(ctx);
        ctx->intree = saved;
        return NULL;

    case C4M_GTNT_OPTION_RULE:
        if (ok) {
            *ok = false;
        }
        saved       = ctx->intree;
        ctx->intree = n;
        handle_option_rule(ctx);
        ctx->intree = saved;
        return NULL;

    default:
        if (ok) {
            *ok = false;
        }
        return NULL;
    }
}

static void
handle_option_rule(c4m_gopt_extraction_ctx *ctx)
{
    c4m_tree_node_t  *n                = ctx->intree;
    int               len              = n->num_kids;
    c4m_parse_node_t *name_node        = c4m_tree_get_contents(n->children[0]);
    c4m_utf8_t       *name             = name_node->info.token->value;
    int               flag_id          = name_node->info.token->tid;
    c4m_list_t       *extracted_values = c4m_list(c4m_type_ref());
    int64_t           key;

    if (flag_id == (C4M_START_TOK_ID + C4M_GOTT_UNKNOWN_OPT)) {
        C4M_CRAISE("Unknown option. TODO: turn into an error.");
    }

    c4m_goption_t *cur_option = hatrack_dict_get(ctx->gctx->all_options,
                                                 name,
                                                 NULL);
    assert(cur_option);

    // when len == 1, no arg was provided.
    if (len != 1) {
        c4m_list_append(extracted_values,
                        nt_get_value(ctx, n->children[2], NULL));

        if (len == 4) {
            n   = n->children[3];
            len = n->num_kids;
            for (int i = 0; i < len; i++) {
                c4m_list_append(extracted_values,
                                nt_get_value(ctx,
                                             n->children[i],
                                             NULL));
            }
        }
    }
    else {
        c4m_list_append(extracted_values, c4m_box_bool(true));
    }
    c4m_goption_t *target = cur_option->linked_option;

    if (!target) {
        target = cur_option;
    }

    key = target->result_key;

    c4m_rt_option_t *existing = hatrack_dict_get(ctx->flags, (void *)key, NULL);

    if (!existing) {
        existing = c4m_gc_alloc_mapped(c4m_rt_option_t, C4M_GC_SCAN_ALL);
        hatrack_dict_put(ctx->flags, (void *)key, existing);
        existing->spec = target;
    }

    if (target->max_args == 1) {
        c4m_obj_t val;

        switch (cur_option->type) {
        case C4M_GOAT_BOOL_T_DEFAULT:
        case C4M_GOAT_BOOL_F_DEFAULT:
        case C4M_GOAT_BOOL_T_ALWAYS:
        case C4M_GOAT_BOOL_F_ALWAYS:

            val = c4m_list_get(extracted_values, 0, NULL);

            if (needs_negation(cur_option, target)) {
                boxed_bool_negation((bool *)val);
            }

            existing->value = val;
            existing->n     = 1;
            return;

        case C4M_GOAT_WORD:
        case C4M_GOAT_INT:
        case C4M_GOAT_FLOAT:
            val = c4m_list_get(extracted_values, 0, NULL);

            existing->value = val;
            existing->n     = 1;
            return;

            // Choices should all have more than one value.
        case C4M_GOAT_CHOICE:
        case C4M_GOAT_CHOICE_T_ALIAS:
        case C4M_GOAT_CHOICE_F_ALIAS:
        default:
            c4m_unreachable();
        }
    }

    switch (cur_option->type) {
    case C4M_GOAT_CHOICE_T_ALIAS:
        if (existing->n) {
            c4m_list_t *cur = (c4m_list_t *)existing->value;
            len             = c4m_list_len(cur);
            c4m_obj_t   us  = c4m_list_get(extracted_values, 0, NULL);
            c4m_type_t *t   = c4m_get_my_type(us);

            for (int i = 0; i < len; i++) {
                c4m_obj_t one = c4m_list_get(cur, i, NULL);
                if (c4m_eq(t, us, one)) {
                    // Nothing to do.
                    return;
                }
            }
        }
        break;
    case C4M_GOAT_CHOICE_F_ALIAS:
        if (!existing->n) {
            return;
        }
        c4m_list_t *cur = (c4m_list_t *)existing->value;
        len             = c4m_list_len(cur);
        c4m_obj_t   us  = c4m_list_get(extracted_values, 0, NULL);
        c4m_type_t *t   = c4m_get_my_type(us);
        c4m_list_t *new = c4m_list(t);

        for (int i = 0; i < len; i++) {
            c4m_obj_t one = c4m_list_get(cur, i, NULL);
            if (!c4m_eq(t, us, one)) {
                c4m_list_append(new, one);
            }
        }
        if (c4m_list_len(new) == len) {
            return;
        }

        existing->n      = 0;
        extracted_values = new;
        break;

    case C4M_GOAT_BOOL_T_DEFAULT:
    case C4M_GOAT_BOOL_F_DEFAULT:
    case C4M_GOAT_BOOL_T_ALWAYS:
    case C4M_GOAT_BOOL_F_ALWAYS:

        if (c4m_list_len(extracted_values) == 0) {
            c4m_list_append(extracted_values, c4m_box_bool(true));
        }

        if (needs_negation(cur_option, target)) {
            negate_bool_array(extracted_values);
        }
        break;
    default:
        break;
    }

    c4m_list_t *new_items;

    if (existing->n) {
        new_items = (c4m_list_t *)existing->value;
        new_items = c4m_list_plus(new_items, extracted_values);
    }
    else {
        new_items = extracted_values;
    }

    len = c4m_list_len(new_items);
    if (len > target->max_args && target->max_args) {
        len       = target->max_args;
        new_items = c4m_list_get_slice(new_items, len - target->max_args, len);
    }

    existing->value = new_items;
    existing->n     = len;

    hatrack_dict_put(ctx->flags, (void *)key, existing);
}

static void
handle_cmdline_arg(c4m_gopt_extraction_ctx *ctx)
{
    c4m_tree_node_t *n = ctx->intree;
    c4m_list_t      *arg_info;
    c4m_obj_t        val;
    bool             ok;

    assert(ctx->cur_cmd);

    arg_info = hatrack_dict_get(ctx->args, ctx->path, NULL);
    val      = nt_get_value(ctx, n, &ok);

    if (ok) {
        c4m_list_append(arg_info, val);
    }
}

static void
gopt_extract_spec(c4m_gopt_extraction_ctx *ctx)
{
    c4m_tree_node_t   *n   = ctx->intree;
    c4m_parse_node_t  *cur = c4m_tree_get_contents(n);
    c4m_grammar_t     *g   = ctx->parser->grammar;
    c4m_gopt_node_type t   = (c4m_gopt_node_type)c4m_parse_get_user_data(g,
                                                                       cur);
    int                len = n->num_kids;

    switch (t) {
    case C4M_GTNT_OPTION_RULE:
        handle_option_rule(ctx);
        return;

    case C4M_GTNT_CMD_TOP:
        handle_subcommand(ctx);
        return;
    case C4M_GTNT_FLOAT_NT:
    case C4M_GTNT_INT_NT:
    case C4M_GTNT_BOOL_NT:
    case C4M_GTNT_WORD_NT:
        handle_cmdline_arg(ctx);
        return;

    default:
        if (is_group_node(cur)) {
            handle_cmdline_arg(ctx);
            return;
        }
        for (int i = 0; i < len; i++) {
            ctx->intree = n->children[i];
            gopt_extract_spec(ctx);
        }
    }

    ctx->intree = n;
}

static void
gopt_extract_top_spec(c4m_gopt_extraction_ctx *ctx)
{
    c4m_tree_node_t  *n   = ctx->intree;
    c4m_parse_node_t *cur = c4m_tree_get_contents(n);
    int               len = n->num_kids;

    ctx->cur_cmd = hatrack_dict_get(ctx->gctx->top_specs,
                                    (void *)cur->id,
                                    NULL);

    c4m_list_t *args = c4m_list(c4m_type_ref());

    hatrack_dict_put(ctx->args, ctx->path, args);

    assert(ctx->cur_cmd);

    for (int i = 0; i < len; i++) {
        ctx->intree = n->children[i];
        gopt_extract_spec(ctx);
    }
}

static c4m_gopt_result_t *
convert_parse_tree(c4m_gopt_ctx *gctx, c4m_tree_node_t *node)
{
    c4m_gopt_extraction_ctx ctx = {
        .flags        = c4m_dict(c4m_type_int(), c4m_type_ref()),
        .args         = c4m_dict(c4m_type_utf8(), c4m_type_ref()),
        .intree       = node->children[0], // Skip the Start production
        .gctx         = gctx,
        .path         = c4m_new_utf8(""),
        .deepest_path = c4m_new_utf8(""),
    };

    hatrack_dict_put(ctx.args, ctx.path, c4m_list(c4m_type_ref()));

    gopt_extract_top_spec(&ctx);

    c4m_gopt_result_t *result = c4m_gc_alloc_mapped(c4m_gopt_result_t,
                                                    C4M_GC_SCAN_ALL);
    result->flags             = ctx.flags;
    result->args              = ctx.args;
    result->errors            = ctx.errors;
    result->cmd               = ctx.deepest_path;

    return result;
}

void
c4m_gopt_add_subcommand(c4m_gopt_cspec *command, c4m_utf8_t *s)
{
    // Nesting check will get handled the next level down.
    c4m_list_t     *rule  = c4m_list(c4m_type_int());
    c4m_list_t     *raw   = c4m_str_split(c4m_to_utf8(s), c4m_new_utf8(" "));
    c4m_list_t     *words = c4m_list(c4m_type_utf8());
    int             n     = c4m_list_len(raw);
    c4m_codepoint_t cp;
    int64_t         l;
    int64_t         ix;
    int64_t         value;

    for (int i = 0; i < n; i++) {
        c4m_str_t *w = c4m_list_get(raw, i, NULL);
        w            = c4m_str_strip(w);
        if (!c4m_str_codepoint_len(w)) {
            continue;
        }

        w = c4m_to_utf8(c4m_str_lower(w));

        while (w->data[0] == '(') {
            c4m_list_append(words, c4m_new_utf8("("));
            l = c4m_str_codepoint_len(w);

            if (l == 1) {
                break;
            }

            w = c4m_to_utf8(c4m_str_slice(w, 1, l--));
        }
        if (!l) {
            continue;
        }

        // This is a little janky; currently we expect spacing after
        // the end of a group.
        //
        // Also, should probably handle commas too.
        //
        // Heck, this should use the earley parser.

        ix = c4m_str_find(w, c4m_new_utf8(")"));

        if (ix > 0) {
            c4m_list_append(words, c4m_to_utf8(c4m_str_slice(w, 0, ix)));
            w = c4m_to_utf8(c4m_str_slice(w, ix, c4m_str_codepoint_len(w)));
        }

        c4m_list_append(words, w);
    }

    n = c4m_list_len(words);

    for (int i = 0; i < n; i++) {
        c4m_str_t *w = c4m_list_get(words, i, NULL);

        if (c4m_str_eq(w, c4m_new_utf8("("))) {
            c4m_list_append(rule, (void *)C4M_GOG_LPAREN);
            continue;
        }

        if (c4m_str_eq(w, c4m_new_utf8("arg"))) {
            c4m_list_append(rule, (void *)C4M_GOG_WORD);
            continue;
        }

        if (c4m_str_eq(w, c4m_new_utf8("word"))) {
            c4m_list_append(rule, (void *)C4M_GOG_WORD);
            continue;
        }

        if (c4m_str_eq(w, c4m_new_utf8("str"))) {
            c4m_list_append(rule, (void *)C4M_GOG_WORD);
            continue;
        }

        if (c4m_str_eq(w, c4m_new_utf8("int"))) {
            c4m_list_append(rule, (void *)C4M_GOG_INT);
            continue;
        }

        if (c4m_str_eq(w, c4m_new_utf8("float"))) {
            c4m_list_append(rule, (void *)C4M_GOG_FLOAT);
            continue;
        }

        if (c4m_str_eq(w, c4m_new_utf8("bool"))) {
            c4m_list_append(rule, (void *)C4M_GOG_BOOL);
            continue;
        }

        if (c4m_str_eq(w, c4m_new_utf8("raw"))) {
            c4m_list_append(rule, (void *)C4M_GOG_RAW);
            if (i + 1 != n) {
                C4M_CRAISE("RAW must be the final argument.");
            }
            continue;
        }
        if (w->data[0] == ')') {
            c4m_list_append(rule, (void *)C4M_GOG_RPAREN);

            switch (c4m_str_codepoint_len(w)) {
            case 1:
                continue;
            case 2:
                break;
            default:
mod_error:
                C4M_CRAISE(
                    "Can currently only have one of '*', '+' or '?' "
                    "after a grouping.");
            }
            cp = w->data[1];

handle_group_modifier:
            switch (cp) {
            case '*':
                c4m_list_append(rule, (void *)C4M_GOG_STAR);
                continue;
            case '+':
                c4m_list_append(rule, (void *)C4M_GOG_PLUS);
                continue;
            case '?':
                c4m_list_append(rule, (void *)C4M_GOG_OPT);
                continue;
            default:
                goto mod_error;
            }
        }
        switch (w->data[0]) {
        case '*':
        case '+':
        case '?':
            l = c4m_list_len(rule);
            if (l != 0) {
                value = (int64_t)c4m_list_get(rule, l - 1, NULL);
                if (value == C4M_GOG_RPAREN) {
                    if (c4m_str_codepoint_len(w) != 1) {
                        goto mod_error;
                    }
                    cp = w->data[0];
                    goto handle_group_modifier;
                }
            }
            goto mod_error;
        default:
            break;
        }

        // At this point, anything we find should be the final
        // argument, and should be a sub-command name.
        if (i + 1 != n) {
            C4M_CRAISE("Subcommand names must be the final argument.");
        }

        hatrack_dict_item_t *values;

        values = hatrack_dict_items_sort(command->sub_commands, (uint64_t *)&l);

        for (int j = 0; j < l; j++) {
            hatrack_dict_item_t *one = &values[j];
            c4m_gopt_cspec      *sub = one->value;

            if (c4m_str_eq(w, sub->name)) {
                c4m_list_append(rule, one->key);
                goto add_it;
            }
        }

        c4m_utf8_t *msg = c4m_cstr_format("Unknown sub-command: [em]{}[/]", w);

        C4M_RAISE(msg);
    }

    // It's okay for the rule to be empty.

add_it:
    c4m_gopt_command_add_rule(command, rule);
}

const c4m_vtable_t c4m_gopt_parser_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)c4m_gopt_init,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)C4M_GC_SCAN_ALL,
    },
};

const c4m_vtable_t c4m_gopt_command_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)c4m_gcommand_init,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)C4M_GC_SCAN_ALL,
    },
};

const c4m_vtable_t c4m_gopt_option_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)c4m_goption_init,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)C4M_GC_SCAN_ALL,
    },
};

// TODO:
// 1- Warnings / errors. Do the right way??
// 2- Test alias handling.
// 3- Callback for ambiguous parse? Or else just intellegently pick one
//    (and warn?)
// 4- Hook up to con4m; allow tying to attributes.

//     Figure out how to return any args. Probably the components when
//     we spec need to be namable so we can set things appropriately.
//     Also should be able to get at the raw string values for
//     non-flag items if you need it.
// 5- Help flag type, and automatic output for it.
// 6- API for accessing docs.
// 7- Logic / options for auto-setting attributes (w/ type checking).
// 8- Debug flag type for exception handling?
// 9- Return raw results, argv style, and full line w/ and w/o flags.

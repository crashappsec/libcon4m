#define C4M_USE_INTERNAL_API
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
void
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

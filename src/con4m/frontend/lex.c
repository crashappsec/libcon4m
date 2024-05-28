#define C4M_USE_INTERNAL_API
#include <con4m.h>

typedef struct {
    char *tt_name;
    bool  show_contents;
} internal_tt_info_t;

static internal_tt_info_t tt_info[] = {
    {"error", false},
    {"space", false},
    {";", false},
    {"newline", false},
    {"comment", true},
    {"~", false},
    {"+", false},
    {"-", false},
    {"*", false},
    {"comment", true},
    {"/", false}, // 10
    {"%", false},
    {"<=", false},
    {"<", false},
    {">=", false},
    {">", false},
    {"!=", false},
    {"!", false},
    {":", false},
    {"=", false},
    {"==", false}, // 20
    {",", false},
    {".", false},
    {"{", false},
    {"}", false},
    {"[", false},
    {"]", false},
    {"(", false},
    {")", false},
    {"and", false},
    {"or", false}, // 30
    {"int", true},
    {"hex", true},
    {"float", true},
    {"string", true},
    {"char", true},
    {"unquoted", true},
    {"true", false},
    {"false", false},
    {"nil", false},
    {"if", false}, // 40
    {"elif", false},
    {"else", false},
    {"for", false},
    {"from", false},
    {"to", false},
    {"break", false},
    {"continue", false},
    {"return", false},
    {"enum", false},
    {"identifier", true}, // 50
    {"func", false},
    {"var", false},
    {"global", false},
    {"const", false},
    {"once", false},
    {"let", false},
    {"private", false},
    {"`", false},
    {"->", false},
    {"object", false}, // 60
    {"while", false},
    {"in", false},
    {"&", false},
    {"|", false},
    {"^", false},
    {"<<", false},
    {">>", false},
    {"typeof", false},
    {"switch", false},
    {"case", false},
    {"+=", false}, // 70
    {"-=", false},
    {"*=", false},
    {"/=", false},
    {"%=", false},
    {"&=", false},
    {"|=", false},
    {"^=", false},
    {"<<=", false},
    {">>=", false}, // 80
    {"eof", false}, // 81
};

c4m_utf8_t *
token_type_to_string(c4m_token_kind_t tk)
{
    return c4m_new_utf8(tt_info[tk].tt_name);
}

typedef struct {
    c4m_file_compile_ctx *ctx;
    c4m_codepoint_t      *start;
    c4m_codepoint_t      *end;
    c4m_codepoint_t      *pos;
    c4m_codepoint_t      *line_start;
    c4m_token_t          *last_token;
    size_t                token_id;
    size_t                line_no;
    size_t                cur_tok_line_no;
    size_t                cur_tok_offset;
} lex_state_t;

// These helpers definitely require us to keep names consistent internally.
//
// They just remove clutter in calling stuff and emphasize the variability:
// - TOK adds a token to the output stream of the given kind;
// - LITERAL_TOK is the same, except the system looks to see if there is
// - a lit modifier at the end; if there is, it copies it into the token.
// - LEX_ERROR adds an error to the broader context object, and longjumps.
#define TOK(kind) output_token(state, kind)
#define LITERAL_TOK(kind)      \
    output_token(state, kind); \
    handle_lit_mod(state)
#define LEX_ERROR(code)          \
    fill_lex_error(state, code); \
    C4M_CRAISE("Exception:" #code "\n")

static const __uint128_t max_intval = (__uint128_t)0xffffffffffffffffULL;

static inline c4m_codepoint_t
next(lex_state_t *state)
{
    if (state->pos >= state->end) {
        return 0;
    }
    return *state->pos++;
}

static inline void
unput(lex_state_t *state)
{
    if (state->pos && state->pos <= state->end) {
        --state->pos;
    }
}

static inline void
advance(lex_state_t *state)
{
    state->pos++;
}

static inline c4m_codepoint_t
peek(lex_state_t *state)
{
    if (state->pos >= state->end) {
        return 0;
    }
    return *(state->pos);
}

static inline void
at_new_line(lex_state_t *state)
{
    state->line_no++;
    state->line_start = state->pos;
}

static inline void
output_token(lex_state_t *state, c4m_token_kind_t kind)
{
    c4m_token_t *tok  = c4m_gc_alloc(c4m_token_t);
    tok->kind         = kind;
    tok->module       = state->ctx;
    tok->start_ptr    = state->start;
    tok->end_ptr      = state->pos;
    tok->token_id     = ++state->token_id;
    tok->line_no      = state->cur_tok_line_no;
    tok->line_offset  = state->cur_tok_offset;
    state->last_token = tok;

    c4m_xlist_append(state->ctx->tokens, tok);
}

static inline void
skip_optional_newline(lex_state_t *state)
{
    c4m_codepoint_t *start = state->pos;

    while (true) {
        switch (peek(state)) {
        case ' ':
        case '\t':
            advance(state);
            continue;
        case '\n':
            advance(state);
            at_new_line(state);
            // We only allow one newline after tokens.  So don't keep
            // running the same loop; we're done when this one finds
            // a non-space character.
            while (true) {
                switch (peek(state)) {
                case ' ':
                case '\t':
                    advance(state);
                    continue;
                default:
                    goto possible_ws_token;
                }
            }
            // Explicitly fall through here out of the nested switch
            // since we're done.
        default:
possible_ws_token:
            if (state->pos != start) {
                TOK(c4m_tt_space);
            }
            return;
        }
    }
}

static inline void
handle_lit_mod(lex_state_t *state)
{
    if (peek(state) != '\'') {
        return;
    }
    advance(state);

    c4m_codepoint_t *lm_start = state->pos;

    while (c4m_codepoint_is_c4m_id_continue(peek(state))) {
        advance(state);
    }

    size_t       n        = (size_t)(state->pos - lm_start);
    c4m_token_t *tok      = state->last_token;
    tok->literal_modifier = c4m_new(c4m_tspec_utf32(),
                                    c4m_kw("length", c4m_ka(n)));
    state->start          = state->pos;
}

static inline void
fill_lex_error(lex_state_t *state, c4m_compile_error_t code)

{
    c4m_token_t *tok = c4m_gc_alloc(c4m_token_t);
    tok->kind        = c4m_tt_error;
    tok->start_ptr   = state->start;
    tok->end_ptr     = state->pos;
    tok->line_no     = state->line_no;
    tok->line_offset = state->start - state->line_start;

    c4m_compile_error *err = c4m_gc_alloc(c4m_compile_error);
    err->code              = code;
    err->current_token     = tok;

    c4m_xlist_append(state->ctx->errors, err);
}

static inline void
scan_unquoted_literal(lex_state_t *state)
{
    // For now, this just scans to the end of the line, and returns a
    // token of type c4m_tt_unquoted_lit.  When it comes time to
    // re-implement the litmod stuff and we add literal parsers for
    // all the builtins, this can generate the proper token up-front.
    while (true) {
        switch (next(state)) {
        case '\n':
            at_new_line(state);
            // fallthrough.
        case 0:
            LITERAL_TOK(c4m_tt_unquoted_lit);
            return;
        }
    }
}

static void
scan_int_or_float_literal(lex_state_t *state)
{
    // This one probably does make more sense to fully parse here.
    // There is an issue:
    //
    // We're using u32 as our internal repr for what we're parsing.
    // But the easiest way to deal w/ floats is to call strtod(),
    // which expects UTF8 (well, ASCII really). We don't want to
    // reconvert (or keep around) the whole remainder of the file, so
    // we just scan forward looking at absolutely every character than
    // can possibly be in a valid float (including E/e, but not NaN /
    // infinity; those will have to be handled as keywords).  We
    // convert that bit back to UTF-8.
    //
    // If did we see a starting character that indicates a float, we
    // know it might be a float, so we keep a record of where the
    // first such character is; then we call strtod(); if strtod()
    // tells us it found a valid parse where the ending point is
    // farther than the first float indicator, then we're
    // done; we just need to set the proper token end point.
    //
    // Otherwise, we re-parse as an int, and we can just do that
    // manually into a __uint128_t (getting the float parse precisely
    // right is not something I relish, even though it can be done
    // faster than w/ strtod).
    //
    // One final note: we already passed the first character before we
    // got here. But state->start does point to the beginning, so we
    // use that when we need to reconstruct the string.

    c4m_codepoint_t *start    = state->start;
    int              ix       = 1; // First index we need to check.
    int              float_ix = 0; // 0 means not a float.

    while (true) {
        switch (start[ix]) {
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
            ix++;
            continue;
        case '.':
            if (float_ix) {
                // Already had a dot or something like that.
                break;
            }
            float_ix = ix++;
            continue;
        case 'e':
        case 'E':
            if (!float_ix) {
                float_ix = ix;
            }
            ix++;
            continue;
        case '+':
        case '-':
            ix++;
            continue;
        default:
            break;
        }
        break;
    }

    c4m_utf32_t *u32 = c4m_new(c4m_tspec_utf32(),
                               c4m_kw("length",
                                      c4m_ka(ix),
                                      "codepoints",
                                      c4m_ka(start)));
    c4m_utf8_t  *u8  = c4m_to_utf8(u32);

    if (float_ix) {
        char  *endp  = NULL;
        double value = strtod((char *)u8->data, &endp);

        if (endp == (char *)u8->data || !endp) {
            // I don't think this one should ever happen here.
            LEX_ERROR(c4m_err_lex_invalid_float_lit);
        }

        if (errno == ERANGE) {
            if (value == HUGE_VAL) {
                LEX_ERROR(c4m_err_lex_float_oflow);
            }
            LEX_ERROR(c4m_err_lex_float_uflow);
        }

        int float_strlen = (int)(endp - u8->data);
        if (float_strlen > float_ix) {
            state->pos = state->start + float_strlen;
            LITERAL_TOK(c4m_tt_float_lit);
            state->last_token->literal_value = (void *)*(uint64_t *)&value;
            return;
        }
    }

    // Either we saw no evidence of a float or the float parse
    // didn't get to any of that evidence, so voila, it's an int token.

    __int128_t val  = 0;
    int        i    = 0;
    size_t     slen = c4m_str_byte_len(u8);
    char      *p    = (char *)u8->data;

    for (; i < (int64_t)slen; i++) {
        char c = *p++;

        switch (c) {
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
            val *= 10;
            val += c - '0';
            if (val > (uint64_t)max_intval) {
                LEX_ERROR(c4m_err_lex_int_oflow);
            }
            continue;
        default:
            goto finished_int;
        }
    }
finished_int: {
    uint64_t n = (uint64_t)val;
    state->pos = state->start + i;
    LITERAL_TOK(c4m_tt_int_lit);
    state->last_token->literal_value = (void *)n;
    return;
}
}

static inline void
scan_hex_literal(lex_state_t *state)
{
    while (true) {
        switch (peek(state)) {
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
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
            advance(state);
            continue;
        default:
            LITERAL_TOK(c4m_tt_hex_lit);
            return;
        }
    }
}

// This only gets called if we already passed a leading 0. So we
// inspect the first char; if it's an 'x' or 'X', we go the hex
// route. Otherwise, we go the int route, which promotes to float
// depending on what it sees.

static inline void
scan_int_float_or_hex_literal(lex_state_t *state)
{
    switch (peek(state)) {
    case 'x':
    case 'X':
        scan_hex_literal(state);
        return;
    default:
        scan_int_or_float_literal(state);
        return;
    }
}

static inline void
scan_tristring(lex_state_t *state)
{
    // Here, we already got 3 quotes. We now need to:
    // 1. Keep track of line numbers when we see newlines.
    // 2. Skip any backtick'd things.
    // 3. Count consecutive quotes.
    // 4. Error when we get to EOF.

    int quote_count = 0;

    while (true) {
        switch (next(state)) {
        case 0:
            LEX_ERROR(c4m_err_lex_eof_in_str_lit);
        case '\n':
            at_new_line(state);
            break;
        case '\\':
            advance(state);
            break;
        case '"':
            if (++quote_count == 3) {
                LITERAL_TOK(c4m_tt_string_lit);
                state->last_token->adjustment = 3;
                return;
            }
            continue; // breaking would reset quote count.
        default:
            break;
        }
        quote_count = 0;
    }
}

static void
scan_string_literal(lex_state_t *state)
{
    // This function only finds the end of the string and keeps track
    // of line numbers; it does not otherwise attempt to handle any
    // parsing of the string itself.
    //
    // That could either be done after we've seen if there's a lit mod,
    // or wait until the parser or ir generator need the data;
    //
    // My choice is to do it as late as possible, because we could
    // then allow people to register litmods and then use them in the
    // same source file (or a dependent source file) if done properly.

    // Here, we know we already passed a single quote. We must first
    // determine if we're looking at a tristring.
    if (peek(state) == '"') {
        advance(state);
        if (peek(state) != '"') {
            // empty string.
            goto finish_single_quote;
        }
        advance(state);
        scan_tristring(state);
        return;
    }

    while (true) {
        c4m_codepoint_t c = next(state);

        switch (c) {
        case 0:
            LEX_ERROR(c4m_err_lex_eof_in_str_lit);
        case '\n':
        case '\r':
            LEX_ERROR(c4m_err_lex_nl_in_str_lit);
        case '\\':
            // Skip absolutely anything that comes next,
            // including a newline.
            advance(state);
            continue;
        case '"':
finish_single_quote:
            LITERAL_TOK(c4m_tt_string_lit);
            state->last_token->adjustment = 1;
            return;
        default:
            continue;
        }
    }
}

// Char literals can be:
// 1. a single character
// 2. \x, \X, \u, \U .. They're all the same. We scan till ' or some
//    error condition (which includes another \).
//    We don't check the value at this point; default char type will
//    error if it's outside the range of valid unicode. We don't even
//    check for it being valid hex; we just scan it.
//    w/ \u and \U I'll probably accept an optional + after the U since
//    officially that's what the unicode consortium does.
// 3. \ followed by any single character.
// -1. If we get a newline or null, it's an error.
// Also, if we get anything after it other than a ', it's an error.
//
// Note specifically that we do NOT turn this into a real char literal
// here. We wait till needed, so we can apply literal modifiers.
static void
scan_char_literal(lex_state_t *state)
{
    switch (next(state)) {
    case 0:
        LEX_ERROR(c4m_err_lex_eof_in_char_lit);
    case '\r':
    case '\n':
        LEX_ERROR(c4m_err_lex_nl_in_char_lit);
    case '\'':
        return;
    case '\\':
        switch (next(state)) {
        case 'x':
        case 'X':
        case 'u':
        case 'U':
            while (true) {
                switch (next(state)) {
                case 0:
                    LEX_ERROR(c4m_err_lex_eof_in_char_lit);
                case '\r':
                case '\n':
                    LEX_ERROR(c4m_err_lex_nl_in_char_lit);
                case '\\':
                    LEX_ERROR(c4m_err_lex_esc_in_esc);
                case '\'':
                    goto finish_up;
                }
            }
        default:
            break;
        }
    default:
        break;
    }
    if (next(state) != '\'') {
        LEX_ERROR(c4m_err_lex_extra_in_char_lit);
    }

finish_up:
    LITERAL_TOK(c4m_tt_char_lit);
    state->last_token->adjustment = 1;
    return;
}

static c4m_dict_t *keywords = NULL;

static inline void
add_keyword(char *keyword, c4m_token_kind_t kind)
{
    c4m_utf8_t *s = c4m_new(c4m_tspec_utf8(),
                            c4m_kw("cstring", c4m_ka(keyword)));
    hatrack_dict_add(keywords, s, (void *)(int64_t)kind);
}

static inline void
init_keywords()
{
    if (keywords != NULL) {
        return;
    }

    keywords = c4m_new(c4m_tspec_dict(c4m_tspec_utf32(), c4m_tspec_i64()));

    add_keyword("True", c4m_tt_true);
    add_keyword("true", c4m_tt_true);
    add_keyword("False", c4m_tt_false);
    add_keyword("false", c4m_tt_false);
    add_keyword("nil", c4m_tt_nil);
    add_keyword("in", c4m_tt_in);
    add_keyword("var", c4m_tt_var);
    add_keyword("global", c4m_tt_global);
    add_keyword("once", c4m_tt_once);
    add_keyword("let", c4m_tt_let);
    add_keyword("private", c4m_tt_private);
    add_keyword("const", c4m_tt_const);
    add_keyword("is", c4m_tt_cmp);
    add_keyword("and", c4m_tt_and);
    add_keyword("or", c4m_tt_or);
    add_keyword("not", c4m_tt_not);
    add_keyword("if", c4m_tt_if);
    add_keyword("elif", c4m_tt_elif);
    add_keyword("else", c4m_tt_else);
    add_keyword("case", c4m_tt_case);
    add_keyword("for", c4m_tt_for);
    add_keyword("while", c4m_tt_while);
    add_keyword("from", c4m_tt_from);
    add_keyword("to", c4m_tt_to);
    add_keyword("break", c4m_tt_break);
    add_keyword("continue", c4m_tt_continue);
    add_keyword("return", c4m_tt_return);
    add_keyword("enum", c4m_tt_enum);
    add_keyword("func", c4m_tt_func);
    add_keyword("object", c4m_tt_object);
    add_keyword("typeof", c4m_tt_typeof);
    add_keyword("switch", c4m_tt_switch);
    add_keyword("infinity", c4m_tt_float_lit);
    add_keyword("NaN", c4m_tt_float_lit);

    c4m_gc_register_root(&keywords, 1);
}

static void
scan_id_or_keyword(lex_state_t *state)
{
    init_keywords();

    // The pointer should be over an id_start
    while (true) {
        c4m_codepoint_t c = next(state);
        if (!c4m_codepoint_is_c4m_id_continue(c)) {
            if (c) {
                unput(state);
            }
            break;
        }
    }

    bool    found  = false;
    int64_t length = (int64_t)(state->pos - state->start);

    if (length == 0) {
        return;
    }

    c4m_utf32_t *as_u32 = c4m_new(
        c4m_tspec_utf32(),
        c4m_kw("codepoints",
               c4m_ka(state->start),
               "length",
               c4m_ka(length)));

    c4m_token_kind_t r = (c4m_token_kind_t)(int64_t)hatrack_dict_get(
        keywords,
        c4m_to_utf8(as_u32),
        &found);

    if (!found) {
        TOK(c4m_tt_identifier);
        return;
    }

    switch (r) {
    case c4m_tt_true:
    case c4m_tt_false:
        LITERAL_TOK(r);
        return;
    case c4m_tt_float_lit: {
        c4m_utf32_t *u32 = c4m_new(
            c4m_tspec_utf32(),
            c4m_kw("length",
                   c4m_ka((int64_t)(state->pos - state->start)),
                   "codepoints",
                   c4m_ka(state->start)));

        c4m_utf8_t *u8    = c4m_to_utf8(u32);
        double      value = strtod((char *)u8->data, NULL);

        LITERAL_TOK(r);
        state->last_token->literal_value = *(void **)&value;
        return;
    }
    default:
        TOK(r);
        return;
    }
}

static void
lex(lex_state_t *state)
{
    while (true) {
        c4m_codepoint_t c;
        c4m_codepoint_t tmp;

        // When we need to escape from nested loops after
        // recognizing a token, it's sometimes easier to short
        // circuit here w/ a goto than to break out of all those
        // loops just to 'continue'.
lex_next_token:
        state->start           = state->pos;
        state->cur_tok_line_no = state->line_no;
        state->cur_tok_offset  = state->start - state->line_start;
        c                      = next(state);

        switch (c) {
        case 0:
            TOK(c4m_tt_eof);
            return;
        case ' ':
        case '\t':
            while (true) {
                switch (peek(state)) {
                case ' ':
                case '\t':
                    advance(state);
                    continue;
                default:
                    goto lex_next_token;
                }
            }
            TOK(c4m_tt_space);
            continue;
        case '\r':
            tmp = next(state);
            if (tmp != '\n') {
                LEX_ERROR(c4m_err_lex_stray_cr);
            }
            // Fallthrough if no exception got raised.
            // fallthrough
        case '\n':
            TOK(c4m_tt_newline);
            at_new_line(state);
            continue;
        case '#':
            // Line comments go to EOF or new line, and we include the
            // newline in the token.
            // Double-slash comments work in con4m too; if we see that,
            // the lexer jumps back up here once it advances past the
            // second slash.
line_comment:
            while (true) {
                switch (next(state)) {
                case '\n':
                    at_new_line(state);
                    TOK(c4m_tt_line_comment);
                    goto lex_next_token;
                case 0: // EOF
                    TOK(c4m_tt_eof);
                    return;
                default:
                    continue;
                }
            }
        case '~':
            TOK(c4m_tt_lock_attr);
            continue;
        case '`':
            TOK(c4m_tt_backtick);
            continue;
        case '+':
            if (peek(state) == '=') {
                advance(state);
                TOK(c4m_tt_plus_eq);
            }
            else {
                TOK(c4m_tt_plus);
            }
            skip_optional_newline(state);
            continue;
        case '-':
            switch (peek(state)) {
            case '=':
                advance(state);
                TOK(c4m_tt_minus_eq);
                break;
            case '>':
                advance(state);
                TOK(c4m_tt_arrow);
                break;
            default:
                TOK(c4m_tt_minus);
                break;
            }
            skip_optional_newline(state);
            continue;
        case '*':
            if (peek(state) == '=') {
                advance(state);
                TOK(c4m_tt_mul_eq);
            }
            else {
                TOK(c4m_tt_mul);
            }
            skip_optional_newline(state);
            continue;
        case '/':
            switch (peek(state)) {
            case '=':
                advance(state);
                TOK(c4m_tt_div_eq);
                skip_optional_newline(state);
                break;
            case '/':
                advance(state);
                goto line_comment;
            case '*':
                advance(state);
                while (true) {
                    switch (next(state)) {
                    case '\n':
                        at_new_line(state);
                        continue;
                    case '*':
                        if (peek(state) == '/') {
                            advance(state);
                            TOK(c4m_tt_long_comment);
                            goto lex_next_token;
                        }
                        continue;
                    case 0:
                        LEX_ERROR(c4m_err_lex_eof_in_comment);
                    default:
                        continue;
                    }
                }
            default:
                TOK(c4m_tt_div);
                skip_optional_newline(state);
                break;
            }
            continue;
        case '%':
            if (peek(state) == '=') {
                advance(state);
                TOK(c4m_tt_mod_eq);
            }
            else {
                TOK(c4m_tt_mod);
            }
            skip_optional_newline(state);
            continue;
        case '<':
            switch (peek(state)) {
            case '=':
                advance(state);
                TOK(c4m_tt_lte);
                break;
            case '<':
                advance(state);
                if (peek(state) == '=') {
                    advance(state);
                    TOK(c4m_tt_shl_eq);
                }
                else {
                    TOK(c4m_tt_shl);
                }
                break;
            default:
                TOK(c4m_tt_lt);
                break;
            }
            skip_optional_newline(state);
            continue;
        case '>':
            switch (peek(state)) {
            case '=':
                advance(state);
                TOK(c4m_tt_gte);
                break;
            case '>':
                advance(state);
                if (peek(state) == '=') {
                    advance(state);
                    TOK(c4m_tt_shr_eq);
                }
                else {
                    TOK(c4m_tt_shr);
                }
                break;
            default:
                TOK(c4m_tt_gt);
                break;
            }
            skip_optional_newline(state);
            continue;
        case '!':
            if (peek(state) == '=') {
                advance(state);
                TOK(c4m_tt_neq);
            }
            else {
                TOK(c4m_tt_not);
            }
            skip_optional_newline(state);
            continue;
        case ';':
            TOK(c4m_tt_semi);
            continue;
        case ':':
            if (peek(state) == '=') {
                advance(state);
                TOK(c4m_tt_assign);
                state->start           = state->pos;
                state->cur_tok_line_no = state->line_no;
                state->cur_tok_offset  = state->start - state->line_start;
                scan_unquoted_literal(state);
            }
            else {
                TOK(c4m_tt_colon);
            }
            continue;
        case '=':
            if (peek(state) == '=') {
                advance(state);
                TOK(c4m_tt_cmp);
            }
            else {
                TOK(c4m_tt_assign);
            }
            skip_optional_newline(state);
            continue;
        case ',':
            TOK(c4m_tt_comma);
            skip_optional_newline(state);
            continue;
        case '.':
            TOK(c4m_tt_period);
            skip_optional_newline(state);
            continue;
        case '{':
            TOK(c4m_tt_lbrace);
            skip_optional_newline(state);
            continue;
        case '}':
            LITERAL_TOK(c4m_tt_rbrace);
            continue;
        case '[':
            TOK(c4m_tt_lbracket);
            skip_optional_newline(state);
            continue;
        case ']':
            LITERAL_TOK(c4m_tt_rbracket);
            continue;
        case '(':
            TOK(c4m_tt_lparen);
            skip_optional_newline(state);
            continue;
        case ')':
            LITERAL_TOK(c4m_tt_rparen);
            continue;
        case '&':
            if (peek(state) == '=') {
                advance(state);
                TOK(c4m_tt_bit_and_eq);
            }
            else {
                TOK(c4m_tt_bit_and);
            }
            skip_optional_newline(state);
            continue;
        case '|':
            if (peek(state) == '=') {
                advance(state);
                TOK(c4m_tt_bit_or_eq);
            }
            else {
                TOK(c4m_tt_bit_or);
            }
            skip_optional_newline(state);
            continue;
        case '^':
            if (peek(state) == '=') {
                advance(state);
                TOK(c4m_tt_bit_xor_eq);
            }
            else {
                TOK(c4m_tt_bit_xor);
            }
            skip_optional_newline(state);
            continue;
        case '0':
            scan_int_float_or_hex_literal(state);
            continue;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            scan_int_or_float_literal(state);
            continue;
        case '\'':
            scan_char_literal(state);
            continue;
        case '"':
            scan_string_literal(state);
            continue;
        default:
            if (!c4m_codepoint_is_c4m_id_start(c)) {
                LEX_ERROR(c4m_err_lex_invalid_char);
            }
            scan_id_or_keyword(state);
            continue;
        }
    }
}

bool
c4m_lex(c4m_file_compile_ctx *ctx, c4m_stream_t *stream)
{
    int outkind;
    outkind = stream->flags & (C4M_F_STREAM_UTF8_OUT | C4M_F_STREAM_UTF32_OUT);

    c4m_obj_t   *raw = c4m_stream_read_all(stream);
    c4m_utf32_t *utf32;
    lex_state_t  lex_info = {
         .token_id   = 0,
         .line_no    = 1,
         .line_start = 0,
         .ctx        = ctx,
    };

    if (raw == NULL) {
        return false;
    }

    switch (outkind) {
    case C4M_F_STREAM_UTF32_OUT:
        utf32 = (c4m_str_t *)raw;

        if (c4m_str_codepoint_len(utf32) == 0) {
            return false;
        }
        break;
    case C4M_F_STREAM_UTF8_OUT:
        if (c4m_str_codepoint_len((c4m_utf8_t *)raw) == 0) {
            return false;
        }

        utf32 = c4m_to_utf32((c4m_utf8_t *)raw);
        break;
    default:
        // A buffer object, which we assume is utf8.
        if (c4m_buffer_len((c4m_buf_t *)raw) == 0) {
            return false;
        }
        utf32 = c4m_to_utf32(c4m_buf_to_utf8_string((c4m_buf_t *)raw));
        break;
    }

    int len             = c4m_str_codepoint_len(utf32);
    ctx->raw            = utf32;
    ctx->tokens         = c4m_new(c4m_tspec_xlist(c4m_tspec_ref()));
    lex_info.start      = (c4m_codepoint_t *)utf32->data;
    lex_info.pos        = (c4m_codepoint_t *)utf32->data;
    lex_info.line_start = (c4m_codepoint_t *)utf32->data;
    lex_info.end        = &((c4m_codepoint_t *)(utf32->data))[len];

    bool error = false;

    C4M_TRY
    {
        lex(&lex_info);
    }
    C4M_EXCEPT
    {
        error = true;
    }
    C4M_TRY_END;

    return !error;
}

c4m_utf8_t *
c4m_format_one_token(c4m_token_t *tok, c4m_str_t *prefix)
{
    int32_t      info_ix = (int)tok->kind;
    int32_t     *num     = c4m_box_i32(tok->token_id);
    c4m_utf8_t  *name    = c4m_new_utf8(tt_info[info_ix].tt_name);
    int32_t     *line    = c4m_box_i32(tok->line_no);
    int32_t     *offset  = c4m_box_i32(tok->line_offset);
    c4m_utf32_t *val;

    val = c4m_new(c4m_tspec_utf32(),
                  c4m_kw("length",
                         c4m_ka((int64_t)(tok->end_ptr - tok->start_ptr)),
                         "codepoints",
                         c4m_ka(tok->start_ptr)));

    return c4m_cstr_format("{}#{} {} ({}:{}) {}",
                           prefix,
                           num,
                           name,
                           line,
                           offset,
                           val);
}

// Start out with any focus on color or other highlighting; just get
// them into a default table for now aimed at debugging, and we'll add
// a facility for styling later.
c4m_grid_t *
c4m_format_tokens(c4m_file_compile_ctx *ctx)
{
    c4m_grid_t *grid = c4m_new(c4m_tspec_grid(),
                               c4m_kw("start_cols",
                                      c4m_ka(5),
                                      "header_rows",
                                      c4m_ka(1),
                                      "stripe",
                                      c4m_ka(true)));

    c4m_xlist_t *row = c4m_new_table_row();
    int64_t      len = c4m_xlist_len(ctx->tokens);

    c4m_xlist_append(row, c4m_new_utf8("Seq #"));
    c4m_xlist_append(row, c4m_new_utf8("Type"));
    c4m_xlist_append(row, c4m_new_utf8("Line #"));
    c4m_xlist_append(row, c4m_new_utf8("Column #"));
    c4m_xlist_append(row, c4m_new_utf8("Value"));
    c4m_grid_add_row(grid, row);

    for (int64_t i = 0; i < len; i++) {
        c4m_token_t *tok     = c4m_xlist_get(ctx->tokens, i, NULL);
        int          info_ix = (int)tok->kind;

        row = c4m_new_table_row();
        c4m_xlist_append(row, c4m_str_from_int(i + 1));
        c4m_xlist_append(row, c4m_new_utf8(tt_info[info_ix].tt_name));
        c4m_xlist_append(row, c4m_str_from_int(tok->line_no));
        c4m_xlist_append(row, c4m_str_from_int(tok->line_offset));

        if (tt_info[info_ix].show_contents) {
            c4m_xlist_append(
                row,
                c4m_new(c4m_tspec_utf32(),
                        c4m_kw("length",
                               c4m_ka((int64_t)(tok->end_ptr - tok->start_ptr)),
                               "codepoints",
                               c4m_ka(tok->start_ptr))));
        }
        else {
            c4m_xlist_append(row, c4m_rich_lit(" "));
        }

        c4m_grid_add_row(grid, row);
    }

    c4m_set_column_style(grid, 0, "snap");
    c4m_set_column_style(grid, 1, "snap");
    c4m_set_column_style(grid, 2, "snap");
    c4m_set_column_style(grid, 3, "snap");
    return grid;
}

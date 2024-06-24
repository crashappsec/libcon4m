#include "con4m.h"

enum {
    LBRAK_IX     = 0,
    COMMA_IX     = 1,
    RBRAK_IX     = 2,
    LPAREN_IX    = 3,
    RPAREN_IX    = 4,
    ARROW_IX     = 5,
    BTICK_IX     = 6,
    STAR_IX      = 7,
    SPACE_IX     = 8,
    LBRACE_IX    = 9,
    RBRACE_IX    = 10,
    COLON_IX     = 11,
    COLON_NSP    = 12,
    SLASH_IX     = 13,
    PERIOD_IX    = 14,
    EMPTY_FMT_IX = 15,
    NEWLINE_IX   = 16,
    PUNC_MAX     = 17,
};

static c4m_str_t *type_punct[PUNC_MAX] = {
    0,
};

static inline void
init_punctuation()
{
    if (type_punct[0] == NULL) {
        type_punct[LBRAK_IX]     = c4m_utf8_repeat('[', 1);
        type_punct[COMMA_IX]     = c4m_new(c4m_type_utf8(),
                                       c4m_kw("cstring", c4m_ka(", ")));
        type_punct[RBRAK_IX]     = c4m_utf8_repeat(']', 1);
        type_punct[LPAREN_IX]    = c4m_utf8_repeat('(', 1);
        type_punct[RPAREN_IX]    = c4m_utf8_repeat(')', 1);
        type_punct[ARROW_IX]     = c4m_new(c4m_type_utf8(),
                                       c4m_kw("cstring", c4m_ka(" -> ")));
        type_punct[BTICK_IX]     = c4m_utf8_repeat('`', 1);
        type_punct[STAR_IX]      = c4m_utf8_repeat('*', 1);
        type_punct[SPACE_IX]     = c4m_utf8_repeat(' ', 1);
        type_punct[LBRACE_IX]    = c4m_utf8_repeat('{', 1);
        type_punct[RBRACE_IX]    = c4m_utf8_repeat('}', 1);
        type_punct[COLON_IX]     = c4m_new(c4m_type_utf8(),
                                       c4m_kw("cstring", c4m_ka(" : ")));
        type_punct[COLON_NSP]    = c4m_utf8_repeat(':', 1);
        type_punct[SLASH_IX]     = c4m_utf8_repeat('/', 1);
        type_punct[EMPTY_FMT_IX] = c4m_new(c4m_type_utf8(),
                                           c4m_kw("cstring", c4m_ka("{}")));
        type_punct[NEWLINE_IX]   = c4m_utf8_repeat('\n', 1);
        c4m_gc_register_root(&type_punct[0], PUNC_MAX);
    }
}

c4m_utf8_t *
c4m_get_lbrak_const()
{
    init_punctuation();
    return type_punct[LBRAK_IX];
}

c4m_utf8_t *
c4m_get_comma_const()
{
    init_punctuation();
    return type_punct[COMMA_IX];
}

c4m_utf8_t *
c4m_get_rbrak_const()
{
    init_punctuation();
    return type_punct[RBRAK_IX];
}

c4m_utf8_t *
c4m_get_lparen_const()
{
    init_punctuation();
    return type_punct[LPAREN_IX];
}

c4m_utf8_t *
c4m_get_rparen_const()
{
    init_punctuation();
    return type_punct[RPAREN_IX];
}

c4m_utf8_t *
c4m_get_arrow_const()
{
    init_punctuation();
    return type_punct[ARROW_IX];
}

c4m_utf8_t *
c4m_get_backtick_const()
{
    init_punctuation();
    return type_punct[BTICK_IX];
}

c4m_utf8_t *
c4m_get_asterisk_const()
{
    init_punctuation();
    return type_punct[STAR_IX];
}

c4m_utf8_t *
c4m_get_space_const()
{
    init_punctuation();
    return type_punct[SPACE_IX];
}

c4m_utf8_t *
c4m_get_lbrace_const()
{
    init_punctuation();
    return type_punct[LBRACE_IX];
}

c4m_utf8_t *
c4m_get_rbrace_const()
{
    init_punctuation();
    return type_punct[RBRACE_IX];
}

c4m_utf8_t *
c4m_get_colon_const()
{
    init_punctuation();
    return type_punct[COLON_IX];
}

c4m_utf8_t *
c4m_get_colon_no_space_const()
{
    init_punctuation();
    return type_punct[COLON_NSP];
}

c4m_utf8_t *
c4m_get_slash_const()
{
    init_punctuation();
    return type_punct[SLASH_IX];
}

c4m_utf8_t *
c4m_get_period_const()
{
    init_punctuation();
    return type_punct[PERIOD_IX];
}

c4m_utf8_t *
c4m_get_empty_fmt_const()
{
    init_punctuation();
    return type_punct[EMPTY_FMT_IX];
}

c4m_utf8_t *
c4m_get_newline_const()
{
    init_punctuation();
    return type_punct[NEWLINE_IX];
}

c4m_utf8_t *
c4m_in_parens(c4m_str_t *s)
{
    return c4m_to_utf8(c4m_str_concat(c4m_get_lparen_const(),
                                      c4m_str_concat(c4m_to_utf8(s),
                                                     c4m_get_rparen_const())));
}

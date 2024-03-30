#include <con4m.h>

enum {
    LBRAK_IX  = 0,
    COMMA_IX  = 1,
    RBRAK_IX  = 2,
    LPAREN_IX = 3,
    RPAREN_IX = 4,
    ARROW_IX  = 5,
    BTICK_IX  = 6,
    STAR_IX   = 7,
    SPACE_IX  = 8,
    LBRACE_IX = 9,
    RBRACE_IX = 10,
    COLON_IX  = 11,
    COLON_NSP = 12,
    PUNC_MAX  = 13
};

static any_str_t *type_punct[PUNC_MAX] = {0, };

static inline void
init_punctuation()
{
    if (type_punct[0] == NULL) {
	type_punct[LBRAK_IX]  = utf8_repeat('[', 1);
	type_punct[COMMA_IX]  = con4m_new(tspec_utf8(),
					  kw("cstring", ka(", ")));
	type_punct[RBRAK_IX]  = utf8_repeat(']', 1);
	type_punct[LPAREN_IX] = utf8_repeat('(', 1);
	type_punct[RPAREN_IX] = utf8_repeat(')', 1);
	type_punct[ARROW_IX]  = con4m_new(tspec_utf8(),
					  kw("cstring", ka(" -> ")));
	type_punct[BTICK_IX]  = utf8_repeat('`', 1);
	type_punct[STAR_IX]   = utf8_repeat('*', 1);
	type_punct[SPACE_IX]  = utf8_repeat(' ', 1);
	type_punct[LBRACE_IX] = utf8_repeat('{', 1);
	type_punct[RBRACE_IX] = utf8_repeat('}', 1);
	type_punct[COLON_IX]  = con4m_new(tspec_utf8(),
					  kw("cstring", ka(" : ")));
	type_punct[COLON_NSP] = utf8_repeat(':', 1);
    }
    con4m_gc_register_root(&type_punct[0], PUNC_MAX);
}

utf8_t *
get_lbrak_const()
{
    init_punctuation();
    return type_punct[LBRAK_IX];
}

utf8_t *
get_comma_const()
{
    init_punctuation();
    return type_punct[COMMA_IX];
}

utf8_t *
get_rbrak_const()
{
    init_punctuation();
    return type_punct[RBRAK_IX];
}

utf8_t *
get_lparen_const()
{
    init_punctuation();
    return type_punct[LPAREN_IX];
}

utf8_t *
get_rparen_const()
{
    init_punctuation();
    return type_punct[RPAREN_IX];
}

utf8_t *
get_arrow_const()
{
    init_punctuation();
    return type_punct[ARROW_IX];
}

utf8_t *
get_backtick_const()
{
    init_punctuation();
    return type_punct[BTICK_IX];
}

utf8_t *
get_asterisk_const()
{
    init_punctuation();
    return type_punct[STAR_IX];
}

utf8_t *
get_space_const()
{
    init_punctuation();
    return type_punct[SPACE_IX];
}

utf8_t *
get_lbrace_const()
{
    init_punctuation();
    return type_punct[LBRACE_IX];
}


utf8_t *
get_rbrace_const()
{
    init_punctuation();
    return type_punct[RBRACE_IX];
}

utf8_t *
get_colon_const()
{
    init_punctuation();
    return type_punct[COLON_IX];
}

utf8_t *
get_colon_no_space_const()
{
    init_punctuation();
    return type_punct[COLON_NSP];
}

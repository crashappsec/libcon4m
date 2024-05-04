#include "con4m.h"

static c4m_dict_t *mod_map[ST_MAX] = {
    NULL,
};

void
c4m_register_literal(c4m_lit_syntax_t st, char *mod, c4m_builtin_t bi)
{
    if (!hatrack_dict_add(mod_map[st],
                          c4m_new_utf8(mod),
                          (void *)(int64_t)bi)) {
        printf("%s\n", mod);
        C4M_CRAISE("Duplicate literal modifier for this syntax type.");
    }
}

static c4m_builtin_t
base_type_from_litmod(c4m_lit_syntax_t st, c4m_utf8_t *mod)
{
    c4m_builtin_t bi;
    bool          found = false;

    if (mod != NULL) {
        bi = (c4m_builtin_t)(int64_t)hatrack_dict_get(mod_map[st], mod, &found);
    }

    if (found) {
        return bi;
    }

    bi = (c4m_builtin_t)(int64_t)hatrack_dict_get(mod_map[st],
                                                  c4m_new_utf8("*"),
                                                  &found);

    if (found) {
        return bi;
    }

    return C4M_T_ERROR;
}

void
c4m_init_literal_handling()
{
    if (mod_map[0] == NULL) {
        c4m_type_t *ts = c4m_tspec_dict(c4m_tspec_utf8(), c4m_tspec_int());

        for (int i = 0; i < ST_MAX; i++) {
            mod_map[i] = c4m_new(ts);
        }

        c4m_gc_register_root(&mod_map[0], ST_MAX);

        c4m_register_literal(ST_Base10, "", C4M_T_INT);
        c4m_register_literal(ST_Base10, "int", C4M_T_INT);
        c4m_register_literal(ST_Base10, "i64", C4M_T_INT);
        c4m_register_literal(ST_Base10, "u64", C4M_T_UINT);
        c4m_register_literal(ST_Base10, "uint", C4M_T_UINT);
        c4m_register_literal(ST_Base10, "u32", C4M_T_U32);
        c4m_register_literal(ST_Base10, "i32", C4M_T_I32);
        c4m_register_literal(ST_Base10, "i8", C4M_T_BYTE);
        c4m_register_literal(ST_Base10, "byte", C4M_T_BYTE);
        c4m_register_literal(ST_Base10, "char", C4M_T_CHAR);
        c4m_register_literal(ST_Base10, "f", C4M_T_F64);
        c4m_register_literal(ST_Base10, "f64", C4M_T_F64);
        c4m_register_literal(ST_Hex, "", C4M_T_INT);
        c4m_register_literal(ST_Hex, "int", C4M_T_INT);
        c4m_register_literal(ST_Hex, "i64", C4M_T_INT);
        c4m_register_literal(ST_Hex, "u64", C4M_T_UINT);
        c4m_register_literal(ST_Hex, "uint", C4M_T_UINT);
        c4m_register_literal(ST_Hex, "u32", C4M_T_U32);
        c4m_register_literal(ST_Hex, "i32", C4M_T_I32);
        c4m_register_literal(ST_Hex, "i8", C4M_T_BYTE);
        c4m_register_literal(ST_Hex, "byte", C4M_T_BYTE);
        c4m_register_literal(ST_Hex, "char", C4M_T_CHAR);
        c4m_register_literal(ST_Float, "f", C4M_T_F64);
        c4m_register_literal(ST_Float, "f64", C4M_T_F64);
        c4m_register_literal(ST_2Quote, "", C4M_T_UTF8);
        c4m_register_literal(ST_2Quote, "*", C4M_T_UTF8);
        c4m_register_literal(ST_2Quote, "u8", C4M_T_UTF8);
        c4m_register_literal(ST_2Quote, "utf8", C4M_T_UTF8);
        c4m_register_literal(ST_2Quote, "r", C4M_T_UTF8);
        c4m_register_literal(ST_2Quote, "rich", C4M_T_UTF8);
        c4m_register_literal(ST_2Quote, "u32", C4M_T_UTF32);
        c4m_register_literal(ST_2Quote, "utf32", C4M_T_UTF32);
        c4m_register_literal(ST_2Quote, "date", C4M_T_DATE);
        c4m_register_literal(ST_2Quote, "time", C4M_T_TIME);
        c4m_register_literal(ST_2Quote, "datetime", C4M_T_DATETIME);
        c4m_register_literal(ST_2Quote, "dur", C4M_T_DURATION);
        c4m_register_literal(ST_2Quote, "duration", C4M_T_DURATION);
        c4m_register_literal(ST_2Quote, "ip", C4M_T_IPV4);
        c4m_register_literal(ST_2Quote, "sz", C4M_T_SIZE);
        c4m_register_literal(ST_2Quote, "size", C4M_T_SIZE);
        c4m_register_literal(ST_2Quote, "url", C4M_T_URL);
        c4m_register_literal(ST_1Quote, "", C4M_T_CHAR);
        c4m_register_literal(ST_1Quote, "c", C4M_T_CHAR);
        c4m_register_literal(ST_1Quote, "char", C4M_T_CHAR);
        c4m_register_literal(ST_1Quote, "b", C4M_T_BYTE);
        c4m_register_literal(ST_1Quote, "byte", C4M_T_BYTE);
        c4m_register_literal(ST_List, "", C4M_T_LIST);
        c4m_register_literal(ST_List, "l", C4M_T_LIST);
        c4m_register_literal(ST_List, "flow", C4M_T_GRID);
        c4m_register_literal(ST_List, "table", C4M_T_GRID);
        c4m_register_literal(ST_List, "ol", C4M_T_GRID);
        c4m_register_literal(ST_List, "ul", C4M_T_GRID);
        c4m_register_literal(ST_List, "list", C4M_T_LIST);
        c4m_register_literal(ST_List, "x", C4M_T_XLIST);
        c4m_register_literal(ST_List, "xlist", C4M_T_XLIST);
        c4m_register_literal(ST_List, "q", C4M_T_QUEUE);
        c4m_register_literal(ST_List, "queue", C4M_T_QUEUE);
        c4m_register_literal(ST_List, "t", C4M_T_TREE);
        c4m_register_literal(ST_List, "tree", C4M_T_TREE);
        c4m_register_literal(ST_List, "r", C4M_T_RING);
        c4m_register_literal(ST_List, "ring", C4M_T_RING);
        c4m_register_literal(ST_List, "log", C4M_T_LOGRING);
        c4m_register_literal(ST_List, "logring", C4M_T_LOGRING);
        c4m_register_literal(ST_List, "s", C4M_T_STACK);
        c4m_register_literal(ST_List, "stack", C4M_T_STACK);
        c4m_register_literal(ST_Dict, "", C4M_T_DICT);
        c4m_register_literal(ST_Dict, "d", C4M_T_DICT);
        c4m_register_literal(ST_Dict, "dict", C4M_T_DICT);
        c4m_register_literal(ST_Dict, "s", C4M_T_SET);
        c4m_register_literal(ST_Dict, "set", C4M_T_SET);
        c4m_register_literal(ST_Tuple, "", C4M_T_TUPLE);
        c4m_register_literal(ST_Tuple, "t", C4M_T_TUPLE);
        c4m_register_literal(ST_Tuple, "tuple", C4M_T_TUPLE);
    }
}

c4m_compile_error_t
c4m_parse_simple_lit(c4m_token_t *tok)
{
    c4m_init_literal_handling();

    c4m_utf8_t         *txt = c4m_token_raw_content(tok);
    c4m_utf8_t         *mod = c4m_to_utf8(tok->literal_modifier);
    c4m_lit_syntax_t    kind;
    c4m_compile_error_t err = c4m_err_no_error;

    switch (tok->kind) {
    case c4m_tt_int_lit:
        kind = ST_Base10;
        break;
    case c4m_tt_hex_lit:
        kind = ST_Hex;
        break;
    case c4m_tt_float_lit:
        kind = ST_Float;
        break;
    case c4m_tt_true:
    case c4m_tt_false:
        kind = ST_Bool;
        break;
    case c4m_tt_string_lit:
        kind = ST_2Quote;
        break;
    case c4m_tt_char_lit:
        kind = ST_1Quote;
        break;
    case c4m_tt_nil:
        // TODO-- one shared null value.
        tok->literal_value = c4m_new(c4m_tspec_void());
        return err;
    default:
        C4M_CRAISE("Token is not a simple literal");
    }

    c4m_builtin_t base_type = base_type_from_litmod(kind, mod);

    if (base_type == C4M_T_ERROR) {
        return c4m_err_parse_no_lit_mod_match;
    }

    c4m_vtable_t  *vtbl = (c4m_vtable_t *)c4m_base_type_info[base_type].vtable;
    c4m_literal_fn fn   = (c4m_literal_fn)vtbl->methods[C4M_BI_FROM_LITERAL];

    if (!fn) {
        return c4m_err_parse_no_lit_mod_match;
    }

    tok->literal_value = (*fn)(txt, kind, mod, &err);

    return err;
}

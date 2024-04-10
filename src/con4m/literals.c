#include "con4m.h"

static dict_t *mod_map[ST_MAX] = {
    NULL,
};

void
c4m_register_literal(c4m_lit_syntax_t st, char *mod, c4m_builtin_t bi)
{
    if (!hatrack_dict_add(mod_map[st], mod, (void *)(int64_t)bi)) {
        C4M_CRAISE("Duplicate literal modifier for this syntax type.");
    }
}

static c4m_builtin_t
base_type_from_litmod(c4m_lit_syntax_t st, char *mod)
{
    c4m_builtin_t bi;
    bool          found;

    bi = (c4m_builtin_t)(int64_t)hatrack_dict_get(mod_map[st], mod, &found);

    if (found) {
        return bi;
    }

    bi = (c4m_builtin_t)(int64_t)hatrack_dict_get(mod_map[st], "*", &found);

    if (found) {
        return bi;
    }

    return C4M_T_ERROR;
}

void
c4m_init_literal_handling()
{
    if (mod_map[0] != NULL) {
        type_spec_t *ts = c4m_tspec_dict(c4m_tspec_utf8(), c4m_tspec_int());

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

object_t
c4m_simple_lit(char            *raw,
               c4m_lit_syntax_t syntax,
               char            *lit_mod,
               c4m_lit_error_t *err)
{
    c4m_init_literal_handling();

    c4m_builtin_t base_type = base_type_from_litmod(syntax, lit_mod);

    if (base_type == C4M_T_ERROR) {
        err->code = LE_NoLitmodMatch;
        return NULL;
    }

    c4m_vtable_t  *vtbl = (c4m_vtable_t *)c4m_base_type_info[base_type].vtable;
    c4m_literal_fn fn   = (c4m_literal_fn)vtbl->methods[C4M_BI_FROM_LITERAL];

    if (!fn) {
        err->code = LE_NoLitmodMatch;
        return NULL;
    }

    return (*fn)(raw, syntax, lit_mod, err);
}

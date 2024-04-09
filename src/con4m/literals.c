#include "con4m.h"

static dict_t *mod_map[ST_MAX] = {
    NULL,
};

void
c4m_register_literal(syntax_t st, char *mod, c4m_builtin_t bi)
{
    if (!hatrack_dict_add(mod_map[st], mod, (void *)(int64_t)bi)) {
        C4M_CRAISE("Duplicate literal modifier for this syntax type.");
    }
}

static c4m_builtin_t
base_type_from_litmod(syntax_t st, char *mod)
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

    return T_TYPE_ERROR;
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

        c4m_register_literal(ST_Base10, "", T_INT);
        c4m_register_literal(ST_Base10, "int", T_INT);
        c4m_register_literal(ST_Base10, "i64", T_INT);
        c4m_register_literal(ST_Base10, "u64", T_UINT);
        c4m_register_literal(ST_Base10, "uint", T_UINT);
        c4m_register_literal(ST_Base10, "u32", T_U32);
        c4m_register_literal(ST_Base10, "i32", T_I32);
        c4m_register_literal(ST_Base10, "i32", T_I32);
        c4m_register_literal(ST_Base10, "i8", T_BYTE);
        c4m_register_literal(ST_Base10, "byte", T_BYTE);
        c4m_register_literal(ST_Base10, "char", T_CHAR);
        c4m_register_literal(ST_Base10, "f", T_F64);
        c4m_register_literal(ST_Base10, "f64", T_F64);
        c4m_register_literal(ST_Hex, "", T_INT);
        c4m_register_literal(ST_Hex, "int", T_INT);
        c4m_register_literal(ST_Hex, "i64", T_INT);
        c4m_register_literal(ST_Hex, "u64", T_UINT);
        c4m_register_literal(ST_Hex, "uint", T_UINT);
        c4m_register_literal(ST_Hex, "u32", T_U32);
        c4m_register_literal(ST_Hex, "i32", T_I32);
        c4m_register_literal(ST_Hex, "i32", T_I32);
        c4m_register_literal(ST_Hex, "i8", T_BYTE);
        c4m_register_literal(ST_Hex, "byte", T_BYTE);
        c4m_register_literal(ST_Hex, "char", T_CHAR);
        c4m_register_literal(ST_Float, "f", T_F64);
        c4m_register_literal(ST_Float, "f64", T_F64);
        c4m_register_literal(ST_2Quote, "", T_UTF8);
        c4m_register_literal(ST_2Quote, "*", T_UTF8);
        c4m_register_literal(ST_2Quote, "u8", T_UTF8);
        c4m_register_literal(ST_2Quote, "utf8", T_UTF8);
        c4m_register_literal(ST_2Quote, "r", T_UTF8);
        c4m_register_literal(ST_2Quote, "rich", T_UTF8);
        c4m_register_literal(ST_2Quote, "u32", T_UTF32);
        c4m_register_literal(ST_2Quote, "utf32", T_UTF32);
        c4m_register_literal(ST_2Quote, "date", T_DATE);
        c4m_register_literal(ST_2Quote, "time", T_TIME);
        c4m_register_literal(ST_2Quote, "datetime", T_DATETIME);
        c4m_register_literal(ST_2Quote, "dur", T_DURATION);
        c4m_register_literal(ST_2Quote, "duration", T_DURATION);
        c4m_register_literal(ST_2Quote, "ip", T_IPV4);
        c4m_register_literal(ST_2Quote, "sz", T_SIZE);
        c4m_register_literal(ST_2Quote, "size", T_SIZE);
        c4m_register_literal(ST_2Quote, "url", T_URL);
        c4m_register_literal(ST_1Quote, "", T_CHAR);
        c4m_register_literal(ST_1Quote, "c", T_CHAR);
        c4m_register_literal(ST_1Quote, "char", T_CHAR);
        c4m_register_literal(ST_1Quote, "b", T_BYTE);
        c4m_register_literal(ST_1Quote, "byte", T_BYTE);
        c4m_register_literal(ST_List, "", T_LIST);
        c4m_register_literal(ST_List, "l", T_LIST);
        c4m_register_literal(ST_List, "flow", T_GRID);
        c4m_register_literal(ST_List, "table", T_GRID);
        c4m_register_literal(ST_List, "ol", T_GRID);
        c4m_register_literal(ST_List, "ul", T_GRID);
        c4m_register_literal(ST_List, "list", T_LIST);
        c4m_register_literal(ST_List, "x", T_XLIST);
        c4m_register_literal(ST_List, "xlist", T_XLIST);
        c4m_register_literal(ST_List, "q", T_QUEUE);
        c4m_register_literal(ST_List, "queue", T_QUEUE);
        c4m_register_literal(ST_List, "t", T_TREE);
        c4m_register_literal(ST_List, "tree", T_TREE);
        c4m_register_literal(ST_List, "r", T_RING);
        c4m_register_literal(ST_List, "ring", T_RING);
        c4m_register_literal(ST_List, "log", T_LOGRING);
        c4m_register_literal(ST_List, "logring", T_LOGRING);
        c4m_register_literal(ST_List, "s", T_STACK);
        c4m_register_literal(ST_List, "stack", T_STACK);
        c4m_register_literal(ST_Dict, "", T_DICT);
        c4m_register_literal(ST_Dict, "d", T_DICT);
        c4m_register_literal(ST_Dict, "dict", T_DICT);
        c4m_register_literal(ST_Dict, "s", T_SET);
        c4m_register_literal(ST_Dict, "set", T_SET);
        c4m_register_literal(ST_Tuple, "", T_TUPLE);
        c4m_register_literal(ST_Tuple, "t", T_TUPLE);
        c4m_register_literal(ST_Tuple, "tuple", T_TUPLE);
    }
}

object_t
c4m_simple_lit(char *raw, syntax_t syntax, char *lit_mod, lit_error_t *err)
{
    c4m_init_literal_handling();

    c4m_builtin_t base_type = base_type_from_litmod(syntax, lit_mod);

    if (base_type == T_TYPE_ERROR) {
        err->code = LE_NoLitmodMatch;
        return NULL;
    }

    c4m_vtable_t *vtbl = (c4m_vtable_t *)builtin_type_info[base_type].vtable;
    literal_fn    fn   = (literal_fn)vtbl->methods[C4M_BI_FROM_LITERAL];

    if (!fn) {
        err->code = LE_NoLitmodMatch;
        return NULL;
    }

    return (*fn)(raw, syntax, lit_mod, err);
}

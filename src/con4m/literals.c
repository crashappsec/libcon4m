#include <con4m.h>

static dict_t *mod_map[ST_MAX] = {
    NULL,
};

void
register_literal(syntax_t st, char *mod, con4m_builtin_t bi)
{
    if (!hatrack_dict_add(mod_map[st], mod, (void *)(int64_t)bi)) {
        CRAISE("Duplicate literal modifier for this syntax type.");
    }
}

static con4m_builtin_t
base_type_from_litmod(syntax_t st, char *mod)
{
    con4m_builtin_t bi;
    bool            found;

    bi = (con4m_builtin_t)(int64_t)hatrack_dict_get(mod_map[st], mod, &found);

    if (found) {
        return bi;
    }

    bi = (con4m_builtin_t)(int64_t)hatrack_dict_get(mod_map[st], "*", &found);

    if (found) {
        return bi;
    }

    return T_TYPE_ERROR;
}

void
init_literal_handling()
{
    if (mod_map[0] != NULL) {
        type_spec_t *ts = tspec_dict(tspec_utf8(), tspec_int());

        for (int i = 0; i < ST_MAX; i++) {
            mod_map[i] = con4m_new(ts);
        }

        con4m_gc_register_root(&mod_map[0], ST_MAX);

        register_literal(ST_Base10, "", T_INT);
        register_literal(ST_Base10, "int", T_INT);
        register_literal(ST_Base10, "i64", T_INT);
        register_literal(ST_Base10, "u64", T_UINT);
        register_literal(ST_Base10, "uint", T_UINT);
        register_literal(ST_Base10, "u32", T_U32);
        register_literal(ST_Base10, "i32", T_I32);
        register_literal(ST_Base10, "i32", T_I32);
        register_literal(ST_Base10, "i8", T_BYTE);
        register_literal(ST_Base10, "byte", T_BYTE);
        register_literal(ST_Base10, "char", T_CHAR);
        register_literal(ST_Base10, "f", T_F64);
        register_literal(ST_Base10, "f64", T_F64);
        register_literal(ST_Hex, "", T_INT);
        register_literal(ST_Hex, "int", T_INT);
        register_literal(ST_Hex, "i64", T_INT);
        register_literal(ST_Hex, "u64", T_UINT);
        register_literal(ST_Hex, "uint", T_UINT);
        register_literal(ST_Hex, "u32", T_U32);
        register_literal(ST_Hex, "i32", T_I32);
        register_literal(ST_Hex, "i32", T_I32);
        register_literal(ST_Hex, "i8", T_BYTE);
        register_literal(ST_Hex, "byte", T_BYTE);
        register_literal(ST_Hex, "char", T_CHAR);
        register_literal(ST_Float, "f", T_F64);
        register_literal(ST_Float, "f64", T_F64);
        register_literal(ST_2Quote, "", T_UTF8);
        register_literal(ST_2Quote, "*", T_UTF8);
        register_literal(ST_2Quote, "u8", T_UTF8);
        register_literal(ST_2Quote, "utf8", T_UTF8);
        register_literal(ST_2Quote, "r", T_UTF8);
        register_literal(ST_2Quote, "rich", T_UTF8);
        register_literal(ST_2Quote, "u32", T_UTF32);
        register_literal(ST_2Quote, "utf32", T_UTF32);
        register_literal(ST_2Quote, "date", T_DATE);
        register_literal(ST_2Quote, "time", T_TIME);
        register_literal(ST_2Quote, "datetime", T_DATETIME);
        register_literal(ST_2Quote, "dur", T_DURATION);
        register_literal(ST_2Quote, "duration", T_DURATION);
        register_literal(ST_2Quote, "ip", T_IPV4);
        register_literal(ST_2Quote, "sz", T_SIZE);
        register_literal(ST_2Quote, "size", T_SIZE);
        register_literal(ST_2Quote, "url", T_URL);
        register_literal(ST_1Quote, "", T_CHAR);
        register_literal(ST_1Quote, "c", T_CHAR);
        register_literal(ST_1Quote, "char", T_CHAR);
        register_literal(ST_1Quote, "b", T_BYTE);
        register_literal(ST_1Quote, "byte", T_BYTE);
        register_literal(ST_List, "", T_LIST);
        register_literal(ST_List, "l", T_LIST);
        register_literal(ST_List, "flow", T_GRID);
        register_literal(ST_List, "table", T_GRID);
        register_literal(ST_List, "ol", T_GRID);
        register_literal(ST_List, "ul", T_GRID);
        register_literal(ST_List, "list", T_LIST);
        register_literal(ST_List, "x", T_XLIST);
        register_literal(ST_List, "xlist", T_XLIST);
        register_literal(ST_List, "q", T_QUEUE);
        register_literal(ST_List, "queue", T_QUEUE);
        register_literal(ST_List, "t", T_TREE);
        register_literal(ST_List, "tree", T_TREE);
        register_literal(ST_List, "r", T_RING);
        register_literal(ST_List, "ring", T_RING);
        register_literal(ST_List, "log", T_LOGRING);
        register_literal(ST_List, "logring", T_LOGRING);
        register_literal(ST_List, "s", T_STACK);
        register_literal(ST_List, "stack", T_STACK);
        register_literal(ST_Dict, "", T_DICT);
        register_literal(ST_Dict, "d", T_DICT);
        register_literal(ST_Dict, "dict", T_DICT);
        register_literal(ST_Dict, "s", T_SET);
        register_literal(ST_Dict, "set", T_SET);
        register_literal(ST_Tuple, "", T_TUPLE);
        register_literal(ST_Tuple, "t", T_TUPLE);
        register_literal(ST_Tuple, "tuple", T_TUPLE);
    }
}

object_t
con4m_simple_lit(char *raw, syntax_t syntax, char *lit_mod, lit_error_t *err)
{
    init_literal_handling();

    con4m_builtin_t base_type = base_type_from_litmod(syntax, lit_mod);

    if (base_type == T_TYPE_ERROR) {
        err->code = LE_NoLitmodMatch;
        return NULL;
    }

    con4m_vtable *vtbl = (con4m_vtable *)builtin_type_info[base_type].vtable;
    literal_fn    fn   = (literal_fn)vtbl->methods[CON4M_BI_FROM_LITERAL];

    if (!fn) {
        err->code = LE_NoLitmodMatch;
        return NULL;
    }

    return (*fn)(raw, syntax, lit_mod, err);
}

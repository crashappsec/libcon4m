#include "con4m.h"

static c4m_dict_t *mod_map[ST_MAX] = {
    NULL,
};

// TODO: Change this when adding objects.
static int       container_bitfield_words = (C4M_NUM_BUILTIN_DTS + 63) / 64;
static uint64_t *list_types               = NULL;
static uint64_t *dict_types;
static uint64_t *set_types;
static uint64_t *tuple_types;
static uint64_t *all_container_types;

int DEBUG_ON = 0;

static inline void
no_more_containers()
{
    all_container_types = c4m_gc_array_alloc(uint64_t,
                                             container_bitfield_words);

    for (int i = 0; i < container_bitfield_words; i++) {
        all_container_types[i] = list_types[i] | dict_types[i];
        all_container_types[i] |= set_types[i] | tuple_types[i];
    }
}

static void
initialize_container_bitfields()
{
    if (list_types == NULL) {
        list_types  = c4m_gc_array_alloc(uint64_t, container_bitfield_words);
        dict_types  = c4m_gc_array_alloc(uint64_t, container_bitfield_words);
        set_types   = c4m_gc_array_alloc(uint64_t, container_bitfield_words);
        tuple_types = c4m_gc_array_alloc(uint64_t, container_bitfield_words);
        c4m_gc_register_root(&list_types, 1);
        c4m_gc_register_root(&dict_types, 1);
        c4m_gc_register_root(&set_types, 1);
        c4m_gc_register_root(&tuple_types, 1);
        c4m_gc_register_root(&all_container_types, 1);
        c4m_gc_register_root(&mod_map, ST_MAX);
    }
}

void
c4m_register_container_type(c4m_builtin_t    bi,
                            c4m_lit_syntax_t st,
                            bool             alt_syntax)
{
    initialize_container_bitfields();
    int word = ((int)bi) / 64;
    int bit  = ((int)bi) % 64;

    switch (st) {
    case ST_List:
        list_types[word] |= 1UL << bit;
        return;
    case ST_Dict:
        if (alt_syntax) {
            set_types[word] |= 1UL << bit;
        }
        else {
            dict_types[word] |= 1UL << bit;
        }
        return;
    case ST_Tuple:
        tuple_types[word] |= 1UL << bit;
        return;
    default:
        c4m_unreachable();
    }
}

void
c4m_register_literal(c4m_lit_syntax_t st, char *mod, c4m_builtin_t bi)
{
    DEBUG_ON          = 1;
    c4m_utf8_t *u8mod = c4m_new_utf8(mod);
    if (!hatrack_dict_add(mod_map[st],
                          u8mod,
                          (void *)(int64_t)bi)) {
        C4M_CRAISE("Duplicate literal modifier for this syntax type.");
    }

    switch (st) {
    case ST_List:
    case ST_Tuple:
        c4m_register_container_type(bi, st, false);
        return;
    case ST_Dict:
        if (bi == C4M_T_SET) {
            c4m_register_container_type(bi, st, true);
        }
        else {
            c4m_register_container_type(bi, st, false);
        }
        return;
    default:
        return;
    }
}

c4m_builtin_t
c4m_base_type_from_litmod(c4m_lit_syntax_t st, c4m_utf8_t *mod)
{
    c4m_builtin_t bi;
    bool          found = false;

    if (mod == NULL) {
        mod = c4m_new_utf8("");
    }
    mod = c4m_to_utf8(mod);

    DEBUG_ON = 1;
    bi       = (c4m_builtin_t)(int64_t)hatrack_dict_get(mod_map[st], mod, &found);

    if (found) {
        return bi;
    }
    bi       = (c4m_builtin_t)(int64_t)hatrack_dict_get(mod_map[st],
                                                  c4m_new_utf8("*"),
                                                  &found);
    DEBUG_ON = 0;

    if (found) {
        return bi;
    }

    return C4M_T_ERROR;
}

void
c4m_init_literal_handling()
{
    if (mod_map[0] == NULL) {
        for (int i = 0; i < ST_MAX; i++) {
            mod_map[i] = c4m_dict(c4m_type_utf8(), c4m_type_int());
        }

        c4m_gc_register_root(&mod_map[0], ST_MAX);

        c4m_register_literal(ST_Bool, "", C4M_T_BOOL);
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
        c4m_register_literal(ST_List, "", C4M_T_XLIST);
        c4m_register_literal(ST_List, "l", C4M_T_XLIST);
        c4m_register_literal(ST_List, "flow", C4M_T_GRID);
        c4m_register_literal(ST_List, "table", C4M_T_GRID);
        c4m_register_literal(ST_List, "ol", C4M_T_GRID);
        c4m_register_literal(ST_List, "ul", C4M_T_GRID);
        c4m_register_literal(ST_List, "list", C4M_T_XLIST);
        c4m_register_literal(ST_List, "f", C4M_T_FLIST);
        c4m_register_literal(ST_List, "flist", C4M_T_FLIST);
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
        no_more_containers();
    }
}

c4m_compile_error_t
c4m_parse_simple_lit(c4m_token_t *tok, c4m_lit_syntax_t *kptr, c4m_utf8_t **lm)
{
    c4m_init_literal_handling();

    c4m_utf8_t         *txt = c4m_token_raw_content(tok);
    c4m_utf8_t         *mod = c4m_to_utf8(tok->literal_modifier);
    c4m_lit_syntax_t    kind;
    c4m_compile_error_t err = c4m_err_no_error;

    if (lm != NULL) {
        *lm = mod;
    }

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
        tok->literal_value = c4m_new(c4m_type_void());
        return err;
    default:
        C4M_CRAISE("Token is not a simple literal");
    }

    c4m_builtin_t base_type = c4m_base_type_from_litmod(kind, mod);

    if (base_type == C4M_T_ERROR) {
        return c4m_err_parse_no_lit_mod_match;
    }

    c4m_vtable_t  *vtbl = (c4m_vtable_t *)c4m_base_type_info[base_type].vtable;
    c4m_literal_fn fn   = (c4m_literal_fn)vtbl->methods[C4M_BI_FROM_LITERAL];

    if (!fn) {
        return c4m_err_parse_no_lit_mod_match;
    }

    tok->literal_value = (*fn)(txt, kind, mod, &err);

    if (kptr != NULL) {
        *kptr = kind;
    }

    return err;
}

bool
c4m_type_has_list_syntax(c4m_type_t *t)
{
    uint64_t bi      = t->details->base_type->typeid;
    int      word    = ((int)bi) / 64;
    int      bit     = ((int)bi) % 64;
    uint64_t to_test = 1UL << bit;

    return list_types[word] & to_test;
}

bool
c4m_type_has_dict_syntax(c4m_type_t *t)
{
    uint64_t bi      = t->details->base_type->typeid;
    int      word    = ((int)bi) / 64;
    int      bit     = ((int)bi) % 64;
    uint64_t to_test = 1UL << bit;

    return dict_types[word] & to_test;
}

bool
c4m_type_has_set_syntax(c4m_type_t *t)
{
    uint64_t bi      = t->details->base_type->typeid;
    int      word    = ((int)bi) / 64;
    int      bit     = ((int)bi) % 64;
    uint64_t to_test = 1UL << bit;

    return set_types[word] & to_test;
}

bool
c4m_type_has_tuple_syntax(c4m_type_t *t)
{
    uint64_t bi      = t->details->base_type->typeid;
    int      word    = ((int)bi) / 64;
    int      bit     = ((int)bi) % 64;
    uint64_t to_test = 1UL << bit;

    return tuple_types[word] & to_test;
}
int
c4m_get_num_bitfield_words()
{
    return container_bitfield_words;
}

uint64_t *
c4m_get_list_bitfield()
{
    c4m_init_literal_handling();

    uint64_t *result = c4m_gc_array_alloc(uint64_t, container_bitfield_words);
    for (int i = 0; i < container_bitfield_words; i++) {
        result[i] = list_types[i];
    }

    return result;
}

uint64_t *
c4m_get_dict_bitfield()
{
    c4m_init_literal_handling();

    uint64_t *result = c4m_gc_array_alloc(uint64_t, container_bitfield_words);
    for (int i = 0; i < container_bitfield_words; i++) {
        result[i] = dict_types[i];
    }

    return result;
}

uint64_t *
c4m_get_set_bitfield()
{
    c4m_init_literal_handling();

    uint64_t *result = c4m_gc_array_alloc(uint64_t, container_bitfield_words);
    for (int i = 0; i < container_bitfield_words; i++) {
        result[i] = set_types[i];
    }

    return result;
}

uint64_t *
c4m_get_tuple_bitfield()
{
    c4m_init_literal_handling();

    uint64_t *result = c4m_gc_array_alloc(uint64_t, container_bitfield_words);
    for (int i = 0; i < container_bitfield_words; i++) {
        result[i] = tuple_types[i];
    }

    return result;
}

uint64_t *
c4m_get_all_containers_bitfield()
{
    c4m_init_literal_handling();

    uint64_t *result = c4m_gc_array_alloc(uint64_t, container_bitfield_words);
    for (int i = 0; i < container_bitfield_words; i++) {
        result[i] = all_container_types[i];
    }

    return result;
}

bool
c4m_partial_inference(c4m_type_t *t)
{
    tv_options_t *tsi = t->details->tsi;

    for (int i = 0; i < container_bitfield_words; i++) {
        if (tsi->container_options[i] ^ all_container_types[i]) {
            return true;
        }
    }

    return false;
}

uint64_t *
c4m_get_no_containers_bitfield()
{
    c4m_init_literal_handling();

    return c4m_gc_array_alloc(uint64_t, container_bitfield_words);
}

bool
c4m_list_syntax_possible(c4m_type_t *t)
{
    tv_options_t *tsi = t->details->tsi;

    for (int i = 0; i < container_bitfield_words; i++) {
        if (tsi->container_options[i] & list_types[i]) {
            return true;
        }
    }

    return false;
}

bool
c4m_dict_syntax_possible(c4m_type_t *t)
{
    tv_options_t *tsi = t->details->tsi;

    for (int i = 0; i < container_bitfield_words; i++) {
        if (tsi->container_options[i] & dict_types[i]) {
            return true;
        }
    }

    return false;
}

bool
c4m_set_syntax_possible(c4m_type_t *t)
{
    tv_options_t *tsi = t->details->tsi;

    for (int i = 0; i < container_bitfield_words; i++) {
        if (tsi->container_options[i] & set_types[i]) {
            return true;
        }
    }

    return false;
}

bool
c4m_tuple_syntax_possible(c4m_type_t *t)
{
    tv_options_t *tsi = t->details->tsi;

    for (int i = 0; i < container_bitfield_words; i++) {
        if (tsi->container_options[i] & tuple_types[i]) {
            return true;
        }
    }

    return false;
}

void
c4m_remove_list_options(c4m_type_t *t)
{
    tv_options_t *tsi = t->details->tsi;
    for (int i = 0; i < container_bitfield_words; i++) {
        tsi->container_options[i] &= ~(list_types[i]);
    }
}

void
c4m_remove_set_options(c4m_type_t *t)
{
    tv_options_t *tsi = t->details->tsi;
    for (int i = 0; i < container_bitfield_words; i++) {
        tsi->container_options[i] &= ~(set_types[i]);
    }
}

void
c4m_remove_dict_options(c4m_type_t *t)
{
    tv_options_t *tsi = t->details->tsi;
    for (int i = 0; i < container_bitfield_words; i++) {
        tsi->container_options[i] &= ~(dict_types[i]);
    }
}

void
c4m_remove_tuple_options(c4m_type_t *t)
{
    tv_options_t *tsi = t->details->tsi;
    for (int i = 0; i < container_bitfield_words; i++) {
        tsi->container_options[i] &= ~(tuple_types[i]);
    }
}

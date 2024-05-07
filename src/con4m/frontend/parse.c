#define C4M_USE_INTERNAL_API

#include "con4m.h"
// TODO: clean up stack to set the tree properly when a RAISE happens.

typedef struct checkpoint_t {
    struct checkpoint_t *prev;
    jmp_buf              env;
    char                *fn;
} checkpoint_t;

typedef struct {
    c4m_tree_node_t      *cur;
    c4m_file_compile_ctx *file_ctx;
    c4m_token_t          *cached_token;
    int32_t               token_ix;
    int32_t               cache_ix;
    int32_t               loop_depth;
    // This is used to figure out whether we should allow a newline
    // after a ), ] or }. If we're inside a literal definition, we
    // allow it. If we're in a literal definition context, the newline
    // is okay, otherwise it is not.
    int32_t               lit_depth;
    hatstack_t           *root_stack;
    bool                  in_function;
    checkpoint_t         *jump_state;
} parse_ctx;

#undef PARSE_DEBUG

#ifdef PARSE_DEBUG
static inline c4m_token_t *_tok_cur(parse_ctx *, int);
#define consume(ctx) _consume(ctx, __LINE__)
#define tok_cur(ctx) _tok_cur(ctx, __LINE__)
#define start_node(ctx, kind, consume_token) \
    _start_node(ctx, kind, consume_token, __LINE__)
#else
static inline c4m_token_t *_tok_cur(parse_ctx *);
#define consume(ctx) _consume(ctx)
#define tok_cur(ctx) _tok_cur(ctx)
#define start_node(ctx, kind, consume_token) \
    _start_node(ctx, kind, consume_token)
#endif

#define DEBUG_CUR(x)                                           \
    {                                                          \
        c4m_utf8_t *prefix = c4m_new_utf8(x);                  \
        c4m_print(c4m_format_one_token(tok_cur(ctx), prefix)); \
    }

static inline void
c4m_end_checkpoint(parse_ctx *ctx)
{
    if (!ctx->jump_state)
        return;
    ctx->jump_state = ctx->jump_state->prev;
}

static inline __attribute__((noreturn)) void
c4m_exit_to_checkpoint(parse_ctx  *ctx,
                       int         code,
                       const char *f,
                       int         line,
                       const char *fn)
{
#ifdef PARSE_DEBUG
    printf("Bailed @%s:%d (func = %s)\n", f, line, fn);
    c4m_print(c4m_format_one_token(tok_cur(ctx),
                                   c4m_new_utf8("Current token: ")));
#endif
    longjmp(ctx->jump_state->env, code);
}

#define DECLARE_CHECKPOINT() \
    int checkpoint_error = 0;

#define ENTER_CHECKPOINT()                             \
    if (!checkpoint_error) {                           \
        checkpoint_t *cp = c4m_gc_alloc(checkpoint_t); \
        cp->prev         = ctx->jump_state;            \
        cp->fn           = (char *)__func__;           \
        ctx->jump_state  = cp;                         \
        checkpoint_error = setjmp(cp->env);            \
        if (checkpoint_error != 0) {                   \
            ctx->jump_state = cp->prev;                \
        }                                              \
    }

#define CHECKPOINT_STATUS() (checkpoint_error)

#define END_CHECKPOINT() c4m_end_checkpoint(ctx)
#define THROW(code)      c4m_exit_to_checkpoint(ctx, \
                                           code,     \
                                           __FILE__, \
                                           __LINE__, \
                                           __func__)

#ifdef PARSE_DEBUG
static inline void _consume(parse_ctx *, int);
#else
static inline void _consume(parse_ctx *);
#endif

static void             identifier(parse_ctx *);
static void             body(parse_ctx *, c4m_pnode_t *);
static inline bool      line_skip_recover(parse_ctx *);
static void             one_tspec_node(parse_ctx *);
static void             typeof_stmt(parse_ctx *);
static void             switch_stmt(parse_ctx *);
static void             continue_stmt(parse_ctx *);
static void             break_stmt(parse_ctx *);
static void             return_stmt(parse_ctx *);
static c4m_tree_node_t *expression(parse_ctx *);
static c4m_tree_node_t *assign(parse_ctx *, c4m_tree_node_t *, c4m_node_kind_t);
static c4m_tree_node_t *member_expr(parse_ctx *, c4m_tree_node_t *);
static c4m_tree_node_t *index_expr(parse_ctx *, c4m_tree_node_t *);
static c4m_tree_node_t *call_expr(parse_ctx *, c4m_tree_node_t *);
static c4m_tree_node_t *section(parse_ctx *, c4m_tree_node_t *);
static bool             literal(parse_ctx *);
static void             label_stmt(parse_ctx *);
static void             assert_stmt(parse_ctx *);
static void             use_stmt(parse_ctx *);
static void             basic_member_expr(parse_ctx *);
static void             formal_list(parse_ctx *);
static c4m_node_kind_t  variable_decl(parse_ctx *);
static c4m_tree_node_t *shl_expr_rhs(parse_ctx *);
static c4m_tree_node_t *shr_expr_rhs(parse_ctx *);
static c4m_tree_node_t *div_expr_rhs(parse_ctx *);
static c4m_tree_node_t *mul_expr_rhs(parse_ctx *);
static c4m_tree_node_t *mod_expr_rhs(parse_ctx *);
static c4m_tree_node_t *minus_expr_rhs(parse_ctx *);
static c4m_tree_node_t *plus_expr_rhs(parse_ctx *);
static c4m_tree_node_t *lt_expr_rhs(parse_ctx *);
static c4m_tree_node_t *gt_expr_rhs(parse_ctx *);
static c4m_tree_node_t *lte_expr_rhs(parse_ctx *);
static c4m_tree_node_t *gte_expr_rhs(parse_ctx *);
static c4m_tree_node_t *eq_expr_rhs(parse_ctx *);
static c4m_tree_node_t *bit_and_expr_rhs(parse_ctx *);
static c4m_tree_node_t *bit_xor_expr_rhs(parse_ctx *);
static c4m_tree_node_t *bit_or_expr_rhs(parse_ctx *);
static c4m_tree_node_t *ne_expr_rhs(parse_ctx *);
static c4m_tree_node_t *and_expr_rhs(parse_ctx *);

typedef struct {
    char *ctype_name;
    int   index;
    bool  can_take_param;
} ctype_name_info_t;

static const ctype_name_info_t ctype_info[] = {
    // clang-format off
    { "void",     0,  false, },
    { "cvoid",    0,  false, },
    { "u8",       1,  false, },
    { "cu8",      1,  false, },
    { "i8",       2,  false, },
    { "ci8",      2,  false, },
    { "u16",      3,  false, },
    { "cu16",     3,  false, },
    { "i16",      4,  false, },
    { "ci16",     4,  false, },
    { "u32",      5,  false, },
    { "cu32",     5,  false, },
    { "i32",      6,  false, },
    { "ci32",     6,  false, },
    { "u64",      7,  false, },
    { "cu64",     7,  false, },
    { "i64",      8,  false, },
    { "ci64",     8,  false, },
    { "cfloat",   9,  false, },
    { "cdouble",  10, false, },
    { "double",   10, false, },
    { "cuchar",   11, false, },
    { "cchar",    12, false, },
    { "short",    13, false, },
    { "cshort",   13, false, },
    { "ushort",   14, false, },
    { "cushort",  14, false, },
    { "cint",     15, false, },
    { "cuint",    16, false, },
    { "long",     17, false, },
    { "ulong",    18, false, },
    { "bool",     19, false, },
    { "size_t",   20, false, },
    { "size",     20, false, },
    { "size_t",   20, false, },
    { "csize",    20, false, },
    { "csize_t",  20, false, },
    { "ssize",    21, false, },
    { "ssize_t",  21, false, },
    { "cssize",   21, false, },
    { "ssize_t",  21, false, },
    { "cssize_t", 21, false, },
    { "ptr",      22, true, },
    { "pointer",  22, true, },
    { "cstring",  23, true, },
    { "carray",   24, true, },
    { "array",    24, true, },
    { NULL,       0,  false, },
    // clang-format on
};

typedef struct {
    char        *name;
    unsigned int show_contents : 1;
    unsigned int terminal      : 1;
    unsigned int literal       : 1;
    unsigned int takes_docs    : 1;
    int          aux_alloc_size;
} node_type_info_t;

static const node_type_info_t node_type_info[] = {
    // clang-format off
    { "nt_error", 1, 1, 0, 0, 0, },
    { "nt_module", 0, 0, 0, 1, 0, },
    { "nt_body", 0, 0, 0, 0, 0, },
    { "nt_assign", 0, 0, 0, 0, 0, },
    { "nt_attr_set_lock", 0, 0, 0, 0, 0, },
    { "nt_cast", 0, 0, 0, 0, 0, },
    { "nt_section", 0, 0, 0, 1, 0, },
    { "nt_if", 0, 0, 0, 0, 0, },
    { "nt_elif", 0, 0, 0, 0, 0, },
    { "nt_else", 0, 0, 0, 0, 0, },
    { "nt_typeof", 0, 0, 0, 0, 0, },
    { "nt_switch", 0, 0, 0, 0, 0, },
    { "nt_for", 0, 0, 0, 0, 0, },
    { "nt_while", 0, 0, 0, 0, 0, },
    { "nt_break", 0, 0, 0, 0, 0, },
    { "nt_continue", 0, 0, 0, 0, 0, },
    { "nt_return", 0, 0, 0, 0, 0, },
    { "nt_simple_lit", 1, 1, 1, 0, 0, },
    { "nt_lit_list", 0, 0, 1, 0, 0, },
    { "nt_lit_dict", 0, 0, 1, 0, 0, },
    { "nt_lit_set", 0, 0, 1, 0, 0, },
    { "nt_lit_empty_dict_or_set", 0, 1, 1, 0, 0, },
    { "nt_lit_tuple", 0, 0, 1, 0, 0, },
    { "nt_lit_unquoted", 1, 1, 1, 0, 0, },
    { "nt_lit_callback", 0, 0, 0, 0, 0, },
    { "nt_lit_tspec", 0, 0, 0, 0, 0, },
    { "nt_lit_tspec_tvar", 1, 1, 1, 0, 0, },
    { "nt_lit_tspec_named_type", 1, 1, 1, 0, 0, },
    { "nt_lit_tspec_parameterized_type", 1, 0, 0, 0, 0, },
    { "nt_lit_tspec_func", 0, 0, 0, 0, 0, },
    { "nt_lit_tspec_varargs", 0, 0, 0, 0, 0, },
    { "nt_lit_tspec_return_type", 0, 0, 0, 0, 0, },
    { "nt_or", 0, 0, 0, 0, 0, },
    { "nt_and", 0, 0, 0, 0, 0, },
    { "nt_cmp", 1, 0, 0, 0, 0, },
    { "nt_binary_op", 1, 0, 0, 0, 0, },
    { "nt_binary_assign_op", 1, 0, 0, 0, 0, },
    { "nt_unary_op", 1, 0, 0, 0, 0, },
    { "nt_enum", 0, 0, 0, 0, 0, },
    { "nt_global_enum", 0, 0, 0, 0, 0, },
    { "nt_enum_item", 1, 0, 0, 0, 0, },
    { "nt_identifier", 1, 1, 0, 0, 0, },
    { "nt_func_def", 1, 0, 0, 1, 0, },
    { "nt_formals", 0, 0, 0, 0, 0, },
    { "nt_varargs_param", 0, 0, 0, 0, 0, },
    { "nt_member", 0, 0, 0, 0, 0, },
    { "nt_index", 0, 0, 0, 0, 0, },
    { "nt_call", 0, 0, 0, 0, 0, },
    { "nt_paren_expr", 0, 0, 0, 0, 0, },
    { "nt_var_decls", 0, 0, 0, 0, 0, },
    { "nt_global_decls", 0, 0, 0, 0, 0, },
    { "nt_const_var_decls", 0, 0, 0, 0, 0, },
    { "nt_const_global_decls", 0, 0, 0, 0, 0, },
    { "nt_const_decls,", 0, 0, 0, 0, 0, },
    { "nt_sym_decl", 0, 0, 0, 0, 0, },
    { "nt_use", 0, 0, 0, 0, 0, },
    { "nt_param_block", 0, 0, 0, 1, 0, },
    { "nt_param_prop", 1, 0, 0, 0, 0, },
    { "nt_extern_block", 0, 0, 0, 1, 0, },
    { "nt_extern_sig", 0, 0, 0, 0, 0, },
    { "nt_extern_param", 1, 0, 0, 0, 0, },
    { "nt_extern_local", 0, 0, 0, 0, 0, },
    { "nt_extern_dll", 0, 0, 0, 0, 0, },
    { "nt_extern_pure", 0, 0, 0, 0, 0, },
    { "nt_extern_holds", 0, 0, 0, 0, 0, },
    { "nt_extern_allocs", 0, 0, 0, 0, 0, },
    { "nt_extern_return", 0, 0, 0, 0, 0, },
    { "nt_extern_expression", 0, 0, 0, 0, 0, },
    { "nt_label", 1, 1, 0, 0, 0, },
    { "nt_case", 0, 0, 0, 0, 0, },
    { "nt_range", 0, 0, 0, 0, 0, },
    { "nt_assert", 0, 0, 0, 0, 0, },
    { "nt_config_spec", 0, 0, 0, 1, 0, },
    { "nt_section_spec", 0, 0, 0, 1, 0, },
    { "nt_section_prop", 1, 0, 0, 0, 0, },
    { "nt_field_spec", 0, 0, 0, 1, 0, },
    { "nt_field_prop", 1, 0, 0, 0, 0, },
    { "nt_short_doc_string", 1, 1, 0, 0, 0, },
    { "nt_long_doc_string", 1, 1, 0, 0, 0, },
    { "nt_expression", 0, 0, 0, 0, 0, },
    // clang-format on
};

static int
lookup_ctype_id(char *found)
{
    ctype_name_info_t *info = (ctype_name_info_t *)&ctype_info[0];

    while (true) {
        if (info->ctype_name == NULL) {
            return -1;
        }
        if (!strcmp(info->ctype_name, found)) {
            return info->index;
        }

        info++;
    }
}

#ifdef PARSE_DEBUG
static inline c4m_token_t *
_tok_cur(parse_ctx *ctx, int line)
#else
static inline c4m_token_t *
_tok_cur(parse_ctx *ctx)
#endif
{
    if (!ctx->cached_token || ctx->token_ix != ctx->cache_ix) {
        ctx->cached_token = c4m_xlist_get(ctx->file_ctx->tokens,
                                          ctx->token_ix,
                                          NULL);
        ctx->cache_ix     = ctx->token_ix;
    }

#ifdef PARSE_DEBUG
    if (line == -1) {
        return ctx->cached_token;
    }

    c4m_utf8_t *prefix = c4m_cstr_format("parse.c line {}: tok_cur(ctx) = ",
                                         c4m_box_i32(line));

    c4m_print(c4m_format_one_token(ctx->cached_token, prefix));
#endif
    return ctx->cached_token;
}

static inline c4m_token_kind_t
tok_kind(parse_ctx *ctx)
{
#ifdef PARSE_DEBUG
    return _tok_cur(ctx, -1)->kind;
#else
    return _tok_cur(ctx)->kind;
#endif
}

static void
_add_parse_error(parse_ctx *ctx, c4m_compile_error_t code, ...)
{
    va_list args;

    va_start(args, code);
    c4m_base_add_error(ctx->file_ctx->errors,
                       code,
                       tok_cur(ctx),
                       c4m_err_severity_error,
                       args);
    va_end(args);
}

#define add_parse_error(ctx, code, ...) \
    _add_parse_error(ctx, code, KFUNC(__VA_ARGS__))

static void __attribute__((noreturn))
_raise_err_at_node(parse_ctx          *ctx,
                   c4m_pnode_t        *n,
                   c4m_compile_error_t code,
                   bool                bail,
                   const char         *f,
                   int                 line,
                   const char         *fn)
{
    c4m_compile_error *err = c4m_gc_alloc(c4m_compile_error);
    err->code              = code;
    err->current_token     = n->token;
    c4m_xlist_append(ctx->file_ctx->errors, err);

    if (bail) {
        c4m_exit_to_checkpoint(ctx, '!', f, line, fn);
    }
    else {
        c4m_exit_to_checkpoint(ctx, '.', f, line, fn);
    }
}

#define raise_err_at_node(ctx, n, code, bail) \
    _raise_err_at_node(ctx, n, code, bail, __FILE__, __LINE__, __func__)

#define EOF_ERROR(ctx) \
    raise_err_at_node(ctx, current_parse_node(ctx), c4m_err_parse_eof, true)

static c4m_utf8_t *repr_one_node(c4m_pnode_t *);

static void
parse_node_init(c4m_pnode_t *self, va_list args)
{
    parse_ctx *ctx = va_arg(args, parse_ctx *);
    self->kind     = va_arg(args, c4m_node_kind_t);
    self->token    = tok_cur(ctx);

    int aux_size = node_type_info[self->kind].aux_alloc_size;

    if (aux_size) {
        self->extra_info = c4m_gc_raw_alloc(aux_size, GC_SCAN_ALL);
    }
}

static inline c4m_pnode_t *
current_parse_node(parse_ctx *ctx)
{
    return (c4m_pnode_t *)c4m_tree_get_contents(ctx->cur);
}

static inline bool
cur_tok_is_end_of_stmt(parse_ctx *ctx)
{
    switch (tok_kind(ctx)) {
    case c4m_tt_semi:
    case c4m_tt_newline:
    case c4m_tt_rbrace:
    case c4m_tt_eof:
        return true;
    default:
        return false;
    }
}

static inline bool
cur_eos_is_skippable(parse_ctx *ctx)
{
    switch (tok_kind(ctx)) {
    case c4m_tt_semi:
    case c4m_tt_newline:
        return true;
    default:
        return false;
    }
}

static inline bool
line_skip_recover(parse_ctx *ctx)
{
    while (!cur_tok_is_end_of_stmt(ctx)) {
        consume(ctx);
    }

    while (cur_tok_is_end_of_stmt(ctx) && cur_eos_is_skippable(ctx)) {
        consume(ctx);
    }

    return true;
}

static void
err_skip_stmt(parse_ctx *ctx, c4m_compile_error_t code)
{
    add_parse_error(ctx, code);
    line_skip_recover(ctx);
}

static inline void
end_of_statement(parse_ctx *ctx)
{
    if (!cur_tok_is_end_of_stmt(ctx)) {
        err_skip_stmt(ctx, c4m_err_parse_expected_stmt_end);
        return;
    }

    while (cur_tok_is_end_of_stmt(ctx) && cur_eos_is_skippable(ctx)) {
        consume(ctx);
    }
}

static void
tok_raw_advance_once(parse_ctx *ctx)
{
    if (ctx->cached_token == NULL) {
        assert(ctx->token_ix == 0);
        tok_cur(ctx);
        return;
    }

    if (ctx->cached_token->kind == c4m_tt_eof) {
        return;
    }

    ctx->token_ix += 1;
}

#ifdef PARSE_DEBUG
static inline void
_consume(parse_ctx *ctx, int line)
#else
static inline void
_consume(parse_ctx *ctx)
#endif
{
    c4m_pnode_t        *pn;
    c4m_comment_node_t *comment;

#ifdef PARSE_DEBUG
    printf("Consume on line %d\n", line);
#endif

    while (true) {
        tok_raw_advance_once(ctx);

        c4m_token_t *t = tok_cur(ctx);
        switch (t->kind) {
        case c4m_tt_space:
            continue; // We ignore all white space tokens.
        case c4m_tt_newline:
            if (ctx->lit_depth != 0) {
                continue;
            }
            else {
                return;
            }
        case c4m_tt_line_comment:
        case c4m_tt_long_comment:
            pn      = (c4m_pnode_t *)c4m_tree_get_contents(ctx->cur);
            comment = c4m_gc_alloc(c4m_comment_node_t);

            comment->comment_tok = t;
            comment->sibling_id  = pn->total_kids++;

            if (pn->comments == NULL) {
                pn->comments = c4m_new(c4m_tspec_xlist(c4m_tspec_ref()));
            }

            c4m_xlist_append(pn->comments, comment);
            continue;
        default:
            return;
        }
    }
}

static c4m_token_kind_t
lookahead(parse_ctx *ctx, int num, bool skip_newline)
{
    c4m_token_t *saved_cache    = ctx->cached_token;
    int64_t      saved_tok_ix   = ctx->token_ix;
    int64_t      saved_cache_ix = ctx->cache_ix;

    while (num > 0) {
        tok_raw_advance_once(ctx);
        switch (tok_kind(ctx)) {
        case c4m_tt_space:
        case c4m_tt_line_comment:
        case c4m_tt_long_comment:
            continue;
        case c4m_tt_newline:
            if (!skip_newline) {
                num -= 1;
            }
            else {
                skip_newline = false;
            }
            continue;
        default:
            num -= 1;
            continue;
        }
    }

    c4m_token_kind_t result = tok_kind(ctx);

    ctx->cached_token = saved_cache;
    ctx->token_ix     = saved_tok_ix;
    ctx->cache_ix     = saved_cache_ix;

    return result;
}

static c4m_token_kind_t
_match(parse_ctx *ctx, ...)
{
    va_list          args;
    c4m_token_kind_t possibility;
    c4m_token_kind_t current_tt = tok_cur(ctx)->kind;

    va_start(args, ctx);

    while (true) {
        possibility = va_arg(args, c4m_token_kind_t);

        if (possibility == c4m_tt_error) {
#ifdef PARSE_DEBUG
            printf("No match for tt = %d\n", current_tt);
#endif
            current_tt = c4m_tt_error;
            break;
        }

        if (possibility == current_tt) {
            break;
        }
    }

    va_end(args);
    return current_tt;
}

#define match(ctx, ...) _match(ctx, KFUNC(__VA_ARGS__))

static inline bool
expect(parse_ctx *ctx, c4m_token_kind_t tk)
{
    if (match(ctx, tk) == tk) {
        consume(ctx);
        return true;
    }

    add_parse_error(ctx,
                    c4m_err_parse_expected_token,
                    token_type_to_string(tk));
    line_skip_recover(ctx);
    return false;
}

static inline bool
identifier_is_builtin_type(parse_ctx *ctx)
{
    char *txt = identifier_text(tok_cur(ctx))->data;

    for (int i = 0; i < C4M_NUM_BUILTIN_DTS; i++) {
        c4m_dt_info_t *tinfo = (c4m_dt_info_t *)&c4m_base_type_info[i];

        switch (tinfo->dt_kind) {
        case C4M_DT_KIND_primitive:
        case C4M_DT_KIND_list:
        case C4M_DT_KIND_tuple:
        case C4M_DT_KIND_dict:
            if (!strcmp(tinfo->name, txt)) {
                return true;
            }
            continue;
        default:
            continue;
        }
    }

    return false;
}

static bool
text_matches(parse_ctx *ctx, char *cstring)
{
    // Will only be called for identifiers.

    c4m_utf8_t *text = identifier_text(tok_cur(ctx));

    return !strcmp(text->data, cstring);
}

static inline void
adopt_kid(parse_ctx *ctx, c4m_tree_node_t *node)
{
    if (node == NULL) {
        add_parse_error(ctx, c4m_err_parse_missing_expression);
        THROW('!');
    }

    c4m_pnode_t *parent = current_parse_node(ctx);
    c4m_pnode_t *kid    = (c4m_pnode_t *)c4m_tree_get_contents(node);
    kid->sibling_id     = parent->total_kids++;

    c4m_tree_adopt_node(ctx->cur, node);
}

#ifdef PARSE_DEBUG
static inline c4m_tree_node_t *
_start_node(parse_ctx *ctx, c4m_node_kind_t kind, bool consume_token, int line)
#else
static inline c4m_tree_node_t *
_start_node(parse_ctx *ctx, c4m_node_kind_t kind, bool consume_token)
#endif
{
    c4m_pnode_t     *child  = c4m_new(c4m_tspec_parse_node(), ctx, kind);
    c4m_tree_node_t *parent = ctx->cur;

    if (consume_token) {
#ifdef PARSE_DEBUG
        printf("In start_node on line: %d\n", line);
#endif
        consume(ctx);
    }

    c4m_tree_node_t *result = c4m_tree_add_node(parent, child);
    ctx->cur                = result;

#ifdef PARSE_DEBUG
    printf("started node: ");
    c4m_print(repr_one_node(child));
#endif

    return result;
}

static inline void
end_node(parse_ctx *ctx)
{
#ifdef PARSE_DEBUG
    printf("Leaving node: ");
    c4m_print(repr_one_node(ctx->cur->contents));
#endif
    ctx->cur = ctx->cur->parent;
}

static inline c4m_tree_node_t *
temporary_tree(parse_ctx *ctx, c4m_node_kind_t nt)
{
    hatstack_push(ctx->root_stack, ctx->cur);
    c4m_pnode_t     *tmproot = c4m_new(c4m_tspec_parse_node(), ctx, nt);
    c4m_tree_node_t *result  = c4m_new(c4m_tspec_tree(c4m_tspec_parse_node()),
                                      c4m_kw("contents", c4m_ka(tmproot)));

    ctx->cur = result;

    return result;
}

static inline c4m_tree_node_t *
restore_tree(parse_ctx *ctx)
{
    c4m_tree_node_t *result = ctx->cur;

    ctx->cur = (c4m_tree_node_t *)hatstack_pop(ctx->root_stack, NULL);

    return result;
}

static void
opt_newlines(parse_ctx *ctx)
{
    while (match(ctx, c4m_tt_newline, c4m_tt_semi) != c4m_tt_error) {
        consume(ctx);
    }
}

static inline void
opt_one_newline(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_newline) {
        consume(ctx);
    }
}

static void
opt_doc_strings(parse_ctx *ctx, c4m_pnode_t *pn)
{
    opt_newlines(ctx);

    if (!match(ctx, c4m_tt_string_lit)) {
        return;
    }

    pn->short_doc = tok_cur(ctx);
    consume(ctx);
    opt_one_newline(ctx);
    if (match(ctx, c4m_tt_string_lit)) {
        pn->long_doc = tok_cur(ctx);
        consume(ctx);
    }
    opt_newlines(ctx);
}

static void
opt_return_type(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_arrow) {
        start_node(ctx, c4m_nt_lit_tspec_return_type, true);
        one_tspec_node(ctx);
        end_node(ctx);
    }
}

static void
one_tspec_node(parse_ctx *ctx)
{
    switch (match(ctx, c4m_tt_backtick, c4m_tt_lparen, c4m_tt_identifier)) {
    case c4m_tt_backtick:
        start_node(ctx, c4m_nt_lit_tspec_tvar, true);
        identifier(ctx);
        end_node(ctx);
        break;
    case c4m_tt_identifier:
        if (lookahead(ctx, 1, false) != c4m_tt_lbracket) {
            // This will generally be a builtin type; user-defined
            // types w/o parameters are actually object[udt]
            start_node(ctx, c4m_nt_lit_tspec_named_type, true);
            end_node(ctx);
        }
        else {
            start_node(ctx, c4m_nt_lit_tspec_parameterized_type, true);
            consume(ctx);
            while (true) {
                one_tspec_node(ctx);
                if (tok_kind(ctx) != c4m_tt_comma) {
                    break;
                }
                consume(ctx);
            }
            if (tok_kind(ctx) == c4m_tt_rbracket) {
                // TODO: error if there's a litmode here.
                consume(ctx);
            }
            else {
                add_parse_error(ctx, c4m_err_parse_missing_type_rbrak);
            }
            end_node(ctx);
        }
        return;
    case c4m_tt_lparen:
        start_node(ctx, c4m_nt_lit_tspec_func, true);

        if (tok_kind(ctx) == c4m_tt_rparen) {
            goto finish_fnspec;
        }

        if (tok_kind(ctx) != c4m_tt_mul) {
            one_tspec_node(ctx);
        }
        else {
            start_node(ctx, c4m_nt_lit_tspec_varargs, true);
            one_tspec_node(ctx);
            end_node(ctx);
            if (tok_kind(ctx) != c4m_tt_rparen) {
                err_skip_stmt(ctx, c4m_err_parse_vararg_wasnt_last_thing);
                end_node(ctx);
                return;
            }
            goto finish_fnspec;
        }

        while (true) {
            switch (match(ctx, c4m_tt_comma, c4m_tt_rparen)) {
            case c4m_tt_comma:
                consume(ctx);

                if (tok_kind(ctx) == c4m_tt_mul) {
                    start_node(ctx, c4m_nt_lit_tspec_varargs, true);
                    one_tspec_node(ctx);
                    end_node(ctx);
                    if (tok_kind(ctx) != c4m_tt_rparen) {
                        err_skip_stmt(ctx,
                                      c4m_err_parse_vararg_wasnt_last_thing);
                        end_node(ctx);
                        return;
                    }
                    goto finish_fnspec;
                }

                one_tspec_node(ctx);
                if (tok_kind(ctx) == c4m_tt_rparen) {
                    goto finish_fnspec;
                }
                continue;
            case c4m_tt_rparen:
                break;
            default:
                err_skip_stmt(ctx, c4m_err_parse_fn_param_syntax);
                end_node(ctx);
                return;
            }
            break;
        }

finish_fnspec:
        consume(ctx);
        opt_return_type(ctx);
        end_node(ctx);
        return;
    default:
        err_skip_stmt(ctx, c4m_err_parse_bad_tspec);
    }
}

// This does not validate the type here. It will accept:
// int[float, float, bar] even though that's clearly not OK since
// int is not parameterized.
static void
type_spec(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_lit_tspec, false);
    one_tspec_node(ctx);
    end_node(ctx);
}

static void
simple_lit(parse_ctx *ctx)
{
    if (match(ctx,
              c4m_tt_int_lit,
              c4m_tt_hex_lit,
              c4m_tt_float_lit,
              c4m_tt_string_lit,
              c4m_tt_float_lit,
              c4m_tt_char_lit,
              c4m_tt_unquoted_lit,
              c4m_tt_true,
              c4m_tt_false,
              c4m_tt_nil)
        == c4m_tt_error) {
        add_parse_error(ctx, c4m_err_parse_need_simple_lit);
    }
    else {
        c4m_token_t *tok = tok_cur(ctx);
        start_node(ctx, c4m_nt_simple_lit, true);
        if (tok->kind == c4m_tt_unquoted_lit) {
            C4M_CRAISE("Unquoted literals not reimplemented yet");
        }
        c4m_compile_error_t err = c4m_parse_simple_lit(tok);
        c4m_utf8_t         *mod;
        c4m_utf8_t         *syntax_kind;

        switch (err) {
        case c4m_err_parse_no_lit_mod_match:
            mod = c4m_to_utf8(tok->literal_modifier);
            switch (tok->kind) {
            case c4m_tt_int_lit:
                syntax_kind = c4m_new_utf8("integer");
                break;
            case c4m_tt_hex_lit:
                syntax_kind = c4m_new_utf8("hex");
                break;
            case c4m_tt_float_lit:
                syntax_kind = c4m_new_utf8("float");
                break;
            case c4m_tt_true:
            case c4m_tt_false:
                syntax_kind = c4m_new_utf8("boolean");
                break;

            case c4m_tt_string_lit:
                syntax_kind = c4m_new_utf8("string");
                break;

            case c4m_tt_char_lit:
                syntax_kind = c4m_new_utf8("character");
                break;

            default:
                C4M_CRAISE("Reached supposedly unreachable code.");
            }

            c4m_error_from_token(ctx->file_ctx, err, tok, mod, syntax_kind);
            break;
        case c4m_err_no_error:
            break;
        default:
            c4m_error_from_token(ctx->file_ctx, err, tok);
        }
        if (err != c4m_err_no_error) {
        }

        end_node(ctx);
    }
}

static void
string_lit(parse_ctx *ctx)
{
    if (match(ctx, c4m_tt_string_lit) != c4m_tt_string_lit) {
        add_parse_error(ctx, c4m_err_parse_need_str_lit);
    }
    else {
        c4m_token_t *tok = tok_cur(ctx);

        start_node(ctx, c4m_nt_simple_lit, true);

        c4m_compile_error_t err = c4m_parse_simple_lit(tok);

        if (err != c4m_err_no_error) {
            c4m_error_from_token(ctx->file_ctx, err, tok);
        }

        end_node(ctx);
    }
}

static void
bool_lit(parse_ctx *ctx)
{
    if (match(ctx, c4m_tt_true, c4m_tt_false) == c4m_tt_error) {
        add_parse_error(ctx, c4m_err_parse_need_bool_lit);
    }
    else {
        c4m_token_t *tok = tok_cur(ctx);

        start_node(ctx, c4m_nt_simple_lit, true);

        c4m_compile_error_t err = c4m_parse_simple_lit(tok);

        if (err != c4m_err_no_error) {
            c4m_error_from_token(ctx->file_ctx, err, tok);
        }

        end_node(ctx);
    }
}

static void
param_items(parse_ctx *ctx)
{
    int got_callback  = false;
    int got_default   = false;
    int got_validator = false;

    while (true) {
        switch (tok_kind(ctx)) {
        case c4m_tt_rbrace:
            return;
        case c4m_tt_newline:
            consume(ctx);
            continue;
        case c4m_tt_eof:
            THROW('!');
        case c4m_tt_identifier:
            break;
        default:
            add_parse_error(ctx,
                            c4m_err_parse_expected_token,
                            c4m_new_utf8("identifier"));
            line_skip_recover(ctx);
            continue;
        }

        char *txt = identifier_text(tok_cur(ctx))->data;
        if (!strcmp(txt, "callback")) {
            if (got_default) {
                add_parse_error(ctx, c4m_err_parse_param_def_and_callback);
                line_skip_recover(ctx);
                continue;
            }
            if (got_callback) {
                add_parse_error(ctx, c4m_err_parse_param_dupe_prop);
                line_skip_recover(ctx);
                continue;
            }
            got_callback = true;
        }
        else {
            if (!strcmp(txt, "default")) {
                if (got_callback) {
                    add_parse_error(ctx, c4m_err_parse_param_def_and_callback);
                    line_skip_recover(ctx);
                    continue;
                }
                if (got_default) {
                    add_parse_error(ctx, c4m_err_parse_param_dupe_prop);
                    line_skip_recover(ctx);
                    continue;
                }
                got_default = true;
            }
            else {
                if (strcmp(txt, "validator")) {
                    add_parse_error(ctx, c4m_err_parse_param_invalid_prop);
                    line_skip_recover(ctx);
                    continue;
                }
                if (got_validator) {
                    add_parse_error(ctx, c4m_err_parse_param_dupe_prop);
                    line_skip_recover(ctx);
                    continue;
                }
                got_validator = true;
            }
        }

        start_node(ctx, c4m_nt_param_prop, true);
        if (!expect(ctx, c4m_tt_colon)) {
            end_node(ctx);
            continue;
        }
        literal(ctx);
        end_node(ctx);
        end_of_statement(ctx);
    }
}

static void
parameter_block(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_param_block, true);

    switch (match(ctx, c4m_tt_identifier, c4m_tt_var)) {
    case c4m_tt_identifier:
        basic_member_expr(ctx);
        break;
    case c4m_tt_var:
        if (variable_decl(ctx) != c4m_nt_var_decls) {
            add_parse_error(ctx, c4m_err_parse_mod_param_no_const);
        }
        break;
    default:
        add_parse_error(ctx, c4m_err_parse_bad_param_start);
        THROW('!');
    }

    opt_one_newline(ctx);

    if (tok_kind(ctx) != c4m_tt_lbrace) {
        end_node(ctx);
        return;
    }

    consume(ctx);
    opt_doc_strings(ctx, current_parse_node(ctx));
    param_items(ctx);

    expect(ctx, c4m_tt_rbrace);
    end_node(ctx);
}

static void
extern_local(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_extern_local, true);
    if (!expect(ctx, c4m_tt_colon)) {
        end_node(ctx);
        return;
    }
    identifier(ctx);
    if (tok_kind(ctx) != c4m_tt_lparen) {
        end_node(ctx);
        err_skip_stmt(ctx, c4m_err_parse_extern_sig_needed);
        return;
    }
    formal_list(ctx);
    opt_return_type(ctx);
    end_of_statement(ctx);
    end_node(ctx);
}

static void
extern_dll(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_extern_local, true);
    if (!expect(ctx, c4m_tt_colon)) {
        end_node(ctx);
        return;
    }
    string_lit(ctx);
    end_of_statement(ctx);
    end_node(ctx);
}

static void
extern_pure(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_extern_pure, true);
    if (!expect(ctx, c4m_tt_colon)) {
        end_node(ctx);
        return;
    }
    bool_lit(ctx);
    end_of_statement(ctx);
    end_node(ctx);
}

static void
extern_holds(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_extern_holds, true);
    if (!expect(ctx, c4m_tt_colon)) {
        end_node(ctx);
        return;
    }
    while (true) {
        if (tok_kind(ctx) != c4m_tt_identifier) {
            add_parse_error(ctx, c4m_err_parse_extern_bad_hold_param);
        }
        else {
            identifier(ctx);
        }
        if (tok_kind(ctx) != c4m_tt_comma) {
            break;
        }
        consume(ctx);
    }

    end_node(ctx);
    end_of_statement(ctx);
    return;
}

static void
extern_allocs(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_extern_allocs, true);
    if (!expect(ctx, c4m_tt_colon)) {
        end_node(ctx);
        return;
    }
    while (true) {
        switch (tok_kind(ctx)) {
        case c4m_tt_identifier:
            identifier(ctx);
            break;
        case c4m_tt_return:
            start_node(ctx, c4m_nt_return, true);
            end_node(ctx);
            break;
        default:
            add_parse_error(ctx, c4m_err_parse_extern_bad_alloc_param);
            break;
        }
        if (tok_kind(ctx) != c4m_tt_comma) {
            break;
        }
        consume(ctx);
    }

    end_node(ctx);
    end_of_statement(ctx);
    return;
}

static void
extern_signature(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_extern_sig, false);
    if (!expect(ctx, c4m_tt_lparen)) {
        THROW('!');
    }

    if (tok_kind(ctx) == c4m_tt_rparen) {
        consume(ctx);
        end_node(ctx);
        opt_return_type(ctx);
        return;
    }
    while (true) {
        if (match(ctx, c4m_tt_identifier) != c4m_tt_identifier) {
            add_parse_error(ctx, c4m_err_parse_csig_id);
            consume(ctx);
        }
        else {
            char   *txt      = identifier_text(tok_cur(ctx))->data;
            int64_t ctype_id = (int64_t)lookup_ctype_id(txt);

            if (ctype_id == -1) {
                add_parse_error(ctx, c4m_err_parse_bad_ctype_id);
                consume(ctx);
            }
            else {
                start_node(ctx, c4m_nt_extern_param, true);
                c4m_pnode_t *n = current_parse_node(ctx);

                n->extra_info = (void *)(uint64_t)ctype_id;
                end_node(ctx);
            }
        }

        if (tok_kind(ctx) != c4m_tt_comma) {
            break;
        }
        consume(ctx);
    }

    expect(ctx, c4m_tt_rparen);
    opt_return_type(ctx);
    end_node(ctx);
}

static void
extern_block(parse_ctx *ctx)
{
    char *txt;

    start_node(ctx, c4m_nt_extern_block, true);
    identifier(ctx);
    extern_signature(ctx);
    opt_one_newline(ctx);
    expect(ctx, c4m_tt_lbrace);
    opt_doc_strings(ctx, current_parse_node(ctx));

    DECLARE_CHECKPOINT();

    while (true) {
        ENTER_CHECKPOINT();
        switch (CHECKPOINT_STATUS()) {
        case 0:
            switch (match(ctx, c4m_tt_rbrace, c4m_tt_identifier)) {
            case c4m_tt_rbrace:
                consume(ctx);
                end_node(ctx);
                END_CHECKPOINT();
                return;
            case c4m_tt_identifier:
                txt = identifier_text(tok_cur(ctx))->data;
                if (!strcmp(txt, "local")) {
                    extern_local(ctx);
                    continue;
                }
                if (!strcmp(txt, "dll")) {
                    extern_dll(ctx);
                    continue;
                }
                if (!strcmp(txt, "pure")) {
                    extern_pure(ctx);
                    continue;
                }
                if (!strcmp(txt, "holds")) {
                    extern_holds(ctx);
                    continue;
                }
                if (!strcmp(txt, "allocs")) {
                    extern_allocs(ctx);
                    continue;
                }
                // Fallthrough
            default:
                err_skip_stmt(ctx, c4m_err_parse_extern_bad_prop);
                continue;
            }
        case '!':
            THROW('!');
        default:
            continue;
        }
    }
}

static void
enum_stmt(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_global) {
        start_node(ctx, c4m_nt_global_enum, true);
        consume(ctx); // enum being next validated by caller.
    }
    else {
        if (lookahead(ctx, 1, false) == c4m_tt_global) {
            start_node(ctx, c4m_nt_global_enum, true);
            consume(ctx);
        }
        else {
            start_node(ctx, c4m_nt_enum, true);
        }
    }

    if (tok_kind(ctx) == c4m_tt_identifier) {
        identifier(ctx);
    }

    opt_one_newline(ctx);
    if (!expect(ctx, c4m_tt_lbrace)) {
        end_node(ctx);
        return;
    }
    opt_doc_strings(ctx, current_parse_node(ctx));

    if (tok_kind(ctx) == c4m_tt_rbrace) {
        add_parse_error(ctx, c4m_err_parse_empty_enum);
        consume(ctx);
        return;
    }

    while (true) {
        if (tok_kind(ctx) != c4m_tt_identifier) {
            add_parse_error(ctx, c4m_err_parse_enum_item);
            THROW('!');
        }
        start_node(ctx, c4m_nt_enum_item, true);

        opt_one_newline(ctx);

        switch (tok_kind(ctx)) {
        case c4m_tt_comma:
            end_node(ctx);
            consume(ctx);
            opt_one_newline(ctx);
            if (tok_kind(ctx) == c4m_tt_rbrace) {
                end_node(ctx);
                consume(ctx);
                return;
            }
            continue;
        case c4m_tt_rbrace:
            end_node(ctx);
            end_node(ctx);
            consume(ctx);
            return;
        case c4m_tt_colon:
        case c4m_tt_assign:
            consume(ctx);
            switch (match(ctx,
                          c4m_tt_int_lit,
                          c4m_tt_hex_lit,
                          c4m_tt_string_lit)) {
            case c4m_tt_error:
                add_parse_error(ctx, c4m_err_parse_enum_value_type);
                consume(ctx);
                end_node(ctx);
                continue;
            default:
                simple_lit(ctx);
                end_node(ctx);
                opt_one_newline(ctx);
                if (tok_kind(ctx) == c4m_tt_rbrace) {
                    end_node(ctx);
                    consume(ctx);
                    return;
                }
                expect(ctx, c4m_tt_comma);
                if (tok_kind(ctx) == c4m_tt_rbrace) {
                    end_node(ctx);
                    consume(ctx);
                    return;
                }
                continue;
            }
        default:
            err_skip_stmt(ctx, c4m_err_parse_enum_item);
            end_node(ctx);
            end_node(ctx);
            return;
        }
    }
}

static void
lock_attr(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_attr_set_lock, true);
    c4m_tree_node_t *expr = expression(ctx);

    switch (tok_kind(ctx)) {
    case c4m_tt_plus_eq:
    case c4m_tt_minus_eq:
    case c4m_tt_mul_eq:
    case c4m_tt_div_eq:
    case c4m_tt_mod_eq:
    case c4m_tt_bit_and_eq:
    case c4m_tt_bit_or_eq:
    case c4m_tt_bit_xor_eq:
    case c4m_tt_shl_eq:
    case c4m_tt_shr_eq:
        adopt_kid(ctx, assign(ctx, expr, c4m_nt_binary_assign_op));
        break;
    case c4m_tt_colon:
    case c4m_tt_assign:
        adopt_kid(ctx, assign(ctx, expr, c4m_nt_assign));
        break;
    default:
        break;
    }

    end_node(ctx);
    end_of_statement(ctx);
}

static void
elif_stmt(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_elif, true);
    adopt_kid(ctx, expression(ctx));
    body(ctx, NULL);
    end_node(ctx);
}

static void
else_stmt(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_else, true);
    body(ctx, NULL);
    end_node(ctx);
}

static void
if_stmt(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_if, true);
    adopt_kid(ctx, expression(ctx));
    body(ctx, NULL);

    while (true) {
        opt_one_newline(ctx);
        switch (tok_kind(ctx)) {
        case c4m_tt_elif:
            elif_stmt(ctx);
            continue;
        case c4m_tt_else:
            else_stmt(ctx);
            break;
        default:
            break;
        }
        break;
    }

    end_node(ctx);
}

static void
for_var_list(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_var_decls, false);

    while (true) {
        identifier(ctx);
        if (tok_kind(ctx) != c4m_tt_comma) {
            end_node(ctx);
            return;
        }
        consume(ctx);
    }
}

static bool
optional_range(parse_ctx *ctx, c4m_tree_node_t *lhs)
{
    if (match(ctx, c4m_tt_to, c4m_tt_comma, c4m_tt_colon) == c4m_tt_error) {
        return false;
    }

    start_node(ctx, c4m_nt_range, true);
    adopt_kid(ctx, lhs);

    adopt_kid(ctx, expression(ctx));
    end_node(ctx);
    return true;
}

static void
range(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_range, false);
    adopt_kid(ctx, expression(ctx));

    if (match(ctx, c4m_tt_to, c4m_tt_comma, c4m_tt_colon) == c4m_tt_error) {
        add_parse_error(ctx, c4m_err_parse_expected_range_tok);
        line_skip_recover(ctx);
        end_node(ctx);
        return;
    }
    consume(ctx);
    adopt_kid(ctx, expression(ctx));
    end_of_statement(ctx);
    end_node(ctx);
}

static void
property_range(parse_ctx *ctx)
{
    consume(ctx);
    if (!expect(ctx, c4m_tt_colon)) {
        return;
    }

    start_node(ctx, c4m_nt_range, false);
    adopt_kid(ctx, expression(ctx));

    if (match(ctx, c4m_tt_to, c4m_tt_comma, c4m_tt_colon) == c4m_tt_error) {
        add_parse_error(ctx, c4m_err_parse_expected_range_tok);
        line_skip_recover(ctx);
        end_node(ctx);
        return;
    }
    consume(ctx);
    adopt_kid(ctx, expression(ctx));
    end_of_statement(ctx);
    end_node(ctx);
}

static void
for_stmt(parse_ctx *ctx)
{
    c4m_tree_node_t *expr;

    start_node(ctx, c4m_nt_for, true);
    for_var_list(ctx);

    switch (match(ctx, c4m_tt_in, c4m_tt_from)) {
    case c4m_tt_in:
        consume(ctx);
        expr = expression(ctx);

        if (!optional_range(ctx, expr)) {
            adopt_kid(ctx, expr);
        }
        break;
    case c4m_tt_from:
        consume(ctx);
        range(ctx);
        break;
    default:
        add_parse_error(ctx, c4m_err_parse_for_syntax);
        // We should skip into a block only if we see a block start,
        // and still add the body, but that is a TODO.
        THROW('!');
    }

    ctx->loop_depth += 1;
    body(ctx, NULL);
    ctx->loop_depth -= 1;
}

static void
while_stmt(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_while, true);
    adopt_kid(ctx, expression(ctx));

    ctx->loop_depth += 1;
    body(ctx, NULL);
    ctx->loop_depth -= 1;
}

static void
case_body(parse_ctx *ctx)
{
    c4m_tree_node_t *expr;

    start_node(ctx, c4m_nt_body, true);

    DECLARE_CHECKPOINT();
    while (true) {
        ENTER_CHECKPOINT();

        switch (CHECKPOINT_STATUS()) {
        case 0:
            switch (tok_kind(ctx)) {
            case c4m_tt_eof:
                EOF_ERROR(ctx);
            case c4m_tt_case:
            case c4m_tt_rbrace:
            case c4m_tt_else:
                end_node(ctx);
                END_CHECKPOINT();
                return;
            case c4m_tt_semi:
            case c4m_tt_newline:
                consume(ctx);
                continue;
            case c4m_tt_enum:
                err_skip_stmt(ctx, c4m_err_parse_enums_are_toplevel);
                continue;
            case c4m_tt_private:
            case c4m_tt_func:
                err_skip_stmt(ctx, c4m_err_parse_funcs_are_toplevel);
                continue;
            case c4m_tt_lock_attr:
                lock_attr(ctx);
                continue;
            case c4m_tt_if:
                if_stmt(ctx);
                continue;
            case c4m_tt_for:
                for_stmt(ctx);
                continue;
            case c4m_tt_while:
                while_stmt(ctx);
                continue;
            case c4m_tt_typeof:
                typeof_stmt(ctx);
                continue;
            case c4m_tt_switch:
                switch_stmt(ctx);
                continue;
            case c4m_tt_continue:
                continue_stmt(ctx);
                continue;
            case c4m_tt_break:
                break_stmt(ctx);
                continue;
            case c4m_tt_return:
                return_stmt(ctx);
                continue;
            case c4m_tt_global:
            case c4m_tt_var:
            case c4m_tt_const:
                variable_decl(ctx);
                end_of_statement(ctx);
                continue;
            case c4m_tt_identifier:
                if (lookahead(ctx, 1, false) == c4m_tt_colon) {
                    switch (lookahead(ctx, 2, true)) {
                    case c4m_tt_for:
                    case c4m_tt_while:
                        label_stmt(ctx);
                        continue;
                    default:
                        break; // fall through to text_matches tho.
                    }
                }
                if (text_matches(ctx, "assert")) {
                    assert_stmt(ctx);
                    continue;
                }
                if (text_matches(ctx, "use")) {
                    use_stmt(ctx);
                    continue;
                }
                if (text_matches(ctx, "parameter")) {
                    err_skip_stmt(ctx, c4m_err_parse_parameter_is_toplevel);
                    continue;
                }
                if (text_matches(ctx, "extern")) {
                    err_skip_stmt(ctx, c4m_err_parse_extern_is_toplevel);
                    extern_block(ctx);
                    continue;
                }
                if (text_matches(ctx, "confspec")) {
                    err_skip_stmt(ctx, c4m_err_parse_confspec_is_toplevel);
                    continue;
                }
                // fallthrough. We'll parse an identifier then parent
                // it based on what we see after.
                // fallthrough
            default:
                expr = expression(ctx);
                switch (tok_kind(ctx)) {
                case c4m_tt_plus_eq:
                case c4m_tt_minus_eq:
                case c4m_tt_mul_eq:
                case c4m_tt_div_eq:
                case c4m_tt_mod_eq:
                case c4m_tt_bit_or_eq:
                case c4m_tt_bit_xor_eq:
                case c4m_tt_bit_and_eq:
                case c4m_tt_shl_eq:
                case c4m_tt_shr_eq:
                    adopt_kid(ctx, assign(ctx, expr, c4m_nt_binary_assign_op));
                    continue;
                case c4m_tt_colon:
                case c4m_tt_assign:
                    adopt_kid(ctx, assign(ctx, expr, c4m_nt_assign));
                    continue;
                case c4m_tt_identifier:
                case c4m_tt_string_lit:
                case c4m_tt_lbrace:
                    adopt_kid(ctx, section(ctx, expr));
                    continue;
                default:
                    adopt_kid(ctx, expr);
                    end_of_statement(ctx);
                    continue;
                }
            }
        case '!':
            THROW('!');
        default:
            continue;
        }
    }
}

static void
case_else_block(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_else, true);

    if (tok_kind(ctx) == c4m_tt_colon) {
        case_body(ctx);
    }
    else {
        opt_one_newline(ctx);
        if (tok_kind(ctx) != c4m_tt_lbrace) {
            add_parse_error(ctx, c4m_err_parse_case_body_start);
            THROW('!');
        }
        body(ctx, NULL);
    }

    end_node(ctx);
}

static void
typeof_case_block(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_case, false);

    while (true) {
        type_spec(ctx);

        if (tok_kind(ctx) != c4m_tt_comma) {
            break;
        }
        consume(ctx);
    }

    if (tok_kind(ctx) == c4m_tt_colon) {
        case_body(ctx);
    }
    else {
        opt_one_newline(ctx);
        if (tok_kind(ctx) != c4m_tt_lbrace) {
            add_parse_error(ctx, c4m_err_parse_case_body_start);
            THROW('!');
        }
        body(ctx, NULL);
    }
    end_node(ctx);
}

static void
typeof_stmt(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_typeof, true);
    basic_member_expr(ctx);
    opt_one_newline(ctx);
    if (!expect(ctx, c4m_tt_lbrace)) {
        THROW('!');
    }
    opt_one_newline(ctx);
    if (!expect(ctx, c4m_tt_case)) {
        THROW('!');
    }

    while (true) {
        typeof_case_block(ctx);

        switch (match(ctx, c4m_tt_case, c4m_tt_rbrace, c4m_tt_else)) {
        case c4m_tt_case:
            consume(ctx);
            continue;
        case c4m_tt_rbrace:
            consume(ctx);
            opt_newlines(ctx);
            end_node(ctx);
            return;
        case c4m_tt_else:
            case_else_block(ctx);
            end_node(ctx);
            expect(ctx, c4m_tt_rbrace);
            return;
        default:
            add_parse_error(ctx, c4m_err_parse_case_else_or_end);
            THROW('!');
        }
    }
}

static void
switch_case_block(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_case, false);

    while (true) {
        c4m_tree_node_t *expr = expression(ctx);

        // clang-format off
        if (tok_kind(ctx) != c4m_tt_colon ||
	    lookahead(ctx, 1, false) != c4m_tt_newline) {
            optional_range(ctx, expr);
        }
        // clang-format on
        else {
            adopt_kid(ctx, expr);
        }

        if (tok_kind(ctx) != c4m_tt_comma) {
            break;
        }
        consume(ctx);
    }

    if (tok_kind(ctx) == c4m_tt_colon) {
        case_body(ctx);
    }
    else {
        opt_one_newline(ctx);
        if (tok_kind(ctx) != c4m_tt_lbrace) {
            add_parse_error(ctx, c4m_err_parse_case_body_start);
            THROW('!');
        }
        body(ctx, NULL);
    }
    end_node(ctx);
}

static void
switch_stmt(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_switch, true);
    adopt_kid(ctx, expression(ctx));
    opt_one_newline(ctx);
    if (!expect(ctx, c4m_tt_lbrace)) {
        THROW('!');
    }
    opt_one_newline(ctx);
    if (!expect(ctx, c4m_tt_case)) {
        THROW('!');
    }

    while (true) {
        switch_case_block(ctx);

        switch (match(ctx, c4m_tt_case, c4m_tt_rbrace, c4m_tt_else)) {
        case c4m_tt_case:
            consume(ctx);
            continue;
        case c4m_tt_rbrace:
            consume(ctx);
            opt_newlines(ctx);
            end_node(ctx);
            return;
        case c4m_tt_else:
            case_else_block(ctx);
            end_node(ctx);
            expect(ctx, c4m_tt_rbrace);
            return;
        default:
            add_parse_error(ctx, c4m_err_parse_case_else_or_end);
            THROW('!');
        }
    }
}

static void
symbol_info(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_var_decls, false);

    while (true) {
        if (tok_kind(ctx) == c4m_tt_mul) {
            break;
        }
        identifier(ctx);
        if (tok_kind(ctx) != c4m_tt_comma) {
            break;
        }
        consume(ctx);
    }

    if (tok_kind(ctx) == c4m_tt_colon) {
        consume(ctx);
        type_spec(ctx);
    }

    end_node(ctx);
}

static void
varargs_param(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_varargs_param, true);
    identifier(ctx);
    if (tok_kind(ctx) == c4m_tt_colon) {
        consume(ctx);
        type_spec(ctx);
    }
    end_node(ctx);
}

static void
formal_list(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_formals, false);
    if (!expect(ctx, c4m_tt_lparen)) {
        THROW('!');
    }
    if (tok_kind(ctx) == c4m_tt_mul) {
        varargs_param(ctx);
        goto finish_formals;
    }

    if (tok_kind(ctx) == c4m_tt_identifier) {
        while (true) {
            symbol_info(ctx);
            if (tok_kind(ctx) == c4m_tt_mul) {
                varargs_param(ctx);
                goto finish_formals;
            }

            if (tok_kind(ctx) != c4m_tt_comma) {
                break;
            }

            consume(ctx);

            if (tok_kind(ctx) == c4m_tt_mul) {
                varargs_param(ctx);
                goto finish_formals;
            }

            // We actually consume this at the top of the loop.
            if (tok_kind(ctx) != c4m_tt_identifier) {
                add_parse_error(ctx, c4m_err_parse_formal_expect_id);
                THROW('!');
            }
        }
    }

finish_formals:
    if (!expect(ctx, c4m_tt_rparen)) {
        THROW('!');
    }
    end_node(ctx);
}

static void
func_def(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_private) {
        start_node(ctx, c4m_nt_func_def, true);
        if (!expect(ctx, c4m_tt_func)) {
            THROW('!');
        }
    }
    else {
        start_node(ctx, c4m_nt_func_def, true);
    }

    ctx->in_function = true;
    identifier(ctx);
    formal_list(ctx);
    opt_return_type(ctx);
    body(ctx, current_parse_node(ctx));
    ctx->in_function = false;
    end_node(ctx);
}

// Returns the node kind so that parameter_block doesn't
// have to go find the node to be able to tell if 'const' was
// used where it shouldn't be.
//
// Does not declare end-of-statement itself, so that parameter_block
// works right.

static c4m_node_kind_t
variable_decl(parse_ctx *ctx)
{
    bool got_var    = false;
    bool got_global = false;
    bool got_const  = false;

    start_node(ctx, c4m_nt_error, false);

    while (true) {
        switch (tok_kind(ctx)) {
        case c4m_tt_var:
            if (got_var) {
                end_node(ctx);
                err_skip_stmt(ctx, c4m_err_parse_decl_kw_x2);
                return c4m_nt_error;
            }
            got_var = true;
            break;
        case c4m_tt_global:
            if (got_global) {
                end_node(ctx);
                err_skip_stmt(ctx, c4m_err_parse_decl_kw_x2);
                return c4m_nt_error;
            }
            got_global = true;
            break;
        case c4m_tt_const:
            if (got_const) {
                end_node(ctx);
                err_skip_stmt(ctx, c4m_err_parse_decl_kw_x2);
                return c4m_nt_error;
            }
            got_const = true;
            break;
        default:
            goto done_with_qualifiers;
        }
        consume(ctx);
    }

done_with_qualifiers:
    if (got_var && got_global) {
        end_node(ctx);
        err_skip_stmt(ctx, c4m_err_parse_decl_2_scopes);
        return c4m_nt_error;
    }

    if (!got_var && !got_global) {
        current_parse_node(ctx)->kind = c4m_nt_const_decls;
    }
    else {
        if (got_var) {
            if (got_const) {
                current_parse_node(ctx)->kind = c4m_nt_const_var_decls;
            }
            else {
                current_parse_node(ctx)->kind = c4m_nt_var_decls;
            }
        }
        else {
            if (got_const) {
                current_parse_node(ctx)->kind = c4m_nt_const_global_decls;
            }
            else {
                current_parse_node(ctx)->kind = c4m_nt_global_decls;
            }
        }
    }

    goto first_sym;
    while (tok_kind(ctx) == c4m_tt_comma) {
        consume(ctx);
first_sym:
        symbol_info(ctx);
    }

    c4m_node_kind_t result = current_parse_node(ctx)->kind;

    end_node(ctx);

    return result;
}

static void
label_stmt(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_label, true);
    consume(ctx); // Colon got validated via lookahead.
    opt_one_newline(ctx);
    end_node(ctx);
}

static void
assert_stmt(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_assert, true);
    adopt_kid(ctx, expression(ctx));
    end_of_statement(ctx);
    end_node(ctx);
}

static void
identifier(parse_ctx *ctx)
{
    if (tok_kind(ctx) != c4m_tt_identifier) {
        add_parse_error(ctx, c4m_err_parse_id_expected);
        THROW('.');
    }

    start_node(ctx, c4m_nt_identifier, true);
    end_node(ctx);
}

static void
break_stmt(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_break, true);
    if (!ctx->loop_depth) {
        add_parse_error(ctx, c4m_err_parse_break_outside_loop);
    }
    if (!cur_tok_is_end_of_stmt(ctx)) {
        identifier(ctx);
    }
    end_node(ctx);
    end_of_statement(ctx);
}

static void
continue_stmt(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_continue, true);
    if (!ctx->loop_depth) {
        add_parse_error(ctx, c4m_err_parse_continue_outside_loop);
    }

    if (!cur_tok_is_end_of_stmt(ctx)) {
        identifier(ctx);
    }

    end_node(ctx);
    end_of_statement(ctx);
}

static void
return_stmt(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_return, true);

    if (!ctx->in_function) {
        add_parse_error(ctx, c4m_err_parse_return_outside_func);
    }

    if (!cur_tok_is_end_of_stmt(ctx)) {
        adopt_kid(ctx, expression(ctx));
    }

    end_node(ctx);
    end_of_statement(ctx);
}

static void
basic_member_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) != c4m_tt_identifier) {
        add_parse_error(ctx, c4m_err_parse_id_expected);
        THROW('.');
    }

    start_node(ctx, c4m_nt_member, false);
    while (true) {
        if (tok_kind(ctx) != c4m_tt_identifier) {
            add_parse_error(ctx, c4m_err_parse_id_member_part);
            end_node(ctx);
            THROW('.');
        }
        identifier(ctx);
        if (tok_kind(ctx) != c4m_tt_period) {
            end_node(ctx);
            return;
        }
        consume(ctx);
    }
}

static void
use_stmt(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_use, true);
    basic_member_expr(ctx);

    if (tok_kind(ctx) == c4m_tt_from) {
        consume(ctx);

        if (tok_kind(ctx) == c4m_tt_string_lit) {
            simple_lit(ctx);
        }
        else {
            err_skip_stmt(ctx, c4m_err_parse_bad_use_uri);
            end_node(ctx);
            return;
        }
    }

    end_of_statement(ctx);
    end_node(ctx);
}

static void
callback_lit(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_lit_callback, true);
    identifier(ctx);
    if (tok_kind(ctx) == c4m_tt_lparen) {
        type_spec(ctx);
    }

    end_node(ctx);
}

static void
unquoted_lit(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_lit_unquoted, true);
    end_node(ctx);
}

static void
list_lit(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_lit_list, true);
    ctx->lit_depth++;

    if (tok_kind(ctx) != c4m_tt_rbracket) {
        while (true) {
            adopt_kid(ctx, expression(ctx));
            if (tok_kind(ctx) != c4m_tt_comma) {
                break;
            }
            consume(ctx);
        }
    }

    current_parse_node(ctx)->extra_info = tok_cur(ctx)->literal_modifier;
    end_node(ctx);
    ctx->lit_depth--;

    expect(ctx, c4m_tt_rbracket);
}

static void
dict_lit(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_lit_empty_dict_or_set, true);
    ctx->lit_depth++;

    if (tok_kind(ctx) == c4m_tt_rbracket) {
        consume(ctx);

finish_lit:
        ctx->lit_depth--;
        end_node(ctx);
        return;
    }

    adopt_kid(ctx, expression(ctx));
    if (tok_kind(ctx) == c4m_tt_colon) {
        current_parse_node(ctx)->kind = c4m_nt_lit_dict;

        while (true) {
            if (!expect(ctx, c4m_tt_colon)) {
                goto finish_lit;
            }
            adopt_kid(ctx, expression(ctx));
            if (tok_kind(ctx) != c4m_tt_comma) {
                break;
            }
            consume(ctx);
            adopt_kid(ctx, expression(ctx));
        }
    }
    else {
        current_parse_node(ctx)->kind = c4m_nt_lit_set;

        while (true) {
            if (tok_kind(ctx) != c4m_tt_comma) {
                break;
            }
            consume(ctx);
            adopt_kid(ctx, expression(ctx));
        }
    }

    current_parse_node(ctx)->extra_info = tok_cur(ctx)->literal_modifier;
    end_node(ctx);
    ctx->lit_depth--;
    expect(ctx, c4m_tt_rbrace);
}

static void
tuple_lit(parse_ctx *ctx)
{
    int num_items = 0;

    start_node(ctx, c4m_nt_lit_tuple, true);
    if (tok_kind(ctx) == c4m_tt_rparen) {
        err_skip_stmt(ctx, c4m_err_parse_no_empty_tuples);
        return;
    }

    ctx->lit_depth++;

    while (true) {
        adopt_kid(ctx, expression(ctx));
        num_items += 1;
        if (tok_kind(ctx) != c4m_tt_comma) {
            break;
        }
        consume(ctx);
    }

    ctx->lit_depth--;

    current_parse_node(ctx)->extra_info = tok_cur(ctx)->literal_modifier;

    if (expect(ctx, c4m_tt_rparen) && num_items < 2) {
        current_parse_node(ctx)->kind = c4m_nt_paren_expr;
    }

    end_node(ctx);
}

static bool
literal(parse_ctx *ctx)
{
    switch (tok_kind(ctx)) {
    case c4m_tt_backtick:
    case c4m_tt_object:
        type_spec(ctx);
        return true;
    case c4m_tt_func:
        if (lookahead(ctx, 1, false) == c4m_tt_identifier) {
            callback_lit(ctx);
        }
        else {
            type_spec(ctx);
        }
        return true;
    case c4m_tt_int_lit:
    case c4m_tt_hex_lit:
    case c4m_tt_float_lit:
    case c4m_tt_string_lit:
    case c4m_tt_char_lit:
    case c4m_tt_true:
    case c4m_tt_false:
    case c4m_tt_nil:
        simple_lit(ctx);
        return true;
    case c4m_tt_unquoted_lit:
        unquoted_lit(ctx);
        return true;
    case c4m_tt_lbracket:
        list_lit(ctx);
        return true;
    case c4m_tt_lbrace:
        dict_lit(ctx);
        return true;
    case c4m_tt_lparen:
        tuple_lit(ctx);
        return true;
    case c4m_tt_identifier:
        if (identifier_is_builtin_type(ctx)) {
            type_spec(ctx);
            return true;
        }
        return false;
    default:
        return false;
    }
}

static void
typical_field_property(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_section_prop, true);
    if (!expect(ctx, c4m_tt_colon)) {
        end_node(ctx);
        return;
    }

    literal(ctx);
    end_of_statement(ctx);
    end_node(ctx);
}

static void
exclusion_field_property(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_section_prop, true);

    if (!expect(ctx, c4m_tt_colon)) {
        end_node(ctx);
        return;
    }

    identifier(ctx);
    while (tok_kind(ctx) == c4m_tt_comma) {
        consume(ctx);
        identifier(ctx);
    }
    end_of_statement(ctx);
    end_node(ctx);
}

static void
type_field_property(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_section_prop, true);

    if (!expect(ctx, c4m_tt_colon)) {
        end_node(ctx);
        return;
    }

    if (tok_kind(ctx) == c4m_tt_arrow) {
        consume(ctx);
        identifier(ctx);
    }
    else {
        type_spec(ctx);
    }

    end_of_statement(ctx);
    end_node(ctx);
}

static void
invalid_field_part(parse_ctx *ctx)
{
    end_node(ctx);
    err_skip_stmt(ctx, c4m_err_parse_invalid_field_part);
    return;
}

static void
field_property(parse_ctx *ctx)
{
    char *txt = identifier_text(tok_cur(ctx))->data;

    switch (txt[0]) {
    case 'c':
        if (strcmp(txt, "choice") && strcmp(txt, "choices")) {
            invalid_field_part(ctx);
        }
        else {
            typical_field_property(ctx);
        }
        return;
    case 'd':
        if (strcmp(txt, "default")) {
            invalid_field_part(ctx);
        }
        else {
            typical_field_property(ctx);
        }
        return;
    case 'e':
        if (strcmp(txt, "exclude") && strcmp(txt, "exclusions")) {
            invalid_field_part(ctx);
        }
        else {
            exclusion_field_property(ctx);
        }
        return;
    case 'h':
        if (strcmp(txt, "hide") && strcmp(txt, "hidden")) {
            invalid_field_part(ctx);
        }
        else {
            typical_field_property(ctx);
        }
        return;
    case 'l':
        if (strcmp(txt, "lock") && strcmp(txt, "locked")) {
            invalid_field_part(ctx);
        }
        else {
            typical_field_property(ctx);
        }
        return;
    case 'r':
        if (!strcmp(txt, "require") || !strcmp(txt, "required")) {
            typical_field_property(ctx);
        }
        else {
            if (!strcmp(txt, "range")) {
                property_range(ctx);
            }
            else {
                invalid_field_part(ctx);
            }
        }
        return;
    case 't':
        if (strcmp(txt, "type")) {
            invalid_field_part(ctx);
        }
        else {
            type_field_property(ctx);
        }
        return;
    case 'v':
        if (strcmp(txt, "validator")) {
            invalid_field_part(ctx);
        }
        else {
            typical_field_property(ctx);
        }
        return;
    default:
        end_node(ctx);
        err_skip_stmt(ctx, c4m_err_parse_invalid_field_part);
        return;
    }
}

static void
field_spec(parse_ctx *ctx)
{
    start_node(ctx, c4m_nt_field_spec, true);
    identifier(ctx);
    opt_one_newline(ctx);
    if (!expect(ctx, c4m_tt_lbrace)) {
        end_node(ctx);
        return;
    }
    opt_doc_strings(ctx, current_parse_node(ctx));

    DECLARE_CHECKPOINT();
    while (true) {
        ENTER_CHECKPOINT();
        switch (CHECKPOINT_STATUS()) {
        case 0:
            switch (tok_kind(ctx)) {
            case c4m_tt_eof:
                EOF_ERROR(ctx);
            case c4m_tt_rbrace:
                consume(ctx);
                end_node(ctx);
                END_CHECKPOINT();
                return;
            case c4m_tt_semi:
            case c4m_tt_newline:
                opt_newlines(ctx);
                continue;
            case c4m_tt_identifier:
                field_property(ctx);
                continue;
            default:
                end_node(ctx);
                err_skip_stmt(ctx, c4m_err_parse_invalid_sec_part);
                END_CHECKPOINT();
                return;
            }
        case '!':
            THROW('!');
        default:
            continue;
        }
    }
}

static void
section_property(parse_ctx *ctx)
{
    char *txt = identifier_text(tok_cur(ctx))->data;

    switch (txt[0]) {
    case 'u':
        if (strcmp(txt, "user_def_ok")) {
            goto invalid_sec_part;
        }

typical_section_prop:
        start_node(ctx, c4m_nt_section_prop, true);

        if (!expect(ctx, c4m_tt_colon)) {
            end_node(ctx);
            return;
        }
        literal(ctx);
        break;

    case 'a':
        if (strcmp(txt, "allow") && strcmp(txt, "allowed")) {
            goto invalid_sec_part;
        }

id_list_section_prop:
        start_node(ctx, c4m_nt_section_prop, true);
        if (!expect(ctx, c4m_tt_colon)) {
            end_node(ctx);
            return;
        }

        identifier(ctx);
        while (tok_kind(ctx) == c4m_tt_comma) {
            consume(ctx);
            identifier(ctx);
        }

        break;

    case 'r':
        if (strcmp(txt, "require") && strcmp(txt, "required")) {
            goto invalid_sec_part;
        }
        goto id_list_section_prop;

    case 'h':

        if (strcmp(txt, "hide") && strcmp(txt, "hidden")) {
            goto invalid_sec_part;
        }

        goto typical_section_prop;

    case 'v':
        if (strcmp(txt, "validator")) {
            goto invalid_sec_part;
        }

        goto typical_section_prop;

    default:
invalid_sec_part:
        end_node(ctx);
        err_skip_stmt(ctx, c4m_err_parse_invalid_sec_part);
        return;
    }

    end_of_statement(ctx);
    end_node(ctx);
    return;
}

static void
object_spec(parse_ctx *ctx, c4m_utf8_t *txt)
{
    start_node(ctx, c4m_nt_section_spec, false);
    // if this isn't the root section, we read a name.
    if (strcmp(txt->data, "root")) {
        identifier(ctx);
    }

    identifier(ctx);

    opt_one_newline(ctx);

    if (!expect(ctx, c4m_tt_lbrace)) {
        return;
    }

    opt_doc_strings(ctx, current_parse_node(ctx));

    DECLARE_CHECKPOINT();
    while (true) {
        ENTER_CHECKPOINT();
        switch (CHECKPOINT_STATUS()) {
        case 0:
            switch (tok_kind(ctx)) {
            case c4m_tt_rbrace:
                consume(ctx);
                opt_newlines(ctx);
                end_node(ctx);
                END_CHECKPOINT();
                return;
            case c4m_tt_semi:
            case c4m_tt_newline:
                consume(ctx);
                continue;
            case c4m_tt_identifier:
                if (!strcmp(identifier_text(tok_cur(ctx))->data, "field")) {
                    field_spec(ctx);
                    continue;
                }
                else {
                    section_property(ctx);
                    continue;
                }
            case c4m_tt_eof:
                EOF_ERROR(ctx);
            default:
                err_skip_stmt(ctx, c4m_err_parse_invalid_token_in_sec);
                continue;
            }
        case '!':
            THROW('!');
        default:
            THROW('.');
        }
    }
}

static void
confspec_block(parse_ctx *ctx)
{
    c4m_utf8_t *txt;

    start_node(ctx, c4m_nt_config_spec, true);
    opt_one_newline(ctx);

    if (!expect(ctx, c4m_tt_lbrace)) {
        return;
    }

    opt_doc_strings(ctx, current_parse_node(ctx));

    while (true) {
        switch (tok_kind(ctx)) {
        case c4m_tt_eof:
            EOF_ERROR(ctx);
        case c4m_tt_rbrace:
            consume(ctx);
            opt_newlines(ctx);
            end_node(ctx);
            return;
        case c4m_tt_semi:
        case c4m_tt_newline:
            consume(ctx);
            continue;
        case c4m_tt_identifier:
            txt = identifier_text(tok_cur(ctx));
            if (!strcmp(txt->data, "named")
                || !strcmp(txt->data, "singleton")
                || !strcmp(txt->data, "root")) {
                object_spec(ctx, txt);
            }
            else {
                add_parse_error(ctx,
                                c4m_err_parse_bad_confspec_sec_type,
                                txt);
                line_skip_recover(ctx);
            }
            continue;

        default:
            add_parse_error(ctx,
                            c4m_err_parse_bad_confspec_sec_type,
                            c4m_token_raw_content(tok_cur(ctx)));
            line_skip_recover(ctx);
            continue;
        }
    }
}

static c4m_tree_node_t *
assign(parse_ctx *ctx, c4m_tree_node_t *lhs, c4m_node_kind_t kind)
{
    c4m_tree_node_t *result;

    temporary_tree(ctx, kind);
    consume(ctx);
    adopt_kid(ctx, lhs);
    adopt_kid(ctx, expression(ctx));
    result = restore_tree(ctx);

    return result;
}

static c4m_tree_node_t *
member_expr(parse_ctx *ctx, c4m_tree_node_t *lhs)
{
    c4m_tree_node_t *result;
    temporary_tree(ctx, c4m_nt_member);
    adopt_kid(ctx, lhs);
    consume(ctx); // Consume the period.
    identifier(ctx);

    while (tok_kind(ctx) == c4m_tt_period) {
        consume(ctx);
        identifier(ctx);
    }

    result = restore_tree(ctx);

    switch (tok_kind(ctx)) {
    case c4m_tt_lbracket:
        return index_expr(ctx, result);
    case c4m_tt_lparen:
        return call_expr(ctx, result);
    default:
        return result;
    }
}

static c4m_tree_node_t *
index_expr(parse_ctx *ctx, c4m_tree_node_t *lhs)
{
    c4m_tree_node_t *result;

    temporary_tree(ctx, c4m_nt_index);
    adopt_kid(ctx, lhs);
    expect(ctx, c4m_tt_lbracket);
    c4m_tree_node_t *item = expression(ctx);

    if (!optional_range(ctx, item)) {
        adopt_kid(ctx, item);
    }

    expect(ctx, c4m_tt_rbracket);
    result = restore_tree(ctx);

    switch (tok_kind(ctx)) {
    case c4m_tt_period:
        return member_expr(ctx, result);
    case c4m_tt_lbracket:
        return index_expr(ctx, result);
    case c4m_tt_lparen:
        return call_expr(ctx, lhs);
    default:
        return result;
    }
}

static c4m_tree_node_t *
call_expr(parse_ctx *ctx, c4m_tree_node_t *lhs)
{
    c4m_tree_node_t *result;

    temporary_tree(ctx, c4m_nt_call);
    adopt_kid(ctx, lhs);
    expect(ctx, c4m_tt_lparen);
    if (tok_kind(ctx) != c4m_tt_rparen) {
        while (true) {
            adopt_kid(ctx, expression(ctx));
            if (tok_kind(ctx) == c4m_tt_comma) {
                consume(ctx);
            }
            else {
                break;
            }
        }
    }

    expect(ctx, c4m_tt_rparen);
    result = restore_tree(ctx);

    switch (tok_kind(ctx)) {
    case c4m_tt_period:
        return member_expr(ctx, result);
    case c4m_tt_lbracket:
        return index_expr(ctx, result);
    case c4m_tt_lparen:
        return call_expr(ctx, lhs);
    default:
        return result;
    }
}

static void
paren_expr(parse_ctx *ctx)
{
    // Not technically a literal, but OK.
    ctx->lit_depth++;
    start_node(ctx, c4m_nt_paren_expr, true);
    adopt_kid(ctx, expression(ctx));
    expect(ctx, c4m_tt_rparen);
    end_node(ctx);
    ctx->lit_depth--;
}

static void
access_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_lparen) {
        paren_expr(ctx);
        return;
    }

    if (tok_kind(ctx) != c4m_tt_identifier) {
        add_parse_error(ctx,
                        c4m_err_parse_expected_token,
                        c4m_new_utf8("identifier"));
        line_skip_recover(ctx);
        return;
    }

    c4m_tree_node_t *lhs = temporary_tree(ctx, c4m_nt_identifier);
    consume(ctx);
    restore_tree(ctx);

    switch (tok_kind(ctx)) {
    case c4m_tt_period:
        adopt_kid(ctx, member_expr(ctx, lhs));
        return;
    case c4m_tt_lbracket:
        adopt_kid(ctx, index_expr(ctx, lhs));
        return;
    case c4m_tt_lparen:
        adopt_kid(ctx, call_expr(ctx, lhs));
        return;
    default:
        adopt_kid(ctx, lhs);
        return;
    }
}

static c4m_tree_node_t *
expression_start(parse_ctx *ctx)
{
    switch (tok_kind(ctx)) {
    case c4m_tt_plus:
        consume(ctx);
        return NULL;
    case c4m_tt_minus:
        temporary_tree(ctx, c4m_nt_unary_op);
        consume(ctx);
        adopt_kid(ctx, expression(ctx));
        return restore_tree(ctx);
    case c4m_tt_not:
        // Here the LHS is an empty string. For TtNot, if there is a LHS,
        // we need to reject that.
        return NULL;

    case c4m_tt_backtick:
    case c4m_tt_object:
    case c4m_tt_func:
    case c4m_tt_int_lit:
    case c4m_tt_hex_lit:
    case c4m_tt_string_lit:
    case c4m_tt_char_lit:
    case c4m_tt_true:
    case c4m_tt_false:
    case c4m_tt_nil:
    case c4m_tt_unquoted_lit:
    case c4m_tt_lbracket:
    case c4m_tt_lbrace:
    case c4m_tt_lparen:
        temporary_tree(ctx, c4m_nt_expression);
        literal(ctx);
        return restore_tree(ctx);

    case c4m_tt_identifier:
        temporary_tree(ctx, c4m_nt_expression);
        if (identifier_is_builtin_type(ctx)) {
            literal(ctx);
        }
        else {
            access_expr(ctx);
        }
        return restore_tree(ctx);
    default:
        err_skip_stmt(ctx, c4m_err_parse_bad_expression_start);
        c4m_exit_to_checkpoint(ctx, '!', __FILE__, __LINE__, __func__);
    }
}

static c4m_tree_node_t *
not_expr(parse_ctx *ctx)
{
    if (match(ctx, c4m_tt_not) == c4m_tt_not) {
        temporary_tree(ctx, c4m_nt_unary_op);
        consume(ctx);
        return restore_tree(ctx);
    }
    return NULL;
}

static c4m_tree_node_t *
shl_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_shl) {
        temporary_tree(ctx, c4m_nt_binary_op);
        consume(ctx);
        adopt_kid(ctx, shl_expr_rhs(ctx));
        return restore_tree(ctx);
    }

    return not_expr(ctx);
}

static c4m_tree_node_t *
shl_expr_rhs(parse_ctx *ctx)
{
    c4m_tree_node_t *expr = expression_start(ctx);

    while (true) {
        c4m_tree_node_t *rhs = shl_expr(ctx);
        if (!rhs) {
            return expr;
        }
        if (expr != NULL) {
            c4m_tree_adopt_and_prepend(rhs, expr);
        }
        expr = rhs;
    }
}

static c4m_tree_node_t *
shr_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_shr) {
        temporary_tree(ctx, c4m_nt_binary_op);
        consume(ctx);
        adopt_kid(ctx, shr_expr_rhs(ctx));
        return restore_tree(ctx);
    }

    return shl_expr(ctx);
}

static c4m_tree_node_t *
shr_expr_rhs(parse_ctx *ctx)
{
    c4m_tree_node_t *expr = expression_start(ctx);

    while (true) {
        c4m_tree_node_t *rhs = shr_expr(ctx);
        if (!rhs) {
            return expr;
        }
        if (expr != NULL) {
            c4m_tree_adopt_and_prepend(rhs, expr);
        }
        expr = rhs;
    }
}

static c4m_tree_node_t *
div_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_div) {
        temporary_tree(ctx, c4m_nt_binary_op);
        consume(ctx);
        adopt_kid(ctx, div_expr_rhs(ctx));
        return restore_tree(ctx);
    }

    return shr_expr(ctx);
}

static c4m_tree_node_t *
div_expr_rhs(parse_ctx *ctx)
{
    c4m_tree_node_t *expr = expression_start(ctx);

    while (true) {
        c4m_tree_node_t *rhs = div_expr(ctx);
        if (!rhs) {
            return expr;
        }
        if (expr != NULL) {
            c4m_tree_adopt_and_prepend(rhs, expr);
        }
        expr = rhs;
    }
}

static c4m_tree_node_t *
mul_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_mul) {
        temporary_tree(ctx, c4m_nt_binary_op);
        consume(ctx);
        adopt_kid(ctx, mul_expr_rhs(ctx));
        return restore_tree(ctx);
    }

    return div_expr(ctx);
}

static c4m_tree_node_t *
mul_expr_rhs(parse_ctx *ctx)
{
    c4m_tree_node_t *expr = expression_start(ctx);

    while (true) {
        c4m_tree_node_t *rhs = mul_expr(ctx);
        if (!rhs) {
            return expr;
        }
        if (expr != NULL) {
            c4m_tree_adopt_and_prepend(rhs, expr);
        }
        expr = rhs;
    }
}

static c4m_tree_node_t *
mod_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_mod) {
        temporary_tree(ctx, c4m_nt_binary_op);
        consume(ctx);
        adopt_kid(ctx, mod_expr_rhs(ctx));
        return restore_tree(ctx);
    }

    return mul_expr(ctx);
}

static c4m_tree_node_t *
mod_expr_rhs(parse_ctx *ctx)
{
    c4m_tree_node_t *expr = expression_start(ctx);

    while (true) {
        c4m_tree_node_t *rhs = mod_expr(ctx);
        if (!rhs) {
            return expr;
        }
        if (expr != NULL) {
            c4m_tree_adopt_and_prepend(rhs, expr);
        }
        expr = rhs;
    }
}

static c4m_tree_node_t *
minus_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_minus) {
        temporary_tree(ctx, c4m_nt_binary_op);
        consume(ctx);
        adopt_kid(ctx, minus_expr_rhs(ctx));
        return restore_tree(ctx);
    }

    return mod_expr(ctx);
}

static c4m_tree_node_t *
minus_expr_rhs(parse_ctx *ctx)
{
    c4m_tree_node_t *expr = expression_start(ctx);

    while (true) {
        c4m_tree_node_t *rhs = minus_expr(ctx);
        if (!rhs) {
            return expr;
        }
        if (expr != NULL) {
            c4m_tree_adopt_and_prepend(rhs, expr);
        }
        expr = rhs;
    }
}

static c4m_tree_node_t *
plus_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_plus) {
        temporary_tree(ctx, c4m_nt_binary_op);
        consume(ctx);
        adopt_kid(ctx, plus_expr_rhs(ctx));
        return restore_tree(ctx);
    }

    return minus_expr(ctx);
}

static c4m_tree_node_t *
plus_expr_rhs(parse_ctx *ctx)
{
    c4m_tree_node_t *expr = expression_start(ctx);

    while (true) {
        c4m_tree_node_t *rhs = plus_expr(ctx);
        if (!rhs) {
            return expr;
        }
        if (expr != NULL) {
            c4m_tree_adopt_and_prepend(rhs, expr);
        }
        expr = rhs;
    }
}

static c4m_tree_node_t *
lt_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_lt) {
        temporary_tree(ctx, c4m_nt_cmp);
        consume(ctx);
        adopt_kid(ctx, lt_expr_rhs(ctx));
        return restore_tree(ctx);
    }

    return plus_expr(ctx);
}

static c4m_tree_node_t *
lt_expr_rhs(parse_ctx *ctx)
{
    c4m_tree_node_t *expr = expression_start(ctx);

    while (true) {
        c4m_tree_node_t *rhs = lt_expr(ctx);
        if (!rhs) {
            return expr;
        }
        if (expr != NULL) {
            c4m_tree_adopt_and_prepend(rhs, expr);
        }
        expr = rhs;
    }
}

static c4m_tree_node_t *
gt_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_gt) {
        temporary_tree(ctx, c4m_nt_cmp);
        consume(ctx);
        adopt_kid(ctx, gt_expr_rhs(ctx));
        return restore_tree(ctx);
    }

    return lt_expr(ctx);
}

static c4m_tree_node_t *
gt_expr_rhs(parse_ctx *ctx)
{
    c4m_tree_node_t *expr = expression_start(ctx);

    while (true) {
        c4m_tree_node_t *rhs = gt_expr(ctx);
        if (!rhs) {
            return expr;
        }
        if (expr != NULL) {
            c4m_tree_adopt_and_prepend(rhs, expr);
        }
        expr = rhs;
    }
}

static c4m_tree_node_t *
lte_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_lte) {
        temporary_tree(ctx, c4m_nt_cmp);
        consume(ctx);
        adopt_kid(ctx, lte_expr_rhs(ctx));
        return restore_tree(ctx);
    }

    return gt_expr(ctx);
}

static c4m_tree_node_t *
lte_expr_rhs(parse_ctx *ctx)
{
    c4m_tree_node_t *expr = expression_start(ctx);

    while (true) {
        c4m_tree_node_t *rhs = lte_expr(ctx);
        if (!rhs) {
            return expr;
        }
        if (expr != NULL) {
            c4m_tree_adopt_and_prepend(rhs, expr);
        }
        expr = rhs;
    }
}

static c4m_tree_node_t *
gte_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_gte) {
        temporary_tree(ctx, c4m_nt_cmp);
        consume(ctx);
        adopt_kid(ctx, gte_expr_rhs(ctx));
        return restore_tree(ctx);
    }

    return lte_expr(ctx);
}

static c4m_tree_node_t *
gte_expr_rhs(parse_ctx *ctx)
{
    c4m_tree_node_t *expr = expression_start(ctx);

    while (true) {
        c4m_tree_node_t *rhs = gte_expr(ctx);
        if (!rhs) {
            return expr;
        }
        if (expr != NULL) {
            c4m_tree_adopt_and_prepend(rhs, expr);
        }
        expr = rhs;
    }
}

static c4m_tree_node_t *
eq_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_cmp) {
        temporary_tree(ctx, c4m_nt_cmp);
        consume(ctx);
        adopt_kid(ctx, eq_expr_rhs(ctx));
        return restore_tree(ctx);
    }

    return gte_expr(ctx);
}

static c4m_tree_node_t *
eq_expr_rhs(parse_ctx *ctx)
{
    c4m_tree_node_t *expr = expression_start(ctx);

    while (true) {
        c4m_tree_node_t *rhs = eq_expr(ctx);
        if (!rhs) {
            return expr;
        }
        if (expr != NULL) {
            c4m_tree_adopt_and_prepend(rhs, expr);
        }
        expr = rhs;
    }
}

static c4m_tree_node_t *
bit_and_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_bit_and) {
        temporary_tree(ctx, c4m_nt_binary_op);
        consume(ctx);
        adopt_kid(ctx, bit_and_expr_rhs(ctx));
        return restore_tree(ctx);
    }

    return eq_expr(ctx);
}

static c4m_tree_node_t *
bit_and_expr_rhs(parse_ctx *ctx)
{
    c4m_tree_node_t *expr = expression_start(ctx);

    while (true) {
        c4m_tree_node_t *rhs = bit_and_expr(ctx);
        if (!rhs) {
            return expr;
        }
        if (expr != NULL) {
            c4m_tree_adopt_and_prepend(rhs, expr);
        }
        expr = rhs;
    }
}

static c4m_tree_node_t *
bit_xor_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_bit_xor) {
        temporary_tree(ctx, c4m_nt_binary_op);
        consume(ctx);
        adopt_kid(ctx, bit_xor_expr_rhs(ctx));
        return restore_tree(ctx);
    }

    return bit_and_expr(ctx);
}

static c4m_tree_node_t *
bit_xor_expr_rhs(parse_ctx *ctx)
{
    c4m_tree_node_t *expr = expression_start(ctx);

    while (true) {
        c4m_tree_node_t *rhs = bit_xor_expr(ctx);
        if (!rhs) {
            return expr;
        }
        if (expr != NULL) {
            c4m_tree_adopt_and_prepend(rhs, expr);
        }
        expr = rhs;
    }
}

static c4m_tree_node_t *
bit_or_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_bit_or) {
        temporary_tree(ctx, c4m_nt_binary_op);
        consume(ctx);
        adopt_kid(ctx, bit_or_expr_rhs(ctx));
        return restore_tree(ctx);
    }

    return bit_xor_expr(ctx);
}

static c4m_tree_node_t *
bit_or_expr_rhs(parse_ctx *ctx)
{
    c4m_tree_node_t *expr = expression_start(ctx);

    while (true) {
        c4m_tree_node_t *rhs = bit_or_expr(ctx);
        if (!rhs) {
            return expr;
        }
        if (expr != NULL) {
            c4m_tree_adopt_and_prepend(rhs, expr);
        }
        expr = rhs;
    }
}

static c4m_tree_node_t *
ne_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_neq) {
        temporary_tree(ctx, c4m_nt_cmp);
        consume(ctx);
        adopt_kid(ctx, ne_expr_rhs(ctx));
        return restore_tree(ctx);
    }

    return bit_or_expr(ctx);
}

static c4m_tree_node_t *
ne_expr_rhs(parse_ctx *ctx)
{
    c4m_tree_node_t *expr = expression_start(ctx);

    while (true) {
        c4m_tree_node_t *rhs = ne_expr(ctx);
        if (!rhs) {
            return expr;
        }
        if (expr != NULL) {
            c4m_tree_adopt_and_prepend(rhs, expr);
        }
        expr = rhs;
    }
}

static c4m_tree_node_t *
and_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_and) {
        temporary_tree(ctx, c4m_nt_and);
        consume(ctx);
        adopt_kid(ctx, and_expr_rhs(ctx));
        return restore_tree(ctx);
    }

    return ne_expr(ctx);
}

static c4m_tree_node_t *
and_expr_rhs(parse_ctx *ctx)
{
    c4m_tree_node_t *expr = expression_start(ctx);

    while (true) {
        c4m_tree_node_t *rhs = and_expr(ctx);
        if (!rhs) {
            return expr;
        }
        if (expr != NULL) {
            c4m_tree_adopt_and_prepend(rhs, expr);
        }
        expr = rhs;
    }
}

static c4m_tree_node_t *
or_expr(parse_ctx *ctx)
{
    if (tok_kind(ctx) == c4m_tt_or) {
        temporary_tree(ctx, c4m_nt_or);
        consume(ctx);
        adopt_kid(ctx, expression(ctx));
        return restore_tree(ctx);
    }

    return and_expr(ctx);
}

static c4m_tree_node_t *
expression(parse_ctx *ctx)
{
    c4m_tree_node_t *expr = expression_start(ctx);

    while (true) {
        c4m_tree_node_t *rhs = or_expr(ctx);
        if (!rhs) {
            return expr;
        }
        if (expr != NULL) {
            c4m_tree_adopt_and_prepend(rhs, expr);
        }
        expr = rhs;
    }
}

static c4m_tree_node_t *
section(parse_ctx *ctx, c4m_tree_node_t *node)
{
    c4m_pnode_t *lhs = (c4m_pnode_t *)c4m_tree_get_contents(node);

    if (lhs->kind != c4m_nt_expression || !c4m_tree_get_number_children(node)) {
        printf("Bad start 1.\n");
bad_start:
        raise_err_at_node(ctx,
                          current_parse_node(ctx),
                          c4m_err_parse_unexpected_after_expr,
                          false);
    }

    lhs = c4m_tree_get_contents(c4m_tree_get_child(node, 0));

    if (lhs->kind != c4m_nt_identifier) {
        printf("Bad start 2.\n");
        goto bad_start;
    }

    c4m_tree_node_t *result = temporary_tree(ctx, c4m_nt_section);

    adopt_kid(ctx, node);

    switch (match(ctx,
                  c4m_tt_identifier,
                  c4m_tt_string_lit,
                  c4m_tt_newline,
                  c4m_tt_lbrace)) {
    case c4m_tt_identifier:
        identifier(ctx);
        break;
    case c4m_tt_string_lit:
        simple_lit(ctx);
        break;
    case c4m_tt_newline:
    case c4m_tt_lbrace:
        break;
    default:
        abort(); // Should be unreachable.
    }

    body(ctx, (c4m_pnode_t *)c4m_tree_get_contents(result));

    restore_tree(ctx);
    return result;
}

static void
body(parse_ctx *ctx, c4m_pnode_t *docstring_target)
{
    // TODO: should 100% have docstrings be a constexpr instead
    // of just a string literal as the only option.

    c4m_tree_node_t *expr;

    opt_one_newline(ctx);

    if (tok_kind(ctx) != c4m_tt_lbrace) {
        raise_err_at_node(ctx,
                          current_parse_node(ctx),
                          c4m_err_parse_expected_brace,
                          false);
    }

    start_node(ctx, c4m_nt_body, true);

    opt_newlines(ctx);
    if (docstring_target != NULL) {
        opt_doc_strings(ctx, docstring_target);
    }
    else {
        if (tok_kind(ctx) == c4m_tt_string_lit) {
            add_parse_error(ctx, c4m_err_parse_not_docable_block);
            consume(ctx);
            if (tok_kind(ctx) == c4m_tt_string_lit) {
                consume(ctx);
            }
        }
    }

    DECLARE_CHECKPOINT();
    while (true) {
        ENTER_CHECKPOINT();
        switch (CHECKPOINT_STATUS()) {
        case 0:
            switch (tok_kind(ctx)) {
            case c4m_tt_eof:
                EOF_ERROR(ctx);
            case c4m_tt_rbrace:
                consume(ctx);
                opt_newlines(ctx);
                end_node(ctx);
                END_CHECKPOINT();
                return;
            case c4m_tt_semi:
            case c4m_tt_newline:
                consume(ctx);
                continue;
            case c4m_tt_enum:
                err_skip_stmt(ctx, c4m_err_parse_enums_are_toplevel);
                continue;
            case c4m_tt_private:
            case c4m_tt_func:
                err_skip_stmt(ctx, c4m_err_parse_funcs_are_toplevel);
                continue;
            case c4m_tt_lock_attr:
                lock_attr(ctx);
                continue;
            case c4m_tt_if:
                if_stmt(ctx);
                continue;
            case c4m_tt_for:
                for_stmt(ctx);
                continue;
            case c4m_tt_while:
                while_stmt(ctx);
                continue;
            case c4m_tt_typeof:
                typeof_stmt(ctx);
                continue;
            case c4m_tt_switch:
                switch_stmt(ctx);
                continue;
            case c4m_tt_continue:
                continue_stmt(ctx);
                continue;
            case c4m_tt_break:
                break_stmt(ctx);
                continue;
            case c4m_tt_return:
                return_stmt(ctx);
                continue;
            case c4m_tt_global:
            case c4m_tt_var:
            case c4m_tt_const:
                variable_decl(ctx);
                end_of_statement(ctx);
                continue;
            case c4m_tt_identifier:
                if (lookahead(ctx, 1, false) == c4m_tt_colon) {
                    switch (lookahead(ctx, 2, true)) {
                    case c4m_tt_for:
                    case c4m_tt_while:
                        label_stmt(ctx);
                        continue;
                    default:
                        break; // fall through.
                    }
                }
                if (text_matches(ctx, "assert")) {
                    assert_stmt(ctx);
                    continue;
                }
                if (text_matches(ctx, "use")) {
                    use_stmt(ctx);
                    continue;
                }
                if (text_matches(ctx, "parameter")) {
                    err_skip_stmt(ctx, c4m_err_parse_parameter_is_toplevel);
                    continue;
                }
                if (text_matches(ctx, "extern")) {
                    err_skip_stmt(ctx, c4m_err_parse_extern_is_toplevel);
                    extern_block(ctx);
                    continue;
                }
                if (text_matches(ctx, "confspec")) {
                    err_skip_stmt(ctx, c4m_err_parse_confspec_is_toplevel);
                    continue;
                }
                // fallthrough. We'll parse an identifier then parent
                // it based on what we see after.
                // fallthrough
            default:
                expr = expression(ctx);
                switch (tok_kind(ctx)) {
                case c4m_tt_plus_eq:
                case c4m_tt_minus_eq:
                case c4m_tt_mul_eq:
                case c4m_tt_div_eq:
                case c4m_tt_mod_eq:
                case c4m_tt_bit_or_eq:
                case c4m_tt_bit_xor_eq:
                case c4m_tt_bit_and_eq:
                case c4m_tt_shl_eq:
                case c4m_tt_shr_eq:
                    adopt_kid(ctx, assign(ctx, expr, c4m_nt_binary_assign_op));
                    continue;
                case c4m_tt_colon:
                case c4m_tt_assign:
                    adopt_kid(ctx, assign(ctx, expr, c4m_nt_assign));
                    continue;
                case c4m_tt_identifier:
                case c4m_tt_string_lit:
                case c4m_tt_lbrace:
                    adopt_kid(ctx, section(ctx, expr));
                    continue;
                default:
                    adopt_kid(ctx, expr);
                    end_of_statement(ctx);
                    continue;
                }
            }
        case '!':
            THROW('!');
        default:
            continue;
        }
    }
}

static c4m_tree_node_t *
module(parse_ctx *ctx)
{
    c4m_tree_node_t *expr;
    c4m_pnode_t     *root   = c4m_new(c4m_tspec_parse_node(),
                                ctx,
                                c4m_nt_module);
    c4m_tree_node_t *result = c4m_new(c4m_tspec_tree(c4m_tspec_parse_node()),
                                      c4m_kw("contents", c4m_ka(root)));

    ctx->cur = result;

    opt_doc_strings(ctx, root);

    DECLARE_CHECKPOINT();
    while (true) {
        ENTER_CHECKPOINT();
        switch (CHECKPOINT_STATUS()) {
        case 0:
            switch (tok_cur(ctx)->kind) {
            case c4m_tt_eof:
                END_CHECKPOINT();
                return result;
            case c4m_tt_semi:
            case c4m_tt_newline:
                consume(ctx);
                continue;
            case c4m_tt_enum:
                enum_stmt(ctx);
                continue;
            case c4m_tt_lock_attr:
                lock_attr(ctx);
                continue;
            case c4m_tt_if:
                if_stmt(ctx);
                continue;
            case c4m_tt_for:
                for_stmt(ctx);
                continue;
            case c4m_tt_while:
                while_stmt(ctx);
                continue;
            case c4m_tt_typeof:
                typeof_stmt(ctx);
                continue;
            case c4m_tt_switch:
                switch_stmt(ctx);
                continue;
            case c4m_tt_continue:
                err_skip_stmt(ctx, c4m_err_parse_continue_outside_loop);
                continue;
            case c4m_tt_break:
                err_skip_stmt(ctx, c4m_err_parse_break_outside_loop);
                continue;
            case c4m_tt_return:
                err_skip_stmt(ctx, c4m_err_parse_return_outside_func);
                continue;
            case c4m_tt_private:
            case c4m_tt_func:
                func_def(ctx);
                continue;
            case c4m_tt_global:
                if (lookahead(ctx, 1, false) == c4m_tt_enum) {
                    enum_stmt(ctx);
                }
                continue;
            case c4m_tt_var:
            case c4m_tt_const:
                variable_decl(ctx);
                end_of_statement(ctx);
                continue;
            case c4m_tt_identifier:
                if (lookahead(ctx, 1, false) == c4m_tt_colon) {
                    switch (lookahead(ctx, 2, true)) {
                    case c4m_tt_for:
                    case c4m_tt_while:
                        label_stmt(ctx);
                        continue;
                    default:
                        break; // fall through.
                    }
                }
                if (text_matches(ctx, "assert")) {
                    assert_stmt(ctx);
                    continue;
                }
                if (text_matches(ctx, "use")) {
                    use_stmt(ctx);
                    continue;
                }
                if (text_matches(ctx, "parameter")) {
                    parameter_block(ctx);
                    continue;
                }
                if (text_matches(ctx, "extern")) {
                    extern_block(ctx);
                    continue;
                }
                if (text_matches(ctx, "confspec")) {
                    confspec_block(ctx);
                    continue;
                }
                // fallthrough. We'll parse an identifier then parent
                // it based on what we see after.
                // fallthrough
            default:
                expr = expression(ctx);

                if (!expr) {
                    END_CHECKPOINT();
                    return result;
                }

                switch (tok_kind(ctx)) {
                case c4m_tt_plus_eq:
                case c4m_tt_minus_eq:
                case c4m_tt_mul_eq:
                case c4m_tt_div_eq:
                case c4m_tt_mod_eq:
                case c4m_tt_bit_or_eq:
                case c4m_tt_bit_xor_eq:
                case c4m_tt_bit_and_eq:
                case c4m_tt_shl_eq:
                case c4m_tt_shr_eq:
                    adopt_kid(ctx, assign(ctx, expr, c4m_nt_binary_assign_op));
                    continue;
                case c4m_tt_colon:
                case c4m_tt_assign:
                    adopt_kid(ctx, assign(ctx, expr, c4m_nt_assign));
                    continue;
                case c4m_tt_identifier:
                case c4m_tt_string_lit:
                case c4m_tt_lbrace:
                    adopt_kid(ctx, section(ctx, expr));
                    continue;
                default:
                    adopt_kid(ctx, expr);
                    end_of_statement(ctx);
                    continue;
                }
            }

        case '!':
        default:
            return NULL;
        }
    }
}

c4m_utf8_t *
c4m_node_type_name(c4m_node_kind_t n)
{
    node_type_info_t *info = (node_type_info_t *)&node_type_info[n];
    return c4m_new_utf8(info->name);
}

static c4m_utf8_t *
repr_one_node(c4m_pnode_t *one)
{
    node_type_info_t *info = (node_type_info_t *)&node_type_info[one->kind];
    c4m_utf8_t       *name = c4m_new_utf8(info->name);
    c4m_utf8_t       *xtra;
    c4m_utf8_t       *doc;
    char             *fmt = "[h1]{}[/] [h2]{}[/] [h3]{}[/]";

    if (info->show_contents && one->token != NULL) {
        c4m_utf8_t *token_text = c4m_token_raw_content(one->token);
        xtra                   = c4m_in_parens(token_text);
    }
    else {
        xtra = c4m_empty_string();
    }

    if (info->takes_docs) {
        if (one->short_doc == NULL) {
            doc = c4m_new_utf8("no docs");
        }
        else {
            if (one->long_doc == NULL) {
                doc = c4m_new_utf8("short doc");
            }
            else {
                doc = c4m_new_utf8("full docs");
            }
        }
    }
    else {
        doc = c4m_empty_string();
    }

    c4m_utf8_t *result = c4m_cstr_format(fmt, name, xtra, doc);

    //    c4m_print(c4m_hex_dump(result->data, result->byte_len));
    return result;
}

void
c4m_print_parse_node(c4m_tree_node_t *n)
{
    c4m_print(c4m_grid_tree(n, c4m_kw("converter", c4m_ka(repr_one_node))));
}

c4m_grid_t *
c4m_format_parse_tree(c4m_file_compile_ctx *ctx)
{
    return c4m_grid_tree(ctx->parse_tree,
                         c4m_kw("converter", c4m_ka(repr_one_node)));
}

bool
c4m_parse(c4m_file_compile_ctx *file_ctx)
{
    parse_ctx ctx = {
        .cur          = NULL,
        .file_ctx     = file_ctx,
        .cached_token = NULL,
        .token_ix     = 0,
        .cache_ix     = -1,
        .root_stack   = c4m_new(c4m_tspec_stack(c4m_tspec_parse_node())),
    };

    // tok_cur() doesn't skip comment tokens; consume() does.  So if a
    // module starts w/ something that is always skipped, we need to
    // skip it up front, or add an extra check to tok_cur.
    while (true) {
        switch (tok_kind(&ctx)) {
        case c4m_tt_newline:
        case c4m_tt_long_comment:
        case c4m_tt_line_comment:
        case c4m_tt_space:
            ctx.token_ix++;
            continue;
        default:
            break;
        }
        break;
    }

    file_ctx->parse_tree = module(&ctx);
    return file_ctx->parse_tree != NULL;
}

const c4m_vtable_t c4m_parse_node_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        (c4m_vtable_entry)parse_node_init,
    },
};

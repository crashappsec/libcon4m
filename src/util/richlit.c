#include "con4m.h"

typedef struct {
    c4m_utf8_t *name;
    union {
        c4m_style_t style;
        c4m_color_t color;
        int         kw_ix;
    } contents;
    uint32_t    flags;
    c4m_style_t prev_style;

} tag_item_t;

typedef struct fmt_frame_t {
    struct fmt_frame_t *next;
    c4m_utf8_t         *raw_contents;
    int32_t             start;
    int32_t             end;
    c4m_style_t         style; // Calculated incremental style from a tag.
} fmt_frame_t;

typedef struct {
    fmt_frame_t *start_frame;
    fmt_frame_t *cur_frame;
    c4m_list_t  *style_directions;
    c4m_utf8_t  *style_text;
    tag_item_t **stack;
    c4m_utf8_t  *raw;
    c4m_style_t  cur_style;
    int          stack_ix;
} style_ctx;

typedef struct {
    c4m_utf8_t  *raw;
    c4m_list_t  *tokens;
    fmt_frame_t *cur_frame;
    style_ctx   *style_ctx;
    c4m_utf8_t  *not_matched;
    int          num_toks;
    int          tok_ix;
    int          num_atoms;
    bool         negating;
    bool         at_start;
    bool         got_percent;
    c4m_style_t  cur_style;
} tag_parse_ctx;

#define F_NEG         (1 << 1)
#define F_BGCOLOR     (1 << 2)
#define F_STYLE_KW    (1 << 3)
#define F_STYLE_CELL  (1 << 4)
#define F_STYLE_COLOR (1 << 5)
#define F_TAG_START   (1 << 6)
#define F_POPPED      (1 << 7)

// top: '[' ['/']
// centity: COLOR ('on' COLOR)
// style:  WORD
// negator: 'no'
// bold:   'b' | 'bold'
// italic: 'i' | 'italic'
// strike: 's' | 'st' | 'strike' | 'strikethru' | 'strikethrough'
// underline: 'u' | 'underline'
// 2underline: 'uu' | '2u'
// reverse: 'r' | 'reverse' | 'inv' | 'inverse'
// title 't' | 'title'
// lower 'l' | 'lower'
// upper 'up' | 'upper'
//
// also: 'default' or 'current' as a color.

static c4m_dict_t *style_keywords = NULL;

#if 0
#include "static/richlit.c"

static inline void
init_style_keywords()
{

    if (style_keywords == NULL) {
        c4m_buf_t    *b = c4m_new(c4m_type_buffer(),
                               c4m_kw("raw",
                                      c4m_ka(_marshaled_style_keywords),
                                      "length",
                                      c4m_ka(1426)));
        c4m_stream_t *s = c4m_new(c4m_type_stream(),
                                  c4m_kw("buffer", c4m_ka(b)));

        c4m_gc_register_root(&style_keywords, 1);
        style_keywords = c4m_unmarshal(s);
    }
}
#else
static inline void
init_style_keywords()
{
    if (style_keywords == NULL) {
        c4m_gc_register_root(&style_keywords, 1);
        c4m_dict_t *d  = c4m_dict(c4m_type_utf8(), c4m_type_int());
        style_keywords = d;

        hatrack_dict_add(d, c4m_new_utf8("no"), (void *)1LLU);
        hatrack_dict_add(d, c4m_new_utf8("b"), (void *)2LLU);
        hatrack_dict_add(d, c4m_new_utf8("bold"), (void *)2LLU);
        hatrack_dict_add(d, c4m_new_utf8("i"), (void *)3LLU);
        hatrack_dict_add(d, c4m_new_utf8("italic"), (void *)3LLU);
        hatrack_dict_add(d, c4m_new_utf8("italics"), (void *)3LLU);
        hatrack_dict_add(d, c4m_new_utf8("st"), (void *)4LLU);
        hatrack_dict_add(d, c4m_new_utf8("strike"), (void *)4LLU);
        hatrack_dict_add(d, c4m_new_utf8("strikethru"), (void *)4LLU);
        hatrack_dict_add(d, c4m_new_utf8("strikethrough"), (void *)4LLU);
        hatrack_dict_add(d, c4m_new_utf8("u"), (void *)5LLU);
        hatrack_dict_add(d, c4m_new_utf8("underline"), (void *)5LLU);
        hatrack_dict_add(d, c4m_new_utf8("uu"), (void *)6LLU);
        hatrack_dict_add(d, c4m_new_utf8("2u"), (void *)6LLU);
        hatrack_dict_add(d, c4m_new_utf8("r"), (void *)7LLU);
        hatrack_dict_add(d, c4m_new_utf8("reverse"), (void *)7LLU);
        hatrack_dict_add(d, c4m_new_utf8("inverse"), (void *)7LLU);
        hatrack_dict_add(d, c4m_new_utf8("invert"), (void *)7LLU);
        hatrack_dict_add(d, c4m_new_utf8("inv"), (void *)7LLU);
        hatrack_dict_add(d, c4m_new_utf8("t"), (void *)8LLU);
        hatrack_dict_add(d, c4m_new_utf8("title"), (void *)8LLU);
        hatrack_dict_add(d, c4m_new_utf8("l"), (void *)9LLU);
        hatrack_dict_add(d, c4m_new_utf8("lower"), (void *)9LLU);
        hatrack_dict_add(d, c4m_new_utf8("up"), (void *)10LLU);
        hatrack_dict_add(d, c4m_new_utf8("upper"), (void *)10LLU);
        hatrack_dict_add(d, c4m_new_utf8("on"), (void *)11LLU);
        hatrack_dict_add(d, c4m_new_utf8("fg"), (void *)12LLU);
        hatrack_dict_add(d, c4m_new_utf8("foreground"), (void *)12LLU);
        hatrack_dict_add(d, c4m_new_utf8("bg"), (void *)13LLU);
        hatrack_dict_add(d, c4m_new_utf8("background"), (void *)13LLU);
        hatrack_dict_add(d, c4m_new_utf8("color"), (void *)14LLU);
        /*
          c4m_dump_c_static_instance_code(d,
          "style_keywords",
          new_utf8("/tmp/style_keys.c"));
        */
    }
}
#endif

#define rich_tok_emit()                       \
    if (p != start) {                         \
        slice = c4m_new(c4m_type_utf8(),      \
                        c4m_kw("cstring",     \
                               c4m_ka(start), \
                               "length",      \
                               p - start));   \
        c4m_list_append(ret, slice);          \
    }

// tokenize the text between '[' and ']' into useful bits.
static c4m_list_t *
tokenize_rich_tag(c4m_utf8_t *s)
{
    c4m_list_t *ret   = c4m_new(c4m_type_list(c4m_type_utf8()));
    char       *p     = s->data;
    char       *end   = s->data + c4m_str_byte_len(s);
    char       *start = p;
    c4m_utf8_t *slice;

    while (p < end) {
        switch (*p) {
        case ' ':
            rich_tok_emit();
            p++;
            start = p;
            continue;
        case 0:
            break;
            // '-' and '/' are aliases; they indicate that subsequent tokens
            // turn OFF things.
            // '+' can be used to turn things back on in the same tag (tags
            // start in this state so it's never really necessary)
            // '%' indicates that the next token(s) represent a color, and
            // we are looking to set the BACKGROUND with the color, not
            // the foreground (which is the default for colors). The '%' was
            // chosen because in Unix shells, jobs are often referred to by
            // %; for instance; backgrounding a job is usually `bg %1`.
            // Of course, you'd use the same syntax to fg it, but hopefully
            // is okay.
        case '/':
        case '-':
        case '+':
        case '%':
            rich_tok_emit();
            start = p;
            p++;
            rich_tok_emit();
            start = p;
            continue;
        default:
            p++;
            continue;
        }
    }
    rich_tok_emit();

    return ret;
}

static inline void
enter_default_state(tag_parse_ctx *ctx)
{
    ctx->got_percent = false;
}

static inline void
unmatched_check(tag_parse_ctx *ctx)
{
    if (ctx->not_matched != NULL) {
        c4m_utf8_t *msg = c4m_cstr_format(
            "When processing rich lit specifier \\[{}\\], could not find a "
            "match for {} in either our color list or style list.",
            ctx->raw,
            ctx->not_matched);
        C4M_RAISE(msg);
    }
}

static inline void
no_pct_check(tag_parse_ctx *ctx)
{
    if (ctx->got_percent == true) {
        c4m_utf8_t *msg = c4m_cstr_format(
            "When processing rich lit specifier \\[{}\\], found a '%' followed "
            "by another special character, when a color was expected.",
            ctx->raw);
        C4M_RAISE(msg);
    }
}

static void
tag_gc_bits(uint64_t *bitmap, tag_item_t *tag)
{
    c4m_mark_raw_to_addr(bitmap, tag, &tag->name);
}

static void
frame_gc_bits(uint64_t *bitmap, fmt_frame_t *frame)
{
    c4m_mark_raw_to_addr(bitmap, frame, &frame->raw_contents);
}

static inline tag_item_t *
alloc_tag_item(tag_parse_ctx *ctx)
{
    tag_item_t *out = c4m_gc_alloc_mapped(tag_item_t, tag_gc_bits);
    out->name       = ctx->not_matched;

    if (ctx->negating == true) {
        out->flags |= F_NEG;
    }

    if (ctx->got_percent) {
        out->flags |= F_BGCOLOR;
    }

    if (ctx->at_start) {
        out->flags |= F_TAG_START;
    }

    ctx->num_atoms++;
    ctx->not_matched = NULL;

    enter_default_state(ctx);

    c4m_list_append(ctx->style_ctx->style_directions, out);

    return out;
}

static inline bool
try_style_keyword(tag_parse_ctx *ctx)
{
    c4m_utf8_t *s = ctx->not_matched;

    uint64_t n = (uint64_t)hatrack_dict_get(style_keywords, s, NULL);

    if (n == 0) {
        return false;
    }

    if (ctx->got_percent == true) {
        c4m_utf8_t *msg = c4m_cstr_format(
            "When processing rich lit specifier \\[{}\\], expected a "
            "color name after '%', but got a builtin style keyword ({}).",
            ctx->raw,
            s);
        C4M_RAISE(msg);
    }

    tag_item_t *out     = alloc_tag_item(ctx);
    out->contents.kw_ix = n;
    out->flags |= F_STYLE_KW;
    return true;
}

static inline bool
try_cell_style(tag_parse_ctx *ctx)
{
    c4m_utf8_t         *s  = ctx->not_matched;
    c4m_render_style_t *rs = c4m_lookup_cell_style((c4m_to_utf8(s))->data);

    if (rs == NULL) {
        return false;
    }

    if (ctx->got_percent == true) {
        c4m_utf8_t *msg = c4m_cstr_format(
            "When processing rich lit specifier \\[{}\\], expected a "
            "color name after '%', but got a style name ({}).",
            ctx->raw,
            s);
        C4M_RAISE(msg);
    }

    tag_item_t *out     = alloc_tag_item(ctx);
    out->contents.style = c4m_str_style(rs);
    out->flags |= F_STYLE_CELL;

    return true;
}

static inline bool
try_color(tag_parse_ctx *ctx)
{
    c4m_utf8_t *s     = ctx->not_matched;
    c4m_color_t color = c4m_lookup_color(s);

    if (color == -1) {
        return false;
    }

    tag_item_t *out     = alloc_tag_item(ctx);
    out->contents.color = color;
    out->flags |= F_STYLE_COLOR;

    return true;
}

#define punc_checks(ctx)  \
    unmatched_check(ctx); \
    no_pct_check(ctx);    \
    enter_default_state(ctx)

// Turn the extracted style block into data that we can then turn into
// a style.
static inline void
internal_parse_style_lit(tag_parse_ctx *ctx)
{
    while (ctx->tok_ix < ctx->num_toks) {
        c4m_utf8_t *text = c4m_list_get(ctx->tokens, ctx->tok_ix++, NULL);
        switch (text->data[0]) {
        case '%':
            punc_checks(ctx);
            ctx->got_percent = true;
            continue;
        case '/':
        case '-':
            punc_checks(ctx);
            ctx->negating = true;
            continue;
        case '+':
            punc_checks(ctx);
            ctx->negating = false;
            continue;
        default:
            if (ctx->not_matched == NULL) {
                ctx->not_matched = text;
            }
            else {
                ctx->not_matched = c4m_cstr_format("{} {}",
                                                   ctx->not_matched,
                                                   text);
            }

            if (try_style_keyword(ctx)) {
                continue;
            }

            if (try_cell_style(ctx)) {
                continue;
            }

            if (try_color(ctx)) {
                continue;
            }
            // Else; next loop.
        }
    }

    if (ctx->not_matched != NULL) {
        c4m_utf8_t *msg = c4m_cstr_format(
            "When processing rich lit specifier \\[{}\\], could not match "
            "{} to a known style or color.",
            ctx->raw,
            ctx->not_matched);
        C4M_RAISE(msg);
    }

    if (ctx->num_atoms == 0) {
        if (ctx->negating == false) {
            c4m_utf8_t *msg = c4m_cstr_format(
                "Empty tags are not allowed in rich literals ({}).",
                ctx->raw);
            C4M_RAISE(msg);
        }
        alloc_tag_item(ctx);
    }
}

static void
parse_style_lit(style_ctx *ctx)
{
    fmt_frame_t *f      = ctx->cur_frame;
    c4m_list_t  *tokens = tokenize_rich_tag(f->raw_contents);

    tag_parse_ctx tag_ctx = {
        .tokens      = tokens,
        .cur_frame   = f,
        .style_ctx   = ctx,
        .not_matched = NULL,
        .num_toks    = c4m_list_len(tokens),
        .tok_ix      = 0,
        .num_atoms   = 0,
        .at_start    = true,
        .negating    = false,
        .got_percent = false,
        .raw         = f->raw_contents,
    };

    init_style_keywords();

    if (f->next != NULL) {
        f->end = f->next->start;
    }
    else {
        f->end = -1;
    }

    internal_parse_style_lit(&tag_ctx);
}

// Extract the raw text between '[' and ']'.
static inline void
c4m_extract_style_blocks(style_ctx *ctx, char *original_input)
{
    int             n   = strlen(original_input);
    char           *p   = original_input;
    char           *end = p + n;
    c4m_codepoint_t cp;
    fmt_frame_t    *style_first = NULL;
    fmt_frame_t    *style_cur   = NULL;
    fmt_frame_t    *tmp;

    char *unstyled_string = alloca(n + 1);
    char *tag_text        = alloca(n);
    bool  in_tag          = false;
    int   unstyled_ix     = 0;
    int   unstyled_cp     = 0;
    int   tag_ix;

    while (p < end) {
        char c = *p++;

        if (in_tag) {
            switch (c) {
            case ']':
                tag_text[tag_ix++] = 0;
                in_tag             = false;
                tmp                = c4m_gc_alloc_mapped(fmt_frame_t,
                                          frame_gc_bits);
                tmp->start         = unstyled_cp;
                tmp->raw_contents  = c4m_new_utf8(tag_text);
                if (style_first == NULL) {
                    style_first = tmp;
                }
                else {
                    style_cur->next = tmp;
                }
                style_cur = tmp;
                continue;
            default:
                tag_text[tag_ix++] = c;
                continue;
            }
        }
        else {
            if (!(c & 0x80)) {
                unstyled_cp++;
            }
            switch (c) {
            case '\\':
                if (p == end) {
                    C4M_CRAISE("Last character was an escape (not allowed).");
                }
                cp = *p++;
                switch (cp) {
                case 'n':
                    unstyled_string[unstyled_ix++] = '\n';
                    break;
                case 'r':
                    unstyled_string[unstyled_ix++] = '\r';
                    break;
                case 't':
                    unstyled_string[unstyled_ix++] = '\t';
                    break;
                default:
                    unstyled_string[unstyled_ix++] = cp;
                    break;
                }
                continue;
            case '[':
                unstyled_cp--;
                in_tag = true;
                tag_ix = 0;
                continue;
            default:
                unstyled_string[unstyled_ix++] = c;
                continue;
            }
        }
    }
    if (in_tag) {
        C4M_RAISE(c4m_cstr_format("EOF in style marker in string: {}",
                                  original_input));
    }
    unstyled_string[unstyled_ix] = 0;
    ctx->style_text              = c4m_new_utf8(unstyled_string);
    ctx->style_directions        = c4m_new(c4m_type_list(c4m_type_ref()));
    ctx->start_frame             = style_first;
    ctx->cur_frame               = style_first;
}

#define OP_EXTRACT (F_STYLE_CELL | F_STYLE_COLOR | F_STYLE_KW)

static void
apply_one_atom(style_ctx *ctx, tag_item_t *atom, uint32_t op)
{
    atom->prev_style = ctx->cur_style;
    atom->flags &= ~F_POPPED;

    ctx->stack[ctx->stack_ix++] = atom;

    switch (op) {
    case F_STYLE_CELL:
        ctx->cur_style = atom->contents.style;
        return;

    case F_STYLE_COLOR:
        if (atom->flags & F_BGCOLOR) {
            ctx->cur_style = c4m_set_bg_color(ctx->cur_style,
                                              atom->contents.color);
        }
        else {
            ctx->cur_style = c4m_set_fg_color(ctx->cur_style,
                                              atom->contents.color);
        }
        return;
    default:
        switch (atom->contents.kw_ix) {
        case 2:
            ctx->cur_style = c4m_add_bold(ctx->cur_style);
            return;
        case 3:
            ctx->cur_style = c4m_add_italic(ctx->cur_style);
            return;
        case 4:
            ctx->cur_style = c4m_add_strikethrough(ctx->cur_style);
            return;
        case 5:
            ctx->cur_style = c4m_add_underline(ctx->cur_style);
            return;
        case 6:
            ctx->cur_style = c4m_add_double_underline(ctx->cur_style);
            return;
        case 7:
            ctx->cur_style = c4m_add_inverse(ctx->cur_style);
            return;
        case 8:
            ctx->cur_style = c4m_add_lower_case(ctx->cur_style);
            return;
        case 9:
            ctx->cur_style = c4m_add_upper_case(ctx->cur_style);
            return;
        case 10:
            ctx->cur_style = c4m_add_title_case(ctx->cur_style);
            return;
        default:
            return;
        }
    }
}

static void
reapply_styles(style_ctx *ctx, uint32_t flags, c4m_list_t *to_apply)
{
    int op_kind = flags & (F_STYLE_CELL | F_STYLE_COLOR);

    while (c4m_list_len(to_apply)) {
        tag_item_t *top = c4m_list_pop(to_apply);

        // If we've popped a color or cell type, we don't want to
        // re-apply any later adds for color or cell.  But if color is
        // set, we only listen if BGCOLOR flags are the same.

        if (top->flags & op_kind) {
            if (!((top->flags & F_BGCOLOR) ^ (flags & F_BGCOLOR))) {
                continue;
            }
        }

        apply_one_atom(ctx, top, top->flags & OP_EXTRACT);
    }
}

// Take the parsed styles in the string so far, and figure out
// what style to emit.
static void
convert_parse_to_style(style_ctx *ctx)
{
    tag_item_t *tag_atom;
    int         n = c4m_list_len(ctx->style_directions);
    int         op_kind;

    ctx->cur_style = 0;
    ctx->stack     = alloca(sizeof(tag_item_t **) * n);
    ctx->stack_ix  = 0;

    for (int i = 0; i < n; i++) {
        tag_atom = c4m_list_get(ctx->style_directions, i, NULL);
        // We reuse these atoms, so reset this flag the first time we see it.
        tag_atom->flags &= ~F_POPPED;

        op_kind = tag_atom->flags & OP_EXTRACT;

        if (tag_atom->flags & F_NEG) {
            if (!tag_atom->name) {
                ctx->cur_style = 0;
                ctx->stack_ix  = 0;
                continue;
            }

            c4m_list_t *we_popped = c4m_new(c4m_type_list(c4m_type_ref()));

            while (true) {
                if (!ctx->stack_ix) {
                    c4m_utf8_t *err = c4m_cstr_format(
                        "In the format string [i]{}[/], "
                        "there is no active element named '{}' to close; "
                        "either it wasn't turned on, or was already turned "
                        "off by an later overriding style in this literal.",
                        ctx->raw,
                        tag_atom->name);
                    C4M_RAISE(err);
                }
                tag_item_t *top = ctx->stack[--ctx->stack_ix];
                if (top->flags & (F_POPPED | F_NEG)) { // Already popped
                    continue;
                }

                top->flags |= F_POPPED;

                if (strcmp(top->name->data, tag_atom->name->data)) {
                    c4m_list_append(we_popped, top);
                    continue;
                }

                ctx->cur_style = top->prev_style;

                reapply_styles(ctx, tag_atom->flags, we_popped);
                break;
            }
            continue;
        }

        // NOT a negative.

        apply_one_atom(ctx, tag_atom, op_kind);
    }

    ctx->cur_frame->style = ctx->cur_style;
}

c4m_utf8_t *
c4m_rich_lit(char *instr)
{
    style_ctx ctx = {
        .raw = c4m_new_utf8(instr),
    };

    // Phase 1, find all the style blocks.
    c4m_extract_style_blocks(&ctx, instr);

    c4m_utf8_t *result = ctx.style_text;

    // If style blobs, parse them. (otherwise, return the whole string).
    if (ctx.start_frame == NULL) {
        return c4m_new(c4m_type_utf8(), c4m_kw("cstring", c4m_ka(instr)));
    }

    int num_styles = 0;
    while (ctx.cur_frame != NULL) {
        parse_style_lit(&ctx);
        convert_parse_to_style(&ctx);

        if (ctx.cur_frame->style != 0) {
            num_styles++;
        }

        ctx.cur_frame = ctx.cur_frame->next;
    }

    if (!num_styles) {
        return result;
    }

    // Final phase, apply the styles.
    c4m_alloc_styles(result, num_styles);

    int i         = 0;
    ctx.cur_frame = ctx.start_frame;

    while (ctx.cur_frame != NULL) {
        if (ctx.cur_frame->style != 0) {
            if (ctx.cur_frame->end == -1) {
                ctx.cur_frame->end = c4m_str_byte_len(result);
            }
            c4m_style_entry_t entry = {
                .start = ctx.cur_frame->start,
                .end   = ctx.cur_frame->end,
                .info  = ctx.cur_frame->style,
            };

            result->styling->styles[i] = entry;
            i++;
        }
        ctx.cur_frame = ctx.cur_frame->next;
    }

    return result;
}

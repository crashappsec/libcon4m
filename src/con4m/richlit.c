#include "con4m.h"

typedef struct fmt_frame_t {
    int32_t             absolute_start;
    int32_t             absolute_end;
    int32_t             cp_start;
    int32_t             cp_end;
    style_t             style;
    struct fmt_frame_t *next;
} fmt_frame_t;

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

#include "static/richlit.c"

static dict_t *style_keywords = NULL;

static inline void
init_style_keywords()
{
    if (style_keywords == NULL) {
        buffer_t *b = c4m_new(c4m_tspec_buffer(),
                              c4m_kw("raw",
                                     c4m_ka(_marshaled_style_keywords),
                                     "length",
                                     c4m_ka(1426)));
        stream_t *s = c4m_new(c4m_tspec_stream(), c4m_kw("buffer", c4m_ka(b)));

        c4m_gc_register_root(&style_keywords, 1);
        style_keywords = c4m_unmarshal(s);
    }
}

// Not using zero to make the 'miss' code easier.

#define F_NEG    (1 << 1)
#define F_BOLD   (1 << 2)
#define F_ITALIC (1 << 3)
#define F_STRIKE (1 << 4)
#define F_U      (1 << 5)
#define F_UU     (1 << 6)
#define F_REV    (1 << 7)
#define F_TCASE  (1 << 8)
#define F_LCASE  (1 << 9)
#define F_UCASE  (1 << 10)
#define F_ON     (1 << 11)
#define F_CUR    (1 << 12)

static void
parse_style_lit(fmt_frame_t *f, utf8_t *instr)
{
    uint64_t        seen         = 0;
    utf8_t         *space        = c4m_get_space_const();
    xlist_t        *parts        = c4m_str_xsplit(instr, space);
    int             len          = c4m_xlist_len(parts);
    int             color_start  = -1;
    style_t         result       = f->style;
    bool            saw_fg       = false;
    bool            on_ok        = false;
    bool            expect_color = false; // Used after the 'on' keyword.
    bool            color_done   = false;
    int64_t         l;                    // used for a string length.
    int64_t         n;                    // Used to count how many styles a string will end up w
    utf8_t         *s;
    render_style_t *rs;
    int             i; // Loop variable needs to survive after loop.

    init_style_keywords();

    if (f->next != NULL) {
        f->cp_end = f->next->cp_start;
    }
    else {
        f->cp_end = -1;
    }

    if (len == 1) {
        s  = c4m_xlist_get(parts, 0, NULL);
        rs = c4m_lookup_cell_style((c4m_to_utf8(s))->data);

        if (rs != NULL) {
            f->style = c4m_str_style(rs);
            if (f->next != NULL) {
                f->next->style = f->style;
            }
            return;
        }

        l = c4m_str_codepoint_len(s);
        switch (l) {
        case 0:
            C4M_CRAISE("Empty style block not allowed.");
        case 1:
            if (s->data[0] == '/') {
                f->style = 0;
                return;
            }
            break;
        default:
            if (s->data[0] == '/') {
                seen |= F_NEG;
                s = c4m_str_slice(s, 1, -1);

                rs = c4m_lookup_cell_style(s->data);
                if (rs != NULL) {
                    // [/style] is not different from [/] since we are
                    // not keeping a stack. This doesn't need to be
                    // styled forward either.
                    f->style = 0;
                    return;
                }
                if (len > 1) {
                    goto skip_first_load;
                }
            }
            else {
                // Special case 'default' by itself to
                // reset any accumulated style.
                if (!strcmp(s->data, "default")) {
                    f->style = 0;
                    return;
                }
            }
            break;
        }
    }

    for (i = 0; i < len; i++) {
        s = c4m_to_utf8(c4m_xlist_get(parts, i, NULL));
        l = c4m_str_codepoint_len(s);

        if (i == 0 && l != 0) {
            if (s->data[0] == '/') {
                seen |= F_NEG;
                if (l == 1) {
                    continue;
                }
                s = c4m_to_utf8(c4m_str_slice(s, 1, -1));
                c4m_xlist_set(parts, i, s);
            }
        }

skip_first_load:

        n = (int64_t)hatrack_dict_get(style_keywords, s, NULL);

        if (n == 0) {
            if (color_start == -1) {
                if (color_done) {
                    C4M_RAISE(c4m_str_concat(
                        c4m_new_utf8("Invalid element in style block: "),
                        s));
                }
                color_start = i;
            }
            continue;
        }
        else {
            if (expect_color && color_start == -1) {
                C4M_CRAISE("Expected a color after the 'on' keyword.");
            }

            on_ok = false;

            if (color_start != -1) {
check_color: {
    // When we exit the loop, if color_start is -1, then
    // we jump back up here to reuse the code, then
    // jump back down to where we calculate the style.

    xlist_t *slice = c4m_xlist_get_slice(parts, color_start, i);
    utf8_t  *cname = c4m_to_utf8(c4m_str_join(slice, space));
    color_t  color = c4m_lookup_color(cname);

    if (color == -1) {
        C4M_RAISE(c4m_str_concat(c4m_new_utf8("Color not found: "),
                                 cname));
    }

    color_start = -1;

    if (saw_fg) {
        color_done = true;
        result     = c4m_set_bg_color(result, color);
    }

    else {
        saw_fg = true;
        on_ok  = true;
        result = c4m_set_fg_color(result, color);
    }
}

                f->style = result;

                if (i == len) {
                    if (f->next != NULL) {
                        f->next->style = result;
                    }
                    return;
                }
            }
            if (seen & F_NEG) {
                switch (n) {
                case 1:
                    C4M_CRAISE("Double negation in one style tag.");
                case 2:
                    result = c4m_remove_bold(result);
                    break;
                case 3:
                    result = c4m_remove_italic(result);
                    break;
                case 4:
                    result = c4m_remove_strikethrough(result);
                    break;
                case 5:
                case 6:
                    result = c4m_remove_underline(result);
                    break;
                case 7:
                    result = c4m_remove_inverse(result);
                    break;
                case 8:
                case 9:
                case 10:
                    result = c4m_remove_case(result);
                    break;
                case 11:
                    C4M_CRAISE(
                        "Use the 'on' keyword to set color, not "
                        "clear it.");
                case 12:
                    result = c4m_remove_fg_color(result);
                    break;
                case 13:
                    result = c4m_remove_bg_color(result);
                    break;
                case 14:
                    result = c4m_remove_all_color(result);
                    break;
                }
            }
            else {
                switch (n) {
                case 1:
                    break; // F_NEG will get set below.
                case 2:
                    result = c4m_add_bold(result);
                    break;
                case 3:
                    result = c4m_add_italic(result);
                    break;
                case 4:
                    result = c4m_add_strikethrough(result);
                    break;
                case 5:
                    result = c4m_add_underline(result);
                    break;
                case 6:
                    result = c4m_add_double_underline(result);
                    break;
                case 7:
                    result = c4m_add_inverse(result);
                    break;
                case 8:
                    result = c4m_add_lower_case(result);
                    break;
                case 9:
                    result = c4m_add_upper_case(result);
                    break;
                case 10:
                    result = c4m_add_title_case(result);
                    break;
                case 11:
                    if (!on_ok) {
                        C4M_CRAISE(
                            "'on' keyword in style tag must appear after "
                            "a valid color.");
                    }
                    on_ok        = false;
                    expect_color = true;
                    break;
                default:
                    C4M_RAISE(
                        c4m_str_concat(s,
                                       c4m_new_utf8(
                                           ": style keyword is for "
                                           "turning off colors. Either "
                                           "add a / to the block before "
                                           "this keyword, or the "
                                           "word 'no'. ")));
                }
            }

            if (saw_fg && !on_ok && !expect_color) {
                color_done = true;
            }

            n = 1 << n;

            if (seen & n) {
                C4M_RAISE(c4m_str_concat(
                    c4m_new_utf8("Duplicate param in style tag: "),
                    s));
            }

            seen |= n;
            continue;
        }
    }

    if (color_start != -1) {
        goto check_color;
    }

    // Whatever style we end up with is still in effect until the
    // end of the string unless explicitly turned off.
    // So pay it forward to the next marked region, which can
    // add and/or subtract from it.
    f->style = result;

    if (f->next != NULL) {
        f->next->style = f->style;
    }
    return;
}

utf8_t *
c4m_rich_lit(char *instr)
{
    buffer_t    *b = c4m_new(c4m_tspec_buffer(), c4m_kw("length", c4m_ka(1)));
    stream_t    *s = c4m_new(c4m_tspec_stream(),
                          c4m_kw("buffer",
                                 c4m_ka(b),
                                 "write",
                                 c4m_ka(1),
                                 "read",
                                 c4m_ka(0)));
    fmt_frame_t *style_next;
    fmt_frame_t *style_top = NULL;
    fmt_frame_t *style_cur = NULL;
    char        *p         = instr;
    char        *end       = p + strlen(instr);
    int          cp_count  = 0;
    int          i         = 0;
    codepoint_t  cp;
    int          one_len;

    // Phase 1, find all the style blocks.
    while (p < end) {
        one_len = utf8proc_iterate((uint8_t *)p, 4, &cp);
        if (one_len < 0) {
            C4M_RAISE(c4m_str_from_int(-1 * ((int64_t)(p - one_len) + 1)));
        }

        switch (cp) {
        case '\\':
            p += one_len;

            if (p >= end) {
                C4M_CRAISE("Last character was an escape (not allowed.");
            }
            one_len = utf8proc_iterate((uint8_t *)p, 4, &cp);
            if (one_len < 0) {
                C4M_RAISE(c4m_str_from_int(-1 * ((int64_t)(p - one_len) + 1)));
            }

            switch (cp) {
            case 'n':
                c4m_stream_putc(s, '\n');
                break;
            case 'r':
                c4m_stream_putc(s, '\r');
                break;
            case 't':
                c4m_stream_putc(s, '\t');
                break;
            default:
                c4m_stream_raw_write(s, one_len, p);
                break;
            }

            p += one_len;
            break;
        case '[':
            p += one_len;
            style_next = (fmt_frame_t *)alloca(sizeof(fmt_frame_t));

            // Zero out the fields we might not set.
            style_next->absolute_end = 0;
            style_next->style        = 0;
            style_next->next         = NULL;

            if (style_top == NULL) {
                style_top = style_next;
            }
            else {
                style_cur->next = style_next;
            }
            style_cur = style_next;

            style_cur->cp_start       = cp_count;
            style_cur->absolute_start = p - instr;

            while (p < end) {
                one_len = utf8proc_iterate((uint8_t *)p, 4, &cp);
                if (one_len < 0) {
                    C4M_RAISE(
                        c4m_str_from_int(-1 * ((int64_t)(p - one_len) + 1)));
                }
                p += one_len;
                if (cp == ']') {
                    goto not_eof;
                }
            }
            C4M_CRAISE("EOF in style marker");

not_eof:
            style_cur->absolute_end = p - instr - 1;
            continue; // do not update the cp count.
        default:
            c4m_stream_raw_write(s, one_len, p);
            p += one_len;
            break;
        }

        cp_count += 1;
    }

    c4m_stream_close(s);

    utf8_t *result = c4m_buffer_to_utf8_string(b);

    // If style blobs, parse them. (otherwise, return the whole string).
    if (style_top == NULL) {
        return c4m_new(c4m_tspec_utf8(), c4m_kw("cstring", c4m_ka(instr)));
    }

    fmt_frame_t *f          = style_top;
    int          num_styles = 0;
    while (f != NULL) {
        utf8_t *s = c4m_new(
            c4m_tspec_utf8(),
            c4m_kw("cstring",
                   c4m_ka(instr + f->absolute_start),
                   "length",
                   c4m_ka(f->absolute_end - f->absolute_start)));
        parse_style_lit(f, s);

        if (f->style != 0) {
            num_styles++;
        }

        f = f->next;
    }

    if (!num_styles) {
        return result;
    }

    // Final phase, apply the styles.
    c4m_alloc_styles(result, num_styles);

    i = 0;
    f = style_top;

    while (f != NULL) {
        if (f->style != 0) {
            if (f->cp_end == -1) {
                f->cp_end = cp_count;
            }
            style_entry_t entry = {
                .start = f->cp_start,
                .end   = f->cp_end,
                .info  = f->style};

            result->styling->styles[i] = entry;
            i++;
        }
        f = f->next;
    }

    return result;
}

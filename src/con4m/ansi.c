#include <con4m.h>

static inline bool
ignore_for_printing(codepoint_t cp)
{
    // This prevents things like the terminal ANSI code escape
    // indicator from being printed when processing user data for
    // terminal outputs.  If the user does add in ANSI sequences, the
    // contents of the sequence minus the escape get treated as actual
    // text. This de-fanging seems best, so that the API can fully
    // control rendering as intended.

    switch (cp) {
    case CP_CATEGORY_CN:
    case CP_CATEGORY_CC:
    case CP_CATEGORY_CF:
    case CP_CATEGORY_CS:
    case CP_CATEGORY_CO:
        if (cp == '\n') {
            return false;
        }
        return true;
    default:
        return false;
    }
}

static inline int
internal_char_render_width(codepoint_t cp)
{
    if (ignore_for_printing(cp)) {
        return 0;
    }
    return utf8proc_charwidth(cp);
}

static void
ansi_render_style_start(uint64_t info, stream_t *outstream)
{
    uint64_t remaining = (~FLAG_MASK) & info;

    if (!info) {
        return;
    }

    stream_putc(outstream, '\e');
    stream_putc(outstream, '[');
    if (info & BOLD_ON) {
        remaining &= ~BOLD_ON;
        stream_putc(outstream, '1');
        if (remaining) {
            stream_putc(outstream, ';');
        }
    }
    if (info & INV_ON) {
        remaining &= ~INV_ON;
        stream_putc(outstream, '7');
        if (remaining) {
            stream_putc(outstream, ';');
        }
    }
    if (info & ST_ON) {
        remaining &= ~ST_ON;
        stream_putc(outstream, '9');
        if (remaining) {
            stream_putc(outstream, ';');
        }
    }
    if (info & ITALIC_ON) {
        remaining &= ~ITALIC_ON;
        stream_putc(outstream, '3');
        if (remaining) {
            stream_putc(outstream, ';');
        }
    }
    if (info & UL_ON) {
        remaining &= ~UL_ON;
        stream_putc(outstream, '4');
        if (remaining) {
            stream_putc(outstream, ';');
        }
    }
    if (info & UL_DOUBLE) {
        remaining &= ~UL_DOUBLE;
        stream_puts(outstream, "21");
        if (remaining) {
            stream_putc(outstream, ';');
        }
    }

    if (info & FG_COLOR_ON) {
        remaining &= ~FG_COLOR_ON;

        if (use_truecolor()) {
            uint8_t r = (uint8_t)((info & ~FG_COLOR_MASK) >> OFFSET_FG_RED);
            uint8_t g = (uint8_t)((info & ~FG_COLOR_MASK) >> OFFSET_FG_GREEN);
            uint8_t b = (uint8_t)((info & ~FG_COLOR_MASK) >> OFFSET_FG_BLUE);
            stream_puts(outstream, "38;2;");
            stream_puti(outstream, r);
            stream_putc(outstream, ';');
            stream_puti(outstream, g);
            stream_putc(outstream, ';');
            stream_puti(outstream, b);
        }
        else {
            stream_puts(outstream, "38;5;");
            stream_puti(outstream, to_vga((int32_t)(info & ~(FG_COLOR_MASK))));
        }
        if (remaining) {
            stream_putc(outstream, ';');
        }
    }

    if (info & BG_COLOR_ON) {
        info &= ~BG_COLOR_ON;

        if (use_truecolor()) {
            uint8_t r = (uint8_t)((info & ~BG_COLOR_MASK) >> OFFSET_BG_RED);
            uint8_t g = (uint8_t)((info & ~BG_COLOR_MASK) >> OFFSET_BG_GREEN);
            uint8_t b = (uint8_t)((info & ~BG_COLOR_MASK) >> OFFSET_BG_BLUE);
            stream_puts(outstream, "48;2;");
            stream_puti(outstream, r);
            stream_putc(outstream, ';');
            stream_puti(outstream, g);
            stream_putc(outstream, ';');
            stream_puti(outstream, b);
        }
        else {
            stream_puts(outstream, "38;5;");
            stream_puti(outstream,
                        to_vga((int32_t)(info & ~(BG_COLOR_MASK) >> OFFSET_BG_BLUE)));
        }
    }
    stream_putc(outstream, 'm');
}

static inline void
ansi_render_style_end(stream_t *outstream)
{
    stream_putc(outstream, '\e');
    stream_putc(outstream, '[');
    stream_putc(outstream, '0');
    stream_putc(outstream, 'm');
}

static inline void
ansi_render_style_final(stream_t *outstream)
{
    stream_puts(outstream, "\e[0m\e[K");
    stream_flush(outstream);
}

static inline void
ansi_render_one_codepoint_plain(codepoint_t cp, stream_t *outstream)
{
    uint8_t tmp[4];
    int     len;

    if (ignore_for_printing(cp)) {
        return;
    }

    len = utf8proc_encode_char(cp, tmp);
    stream_raw_write(outstream, len, (char *)tmp);
}

static inline void
ansi_render_one_codepoint_lower(codepoint_t cp, stream_t *outstream)
{
    ansi_render_one_codepoint_plain(utf8proc_tolower(cp), outstream);
}

static inline void
ansi_render_one_codepoint_upper(codepoint_t cp, stream_t *outstream)
{
    ansi_render_one_codepoint_plain(utf8proc_toupper(cp), outstream);
}

static inline bool
ansi_render_one_codepoint_title(codepoint_t cp, bool go_up, stream_t *outstream)
{
    if (go_up) {
        ansi_render_one_codepoint_upper(cp, outstream);
        return 0;
    }
    ansi_render_one_codepoint_plain(cp, outstream);
    return codepoint_is_space(cp);
}

void
utf8_ansi_render(const utf8_t *s, stream_t *outstream)
{
    if (!s) {
        return;
    }

    style_t        default_style = get_default_style();
    style_t        current_style = default_style;
    uint64_t       casing        = current_style & TITLE_CASE;
    int32_t        cp_ix         = 0;
    int32_t        cp_stop       = 0;
    uint32_t       style_ix      = 0;
    u8_state_t     style_state   = U8_STATE_START_DEFAULT;
    uint8_t       *p             = (uint8_t *)s->data;
    uint8_t       *end           = p + s->codepoints;
    style_entry_t *entry         = NULL;
    bool           case_up       = true;
    codepoint_t    codepoint;

    style_state = U8_STATE_START_DEFAULT;

    if (s->styling != NULL && ((uint64_t)s->styling != STYLE_INVALID) && s->styling->num_entries != 0) {
        entry = &s->styling->styles[0];
        if (entry->start == 0) {
            style_state = U8_STATE_START_STYLE;
        }
        else {
            cp_stop = entry->start;
        }
    }
    else {
        cp_stop = s->codepoints;
    }

    while (p < end) {
        switch (style_state) {
        case U8_STATE_START_DEFAULT:
            if (current_style != 0) {
                ansi_render_style_end(outstream);
            }

            current_style = default_style;
            casing        = current_style & TITLE_CASE;
            case_up       = true;

            if (entry != NULL) {
                cp_stop = entry->start;
            }
            else {
                cp_stop = s->codepoints;
            }

            ansi_render_style_start(current_style, outstream);

            style_state = U8_STATE_DEFAULT_STYLE;
            continue;

        case U8_STATE_START_STYLE:
            current_style = entry->info;
            casing        = current_style & TITLE_CASE;
            cp_stop       = entry->end;
            case_up       = true;

            ansi_render_style_start(current_style, outstream);
            style_state = U8_STATE_IN_STYLE;
            continue;

        case U8_STATE_DEFAULT_STYLE:
            if (cp_ix == cp_stop) {
                if (current_style != 0) {
                    ansi_render_style_end(outstream);
                }
                if (entry != NULL) {
                    style_state = U8_STATE_START_STYLE;
                }
                else {
                    break;
                }
                continue;
            }
            break;

        case U8_STATE_IN_STYLE:
            if (cp_ix == cp_stop) {
                if (current_style != 0) {
                    ansi_render_style_end(outstream);
                }
                style_ix += 1;
                if (style_ix == s->styling->num_entries) {
                    entry       = NULL;
                    style_state = U8_STATE_START_DEFAULT;
                }
                else {
                    entry = &s->styling->styles[style_ix];
                    if (cp_ix == entry->start) {
                        style_state = U8_STATE_START_STYLE;
                    }
                    else {
                        style_state = U8_STATE_START_DEFAULT;
                    }
                }
                continue;
            }
            break;
        }

        int tmp = utf8proc_iterate(p, 4, &codepoint);
        assert(tmp > 0);
        p += tmp;
        cp_ix += 1;

        switch (casing) {
        case UPPER_CASE:
            ansi_render_one_codepoint_upper(codepoint, outstream);
            break;
        case LOWER_CASE:
            ansi_render_one_codepoint_lower(codepoint, outstream);
            break;
        case TITLE_CASE:
            case_up = ansi_render_one_codepoint_title(codepoint, case_up, outstream);
            break;
        default:
            ansi_render_one_codepoint_plain(codepoint, outstream);
            break;
        }
    }

    ansi_render_style_final(outstream);
}

// This will have to convert characters to utf-8, since terminals
// generally do not support other encodings.
static void
ansi_render_u32_region(const utf32_t *s, int32_t from, int32_t to, style_t style, stream_t *outstream)
{
    codepoint_t *p   = (codepoint_t *)(s->data);
    bool         cap = true;

    if (style != 0) {
        ansi_render_style_start(style, outstream);
    }

    switch (style & TITLE_CASE) {
    case UPPER_CASE:
        for (int32_t i = from; i < to; i++) {
            ansi_render_one_codepoint_upper(p[i], outstream);
        }
        break;
    case LOWER_CASE:
        for (int32_t i = from; i < to; i++) {
            ansi_render_one_codepoint_lower(p[i], outstream);
        }
        break;
    case TITLE_CASE:
        for (int32_t i = from; i < to; i++) {
            cap = ansi_render_one_codepoint_title(p[i], cap, outstream);
        }
        break;
    default:
        for (int32_t i = from; i < to; i++) {
            ansi_render_one_codepoint_plain(p[i], outstream);
        }
        break;
    }

    ansi_render_style_final(outstream);
}

void
utf32_ansi_render(const utf32_t *s, int32_t start_ix, int32_t end_ix, stream_t *outstream)
{
    if (!s) {
        return;
    }

    int32_t len    = string_codepoint_len(s);
    style_t style0 = get_default_style();

    if (start_ix < 0) {
        start_ix += len;
    }
    if (end_ix <= 0) {
        end_ix += len;
    }

    if (s->styling == NULL || (uint64_t)s->styling == STYLE_INVALID) {
        ansi_render_u32_region(s, start_ix, end_ix, style0, outstream);
        return;
    }

    int32_t cur        = start_ix;
    int     num_styles = s->styling->num_entries;

    for (int i = 0; i < num_styles; i++) {
        int32_t ss = s->styling->styles[i].start;
        int32_t se = s->styling->styles[i].end;

        if (se <= cur) {
            continue;
        }

        if (ss > cur) {
            ansi_render_u32_region(s, cur, min(ss, end_ix), style0, outstream);
            cur = ss;
        }

        if (ss <= cur && se >= cur) {
            int32_t stopat = min(se, end_ix);
            ansi_render_u32_region(s, cur, stopat, s->styling->styles[i].info, outstream);
            cur = stopat;
        }

        if (cur == end_ix) {
            return;
        }
    }

    if (cur != end_ix) {
        ansi_render_u32_region(s, cur, end_ix, style0, outstream);
    }
}

void
ansi_render(const any_str_t *s, stream_t *out)
{
    if (!s) {
        return;
    }

    if (string_is_u32(s)) {
        utf32_ansi_render(s, 0, 0, out);
    }
    else {
        utf8_ansi_render(s, out);
    }
}

void
ansi_render_to_width(const any_str_t *s, int32_t width, int32_t hang, stream_t *out)
{
    if (!s) {
        return;
    }

    utf32_t *as_u32 = force_utf32(s);
    int32_t  i;

    if (width <= 0) {
        width = 20;
    }

    break_info_t *line_starts = wrap_text(as_u32, width, hang);

    for (i = 0; i < line_starts->num_breaks - 1; i++) {
        utf32_ansi_render(as_u32, line_starts->breaks[i], line_starts->breaks[i + 1], out);
        stream_putc(out, '\n');
    }

    if (i == line_starts->num_breaks - 1) {
        utf32_ansi_render(as_u32, line_starts->breaks[i], string_codepoint_len(as_u32), out);
    }
}

static inline size_t
internal_render_len_u32(const utf32_t *s)
{
    codepoint_t *p     = (codepoint_t *)s->data;
    int32_t      len   = string_codepoint_len(s);
    size_t       count = 0;

    for (int i = 0; i < len; i++) {
        count += internal_char_render_width(p[i]);
    }

    return count;
}

static inline size_t
internal_render_len_u8(const utf8_t *s)
{
    uint8_t    *p   = (uint8_t *)s->data;
    uint8_t    *end = p + s->byte_len;
    codepoint_t cp;
    size_t      count = 0;

    while (p < end) {
        p += utf8proc_iterate(p, 4, &cp);
        count += internal_char_render_width(cp);
    }

    return count;
}

size_t
ansi_render_len(const any_str_t *s)
{
    if (!s) {
        return 0;
    }

    if (string_is_u32(s)) {
        return internal_render_len_u32(s);
    }
    else {
        return internal_render_len_u8(s);
    }
}

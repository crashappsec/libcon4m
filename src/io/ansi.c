#include "con4m.h"

static inline bool
ignore_for_printing(c4m_codepoint_t cp)
{
    // This prevents things like the terminal ANSI code escape
    // indicator from being printed when processing user data for
    // terminal outputs.  If the user does add in ANSI sequences, the
    // contents of the sequence minus the escape get treated as actual
    // text. This de-fanging seems best, so that the API can fully
    // control rendering as intended.

    switch (cp) {
    case UTF8PROC_CATEGORY_CN:
    case UTF8PROC_CATEGORY_CC:
    case UTF8PROC_CATEGORY_CF:
    case UTF8PROC_CATEGORY_CS:
    case UTF8PROC_CATEGORY_CO:
        if (cp == '\n') {
            return false;
        }
        return true;
    default:
        return false;
    }
}

static inline int
internal_char_render_width(c4m_codepoint_t cp)
{
    if (ignore_for_printing(cp)) {
        return 0;
    }
    return utf8proc_charwidth(cp);
}

static void
ansi_render_style_start(uint64_t info, c4m_stream_t *outstream)
{
    uint64_t remaining = (~C4M_STY_CLEAR_FLAGS) & info;

    if (!info) {
        return;
    }

    c4m_stream_putc(outstream, '\e');
    c4m_stream_putc(outstream, '[');
    if (info & C4M_STY_BOLD) {
        remaining &= ~C4M_STY_BOLD;
        c4m_stream_putc(outstream, '1');
        if (remaining) {
            c4m_stream_putc(outstream, ';');
        }
    }
    if (info & C4M_STY_REV) {
        remaining &= ~C4M_STY_REV;
        c4m_stream_putc(outstream, '7');
        if (remaining) {
            c4m_stream_putc(outstream, ';');
        }
    }
    if (info & C4M_STY_ST) {
        remaining &= ~C4M_STY_ST;
        c4m_stream_putc(outstream, '9');
        if (remaining) {
            c4m_stream_putc(outstream, ';');
        }
    }
    if (info & C4M_STY_ITALIC) {
        remaining &= ~C4M_STY_ITALIC;
        c4m_stream_putc(outstream, '3');
        if (remaining) {
            c4m_stream_putc(outstream, ';');
        }
    }
    if (info & C4M_STY_UL) {
        remaining &= ~C4M_STY_UL;
        c4m_stream_putc(outstream, '4');
        if (remaining) {
            c4m_stream_putc(outstream, ';');
        }
    }
    if (info & C4M_STY_UUL) {
        remaining &= ~C4M_STY_UUL;
        c4m_stream_puts(outstream, "21");
        if (remaining) {
            c4m_stream_putc(outstream, ';');
        }
    }

    if (info & C4M_STY_FG) {
        remaining &= ~C4M_STY_FG;

        if (c4m_use_truecolor()) {
            uint8_t r = (uint8_t)((info & ~C4M_STY_CLEAR_FG) >> C4M_OFFSET_FG_RED);
            uint8_t g = (uint8_t)((info & ~C4M_STY_CLEAR_FG) >> C4M_OFFSET_FG_GREEN);
            uint8_t b = (uint8_t)((info & ~C4M_STY_CLEAR_FG) >> C4M_OFFSET_FG_BLUE);
            c4m_stream_puts(outstream, "38;2;");
            c4m_stream_puti(outstream, r);
            c4m_stream_putc(outstream, ';');
            c4m_stream_puti(outstream, g);
            c4m_stream_putc(outstream, ';');
            c4m_stream_puti(outstream, b);
        }
        else {
            c4m_stream_puts(outstream, "38;5;");
            int32_t color = (int32_t)(info & ~(C4M_STY_CLEAR_FG));
            c4m_stream_puti(outstream, c4m_to_vga(color));
        }
        if (remaining) {
            c4m_stream_putc(outstream, ';');
        }
    }

    if (info & C4M_STY_BG) {
        info &= ~C4M_STY_BG;

        if (c4m_use_truecolor()) {
            uint8_t r = (uint8_t)((info & ~C4M_STY_CLEAR_BG) >> C4M_OFFSET_BG_RED);
            uint8_t g = (uint8_t)((info & ~C4M_STY_CLEAR_BG) >> C4M_OFFSET_BG_GREEN);
            uint8_t b = (uint8_t)((info & ~C4M_STY_CLEAR_BG) >> C4M_OFFSET_BG_BLUE);
            c4m_stream_puts(outstream, "48;2;");
            c4m_stream_puti(outstream, r);
            c4m_stream_putc(outstream, ';');
            c4m_stream_puti(outstream, g);
            c4m_stream_putc(outstream, ';');
            c4m_stream_puti(outstream, b);
        }
        else {
            c4m_stream_puts(outstream, "38;5;");
            int32_t toand = (int32_t) ~(C4M_STY_CLEAR_BG) >> C4M_OFFSET_BG_BLUE;
            c4m_stream_puti(outstream, c4m_to_vga(info & toand));
        }
    }
    c4m_stream_putc(outstream, 'm');
}

static inline void
ansi_render_style_end(c4m_stream_t *outstream)
{
    c4m_stream_putc(outstream, '\e');
    c4m_stream_putc(outstream, '[');
    c4m_stream_putc(outstream, '0');
    c4m_stream_putc(outstream, 'm');
}

static inline void
ansi_render_style_final(c4m_stream_t *outstream)
{
    c4m_stream_puts(outstream, "\e[0m\e[K");
    c4m_stream_flush(outstream);
}

static inline void
ansi_render_one_codepoint_plain(c4m_codepoint_t cp, c4m_stream_t *outstream)
{
    uint8_t tmp[4];
    int     len;

    if (ignore_for_printing(cp)) {
        return;
    }

    len = utf8proc_encode_char(cp, tmp);
    c4m_stream_raw_write(outstream, len, (char *)tmp);
}

static inline void
ansi_render_one_codepoint_lower(c4m_codepoint_t cp, c4m_stream_t *outstream)
{
    ansi_render_one_codepoint_plain(utf8proc_tolower(cp), outstream);
}

static inline void
ansi_render_one_codepoint_upper(c4m_codepoint_t cp, c4m_stream_t *outstream)
{
    ansi_render_one_codepoint_plain(utf8proc_toupper(cp), outstream);
}

static inline bool
ansi_render_one_c4m_codepoint_title(c4m_codepoint_t cp, bool go_up, c4m_stream_t *outstream)
{
    if (go_up) {
        ansi_render_one_codepoint_upper(cp, outstream);
        return 0;
    }
    ansi_render_one_codepoint_plain(cp, outstream);
    return c4m_codepoint_is_space(cp);
}

void
c4m_utf8_ansi_render(const c4m_utf8_t *s, c4m_stream_t *outstream)
{
    if (!s) {
        return;
    }

    c4m_style_t        default_style = c4m_get_default_style();
    c4m_style_t        current_style = default_style;
    uint64_t           casing        = current_style & C4M_STY_TITLE;
    int32_t            cp_ix         = 0;
    int32_t            cp_stop       = 0;
    uint32_t           style_ix      = 0;
    c4m_u8_state_t     style_state   = C4M_U8_STATE_START_DEFAULT;
    uint8_t           *p             = (uint8_t *)s->data;
    uint8_t           *end           = p + s->codepoints;
    c4m_style_entry_t *entry         = NULL;
    bool               case_up       = true;
    c4m_codepoint_t    codepoint;

    style_state = C4M_U8_STATE_START_DEFAULT;

    if (s->styling != NULL && ((uint64_t)s->styling != C4M_STY_BAD) && s->styling->num_entries != 0) {
        entry = &s->styling->styles[0];
        if (entry->start == 0) {
            style_state = C4M_U8_STATE_START_STYLE;
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
        case C4M_U8_STATE_START_DEFAULT:
            if (current_style != 0) {
                ansi_render_style_end(outstream);
            }

            current_style = default_style;
            casing        = current_style & C4M_STY_TITLE;
            case_up       = true;

            if (entry != NULL) {
                cp_stop = entry->start;
            }
            else {
                cp_stop = s->codepoints;
            }

            ansi_render_style_start(current_style, outstream);

            style_state = C4M_U8_STATE_DEFAULT_STYLE;
            continue;

        case C4M_U8_STATE_START_STYLE:
            current_style = entry->info;
            casing        = current_style & C4M_STY_TITLE;
            cp_stop       = entry->end;
            case_up       = true;

            ansi_render_style_start(current_style, outstream);
            style_state = C4M_U8_STATE_IN_STYLE;
            continue;

        case C4M_U8_STATE_DEFAULT_STYLE:
            if (cp_ix == cp_stop) {
                if (current_style != 0) {
                    ansi_render_style_end(outstream);
                }
                if (entry != NULL) {
                    style_state = C4M_U8_STATE_START_STYLE;
                }
                else {
                    break;
                }
                continue;
            }
            break;

        case C4M_U8_STATE_IN_STYLE:
            if (cp_ix == cp_stop) {
                if (current_style != 0) {
                    ansi_render_style_end(outstream);
                }
                style_ix += 1;
                if (style_ix == s->styling->num_entries) {
                    entry       = NULL;
                    style_state = C4M_U8_STATE_START_DEFAULT;
                }
                else {
                    entry = &s->styling->styles[style_ix];
                    if (cp_ix == entry->start) {
                        style_state = C4M_U8_STATE_START_STYLE;
                    }
                    else {
                        style_state = C4M_U8_STATE_START_DEFAULT;
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
        case C4M_STY_UPPER:
            ansi_render_one_codepoint_upper(codepoint, outstream);
            break;
        case C4M_STY_LOWER:
            ansi_render_one_codepoint_lower(codepoint, outstream);
            break;
        case C4M_STY_TITLE:
            case_up = ansi_render_one_c4m_codepoint_title(codepoint, case_up, outstream);
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
ansi_render_u32_region(const c4m_utf32_t *s,
                       int32_t            from,
                       int32_t            to,
                       c4m_style_t        style,
                       c4m_stream_t      *outstream)
{
    c4m_codepoint_t *p   = (c4m_codepoint_t *)(s->data);
    bool             cap = true;

    if (style != 0) {
        ansi_render_style_start(style, outstream);
    }

    switch (style & C4M_STY_TITLE) {
    case C4M_STY_UPPER:
        for (int32_t i = from; i < to; i++) {
            ansi_render_one_codepoint_upper(p[i], outstream);
        }
        break;
    case C4M_STY_LOWER:
        for (int32_t i = from; i < to; i++) {
            ansi_render_one_codepoint_lower(p[i], outstream);
        }
        break;
    case C4M_STY_TITLE:
        for (int32_t i = from; i < to; i++) {
            cap = ansi_render_one_c4m_codepoint_title(p[i], cap, outstream);
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
c4m_utf32_ansi_render(const c4m_utf32_t *s,
                      int32_t            start_ix,
                      int32_t            end_ix,
                      c4m_stream_t      *outstream)
{
    if (!s) {
        return;
    }

    int32_t     len    = c4m_str_codepoint_len(s);
    c4m_style_t style0 = c4m_get_default_style();

    if (start_ix < 0) {
        start_ix += len;
    }
    if (end_ix <= 0) {
        end_ix += len;
    }

    if (s->styling == NULL || (uint64_t)s->styling == C4M_STY_BAD) {
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
            ansi_render_u32_region(s, cur, c4m_min(ss, end_ix), style0, outstream);
            cur = ss;
        }

        if (ss <= cur && se >= cur) {
            int32_t stopat = c4m_min(se, end_ix);
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
c4m_ansi_render(const c4m_str_t *s, c4m_stream_t *out)
{
    if (!s) {
        return;
    }

    if (c4m_str_is_u32(s)) {
        c4m_utf32_ansi_render(s, 0, 0, out);
    }
    else {
        c4m_utf8_ansi_render(s, out);
    }
}

static int32_t
internal_truncate_to_width(c4m_utf32_t *s,
                           int32_t      end,
                           int32_t      width)
{
    int i = 0;

    c4m_codepoint_t *cp = (c4m_codepoint_t *)s->data;

    for (i = 0; i < end; i++) {
        width -= c4m_codepoint_width(*cp++);

        if (width < 0) {
            return i;
        }
    }

    return end;
}

void
c4m_ansi_render_to_width(const c4m_str_t *s,
                         int32_t          width,
                         int32_t          hang,
                         c4m_stream_t    *out)
{
    if (!s) {
        return;
    }

    c4m_utf32_t *as_u32 = c4m_to_utf32(s);
    int32_t      i;

    c4m_list_t *lines = c4m_str_split(as_u32, c4m_get_newline_const());
    int         n     = c4m_list_len(lines);

    if (width <= 0) {
        width = C4M_MIN_RENDER_WIDTH;
    }

    for (i = 0; i < n; i++) {
        c4m_utf32_t *line = c4m_list_get(lines, i, NULL);
        int32_t      end  = c4m_str_codepoint_len(line);

        if (end > width) {
            end = internal_truncate_to_width(line, end, width);
        }
        c4m_utf32_ansi_render(line, 0, end, out);
        if (i + 1 == n) {
            break;
        }
        c4m_stream_putcp(out, '\n');
    }
}

static inline size_t
internal_render_len_u32(const c4m_utf32_t *s)
{
    c4m_codepoint_t *p     = (c4m_codepoint_t *)s->data;
    int32_t          len   = c4m_str_codepoint_len(s);
    size_t           count = 0;

    for (int i = 0; i < len; i++) {
        count += internal_char_render_width(p[i]);
    }

    return count;
}

static inline size_t
internal_render_len_u8(const c4m_utf8_t *s)
{
    uint8_t        *p   = (uint8_t *)s->data;
    uint8_t        *end = p + s->byte_len;
    c4m_codepoint_t cp;
    size_t          count = 0;

    while (p < end) {
        p += utf8proc_iterate(p, 4, &cp);
        count += internal_char_render_width(cp);
    }

    return count;
}

size_t
c4m_ansi_render_len(const c4m_str_t *s)
{
    if (!s) {
        return 0;
    }

    if (c4m_str_is_u32(s)) {
        return internal_render_len_u32(s);
    }
    else {
        return internal_render_len_u8(s);
    }
}

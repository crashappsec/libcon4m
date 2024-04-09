#include "con4m.h"

style_t default_style = 0;

style_t
c4m_apply_bg_color(style_t style, utf8_t *name)
{
    int64_t color = (int64_t)c4m_lookup_color(name);

    if (color & ~0xffffffUL) {
        return style;
    }

    return (style & C4M_STY_CLEAR_BG) | (color << 24) | C4M_STY_BG;
}

style_t
c4m_apply_fg_color(style_t style, utf8_t *name)
{
    int64_t color = (int64_t)c4m_lookup_color(name);

    if (color & ~0xffffffUL) {
        return style;
    }

    return (style & C4M_STY_CLEAR_FG) | color | C4M_STY_FG;
}
void
c4m_style_gaps(any_str_t *s, style_t gapstyle)
{
    if (!s->styling || !s->styling->num_entries) {
        c4m_str_apply_style(s, gapstyle, 0);
        return;
    }

    int num_gaps = 0;
    int last_end = 0;
    int num_cp   = c4m_str_codepoint_len(s);

    for (int i = 0; i < s->styling->num_entries; i++) {
        style_entry_t style = s->styling->styles[i];
        if (style.start > last_end) {
            num_gaps++;
        }
        last_end = style.end;
    }
    if (num_cp > last_end) {
        num_gaps++;
    }

    if (!num_gaps) {
        return;
    }
    style_info_t *old    = s->styling;
    int           new_ix = 0;

    c4m_alloc_styles(s, old->num_entries + num_gaps);

    last_end = 0;

    for (int i = 0; i < old->num_entries; i++) {
        style_entry_t style = s->styling->styles[i];

        if (style.start > last_end) {
            style_entry_t filler = {
                .start = last_end,
                .end   = style.start,
                .info  = gapstyle};

            s->styling->styles[new_ix++] = filler;
        }
        s->styling->styles[new_ix++] = old->styles[i];
        last_end                     = old->styles[i].end;
    }
    if (last_end != num_cp) {
        style_entry_t filler = {
            .start = last_end,
            .end   = num_cp,
            .info  = gapstyle};

        s->styling->styles[new_ix] = filler;
    }
}

void
c4m_str_layer_style(any_str_t *s, style_t additions, style_t subtractions)
{
    if (!s->styling || !s->styling->num_entries) {
        c4m_str_set_style(s, additions);
        return;
    }

    style_t turn_off = ~(subtractions & ~C4M_STY_CLEAR_FLAGS);

    if (additions & C4M_STY_FG) {
        turn_off |= ~C4M_STY_CLEAR_FG;
    }
    if (additions & C4M_STY_BG) {
        turn_off |= ~C4M_STY_CLEAR_BG;
    }

    for (int i = 0; i < s->styling->num_entries; i++) {
        s->styling->styles[i].info &= turn_off;
        s->styling->styles[i].info |= additions;
    }
}

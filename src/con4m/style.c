#include <con4m.h>

style_t default_style = 0;

style_t
apply_bg_color(style_t style, utf8_t *name)
{
    int64_t color = (int64_t)lookup_color(name);

    if (color & ~0xffffffUL) {
        return style;
    }

    return (style & BG_COLOR_MASK) | (color << 24) | BG_COLOR_ON;
}

style_t
apply_fg_color(style_t style, utf8_t *name)
{
    int64_t color = (int64_t)lookup_color(name);

    if (color & ~0xffffffUL) {
        return style;
    }

    return (style & FG_COLOR_MASK) | color | FG_COLOR_ON;
}
void
style_gaps(any_str_t *s, style_t gapstyle)
{
    if (!s->styling || !s->styling->num_entries) {
        string_apply_style(s, gapstyle, 0);
        return;
    }

    int num_gaps = 0;
    int last_end = 0;
    int num_cp   = string_codepoint_len(s);

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

    alloc_styles(s, old->num_entries + num_gaps);

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
string_layer_style(any_str_t *s, style_t additions, style_t subtractions)
{
    if (!s->styling || !s->styling->num_entries) {
        string_set_style(s, additions);
        return;
    }

    style_t turn_off = ~(subtractions & ~FLAG_MASK);

    if (additions & FG_COLOR_ON) {
        turn_off |= ~FG_COLOR_MASK;
    }
    if (additions & BG_COLOR_ON) {
        turn_off |= ~BG_COLOR_MASK;
    }

    for (int i = 0; i < s->styling->num_entries; i++) {
        s->styling->styles[i].info &= turn_off;
        s->styling->styles[i].info |= additions;
    }
}

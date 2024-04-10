#include "con4m.h"
#include "./static/colors.c"

static c4m_dict_t *color_table = NULL;

static inline c4m_dict_t *
get_color_table()
{
    if (color_table == NULL) {
        c4m_buf_t    *b = c4m_new(c4m_tspec_buffer(),
                               c4m_kw("raw",
                                      c4m_ka(_marshaled_color_table),
                                      "length",
                                      c4m_ka(44237)));
        c4m_stream_t *s = c4m_new(c4m_tspec_stream(),
                                  c4m_kw("buffer", c4m_ka(b)));
        c4m_gc_register_root(&color_table, 1);
        color_table = c4m_unmarshal(s);
    }
    return color_table;
}

c4m_color_t
c4m_lookup_color(c4m_utf8_t *name)
{
    bool        found  = false;
    c4m_color_t result = (c4m_color_t)(int64_t)hatrack_dict_get(
        get_color_table(),
        name,
        &found);

    if (found == false) {
        return -1;
    }

    return result;
}

c4m_color_t
c4m_to_vga(c4m_color_t truecolor)
{
    c4m_color_t r = (truecolor >> 16) & 0xff;
    c4m_color_t g = (truecolor >> 8) & 0xff;
    c4m_color_t b = (truecolor >> 0) & 0xff;

    // clang-format off
    return ((c4m_color_t)(r * 7 / 255) << 5) |
	((c4m_color_t)(g * 7 / 255) << 2) |
	((c4m_color_t)(b * 3 / 255));
    // clang-format on
}

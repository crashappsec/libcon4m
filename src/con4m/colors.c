#include <con4m.h>

#include "./static/colors.c"

static dict_t * color_table = NULL;

static inline dict_t *
get_color_table()
{
    if (color_table == NULL) {
	buffer_t *b = con4m_new(tspec_buffer(),
				kw("raw", ka(_marshaled_color_table),
				   "length", ka(44237)));
        stream_t *s = con4m_new(tspec_stream(), kw("buffer", ka(b)));
                con4m_gc_register_root(&color_table, 1);
        color_table = con4m_unmarshal(s);
    }
    return color_table;
}

color_t
lookup_color(utf8_t *name) {
    bool    found  = false;
    color_t result = (color_t)(int64_t)hatrack_dict_get(get_color_table(),
							name, &found);

    if (found == false) {
	return -1;
    }

    return result;
}

color_t
to_vga(color_t truecolor)
{
    color_t r = (truecolor >> 16) & 0xff;
    color_t g = (truecolor >> 8)  & 0xff;
    color_t b = (truecolor >> 0)  & 0xff;

    return ((color_t)(r * 7 / 255) << 5) |
	   ((color_t)(g * 7 / 255) << 2) |
           ((color_t)(b * 3 / 255));
}

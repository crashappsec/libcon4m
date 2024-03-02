#include <con4m.h>


style_t default_style = 0;

void
set_default_style(style_t s)
{
    default_style = s;
}

style_t
get_default_style()
{
    return default_style;
}

style_t
new_style()
{
    return (uint64_t)0;
}

style_t
add_bold(style_t style)
{
    return style | BOLD_ON;
}

style_t
remove_bold(style_t style)
{
    return style & ~BOLD_ON;
}

style_t
add_inverse(style_t style)
{
    return style | INV_ON;
}

style_t
remove_inverse(style_t style)
{
    return style & ~INV_ON;
}

style_t
add_strikethrough(style_t style)
{
    return style | ST_ON;
}

style_t
remove_strikethrough(style_t style)
{
    return style & ~ST_ON;
}

style_t
add_italic(style_t style)
{
    return style | ITALIC_ON;
}

style_t
remove_italic(style_t style)
{
    return style & ~ITALIC_ON;
}

style_t
add_underline(style_t style)
{
    return (style | UL_ON) & ~UL_DOUBLE;
}

style_t
add_double_underline(style_t style)
{
    return (style | UL_DOUBLE) & ~UL_ON;
}

style_t
remove_underline(style_t style)
{
    return style & ~(UL_ON | UL_DOUBLE);
}

style_t
add_bg_color(style_t style, uint8_t red, uint8_t green, uint8_t blue)
{
    return (style & BG_COLOR_MASK) | BG_COLOR_ON |
	((uint64_t)red)   << OFFSET_BG_RED |
	((uint64_t)green) << OFFSET_BG_GREEN |
	((uint64_t)blue)  << OFFSET_BG_BLUE;
}

style_t
add_fg_color(style_t style, uint8_t red, uint8_t green, uint8_t blue)
{
    return (style & FG_COLOR_MASK) | FG_COLOR_ON |
	((uint64_t)red)   << OFFSET_FG_RED |
	((uint64_t)green) << OFFSET_FG_GREEN |
	((uint64_t)blue)  << OFFSET_FG_BLUE;
}

style_t
apply_bg_color(style_t style, color_t c)
{
    color_info_t info = color_data[c];
    return add_bg_color(style, info.rgb.r, info.rgb.g, info.rgb.b);
}

style_t
apply_fg_color(style_t style, color_t c)
{
    color_info_t info = color_data[c];
    return add_fg_color(style, info.rgb.r, info.rgb.g, info.rgb.b);
}

style_t
add_upper_case(style_t style)
{
    return (style & ~TITLE_CASE) | UPPER_CASE;
}

style_t
add_lower_case(style_t style)
{
    return (style & ~TITLE_CASE) | LOWER_CASE;
}

style_t
add_title_case(style_t style)
{
    return style | TITLE_CASE;
}

style_t
remove_case(style_t style)
{
    return style & ~TITLE_CASE;
}

style_t
remove_bg_color(style_t style)
{
    return style & (BG_COLOR_MASK & ~BG_COLOR_ON);
}

style_t
remove_fg_color(style_t style)
{
    return style & (FG_COLOR_MASK & ~FG_COLOR_ON);
}

style_t
remove_all_color(style_t style)
{
    // This should mainly constant fold down to a single AND.
    return style & (FG_COLOR_MASK & BG_COLOR_MASK &
		    ~(FG_COLOR_ON | BG_COLOR_ON));
}

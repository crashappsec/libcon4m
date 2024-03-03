#include <con4m.h>

void
test1() {
    style_t style1;
    style_t style2;

    style1 = new_style();
    style1 = apply_bg_color(style1, COLOR_BLACK);
    style1 = add_title_case(style1);
    style2 = apply_fg_color(style1, COLOR_JAZZBERRY);
    style1 = apply_fg_color(style1, COLOR_ATOMIC_LIME);
    style1 = add_italic(add_underline(style1));
    style2 = add_italic(style2);
    style2 = add_upper_case(style2);


    str_t *s1 = c4str_from_cstr("\ehello,");
    str_t *s2 = c4str_from_cstr(" world!");
    str_t *s3 = c4str_from_cstr(" magic?\n");

    ansi_render(s1, stdout);
    ansi_render(s2, stdout);
    ansi_render(s3, stdout);

    c4str_apply_style(s1, style1);
    s1 = c4str_u8_to_u32(s1);
    c4str_apply_style(s3, style2);
    s2 = c4str_u8_to_u32(s2);
    s3 = c4str_u8_to_u32(s3);

    ansi_render(s1, stdout);
    ansi_render(s2, stdout);
    ansi_render(s3, stdout);

    str_t *s  = c4str_concat(s1, s2);
    s         = c4str_concat(s,  s3);

    ansi_render(s, stdout);

    break_info_t *g;


    g = get_grapheme_breaks(s, 1, 10);

    for (int i = 0; i < g->num_breaks; i++) {
	printf("%d ", g->breaks[i]);
    }

    printf("\n");

    g = get_all_line_break_ops(s);
    for (int i = 0; i < g->num_breaks; i++) {
	printf("%d ", g->breaks[i]);
    }

    printf("\n");

    g = get_line_breaks(s);
    for (int i = 0; i < g->num_breaks; i++) {
	printf("%d ", g->breaks[i]);
    }

    printf("\n");
}

void
test2() {
    style_t style1;
    style_t style2;

    style1 = new_style();
    style1 = apply_fg_color(style1, COLOR_BLACK);
    style1 = add_title_case(style1);
    style2 = apply_fg_color(style1, COLOR_JAZZBERRY);
    style1 = apply_bg_color(style1, COLOR_ATOMIC_LIME);
    style1 = add_italic(add_underline(style1));
    style2 = add_italic(style2);
    style2 = add_upper_case(style2);


    str_t *w1 = c4str_from_cstr("Once upon a time, there was a ");
    str_t *w2 = c4str_from_cstr("thing I cared about. But then I ");
    str_t *w3 = c4str_from_cstr(
	"stopped caring. I don't really remember what it was, though. Do ");
    str_t *w4 = c4str_from_cstr(
	"you? No, I didn't think so, because it wasn't really all that "
	"interesting, to be quite honest. Maybe someday I'll find something "
	"interesting to care about, besides my family. Oh yeah, that's "
	"what it was, my family! Oh, wait, no, they're either not interesting, "
	"or I don't care about them.\n");
    str_t *w5 = c4str_from_cstr("Basically AirTags for Software");
    str_t *w6 = c4str_from_cstr("\n");

    c4str_apply_style(w2, style1);
    c4str_apply_style(w3, style2);
    c4str_apply_style(w5, style1);

    str_t *to_wrap = c4str_concat(w1, w2);
    to_wrap        = c4str_concat(to_wrap, w3);
    to_wrap        = c4str_concat(to_wrap, w4);
    to_wrap        = c4str_concat(to_wrap, w5);
    to_wrap        = c4str_concat(to_wrap, w6);

    real_str_t *real = to_internal(to_wrap);

    str_t *dump1 = hex_dump(real->styling,
                            alloc_style_len(real),
			    (uint64_t)real->styling,
			    80,
			    "Style dump\n");

    str_t *dump2 = hex_dump(real,
			    real_alloc_len(real),
			    (uint64_t)real,
			    80,
			    "String Dump\n");

    ansi_render(dump1, stderr);
    ansi_render(dump2, stderr);

    size_t cols;
    terminal_dimensions(&cols, NULL);
    printf("%d cols\n", cols);

    ansi_render_to_width(to_wrap, cols, 0, stdout);
}

void
test_rand64()
{
    uint64_t random = 0;

    random = con4m_rand64();
    printf("Random value: %16llx\n", random);
    assert(random != 0);
}

int
main(int argc, char **argv, char **envp)
{
    test_rand64();
    test1();
    test2();
}

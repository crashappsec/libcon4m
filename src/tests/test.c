#include <con4m.h>

style_t style1;
style_t style2;
size_t  term_width;

STATIC_ASCII_STR(str_test, "Welcome to the testing center. First order of "
		 "business: This is a static string, stored in static memory."
		 "However, we have not set any styling information on it.\n");

void
test1() {
    style1 = lookup_text_style("h1");
    style2 = lookup_text_style("h2");
    style2 = add_upper_case(style2);


    str_t *s1 = con4m_new(T_STR, "cstring", "\ehello,", "style", style1);
    str_t *s2 = con4m_new(T_STR, "cstring", " world!");
    str_t *s3 = con4m_new(T_STR, "cstring", " magic?\n");

    con4m_gc_register_root(&s1, 1);
    con4m_gc_register_root(&s2, 1);
    con4m_gc_register_root(&s3, 1);

    ansi_render(s1, stdout);
    ansi_render(s2, stdout);
    ansi_render(s3, stdout);

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
    printf("That was at %p\n", s);

    break_info_t *g;

    con4m_gc_register_root(&s, 1);
    con4m_gc_register_root(&g, 1);

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

    con4m_gc_thread_collect();

    printf("s is now at: %p\n Let's render s again.\n", s);
    ansi_render(s, stdout);
}

str_t *
test2() {
    str_t *w1 = con4m_new(T_STR, "cstring", "Once upon a time, there was a ");
    str_t *w2 = con4m_new(T_STR, "cstring", "thing I cared about. But then I ");
    str_t *w3 = con4m_new(T_STR, "cstring",
	"stopped caring. I don't really remember what it was, though. Do ");
    str_t *w4 = con4m_new(T_STR, "cstring",
	"you? No, I didn't think so, because it wasn't really all that "
	"interesting, to be quite honest. Maybe someday I'll find something "
	"interesting to care about, besides my family. Oh yeah, that's "
	"what it was, my family! Oh, wait, no, they're either not interesting, "
	"or I don't care about them.\n");
    str_t *w5 = con4m_new(T_STR, "cstring", "Basically AirTags for Software");
    str_t *w6 = con4m_new(T_STR, "cstring", "\n");


    con4m_gc_register_root(&w1, 1);
    con4m_gc_register_root(&w2, 1);
    con4m_gc_register_root(&w3, 1);
    con4m_gc_register_root(&w4, 1);
    con4m_gc_register_root(&w5, 1);
    con4m_gc_register_root(&w6, 1);

    c4str_apply_style(w2, style1);
    c4str_apply_style(w3, style2);
    c4str_apply_style(w5, style1);

    str_t *to_wrap;

    con4m_gc_register_root(&to_wrap, 1);

    to_wrap        = c4str_concat(w1, w2);
    to_wrap        = c4str_concat(to_wrap, w3);
    to_wrap        = c4str_concat(to_wrap, w4);
    to_wrap        = c4str_concat(to_wrap, w5);
    to_wrap        = c4str_concat(to_wrap, w6);

    real_str_t *real = to_internal(to_wrap);
    con4m_gc_register_root(&real, 1);


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

    con4m_gc_register_root(&dump1, 1);
    con4m_gc_register_root(&dump2, 1);

    ansi_render(dump1, stderr);
    ansi_render(dump2, stderr);

    ansi_render_to_width(to_wrap, term_width, 0, stdout);
    con4m_gc_thread_collect();
    return to_wrap;
}

void
test_rand64()
{
    uint64_t random = 0;

    random = con4m_rand64();
    printf("Random value: %16llx\n", random);
    assert(random != 0);
}

void
test3(str_t *to_slice)
{
    //ansi_render(c4str_slice(to_slice, 10, 50), stdout);
    ansi_render_to_width(c4str_slice(to_slice, 10, 50), term_width, 0, stdout);
    printf("\n");
    ansi_render_to_width(c4str_slice(to_slice, 40, 100), term_width, 0, stdout);
    printf("\n");
}

void
test4()
{
    str_t *w1 = con4m_new(T_STR, "cstring", "Once upon a time, there was a ");
    str_t *w2 = con4m_new(T_STR, "cstring", "thing I cared about. But then I ");
    str_t *w3 = con4m_new(T_STR, "cstring",
	"stopped caring. I don't really remember what it was, though. Do ");
    str_t *w4 = con4m_new(T_STR, "cstring",
	"you? No, I didn't think so, because it wasn't really all that "
	"interesting, to be quite honest. Maybe someday I'll find something "
	"interesting to care about, besides my family. Oh yeah, that's "
	"what it was, my family! Oh, wait, no, they're either not interesting, "
        "or I don't care about them.", "style", style1);
    str_t *w5 = con4m_new(T_STR, "cstring", "\n");
    str_t *w6 = con4m_new(T_STR, "cstring", "Basically AirTags for Software\n");

    con4m_gc_register_root(&w1, 1);
    con4m_gc_register_root(&w2, 1);
    con4m_gc_register_root(&w3, 1);
    con4m_gc_register_root(&w4, 1);
    con4m_gc_register_root(&w5, 1);
    con4m_gc_register_root(&w6, 1);


    hatrack_dict_t *d = con4m_new(T_DICT, HATRACK_DICT_KEY_TYPE_CSTR);

    con4m_gc_register_root(&d, 1);

    hatrack_dict_put(d, w1, "w1");
    hatrack_dict_put(d, w2, "w2");
    hatrack_dict_put(d, w3, "w3");
    hatrack_dict_put(d, w4, "w4");
    hatrack_dict_put(d, w5, "w5");
    hatrack_dict_put(d, w6, "w6");

    uint64_t num;

    hatrack_dict_item_t *view = hatrack_dict_items_sort(d, &num);

    for (uint64_t i = 0; i < num; i++) {
	ansi_render((str_t *)(view[i].key), stderr);
    }

    con4m_gc_thread_collect();
}

void
table_test()
{
    str_t      *test1 = con4m_new(T_STR, "cstring", "Some example??"
	" Let's make it a fairly long example, so it will be sure to need"
	" some reynolds' wrap.");
    str_t      *test2 = con4m_new(T_STR, "cstring", "Some other example.");
    str_t      *test3 = con4m_new(T_STR, "cstring", "Example 3.");
    str_t      *test4 = con4m_new(T_STR, "cstring", "Defaults.");
    str_t      *test5 = con4m_new(T_STR, "cstring", "Last one.");
    grid_t     *g    = con4m_new(T_GRID, "rows", 3, "cols", 3,
	                         "border_theme", "bold_dash2");

    grid_set_cell_contents(g, 0, 0, to_internal(test1));
    grid_set_cell_contents(g, 0, 1, to_internal(test2));
    grid_set_cell_contents(g, 0, 2, to_internal(test4));
    grid_set_cell_contents(g, 1, 2, to_internal(test3));
    grid_set_cell_contents(g, 1, 1, to_internal(test5));
    grid_add_col_span(g, "num_cols", 2, "string", test1);
//    grid_set_cell_contents(g, 1, 0, to_internal(empty_string()));
    c4str_apply_style(test1, style1);
    c4str_apply_style(test2, style2);
    c4str_apply_style(test3, style1);
    c4str_apply_style(test5, style2);
    ansi_render(con4m_value_obj_repr(g), stdout);
}

void
ordered_list_test()
{
    str_t *test1   = con4m_new(T_STR, "cstring",
			       "This is a good point, one that you haven't "
			       "heard before.");
    str_t *test2   = con4m_new(T_STR, "cstring",
			       "This is a point that's just as valid, but you "
			       "already know it.");
    str_t *test3   = con4m_new(T_STR, "cstring", "This is a small point.");
    str_t *test4   = con4m_new(T_STR, "cstring", "Conclusion.");
    flexarray_t *l = con4m_new(T_LIST, "length", 12);

    flexarray_set(l, 0, to_internal(test1));
    flexarray_set(l, 1, to_internal(test2));
    flexarray_set(l, 2, to_internal(test3));
    flexarray_set(l, 3, to_internal(test4));
    flexarray_set(l, 4, to_internal(test1));
    flexarray_set(l, 5, to_internal(test2));
    flexarray_set(l, 6, to_internal(test3));
    flexarray_set(l, 7, to_internal(test4));
    flexarray_set(l, 8, to_internal(test1));
    flexarray_set(l, 9, to_internal(test2));
    flexarray_set(l, 0xa, to_internal(test3));
    flexarray_set(l, 0xb, to_internal(test4));

    printf("\n");
    grid_t      *g = ordered_list(l);
    ansi_render(con4m_value_obj_repr(g), stdout);
    grid_t      *h = unordered_list(l);
    ansi_render(con4m_value_obj_repr(h), stdout);
}

int
main(int argc, char **argv, char **envp)
{

    install_default_styles();
    terminal_dimensions(&term_width, NULL);
    ansi_render_to_width(str_test, term_width, 0, stdout);
    test_rand64();
    // Test basic string and single threaded GC.
    test1();
    style1 = apply_bg_color(style1, "alice blue");
    str_t *to_slice = test2();
    con4m_gc_register_root(&to_slice, 1);
    test3(to_slice);
    to_slice = NULL;
    test4();
    table_test();
    ordered_list_test();
    STATIC_ASCII_STR(local_test, "\nGoodbye!\n");
    ansi_render(local_test, stdout);

    printf("Sample style: %.16llx\n", style1);
}

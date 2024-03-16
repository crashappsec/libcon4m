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


    any_str_t *s1 = con4m_new(T_UTF8, "cstring", "\ehello,", "style", style1);
    any_str_t *s2 = con4m_new(T_UTF8, "cstring", " world!");
    any_str_t *s3 = con4m_new(T_UTF8, "cstring", " magic?\n");

    con4m_gc_register_root(&s1, 1);
    con4m_gc_register_root(&s2, 1);
    con4m_gc_register_root(&s3, 1);

    ansi_render(s1, stdout);
    ansi_render(s2, stdout);
    ansi_render(s3, stdout);

    s1 = force_utf32(s1);
    string_apply_style(s3, style2);
    s2 = force_utf32(s2);
    s3 = force_utf32(s3);

    ansi_render(s1, stdout);
    ansi_render(s2, stdout);
    ansi_render(s3, stdout);

    utf32_t *s  = string_concat(s1, s2);
    s           = string_concat(s,  s3);

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

any_str_t *
test2() {
    utf8_t *w1 = con4m_new(T_UTF8, "cstring", "Once upon a time, there was a ");
    utf8_t *w2 = con4m_new(T_UTF8, "cstring", "thing I cared about. But then ");
    utf8_t *w3 = con4m_new(T_UTF8, "cstring",
	"I stopped caring. I don't really remember what it was, though. Do ");
    utf8_t *w4 = con4m_new(T_UTF8, "cstring",
	"you? No, I didn't think so, because it wasn't really all that "
	"interesting, to be quite honest. Maybe someday I'll find something "
	"interesting to care about, besides my family. Oh yeah, that's "
	"what it was, my family! Oh, wait, no, they're either not interesting, "
	"or I don't care about them.\n");
    utf8_t *w5 = con4m_new(T_UTF8, "cstring", "Basically AirTags for Software");
    utf8_t *w6 = con4m_new(T_UTF8, "cstring", "\n");


    con4m_gc_register_root(&w1, 1);
    con4m_gc_register_root(&w2, 1);
    con4m_gc_register_root(&w3, 1);
    con4m_gc_register_root(&w4, 1);
    con4m_gc_register_root(&w5, 1);
    con4m_gc_register_root(&w6, 1);

    string_apply_style(w2, style1);
    string_apply_style(w3, style2);
    string_apply_style(w5, style1);

    utf32_t *to_wrap;

    con4m_gc_register_root(&to_wrap, 1);

    to_wrap        = string_concat(w1, w2);
    to_wrap        = string_concat(to_wrap, w3);
    to_wrap        = string_concat(to_wrap, w4);
    to_wrap        = string_concat(to_wrap, w5);
    to_wrap        = string_concat(to_wrap, w6);

    utf8_t *dump1 = hex_dump(to_wrap->styling,
			     alloc_style_len(to_wrap),
			     (uint64_t)to_wrap->styling,
			     80,
			     "Style dump\n");

    utf8_t *dump2 = hex_dump(to_wrap,
			     to_wrap->byte_len,
			     (uint64_t)to_wrap,
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
test3(any_str_t *to_slice)
{
    //ansi_render(string_slice(to_slice, 10, 50), stdout);
    ansi_render_to_width(string_slice(to_slice, 10, 50), term_width, 0, stdout);
    printf("\n");
    ansi_render_to_width(string_slice(to_slice, 40, 100), term_width, 0,
			 stdout);
    printf("\n");
}

void
test4()
{
    utf8_t *w1 = con4m_new(T_UTF8, "cstring", "Once upon a time, there was a ");
    utf8_t *w2 = con4m_new(T_UTF8, "cstring", "thing I cared about. But then ");
    utf8_t *w3 = con4m_new(T_UTF8, "cstring",
	"I stopped caring. I don't really remember what it was, though. Do ");
    utf8_t *w4 = con4m_new(T_UTF8, "cstring",
	"you? No, I didn't think so, because it wasn't really all that "
	"interesting, to be quite honest. Maybe someday I'll find something "
	"interesting to care about, besides my family. Oh yeah, that's "
	"what it was, my family! Oh, wait, no, they're either not interesting, "
	"or I don't care about them.\n");
    utf8_t *w5 = con4m_new(T_UTF8, "cstring", "Basically AirTags for Software");
    utf8_t *w6 = con4m_new(T_UTF8, "cstring", "\n");

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
	ansi_render((any_str_t *)(view[i].key), stderr);
    }

    con4m_gc_thread_collect();
}

void
table_test()
{
    utf8_t     *test1 = con4m_new(T_UTF8, "cstring", "Some example 🤯 🤯 🤯"
	" Let's make it a fairly long 🤯 example, so it will be sure to need"
	" some reynolds' wrap.");
    utf8_t     *test2 = con4m_new(T_UTF8, "cstring", "Some other example.");
    utf8_t     *test3 = con4m_new(T_UTF8, "cstring", "Example 3.");
    utf8_t     *test4 = con4m_new(T_UTF8, "cstring", "Defaults.");
    utf8_t     *test5 = con4m_new(T_UTF8, "cstring", "Last one.");
    grid_t     *g     = con4m_new(T_GRID, "start_rows", 4, "start_cols", 3,
				  "header_rows", 1);
    utf8_t     *hdr   = con4m_new(T_UTF8, "cstring", "Yes, this is a table.");


    grid_add_row(g, to_str_renderable(hdr, "td"));
    grid_add_cell(g, test1);
    grid_add_cell(g, test2);
    grid_add_cell(g, test4);
    grid_set_cell_contents(g, 2, 2, test3);
    grid_set_cell_contents(g, 2, 1, test5);
    grid_add_col_span(g, to_str_renderable(test1, "td"),  3, 0, 2);
    grid_set_cell_contents(g, 2, 0, empty_string());
    // If we don't explicitly set this, there can be some render issues
    // when there's not enough room.
    grid_set_cell_contents(g, 3, 2, empty_string());
    string_apply_style(test1, style1);
    string_apply_style(test2, style2);
    string_apply_style(test3, style1);
    string_apply_style(test5, style2);
    con4m_new(T_RENDER_STYLE, "flex_units", 3, "tag", "col1");
    con4m_new(T_RENDER_STYLE, "flex_units", 2, "tag", "col3");
//    con4m_new(T_RENDER_STYLE, "width_pct", 10., "tag", "col1");
//    con4m_new(T_RENDER_STYLE, "width_pct", 30., "tag", "col3");
    apply_column_style(g, 0, "col1");
    apply_column_style(g, 2, "col3");

    // Ordered / unordered lists.
    utf8_t *ol1    = con4m_new(T_UTF8, "cstring",
			       "This is a good point, one that you haven't "
			       "heard before.");
    utf8_t *ol2    = con4m_new(T_UTF8, "cstring",
			       "This is a point that's just as valid, but you "
			       "already know it.");
    utf8_t *ol3    = con4m_new(T_UTF8, "cstring", "This is a small point.");
    utf8_t  *ol4   = con4m_new(T_UTF8, "cstring", "Conclusion.");
    flexarray_t *l = con4m_new(T_LIST, "length", 12);

    flexarray_set(l, 0, ol1);
    flexarray_set(l, 1, ol2);
    flexarray_set(l, 2, ol3);
    flexarray_set(l, 3, ol4);
    flexarray_set(l, 4, ol1);
    flexarray_set(l, 5, ol2);
    flexarray_set(l, 6, ol3);
    flexarray_set(l, 7, ol4);
    flexarray_set(l, 8, ol1);
    flexarray_set(l, 9, ol2);
    flexarray_set(l, 0xa, ol3);
    flexarray_set(l, 0xb, ol4);

    grid_t *ol   = ordered_list(l);
    grid_t *ul   = unordered_list(l);

    grid_stripe_rows(ol);

    grid_t *flow = grid_flow(3, g, ul, ol);
    grid_add_cell(flow, test1);
    ansi_render(con4m_value_obj_repr(flow), stdout);
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
    //style1 = apply_bg_color(style1, "alice blue");
    any_str_t *to_slice = test2();
    con4m_gc_register_root(&to_slice, 1);
    test3(to_slice);
    to_slice = NULL;
    test4();
    table_test();
    STATIC_ASCII_STR(local_test, "\nGoodbye!\n");
    ansi_render(local_test, stdout);

    printf("Sample style: %.16llx\n", style1);
}

#define C4M_USE_INTERNAL_API
#include "con4m.h"

c4m_style_t style1;
c4m_style_t style2;
size_t      term_width;

C4M_STATIC_ASCII_STR(str_test,
                     "Welcome to the testing center. First order of "
                     "business: This is a static string, stored in static "
                     "memory. However, we have not set any styling "
                     "information on it.\n");
c4m_stream_t *sout;
c4m_stream_t *serr;

void
test1()
{
    style1 = c4m_lookup_text_style("h1");
    style2 = c4m_lookup_text_style("h2");
    style2 = c4m_add_upper_case(style2);

    c4m_str_t *s1 = c4m_new(c4m_tspec_utf8(),
                            c4m_kw("cstring",
                                   c4m_ka("\ehello,"),
                                   "style",
                                   c4m_ka(style1)));

    c4m_str_t *s2 = c4m_new(c4m_tspec_utf8(),
                            c4m_kw("cstring", c4m_ka(" world!")));
    c4m_str_t *s3 = c4m_new(c4m_tspec_utf8(),
                            c4m_kw("cstring", c4m_ka(" magic?\n")));

    // c4m_gc_register_root(&s1, 1);
    // c4m_gc_register_root(&s2, 1);
    // c4m_gc_register_root(&s3, 1);

    c4m_ansi_render(s1, sout);
    c4m_ansi_render(s2, sout);
    c4m_ansi_render(s3, sout);

    s1 = c4m_to_utf32(s1);
    c4m_str_set_style(s3, style2);
    s2 = c4m_to_utf32(s2);
    s3 = c4m_to_utf32(s3);

    c4m_ansi_render(s1, sout);
    c4m_ansi_render(s2, sout);
    c4m_ansi_render(s3, sout);

    c4m_utf32_t *s = c4m_str_concat(s1, s2);
    s              = c4m_str_concat(s, s3);

    c4m_ansi_render(s, sout);
    printf("That was at %p\n", s);

    c4m_break_info_t *g;

    // c4m_gc_register_root(&s, 1);
    // c4m_gc_register_root(&g, 1);

    g = c4m_get_grapheme_breaks(s, 1, 10);

    for (int i = 0; i < g->num_breaks; i++) {
        printf("%d ", g->breaks[i]);
    }

    printf("\n");

    g = c4m_get_all_line_break_ops(s);
    for (int i = 0; i < g->num_breaks; i++) {
        printf("%d ", g->breaks[i]);
    }

    printf("\n");

    g = c4m_get_line_breaks(s);
    for (int i = 0; i < g->num_breaks; i++) {
        printf("%d ", g->breaks[i]);
    }

    printf("\n");

    c4m_gc_thread_collect();

    printf("s is now at: %p\n Let's render s again.\n", s);
    c4m_ansi_render(s, sout);
}

c4m_str_t *
test2()
{
    c4m_utf8_t *w1 = c4m_new(
        c4m_tspec_utf8(),
        c4m_kw("cstring",
               c4m_ka("Once upon a time, there was a ")));
    c4m_utf8_t *w2 = c4m_new(
        c4m_tspec_utf8(),
        c4m_kw("cstring",
               c4m_ka("thing I cared about. But then ")));
    c4m_utf8_t *w3 = c4m_new(
        c4m_tspec_utf8(),
        c4m_kw("cstring",
               c4m_ka("I stopped caring. I don't really "
                      "remember what it was, though. Do ")));
    c4m_utf8_t *w4 = c4m_new(
        c4m_tspec_utf8(),
        c4m_kw("cstring",
               c4m_ka("you? No, I didn't think so, because it "
                      "wasn't really all that "
                      "interesting, to be quite honest. "
                      "Maybe someday I'll find something "
                      "interesting to care about, besides my "
                      "family. Oh yeah, that's "
                      "what it was, my family! Oh, wait, no, "
                      "they're either not interesting, "
                      "or I don't care about them.\n")));
    c4m_utf8_t *w5 = c4m_new(
        c4m_tspec_utf8(),
        c4m_kw("cstring",
               c4m_ka("Basically AirTags for Software")));
    c4m_utf8_t *w6 = c4m_new(
        c4m_tspec_utf8(),
        c4m_kw("cstring", c4m_ka("\n")));

    c4m_str_set_style(w2, style1);
    c4m_str_set_style(w3, style2);
    c4m_str_set_style(w5, style1);

    c4m_utf32_t *to_wrap;

    to_wrap = c4m_str_concat(w1, w2);
    to_wrap = c4m_str_concat(to_wrap, w3);
    to_wrap = c4m_str_concat(to_wrap, w4);
    to_wrap = c4m_str_concat(to_wrap, w5);
    to_wrap = c4m_str_concat(to_wrap, w6);

    c4m_utf8_t *dump1 = c4m_hex_dump(to_wrap->styling,
                                     c4m_alloc_style_len(to_wrap),
                                     c4m_kw("start_offset",
                                            c4m_ka(to_wrap->styling),
                                            "width",
                                            c4m_ka(80),
                                            "prefix",
                                            c4m_ka("Style dump\n")));

    c4m_utf8_t *dump2 = c4m_hex_dump(to_wrap,
                                     to_wrap->byte_len,
                                     c4m_kw("start_offset",
                                            c4m_ka((uint64_t)to_wrap),
                                            "width",
                                            c4m_ka(80),
                                            "prefix",
                                            c4m_ka("String Dump\n")));

    c4m_ansi_render(dump1, serr);
    c4m_ansi_render(dump2, serr);

    c4m_ansi_render_to_width(to_wrap, term_width, 0, sout);
    c4m_gc_thread_collect();
    return to_wrap;
}

void
test_rand64()
{
    uint64_t random = 0;

    random = c4m_rand64();
    printf("Random value: %16llx\n", (unsigned long long)random);
    assert(random != 0);
}

void
test3(c4m_str_t *to_slice)
{
    // c4m_ansi_render(c4m_str_slice(to_slice, 10, 50), sout);
    c4m_ansi_render_to_width(c4m_str_slice(to_slice, 10, 50),
                             term_width,
                             0,
                             sout);
    printf("\n");
    c4m_ansi_render_to_width(c4m_str_slice(to_slice, 40, 100),
                             term_width,
                             0,
                             sout);
    printf("\n");
}

void
test4()
{
    c4m_utf8_t *w1 = c4m_new(
        c4m_tspec_utf8(),
        c4m_kw("cstring",
               c4m_ka("Once upon a time, there was a ")));
    c4m_utf8_t *w2 = c4m_new(
        c4m_tspec_utf8(),
        c4m_kw("cstring",
               c4m_ka("thing I cared about. But then ")));
    c4m_utf8_t *w3 = c4m_new(
        c4m_tspec_utf8(),
        c4m_kw("cstring",
               c4m_ka("I stopped caring. I don't really "
                      "remember what it was, though. Do ")));
    c4m_utf8_t *w4 = c4m_new(
        c4m_tspec_utf8(),
        c4m_kw("cstring",
               c4m_ka("you? No, I didn't think so, because it "
                      "wasn't really all that interesting, to "
                      "be quite honest. Maybe someday I'll "
                      "find something interesting to care "
                      "about, besides my family. Oh yeah, "
                      "that's what it was, my family! Oh, "
                      "wait, no, they're either not "
                      "interesting, or I don't care about "
                      "them.\n")));
    c4m_utf8_t *w5 = c4m_new(c4m_tspec_utf8(),
                             c4m_kw("cstring",
                                    c4m_ka("Basically AirTags for Software")));
    c4m_utf8_t *w6 = c4m_new(c4m_tspec_utf8(), c4m_kw("cstring", c4m_ka("\n")));

    c4m_dict_t *d = c4m_new(c4m_tspec_dict(c4m_tspec_utf8(),
                                           c4m_tspec_ref()));

    c4m_gc_register_root(&d, 1);

    hatrack_dict_put(d, w1, "w1");
    hatrack_dict_put(d, w2, "w2");
    hatrack_dict_put(d, w3, "w3");
    hatrack_dict_put(d, w4, "w4");
    hatrack_dict_put(d, w5, "w5");
    hatrack_dict_put(d, w6, "w6");

    uint64_t num;

    hatrack_dict_item_t *view = hatrack_dict_items_sort(d, &num);

    for (uint64_t i = 0; i < num; i++) {
        c4m_ansi_render((c4m_str_t *)(view[i].key), serr);
    }

    c4m_gc_thread_collect();
}

void
table_test()
{
    c4m_utf8_t *test1 = c4m_new(c4m_tspec_utf8(),
                                c4m_kw("cstring",
                                       c4m_ka("Some example 🤯 🤯 🤯"
                                              " Let's make it a fairly "
                                              "long 🤯 example, so it will "
                                              "be sure to need some reynolds' "
                                              "wrap.")));
    c4m_utf8_t *test2 = c4m_new(c4m_tspec_utf8(),
                                c4m_kw("cstring",
                                       c4m_ka("Some other example.")));
    c4m_utf8_t *test3 = c4m_new(c4m_tspec_utf8(),
                                c4m_kw("cstring",
                                       c4m_ka("Example 3.")));
    c4m_utf8_t *test4 = c4m_new(c4m_tspec_utf8(),
                                c4m_kw("cstring",
                                       c4m_ka("Defaults.")));
    c4m_utf8_t *test5 = c4m_new(c4m_tspec_utf8(),
                                c4m_kw("cstring",
                                       c4m_ka("Last one.")));
    c4m_grid_t *g     = c4m_new(c4m_tspec_grid(),
                            c4m_kw("start_rows",
                                   c4m_ka(4),
                                   "start_cols",
                                   c4m_ka(3),
                                   "header_rows",
                                   c4m_ka(1)));
    c4m_utf8_t *hdr   = c4m_new(c4m_tspec_utf8(),
                              c4m_kw("cstring",
                                     c4m_ka("Yes, this is a table.")));

    c4m_grid_add_row(g, c4m_to_str_renderable(hdr, "td"));
    c4m_grid_add_cell(g, test1);
    c4m_grid_add_cell(g, test2);
    c4m_grid_add_cell(g, test4);
    c4m_grid_set_cell_contents(g, 2, 2, test3);
    c4m_grid_set_cell_contents(g, 2, 1, test5);
    c4m_grid_add_col_span(g, c4m_to_str_renderable(test1, "td"), 3, 0, 2);
    c4m_grid_set_cell_contents(g, 2, 0, c4m_empty_string());
    // If we don't explicitly set this, there can be some render issues
    // when there's not enough room.
    c4m_grid_set_cell_contents(g, 3, 2, c4m_empty_string());
    c4m_str_set_style(test1, style1);
    c4m_str_set_style(test2, style2);
    c4m_str_set_style(test3, style1);
    c4m_str_set_style(test5, style2);
    c4m_new(c4m_tspec_render_style(),
            c4m_kw("flex_units", c4m_ka(3), "tag", c4m_ka("col1")));
    c4m_new(c4m_tspec_render_style(),
            c4m_kw("flex_units", c4m_ka(2), "tag", c4m_ka("col3")));
    //    c4m_new(c4m_tspec_render_style(), "width_pct", 10., "tag", "col1");
    //    c4m_new(c4m_tspec_render_style(), "width_pct", 30., "tag", "col3");
    c4m_set_column_style(g, 0, "col1");
    c4m_set_column_style(g, 2, "col3");

    // Ordered / unordered lists.
    c4m_utf8_t  *ol1 = c4m_new(c4m_tspec_utf8(),
                              c4m_kw("cstring",
                                     c4m_ka("This is a good point, one that you "
                                             "haven't heard before.")));
    c4m_utf8_t  *ol2 = c4m_new(c4m_tspec_utf8(),
                              c4m_kw("cstring",
                                     c4m_ka("This is a point that's also valid,"
                                             " but you already know it.")));
    c4m_utf8_t  *ol3 = c4m_new(c4m_tspec_utf8(),
                              c4m_kw("cstring",
                                     c4m_ka("This is a small point.")));
    c4m_utf8_t  *ol4 = c4m_new(c4m_tspec_utf8(),
                              c4m_kw("cstring", c4m_ka("Conclusion.")));
    flexarray_t *l   = c4m_new(c4m_tspec_list(c4m_tspec_utf8()),
                             c4m_kw("length", c4m_ka(12)));

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

    c4m_grid_t *ol = c4m_ordered_list(l);
    c4m_grid_t *ul = c4m_unordered_list(l);

    c4m_grid_stripe_rows(ol);

    c4m_grid_t *flow = c4m_grid_flow(3, g, ul, ol);
    c4m_grid_add_cell(flow, test1);
    c4m_ansi_render(c4m_value_obj_repr(flow), sout);
}

void
sha_test()
{
    c4m_utf8_t *test1 = c4m_new(c4m_tspec_utf8(),
                                c4m_kw("cstring",
                                       c4m_ka("Some example 🤯 🤯 🤯"
                                              " Let's make it a fairly long 🤯 "
                                              "example, so it will be sure to need"
                                              " some reynolds' wrap.")));

    c4m_sha_t *ctx = c4m_new(c4m_tspec_hash());
    c4m_sha_string_update(ctx, test1);
    c4m_buf_t *b = c4m_sha_finish(ctx);

    printf("Sha256 is: ");
    c4m_ansi_render(c4m_value_obj_repr(b), sout);
    printf("\n");
}

void
type_tests()
{
    c4m_type_t *t1 = c4m_tspec_int();
    c4m_type_t *t2 = c4m_tspec_grid();
    c4m_type_t *t3 = c4m_tspec_dict(t1, t2);

    c4m_ansi_render(c4m_value_obj_repr(t3), sout);
    printf("\n");

    c4m_type_t *t4 = c4m_new_typevar(c4m_global_type_env);
    c4m_type_t *t5 = c4m_new_typevar(c4m_global_type_env);
    c4m_type_t *t6 = c4m_tspec_dict(t4, t5);

    c4m_ansi_render(c4m_value_obj_repr(t6), sout);
    printf("\n");
    c4m_ansi_render(c4m_value_obj_repr(c4m_merge_types(t3, t6)), sout);
    printf("\n");
}

void
c4m_stream_tests()
{
    c4m_utf8_t   *n  = c4m_new(c4m_tspec_utf8(),
                            c4m_kw("cstring",
                                   c4m_ka("../meson.build")));
    c4m_stream_t *s1 = c4m_new(c4m_tspec_stream(),
                               c4m_kw("filename", c4m_ka(n)));
    c4m_buf_t    *b  = c4m_new(c4m_tspec_buffer(),
                           c4m_kw("length", c4m_ka(16)));
    c4m_stream_t *s2 = c4m_new(c4m_tspec_stream(),
                               c4m_kw("buffer",
                                      c4m_ka(b),
                                      "write",
                                      c4m_ka(1)));

    c4m_style_t sty = c4m_add_bold(c4m_add_italic(c4m_new_style()));

    while (true) {
        c4m_utf8_t *s = c4m_stream_read(s1, 16);

        if (c4m_len(s) == 0) {
            break;
        }

        c4m_stream_write_object(s2, s);
    }

    c4m_print(c4m_hex_dump(b->data, b->byte_len));
    c4m_utf8_t *s = c4m_buf_to_utf8_string(b);

    c4m_str_set_style(s, sty);
    c4m_print(s);
}

extern c4m_color_info_t color_data[];

void
marshal_test()
{
    c4m_utf8_t   *contents = c4m_new(c4m_tspec_utf8(),
                                   c4m_kw("cstring",
                                          c4m_ka("This is a test of marshal.\n")));
    c4m_buf_t    *b        = c4m_new(c4m_tspec_buffer(),
                           c4m_kw("length", c4m_ka(16)));
    c4m_stream_t *s        = c4m_new(c4m_tspec_stream(),
                              c4m_kw("buffer",
                                     c4m_ka(b),
                                     "write",
                                     c4m_ka(1),
                                     "read",
                                     c4m_ka(0)));

    c4m_marshal(contents, s);
    c4m_stream_close(s);

    s = c4m_new(c4m_tspec_stream(), c4m_kw("buffer", c4m_ka(b)));

    c4m_utf8_t *new_str = c4m_unmarshal(s);

    c4m_ansi_render(new_str, sout);
}

#if 0
// This works, and now is in color.c locally.
#include "/tmp/color.c"

void
marshal_test2()
{
    c4m_dict_t *d = c4m_new(c4m_tspec_dict(c4m_tspec_utf8(), c4m_tspec_int()));
    int n;

    for (n = 0; color_data[n].name != NULL; n++) {
	c4m_utf8_t  *color = c4m_new(c4m_tspec_utf8(),
				   c4m_kw("cstring",
					  c4m_ka(color_data[n].name)));
	int64_t rgb    = (int64_t)color_data[n].rgb;

	hatrack_dict_put(d, color, (void *)rgb);
    }

    printf("Writing test color dictionary to /tmp/color.c\n");
    c4m_dump_c_static_instance_code(d, "color_table",
				c4m_new(c4m_tspec_utf8(),
					  c4m_kw("cstring",
					     c4m_ka("/tmp/color.c"))));

    for (int64_t i = 0; i < n - 1; i++) {
	char   *ckey = color_data[i].name;
	if (ckey == NULL) {
	    continue;
	}
	c4m_utf8_t *key  = c4m_new(c4m_tspec_utf8(),
				   c4m_kw("cstring", karg(ckey)));
	int64_t val  = c4m_lookup_color(key);
	printf("%s: %06llx\n", key->data, val);
    }
}
#endif

void
create_dict_lit()
{
    c4m_dict_t *d = c4m_new(c4m_tspec_dict(c4m_tspec_utf8(), c4m_tspec_int()));

    hatrack_dict_add(d, c4m_new_utf8("no"), (void *)1LLU);
    hatrack_dict_add(d, c4m_new_utf8("b"), (void *)2LLU);
    hatrack_dict_add(d, c4m_new_utf8("bold"), (void *)2LLU);
    hatrack_dict_add(d, c4m_new_utf8("i"), (void *)3LLU);
    hatrack_dict_add(d, c4m_new_utf8("italic"), (void *)3LLU);
    hatrack_dict_add(d, c4m_new_utf8("italics"), (void *)3LLU);
    hatrack_dict_add(d, c4m_new_utf8("st"), (void *)4LLU);
    hatrack_dict_add(d, c4m_new_utf8("strike"), (void *)4LLU);
    hatrack_dict_add(d, c4m_new_utf8("strikethru"), (void *)4LLU);
    hatrack_dict_add(d, c4m_new_utf8("strikethrough"), (void *)4LLU);
    hatrack_dict_add(d, c4m_new_utf8("u"), (void *)5LLU);
    hatrack_dict_add(d, c4m_new_utf8("underline"), (void *)5LLU);
    hatrack_dict_add(d, c4m_new_utf8("uu"), (void *)6LLU);
    hatrack_dict_add(d, c4m_new_utf8("2u"), (void *)6LLU);
    hatrack_dict_add(d, c4m_new_utf8("r"), (void *)7LLU);
    hatrack_dict_add(d, c4m_new_utf8("reverse"), (void *)7LLU);
    hatrack_dict_add(d, c4m_new_utf8("inverse"), (void *)7LLU);
    hatrack_dict_add(d, c4m_new_utf8("invert"), (void *)7LLU);
    hatrack_dict_add(d, c4m_new_utf8("inv"), (void *)7LLU);
    hatrack_dict_add(d, c4m_new_utf8("t"), (void *)8LLU);
    hatrack_dict_add(d, c4m_new_utf8("title"), (void *)8LLU);
    hatrack_dict_add(d, c4m_new_utf8("l"), (void *)9LLU);
    hatrack_dict_add(d, c4m_new_utf8("lower"), (void *)9LLU);
    hatrack_dict_add(d, c4m_new_utf8("up"), (void *)10LLU);
    hatrack_dict_add(d, c4m_new_utf8("upper"), (void *)10LLU);
    hatrack_dict_add(d, c4m_new_utf8("on"), (void *)11LLU);
    hatrack_dict_add(d, c4m_new_utf8("fg"), (void *)12LLU);
    hatrack_dict_add(d, c4m_new_utf8("foreground"), (void *)12LLU);
    hatrack_dict_add(d, c4m_new_utf8("bg"), (void *)13LLU);
    hatrack_dict_add(d, c4m_new_utf8("background"), (void *)13LLU);
    hatrack_dict_add(d, c4m_new_utf8("color"), (void *)14LLU);

    c4m_dump_c_static_instance_code(d,
                                    "style_keywords",
                                    c4m_new_utf8("/tmp/style_keys.c"));
}

void
c4m_rich_lit_test()
{
    c4m_utf8_t *test;

    test = c4m_rich_lit("H[atomic lime]ello, [jazzberry]world!");
    c4m_print(test);

    test = c4m_rich_lit("[atomic lime]Hello, [jazzberry]world[/]!");
    c4m_print(test);

    test = c4m_rich_lit("[atomic lime on jazzberry]Hello, world[/]!");
    c4m_print(test);

    test = c4m_rich_lit("[jazzberry on atomic lime]Hello, world![/]");
    c4m_print(test);

    test = c4m_rich_lit(
        "[bold italic jazzberry on atomic lime]Hello,[/color] "
        "world!");
    c4m_print(test);

    test = c4m_rich_lit(
        "[bold italic jazzberry on atomic lime]Hello,"
        "[/color bold] world!");
    c4m_print(test);

    test = c4m_rich_lit(
        "[bold italic jazzberry on atomic lime]Hello,"
        "[/color bold italic] world!");
    c4m_print(test);

    test = c4m_rich_lit(
        "[bold italic jazzberry on atomic lime]Hello,[/bg bold] "
        "world!");
    c4m_print(test);

    test = c4m_rich_lit(
        "[bold italic u jazzberry on atomic lime]Hello,[/bold] "
        "world!\n\n");
    c4m_print(test);

    test = c4m_rich_lit(
        "[bold italic atomic lime on jazzberry]Hello,[/bold fg] "
        "world!");
    c4m_print(test);

    test = c4m_rich_lit("[h2]Hello, world!");
    c4m_print(test);

    test = c4m_rich_lit("[h2]Hello, [i u]world[/i u], it is me!");
    c4m_print(test);

    c4m_print(test, test, c4m_kw("no_color", c4m_ka(true), "sep", c4m_ka('&')));
}

bool
test_tree_search(int64_t kind_as_64, c4m_tree_node_t *node)
{
    c4m_node_kind_t kind  = (c4m_node_kind_t)(unsigned int)kind_as_64;
    c4m_pnode_t    *pnode = c4m_tree_get_contents(node);

    if (kind == c4m_nt_error) {
        return true;
    }

    return kind == pnode->kind;
}

#if 0
void
test_compiler()
{
    c4m_xlist_t          *files  = c4m_get_program_arguments();
    int64_t               l      = c4m_xlist_len(files);
    c4m_str_t            *mname  = c4m_rich_lit("test1");
    c4m_utf8_t           *joiner = c4m_new_utf8("../tests/");
    c4m_utf8_t           *slash  = c4m_get_slash_const();
    c4m_file_compile_ctx *ctx;

    for (int64_t i = 0; i < l; i++) {
        c4m_utf8_t *fname = c4m_xlist_get(files, i, NULL);
        c4m_utf8_t *path  = c4m_xlist_get(files, i, NULL);

        if (c4m_str_find(fname, slash) == -1) {
            path = c4m_str_concat(joiner, fname);
        }
        else {
            path = fname;
        }

        ctx = new_file_compile_ctx(
            mname,
            c4m_kw("uri_path", c4m_ka(path)));

        c4m_stream_t *stream = c4m_load_code(ctx);
        if (stream == NULL) {
            c4m_print(c4m_cstr_format("[red]error: [/] Couldn't read file: {}",
                                      fname));
            continue;
        }
        if (c4m_lex(ctx, stream) == false) {
            c4m_print(c4m_cstr_format("[red]error: [/] Lex failed for: {}",
                                      fname));
            c4m_print(c4m_format_errors(ctx));
            continue;
        }

        c4m_print(c4m_format_tokens(ctx));
        c4m_parse(ctx);
        c4m_print(c4m_format_parse_tree(ctx));

        c4m_print(c4m_cstr_format("[atomic lime]info: [/] Finished parsing: {}",
                                  fname));

        c4m_grid_t *err_output = c4m_format_errors(ctx);

        if (err_output != NULL) {
            c4m_print(err_output);
        }
    }

    if (l < 1) {
        return;
    }

    c4m_tpat_node_t *pat;
    pat = c4m_tpat_find((void *)c4m_nt_body,
                        1,
                        c4m_tpat_match((void *)c4m_nt_break,
                                       0,
                                       c4m_tpat_match((void *)c4m_nt_error,
                                                      0)));

    c4m_xlist_t *captures = NULL;

    bool result = c4m_tree_match(ctx->parse_tree,
                                 pat,
                                 (c4m_cmp_fn)test_tree_search,
                                 &captures);

    printf("Res = %d; # caps: %lld\n", result, c4m_xlist_len(captures));

    c4m_print_parse_node((c4m_tree_node_t *)c4m_xlist_get(captures, 0, NULL));

    c4m_file_decl_pass(ctx);

    c4m_grid_t *err_output = c4m_format_errors(ctx);

    if (err_output != NULL) {
        c4m_print(err_output);
    }
}
#else
#define test_compiler(...)
#endif

void
test_format()
{
    c4m_str_t *s;
    s = c4m_cstr_format("Test 0");
    c4m_print(s);

    s = c4m_cstr_format("[red]Test 1:[/] [brown]{:c}[/] : [blue]{}[/] [i]woo.[/]",
                        c4m_box_u32(100),
                        c4m_rich_lit("Hello"));
    c4m_print(s);
    s = c4m_cstr_format("[red]Test 2:[/] [brown]{:d}[/] : [red]{:}[/]",
                        c4m_box_u32(100),
                        c4m_box_u32(100));
    c4m_print(s);

    s = c4m_cstr_format("[red]Test 3:[/] {1} : [blue]{0:n}[/]\n",
                        c4m_box_u32(100),
                        c4m_rich_lit("Hello"));
    c4m_print(s);
}

void
test_path()
{
    c4m_utf8_t *user = c4m_get_user_name();

    c4m_utf8_t *tests[] = {
        c4m_new_utf8("/"),
        c4m_cstr_format("/home/{}/dev/libcon4m/", user),
        c4m_cstr_format("~{}/dev/libcon4m/", user),
        c4m_cstr_format("~{}/dev/libcon4m/../con4m////src//", user),
        c4m_cstr_format("~{}/dev/libcon4m/.././con4m/././///src//", user),
        c4m_new_utf8(""),
        c4m_new_utf8("~"),
        NULL,
    };

    c4m_utf8_t *one;
    int         i = 0;

    c4m_print(c4m_cstr_format("[h2]Path resolution tests"));

    while ((one = tests[i++]) != NULL) {
        c4m_print(c4m_cstr_format(
            "[h4]Test #{}:[/]\n[u]input:[/] [i]{}[/]\n[u]output:[/] [em]{}\n",
            c4m_box_u64(i),
            one,
            c4m_resolve_path(one)));
    }
}

#undef STACK_SCAN_TEST

int
main(int argc, char **argv, char **envp)
{
#ifdef STACK_SCAN_TEST
    uint64_t top, bottom;
#endif

    c4m_init(argc, argv, envp);

    sout = c4m_get_stdout();
    serr = c4m_get_stderr();

    C4M_TRY
    {
        c4m_install_default_styles();
        c4m_terminal_dimensions(&term_width, NULL);
        c4m_ansi_render_to_width(str_test, term_width, 0, sout);
        test_rand64();
        // Test basic string and single threaded GC.
        test1();
        // style1 = apply_bg_color(style1, "alice blue");
        c4m_str_t *to_slice = test2();
        // c4m_gc_register_root(&to_slice, 1);
        test3(to_slice);
        to_slice = NULL;
        test4();
        table_test();

        printf("Sample style: %.16llx\n", (unsigned long long)style1);
        sha_test();

        type_tests();
        c4m_stream_tests();
        marshal_test();
        // marshal_test2();
        create_dict_lit();
        c4m_rich_lit_test();
        c4m_print(c4m_box_u32((int32_t)-1));
        c4m_print(c4m_box_i32((int32_t)-1));

        test_format();
        test_path();
        test_compiler();

        C4M_STATIC_ASCII_STR(local_test, "Goodbye!");
        // c4m_ansi_render(local_test, sout);
        c4m_print((c4m_obj_t *)local_test);
        C4M_CRAISE("Except maybe not!");
    }
    C4M_EXCEPT
    {
        c4m_exception_t *e = C4M_X_CUR();
        printf("Just kidding. An exception was raised before exit:\n");
        switch (e->code) {
        default:
            c4m_stream_puts(serr, c4m_exception_get_file(e)->data);
            c4m_stream_puti(serr, c4m_exception_get_line(e));
            c4m_stream_puts(serr, ": Caught you, exception man: ");
            c4m_ansi_render(c4m_exception_get_message(C4M_X_CUR()), serr);
            c4m_stream_putc(serr, '\n');
            C4M_JUMP_TO_TRY_END();
        };
    }
    C4M_TRY_END;
    c4m_stream_puts(serr, "This theoretically should run.\n");

#ifdef STACK_SCAN_TEST
    c4m_get_stack_scan_region(&top, &bottom);

    uint64_t q = bottom - top;

    // Give ourselves something to see where the real start is.
    bottom        = 0x4141414141414141;
    c4m_utf8_t *s = c4m_hex_dump((void *)top, q, top, 80, "");
    c4m_stream_puts(sout, s->data);
    c4m_stream_putc(sout, '\n');

    bottom = top + q;
    printf("(start) = %p; (end) = %p (%llu bytes)\n",
           (void *)top,
           (void *)bottom,
           (unsigned long long)q);
#endif
}

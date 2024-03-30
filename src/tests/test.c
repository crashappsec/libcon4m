#include <con4m.h>

style_t style1;
style_t style2;
size_t  term_width;

STATIC_ASCII_STR(str_test, "Welcome to the testing center. First order of "
		 "business: This is a static string, stored in static memory."
		 "However, we have not set any styling information on it.\n");
stream_t *sout;
stream_t *serr;

void
test1() {
    style1 = lookup_text_style("h1");
    style2 = lookup_text_style("h2");
    style2 = add_upper_case(style2);


    any_str_t *s1 = con4m_new(tspec_utf8(),
			      kw("cstring", ka("\ehello,"),
				 "style", ka(style1)));

    any_str_t *s2 = con4m_new(tspec_utf8(), kw("cstring", ka(" world!")));
    any_str_t *s3 = con4m_new(tspec_utf8(), kw("cstring", ka(" magic?\n")));

    //con4m_gc_register_root(&s1, 1);
    //con4m_gc_register_root(&s2, 1);
    //con4m_gc_register_root(&s3, 1);

    ansi_render(s1, sout);
    ansi_render(s2, sout);
    ansi_render(s3, sout);

    s1 = force_utf32(s1);
    string_set_style(s3, style2);
    s2 = force_utf32(s2);
    s3 = force_utf32(s3);

    ansi_render(s1, sout);
    ansi_render(s2, sout);
    ansi_render(s3, sout);

    utf32_t *s  = string_concat(s1, s2);
    s           = string_concat(s,  s3);

    ansi_render(s, sout);
    printf("That was at %p\n", s);

    break_info_t *g;

    //con4m_gc_register_root(&s, 1);
    //con4m_gc_register_root(&g, 1);

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
    ansi_render(s, sout);
}

any_str_t *
test2() {
    utf8_t *w1 = con4m_new(tspec_utf8(),
			   kw("cstring", ka("Once upon a time, there was a ")));
    utf8_t *w2 = con4m_new(tspec_utf8(),
			   kw("cstring", ka("thing I cared about. But then ")));
    utf8_t *w3 = con4m_new(tspec_utf8(),
			   kw("cstring",
			      ka("I stopped caring. I don't really remember "
				 "what it was, though. Do ")));
    utf8_t *w4 = con4m_new(tspec_utf8(),
			   kw("cstring",
      ka("you? No, I didn't think so, because it wasn't really all that "
	"interesting, to be quite honest. Maybe someday I'll find something "
	"interesting to care about, besides my family. Oh yeah, that's "
	"what it was, my family! Oh, wait, no, they're either not interesting, "
	 "or I don't care about them.\n")));
    utf8_t *w5 = con4m_new(tspec_utf8(),
			   kw("cstring", ka("Basically AirTags for Software")));
    utf8_t *w6 = con4m_new(tspec_utf8(),
			   kw("cstring", ka("\n")));

    string_set_style(w2, style1);
    string_set_style(w3, style2);
    string_set_style(w5, style1);

    utf32_t *to_wrap;

    to_wrap        = string_concat(w1, w2);
    to_wrap        = string_concat(to_wrap, w3);
    to_wrap        = string_concat(to_wrap, w4);
    to_wrap        = string_concat(to_wrap, w5);
    to_wrap        = string_concat(to_wrap, w6);

    utf8_t *dump1 = hex_dump(to_wrap->styling,
			     alloc_style_len(to_wrap),
			     kw("start_offset", ka(to_wrap->styling),
				"width", ka(80),
				"prefix", ka("Style dump\n")));

    utf8_t *dump2 = hex_dump(to_wrap,
			     to_wrap->byte_len,
			     kw("start_offset", ka((uint64_t)to_wrap),
				"width", ka(80),
				"prefix", ka("String Dump\n")));

    ansi_render(dump1, serr);
    ansi_render(dump2, serr);

    ansi_render_to_width(to_wrap, term_width, 0, sout);
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
    //ansi_render(string_slice(to_slice, 10, 50), sout);
    ansi_render_to_width(string_slice(to_slice, 10, 50), term_width, 0, sout);
    printf("\n");
    ansi_render_to_width(string_slice(to_slice, 40, 100), term_width, 0,
			 sout);
    printf("\n");
}

void
test4()
{
    utf8_t *w1 = con4m_new(tspec_utf8(),
			   kw("cstring", ka("Once upon a time, there was a ")));
    utf8_t *w2 = con4m_new(tspec_utf8(),
			   kw("cstring", ka("thing I cared about. But then ")));
    utf8_t *w3 = con4m_new(tspec_utf8(), kw("cstring",
			   ka("I stopped caring. I don't really remember "
			      "what it was, though. Do ")));
    utf8_t *w4 = con4m_new(tspec_utf8(), kw("cstring", ka(
	"you? No, I didn't think so, because it wasn't really all that "
	"interesting, to be quite honest. Maybe someday I'll find something "
	"interesting to care about, besides my family. Oh yeah, that's "
	"what it was, my family! Oh, wait, no, they're either not interesting, "
	"or I don't care about them.\n")));
    utf8_t *w5 = con4m_new(tspec_utf8(),
			   kw("cstring", ka("Basically AirTags for Software")));
    utf8_t *w6 = con4m_new(tspec_utf8(), kw("cstring", ka("\n")));

    hatrack_dict_t *d = con4m_new(tspec_dict(tspec_utf8(), tspec_ref()));

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
	ansi_render((any_str_t *)(view[i].key), serr);
    }

    con4m_gc_thread_collect();
}

void
table_test()
{
    utf8_t     *test1 = con4m_new(tspec_utf8(),
				  kw("cstring", ka("Some example 🤯 🤯 🤯"
	" Let's make it a fairly long 🤯 example, so it will be sure to need"
						   " some reynolds' wrap.")));
    utf8_t     *test2 = con4m_new(tspec_utf8(),
				  kw("cstring", ka("Some other example.")));
    utf8_t     *test3 = con4m_new(tspec_utf8(),
				  kw("cstring", ka("Example 3.")));
    utf8_t     *test4 = con4m_new(tspec_utf8(), kw("cstring", ka("Defaults.")));
    utf8_t     *test5 = con4m_new(tspec_utf8(), kw("cstring", ka("Last one.")));
    grid_t     *g     = con4m_new(tspec_grid(),
				  kw("start_rows", ka(4), "start_cols", ka(3),
				     "header_rows", ka(1)));
    utf8_t     *hdr   = con4m_new(tspec_utf8(),
				  kw("cstring", ka("Yes, this is a table.")));

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
    string_set_style(test1, style1);
    string_set_style(test2, style2);
    string_set_style(test3, style1);
    string_set_style(test5, style2);
    con4m_new(tspec_render_style(), kw("flex_units", ka(3),
				       "tag", ka("col1")));
    con4m_new(tspec_render_style(), kw("flex_units", ka(2), "tag", ka("col3")));
//    con4m_new(tspec_render_style(), "width_pct", 10., "tag", "col1");
//    con4m_new(tspec_render_style(), "width_pct", 30., "tag", "col3");
    set_column_style(g, 0, "col1");
    set_column_style(g, 2, "col3");

    // Ordered / unordered lists.
    utf8_t *ol1    = con4m_new(tspec_utf8(), kw("cstring", ka(
			       "This is a good point, one that you haven't "
			       "heard before.")));
    utf8_t *ol2    = con4m_new(tspec_utf8(), kw("cstring", ka(
			       "This is a point that's just as valid, but you "
			       "already know it.")));
    utf8_t *ol3    = con4m_new(tspec_utf8(),
			       kw("cstring", ka("This is a small point.")));
    utf8_t  *ol4   = con4m_new(tspec_utf8(), kw("cstring", ka("Conclusion.")));
    flexarray_t *l = con4m_new(tspec_list(tspec_utf8()), kw("length", ka(12)));

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
    ansi_render(con4m_value_obj_repr(flow), sout);
}

void
sha_test()
{
    utf8_t     *test1 = con4m_new(tspec_utf8(),
				  kw("cstring", ka("Some example 🤯 🤯 🤯"
	" Let's make it a fairly long 🤯 example, so it will be sure to need"
						   " some reynolds' wrap.")));

    sha_ctx *ctx = con4m_new(tspec_hash());
    sha_string_update(ctx, test1);
    buffer_t *b = sha_finish(ctx);

    printf("Sha256 is: ");
    ansi_render(con4m_value_obj_repr(b), sout);
    printf("\n");
}

void
type_tests()
{

    type_spec_t *t1 = tspec_int();
    type_spec_t *t2 = tspec_grid();
    type_spec_t *t3 = tspec_dict(t1, t2);

    ansi_render(con4m_value_obj_repr(t3), sout);
    printf("\n");

    type_spec_t *t4 = type_spec_new_typevar(global_type_env);
    type_spec_t *t5 = type_spec_new_typevar(global_type_env);
    type_spec_t *t6 = tspec_dict(t4, t5);

    ansi_render(con4m_value_obj_repr(t6), sout);
    printf("\n");
    ansi_render(con4m_value_obj_repr(merge_types(t3, t6)), sout);
    printf("\n");
}

void
stream_tests()
{

    utf8_t   *n   = con4m_new(tspec_utf8(), kw("cstring",
					       ka("../meson.build")));
    stream_t *s1  = con4m_new(tspec_stream(), kw("filename", ka(n)));
    buffer_t *b   = con4m_new(tspec_buffer(), kw("length", ka(16)));
    stream_t *s2  = con4m_new(tspec_stream(), kw("buffer", ka(b),
						 "write", ka(1)));
    style_t   sty = add_bold(add_italic(new_style()));

    while (true) {
	utf8_t *s = stream_read(s1, 16);

	if (con4m_len(s) == 0) {
	    break;
	}

	stream_write_object(s2, s);
    }

    print(hex_dump(b->data, b->byte_len));
    utf8_t *s = buffer_to_utf8_string(b);

    string_set_style(s, sty);
    print(s);
}

extern color_info_t color_data[];

void
marshal_test()
{
    utf8_t   *contents = con4m_new(tspec_utf8(),
				   kw("cstring",
				      ka("This is a test of marshal.\n")));
    buffer_t *b = con4m_new(tspec_buffer(), kw("length", ka(16)));
    stream_t *s = con4m_new(tspec_stream(),
			    kw("buffer", ka(b),
			       "write", ka(1),
			       "read", ka(0)));

    con4m_marshal(contents, s);
    stream_close(s);

    s = con4m_new(tspec_stream(), kw("buffer", ka(b)));

    utf8_t *new_str = con4m_unmarshal(s);

    ansi_render(new_str, sout);

}

#if 0
// This works, and now is in color.c locally.
#include "/tmp/color.c"

void
marshal_test2()
{
    dict_t *d = con4m_new(tspec_dict(tspec_utf8(), tspec_int()));
    int n;

    for (n = 0; color_data[n].name != NULL; n++) {
	utf8_t  *color = con4m_new(tspec_utf8(),
				   kw("cstring", ka(color_data[n].name)));
	int64_t rgb    = (int64_t)color_data[n].rgb;

	hatrack_dict_put(d, color, (void *)rgb);
    }

    printf("Writing test color dictionary to /tmp/color.c\n");
    dump_c_static_instance_code(d, "color_table",
				con4m_new(tspec_utf8(),
					  kw("cstring",
					     ka("/tmp/color.c"))));

    for (int64_t i = 0; i < n - 1; i++) {
	char   *ckey = color_data[i].name;
	if (ckey == NULL) {
	    continue;
	}
	utf8_t *key  = con4m_new(tspec_utf8(), kw("cstring", karg(ckey)));
	int64_t val  = lookup_color(key);
	printf("%s: %06llx\n", key->data, val);
    }
}
#endif

void
create_dict_lit()
{
    dict_t *d = con4m_new(tspec_dict(tspec_utf8(), tspec_int()));

    hatrack_dict_add(d, new_utf8("no"), (void *)1LLU);
    hatrack_dict_add(d, new_utf8("b"), (void *)2LLU);
    hatrack_dict_add(d, new_utf8("bold"), (void *)2LLU);
    hatrack_dict_add(d, new_utf8("i"), (void *)3LLU);
    hatrack_dict_add(d, new_utf8("italic"), (void *)3LLU);
    hatrack_dict_add(d, new_utf8("italics"), (void *)3LLU);
    hatrack_dict_add(d, new_utf8("st"), (void *)4LLU);
    hatrack_dict_add(d, new_utf8("strike"), (void *)4LLU);
    hatrack_dict_add(d, new_utf8("strikethru"), (void *)4LLU);
    hatrack_dict_add(d, new_utf8("strikethrough"), (void *)4LLU);
    hatrack_dict_add(d, new_utf8("u"), (void *)5LLU);
    hatrack_dict_add(d, new_utf8("underline"), (void *)5LLU);
    hatrack_dict_add(d, new_utf8("uu"), (void *)6LLU);
    hatrack_dict_add(d, new_utf8("2u"), (void *)6LLU);
    hatrack_dict_add(d, new_utf8("r"), (void *)7LLU);
    hatrack_dict_add(d, new_utf8("reverse"), (void *)7LLU);
    hatrack_dict_add(d, new_utf8("inverse"), (void *)7LLU);
    hatrack_dict_add(d, new_utf8("invert"), (void *)7LLU);
    hatrack_dict_add(d, new_utf8("inv"), (void *)7LLU);
    hatrack_dict_add(d, new_utf8("t"), (void *)8LLU);
    hatrack_dict_add(d, new_utf8("title"), (void *)8LLU);
    hatrack_dict_add(d, new_utf8("l"), (void *)9LLU);
    hatrack_dict_add(d, new_utf8("lower"), (void *)9LLU);
    hatrack_dict_add(d, new_utf8("up"), (void *)10LLU);
    hatrack_dict_add(d, new_utf8("upper"), (void *)10LLU);
    hatrack_dict_add(d, new_utf8("on"), (void *)11LLU);
    hatrack_dict_add(d, new_utf8("fg"), (void *)12LLU);
    hatrack_dict_add(d, new_utf8("foreground"), (void *)12LLU);
    hatrack_dict_add(d, new_utf8("bg"), (void *)13LLU);
    hatrack_dict_add(d, new_utf8("background"), (void *)13LLU);
    hatrack_dict_add(d, new_utf8("color"), (void *)14LLU);

    dump_c_static_instance_code(d, "style_keywords",
				new_utf8("/tmp/style_keys.c"));
}

void
rich_lit_test()
{
    utf8_t *test;

    test = rich_lit("H[atomic lime]ello, [jazzberry]world!");
    print(test);


    test = rich_lit("[atomic lime]Hello, [jazzberry]world[/]!");
    print(test);

    test = rich_lit("[atomic lime on jazzberry]Hello, world[/]!");
    print(test);

    test = rich_lit("[jazzberry on atomic lime]Hello, world![/]");
    print(test);

    test = rich_lit("[bold italic jazzberry on atomic lime]Hello,[/color] "
		    "world!");
    print(test);

    test = rich_lit("[bold italic jazzberry on atomic lime]Hello,"
		    "[/color bold] world!");
    print(test);

    test = rich_lit("[bold italic jazzberry on atomic lime]Hello,"
		    "[/color bold italic] world!");
    print(test);

    test = rich_lit("[bold italic jazzberry on atomic lime]Hello,[/bg bold] "
		    "world!");
    print(test);

    test = rich_lit("[bold italic u jazzberry on atomic lime]Hello,[/bold] "
		    "world!\n\n");
    print(test);

    test = rich_lit("[bold italic atomic lime on jazzberry]Hello,[/bold fg] "
		    "world!");
    print(test);

    test = rich_lit("[h2]Hello, world!");
    print(test);

    print(test, test, kw("no_color", ka(true), "sep", ka('&')));

}

int
main(int argc, char **argv, char **envp)
{
    uint64_t top, bottom;

    sout = get_stdout();
    serr = get_stderr();

    TRY {
	install_default_styles();
	terminal_dimensions(&term_width, NULL);
	ansi_render_to_width(str_test, term_width, 0, sout);
	test_rand64();
	// Test basic string and single threaded GC.
	test1();
	//style1 = apply_bg_color(style1, "alice blue");
	any_str_t *to_slice = test2();
	// con4m_gc_register_root(&to_slice, 1);
	test3(to_slice);
	to_slice = NULL;
	test4();
	table_test();

	printf("Sample style: %.16llx\n", style1);
	sha_test();

	type_tests();
	stream_tests();
	marshal_test();
	//marshal_test2();
	create_dict_lit();
	rich_lit_test();
	STATIC_ASCII_STR(local_test, "Goodbye!");
	//ansi_render(local_test, sout);
	print((object_t *)local_test);
	CRAISE("Except maybe not!");
    }
    EXCEPT
    {
	exception_t *e = X_CUR();
        printf("Just kidding. An exception was raised before exit:\n");
	switch(e->code) {
	default:
	    stream_puts(serr, exception_get_file(e)->data);
	    stream_puti(serr, exception_get_line(e));
	    stream_puts(serr, ": Caught you, exception man: ");
	    ansi_render(exception_get_message(X_CUR()), serr);
	    stream_putc(serr, '\n');
	    JUMP_TO_TRY_END();
	};
    }
    TRY_END;
    stream_puts(serr, "This theoretically should run.\n");

    if (argc > 1) {
	get_stack_scan_region(&top, &bottom);

	uint64_t q = bottom - top;

	// Give ourselves something to see where the real start is.
	bottom = 0x4141414141414141;
	utf8_t *s = hex_dump((void *)top, q, top, 80, "");
	stream_puts(sout, s->data);
	stream_putc(sout, '\n');

	bottom = top + q;
	printf("(start) = %p; (end) = %p (%llu bytes)\n", (void *)top,
	       (void *)bottom, q);
    }
}

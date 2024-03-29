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


    any_str_t *s1 = con4m_new(tspec_utf8(),
			      "cstring", "\ehello,",
			      "style", style1);

    any_str_t *s2 = con4m_new(tspec_utf8(), "cstring", " world!");
    any_str_t *s3 = con4m_new(tspec_utf8(), "cstring", " magic?\n");

    //con4m_gc_register_root(&s1, 1);
    //con4m_gc_register_root(&s2, 1);
    //con4m_gc_register_root(&s3, 1);

    ansi_render(s1, stdout);
    ansi_render(s2, stdout);
    ansi_render(s3, stdout);

    s1 = force_utf32(s1);
    string_set_style(s3, style2);
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
    ansi_render(s, stdout);
}

any_str_t *
test2() {
    utf8_t *w1 = con4m_new(tspec_utf8(),
			   "cstring", "Once upon a time, there was a ");
    utf8_t *w2 = con4m_new(tspec_utf8(),
			   "cstring", "thing I cared about. But then ");
    utf8_t *w3 = con4m_new(tspec_utf8(),
			   "cstring",
	"I stopped caring. I don't really remember what it was, though. Do ");
    utf8_t *w4 = con4m_new(tspec_utf8(),
			   "cstring",
	"you? No, I didn't think so, because it wasn't really all that "
	"interesting, to be quite honest. Maybe someday I'll find something "
	"interesting to care about, besides my family. Oh yeah, that's "
	"what it was, my family! Oh, wait, no, they're either not interesting, "
	"or I don't care about them.\n");
    utf8_t *w5 = con4m_new(tspec_utf8(),
			   "cstring", "Basically AirTags for Software");
    utf8_t *w6 = con4m_new(tspec_utf8(),
			   "cstring", "\n");

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
			     (uint64_t)to_wrap->styling,
			     80,
			     "Style dump\n");

    utf8_t *dump2 = hex_dump(to_wrap,
			     to_wrap->byte_len,
			     (uint64_t)to_wrap,
			     80,
			     "String Dump\n");

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
    utf8_t *w1 = con4m_new(tspec_utf8(),
			   "cstring", "Once upon a time, there was a ");
    utf8_t *w2 = con4m_new(tspec_utf8(),
			   "cstring", "thing I cared about. But then ");
    utf8_t *w3 = con4m_new(tspec_utf8(), "cstring",
	"I stopped caring. I don't really remember what it was, though. Do ");
    utf8_t *w4 = con4m_new(tspec_utf8(), "cstring",
	"you? No, I didn't think so, because it wasn't really all that "
	"interesting, to be quite honest. Maybe someday I'll find something "
	"interesting to care about, besides my family. Oh yeah, that's "
	"what it was, my family! Oh, wait, no, they're either not interesting, "
	"or I don't care about them.\n");
    utf8_t *w5 = con4m_new(tspec_utf8(),
			   "cstring", "Basically AirTags for Software");
    utf8_t *w6 = con4m_new(tspec_utf8(), "cstring", "\n");

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
	ansi_render((any_str_t *)(view[i].key), stderr);
    }

    con4m_gc_thread_collect();
}

void
table_test()
{
    utf8_t     *test1 = con4m_new(tspec_utf8(),
				  "cstring", "Some example 🤯 🤯 🤯"
	" Let's make it a fairly long 🤯 example, so it will be sure to need"
	" some reynolds' wrap.");
    utf8_t     *test2 = con4m_new(tspec_utf8(),
				  "cstring", "Some other example.");
    utf8_t     *test3 = con4m_new(tspec_utf8(),
				  "cstring", "Example 3.");
    utf8_t     *test4 = con4m_new(tspec_utf8(), "cstring", "Defaults.");
    utf8_t     *test5 = con4m_new(tspec_utf8(), "cstring", "Last one.");
    grid_t     *g     = con4m_new(tspec_grid(),
				  "start_rows", 4, "start_cols", 3,
				  "header_rows", 1);
    utf8_t     *hdr   = con4m_new(tspec_utf8(),
				  "cstring", "Yes, this is a table.");

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
    con4m_new(tspec_render_style(), "flex_units", 3, "tag", "col1");
    con4m_new(tspec_render_style(), "flex_units", 2, "tag", "col3");
//    con4m_new(tspec_render_style(), "width_pct", 10., "tag", "col1");
//    con4m_new(tspec_render_style(), "width_pct", 30., "tag", "col3");
    set_column_style(g, 0, "col1");
    set_column_style(g, 2, "col3");

    // Ordered / unordered lists.
    utf8_t *ol1    = con4m_new(tspec_utf8(), "cstring",
			       "This is a good point, one that you haven't "
			       "heard before.");
    utf8_t *ol2    = con4m_new(tspec_utf8(), "cstring",
			       "This is a point that's just as valid, but you "
			       "already know it.");
    utf8_t *ol3    = con4m_new(tspec_utf8(),
			       "cstring", "This is a small point.");
    utf8_t  *ol4   = con4m_new(tspec_utf8(), "cstring", "Conclusion.");
    flexarray_t *l = con4m_new(tspec_list(tspec_utf8()), "length", 12);

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

void
sha_test()
{
    utf8_t     *test1 = con4m_new(tspec_utf8(),
				  "cstring", "Some example 🤯 🤯 🤯"
	" Let's make it a fairly long 🤯 example, so it will be sure to need"
	" some reynolds' wrap.");

    sha_ctx *ctx = con4m_new(tspec_hash());
    sha_string_update(ctx, test1);
    buffer_t *b = sha_finish(ctx);

    printf("Sha256 is: ");
    ansi_render(con4m_value_obj_repr(b), stdout);
    printf("\n");
}

void
type_tests()
{

    type_spec_t *t1 = tspec_int();
    type_spec_t *t2 = tspec_grid();
    type_spec_t *t3 = tspec_dict(t1, t2);

    ansi_render(con4m_value_obj_repr(t3), stdout);
    printf("\n");

    type_spec_t *t4 = type_spec_new_typevar(global_type_env);
    type_spec_t *t5 = type_spec_new_typevar(global_type_env);
    type_spec_t *t6 = tspec_dict(t4, t5);

    ansi_render(con4m_value_obj_repr(t6), stdout);
    printf("\n");
    ansi_render(con4m_value_obj_repr(merge_types(t3, t6)), stdout);
    printf("\n");
}

void
stream_tests()
{

    utf8_t   *n   = con4m_new(tspec_utf8(), "cstring", "../meson.build");
    stream_t *s1  = con4m_new(tspec_stream(), "filename", n);
    buffer_t *b   = con4m_new(tspec_buffer(), "length", 16);
    stream_t *s2  = con4m_new(tspec_stream(), "buffer", b, "write", 1);
    style_t   sty = add_bold(add_italic(new_style()));

    while (true) {
	utf8_t *s = stream_read(s1, 16);

	if (con4m_len(s) == 0) {
	    break;
	}

	stream_write_object(s2, s);
    }

    print_hex(b->data, b->byte_len, "Buffer");
    utf8_t *s = buffer_to_utf8_string(b);

    string_set_style(s, sty);
    ansi_render(s, stdout);
}

extern color_info_t color_data[];

void
marshal_test()
{
    utf8_t   *contents = con4m_new(tspec_utf8(),
				   "cstring",  "This is a test of marshal.");
    buffer_t *b = con4m_new(tspec_buffer(), "length", 16);
    stream_t *s = con4m_new(tspec_stream(),
			    "buffer", b,
			    "write", 1,
			    "read", 0);

    con4m_marshal(contents, s);
    stream_close(s);

    s = con4m_new(tspec_stream(), "buffer", b);

    utf8_t *new_str = con4m_unmarshal(s);

    ansi_render(new_str, stdout);

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
	utf8_t  *color = con4m_new(tspec_utf8(), "cstring", color_data[n].name);
	int64_t rgb    = (int64_t)color_data[n].rgb;

	hatrack_dict_put(d, color, (void *)rgb);
    }

    printf("Writing test color dictionary to /tmp/color.c\n");
    dump_c_static_instance_code(d, "color_table",
		con4m_new(tspec_utf8(), "cstring", "/tmp/color.c"));

    for (int64_t i = 0; i < n - 1; i++) {
	char   *ckey = color_data[i].name;
	if (ckey == NULL) {
	    continue;
	}
	utf8_t *key  = con4m_new(tspec_utf8(), "cstring", ckey);
	int64_t val  = lookup_color(key);
	printf("%s: %06llx\n", key->data, val);
    }
}
#endif

int
main(int argc, char **argv, char **envp)
{
    uint64_t top, bottom;

    TRY {
	install_default_styles();
	terminal_dimensions(&term_width, NULL);
	ansi_render_to_width(str_test, term_width, 0, stdout);
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
	STATIC_ASCII_STR(local_test, "\nGoodbye!\n");
	ansi_render(local_test, stdout);
	CRAISE("Except maybe not!");
    }
    EXCEPT
    {
	exception_t *e = X_CUR();
        printf("Just kidding. An exception was raised before exit:\n");
	switch(e->code) {
	default:
	    fprintf(stderr, "%s:%lld: Caught you, exception face: ",
	    exception_get_file(e)->data,
	    exception_get_line(e));
	    ansi_render(exception_get_message(X_CUR()), stderr);
	    fputc('\n', stderr);
	    JUMP_TO_TRY_END();
	};
    }
    TRY_END;
    printf("This theoretically should run.\n");

    if (argc > 1) {
	get_stack_scan_region(&top, &bottom);

	uint64_t q = bottom - top;

	// Give ourselves something to see where the real start is.
	bottom = 0x4141414141414141;
	utf8_t *s = hex_dump((void *)top, q, top, 80, "");
	printf("%s\n", s->data);

	bottom = top + q;
	printf("(start) = %p; (end) = %p (%llu bytes)\n", (void *)top,
	       (void *)bottom, q);
    }
}

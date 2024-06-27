#define C4M_USE_INTERNAL_API
#include "con4m.h"

size_t term_width;

static bool dev_mode = false;

typedef struct {
    c4m_utf8_t  *expected_output;
    c4m_xlist_t *expected_errors;
    bool         ignore_output;
} c4m_test_kat;

static void
err_basic_usage(c4m_utf8_t *fname)
{
    c4m_printf(
        "[red]error:[/][em]{}[/]: Bad test case format. The second doc "
        "string may have 0 or 1 [em]$output[/] sections and 0 or 1 "
        "[em]$errors[/] sections ONLY. If neither are provided, "
        "then the harness expects no errors and ignores output. "
        "There may be nothing else in the doc string except whitespace.\n"
        "\n[i]Note: If you want to explicitly test for no output, then "
        "provide `$output:` with nothing following.",
        fname);
}

static void
extract_output(c4m_test_kat *kat, c4m_utf32_t *s, int64_t start, int64_t end)
{
    s                    = c4m_str_slice(s, start, end);
    kat->expected_output = c4m_to_utf8(c4m_str_strip(s));
}

static void
extract_errors(c4m_test_kat *kat, c4m_utf32_t *s, int64_t start, int64_t end)
{
    s                    = c4m_str_slice(s, start, end);
    kat->expected_errors = c4m_xlist(c4m_type_utf8());
    c4m_xlist_t *split   = c4m_str_xsplit(s, c4m_new_utf8("\n"));
    int          l       = c4m_xlist_len(split);

    for (int i = 0; i < l; i++) {
        s = c4m_str_strip(c4m_xlist_get(split, i, NULL));

        if (!c4m_str_codepoint_len(s)) {
            continue;
        }

        c4m_xlist_append(kat->expected_errors, c4m_to_utf8(s));
    }
}

static c4m_test_kat *
c4m_parse_kat(c4m_str_t *path, c4m_str_t *s)
{
    s = c4m_str_strip(s);

    c4m_test_kat *result = c4m_gc_alloc(c4m_test_kat);
    c4m_utf8_t   *output = c4m_new_utf8("$output:");
    c4m_utf8_t   *errors = c4m_new_utf8("$errors:");
    int64_t       outix  = c4m_str_find(s, output);
    int64_t       errix  = c4m_str_find(s, errors);

    if (outix == -1 && errix == -1) {
        if (c4m_str_codepoint_len(s) != 0) {
            err_basic_usage(path);
            return NULL;
        }
        return result;
    }

    if (outix == -1) {
        if (errix != 0) {
            err_basic_usage(path);
            return NULL;
        }
        extract_errors(result, s, 9, -1);
        result->ignore_output = 1;
        return result;
    }

    if (errix == -1) {
        if (outix != 0) {
            err_basic_usage(path);
            return NULL;
        }
        extract_output(result, s, 9, -1);
        return result;
    }

    if (outix != 0 && errix != 0) {
        err_basic_usage(path);
        return NULL;
    }

    if (errix != 0) {
        extract_output(result, s, 9, errix);
        extract_errors(result, s, errix + 9, -1);
    }
    else {
        extract_errors(result, s, 9, outix);
        extract_output(result, s, outix + 9, -1);
    }

    return result;
}

static c4m_test_kat *
c4m_extract_kat(c4m_utf8_t *path)
{
    c4m_file_compile_ctx *ctx = c4m_gc_alloc(c4m_file_compile_ctx);
    c4m_stream_t         *s   = c4m_file_instream(path, C4M_T_UTF8);

    c4m_lex(ctx, s);

    bool have_doc1 = false;
    int  l         = c4m_xlist_len(ctx->tokens);

    for (int i = 0; i < l; i++) {
        c4m_token_t *t = c4m_xlist_get(ctx->tokens, i, NULL);

        switch (t->kind) {
        case c4m_tt_space:
        case c4m_tt_newline:
        case c4m_tt_line_comment:
        case c4m_tt_long_comment:
            continue;
        case c4m_tt_string_lit:
            if (!have_doc1) {
                have_doc1 = true;
                continue;
            }
            return c4m_parse_kat(path, c4m_token_raw_content(t));
        default:
            return NULL;
        }
    }

    return NULL;
}

static void
show_dev_compile_info(c4m_compile_ctx *ctx)
{
    if (!dev_mode) {
        return;
    }

    c4m_print(c4m_format_tokens(ctx->entry_point));

    for (int i = 0; i < c4m_xlist_len(ctx->module_ordering); i++) {
        c4m_file_compile_ctx *f = c4m_xlist_get(ctx->module_ordering,
                                                i,
                                                NULL);

        c4m_print(c4m_cstr_format("[h1]Processing module {}", f->path));
        if (ctx->entry_point->parse_tree) {
            c4m_print(c4m_format_parse_tree(ctx->entry_point));
        }
        else {
            continue;
        }

        if (ctx->entry_point->cfg) {
            c4m_print(c4m_cstr_format("[h1]Toplevel CFG for {}", f->path));
            c4m_print(c4m_cfg_repr(ctx->entry_point->cfg));
        }
        else {
            continue;
        }

        for (int j = 0; j < c4m_xlist_len(f->fn_def_syms); j++) {
            c4m_scope_entry_t *sym  = c4m_xlist_get(f->fn_def_syms,
                                                   j,
                                                   NULL);
            c4m_fn_decl_t     *decl = sym->value;
            c4m_print(c4m_cstr_format("[h1]CFG for Function {}{}",
                                      sym->name,
                                      sym->type));
            c4m_print(c4m_cfg_repr(decl->cfg));
            c4m_print(c4m_cstr_format("[h2]Function Scope for {}{}",
                                      sym->name,
                                      sym->type));
            c4m_print(c4m_format_scope(decl->signature_info->fn_scope));
        }

        c4m_print(c4m_rich_lit("[h2]Global Scope"));
        c4m_print(c4m_format_scope(ctx->final_globals));
        c4m_print(c4m_rich_lit("[h2]Module Scope"));
        c4m_print(c4m_format_scope(ctx->entry_point->module_scope));
    }
}

static void
show_dev_disasm(c4m_vm_t *vm, c4m_zmodule_info_t *m)
{
    c4m_grid_t *g = c4m_disasm(vm, m);
    c4m_print(g);
    c4m_print(c4m_cstr_format("Module [em]{}[/] disassembly done.",
                              m->path));
    c4m_print(c4m_rich_lit("[h2]Module Source Code"));
    c4m_print(m->source);
}

c4m_dict_t *
build_file_list()
{
    bool          fatal      = false;
    c4m_xlist_t  *argv       = c4m_get_program_arguments();
    c4m_xlist_t  *to_recurse = c4m_xlist(c4m_type_utf8());
    c4m_dict_t   *result     = c4m_dict(c4m_type_utf8(), c4m_type_ref());
    c4m_utf8_t   *test_dir   = c4m_get_env(c4m_new_utf8("CON4M_TEST_DIR"));
    c4m_utf8_t   *ext        = c4m_new_utf8(".c4m");
    c4m_test_kat *kat;

    int n = c4m_xlist_len(argv);

    if (test_dir == NULL) {
        test_dir = c4m_new_utf8("../tests/");
    }

    test_dir = c4m_resolve_path(test_dir);

    if (!n) {
        n    = 1;
        argv = c4m_xlist(c4m_type_utf8());
        c4m_xlist_append(argv, test_dir);
    }

    for (int i = 0; i < n; i++) {
        c4m_utf8_t *s = c4m_to_utf8(c4m_xlist_get(argv, i, NULL));
        s             = c4m_resolve_path(s);
        switch (c4m_get_file_kind(s)) {
        case C4M_FK_IS_REG_FILE:
        case C4M_FK_IS_FLINK:
            // Don't worry about the extension if the explicitly
            // passed a file name.
            kat = c4m_extract_kat(s);
            hatrack_dict_put(result, s, kat);
            continue;
        case C4M_FK_IS_DIR:
        case C4M_FK_IS_DLINK:
            c4m_xlist_append(to_recurse, s);
            continue;
        case C4M_FK_NOT_FOUND:
            c4m_printf("[red]error:[/] No such file or directory: {}", s);
            fatal = true;
            continue;
        default:
            c4m_printf("[red]error:[/] Cannot process special file: {}", s);
            fatal = true;
            continue;
        }
    }

    if (fatal) {
        exit(-1);
    }

    n = c4m_xlist_len(to_recurse);
    for (int i = 0; i < n; i++) {
        int          num_hits = 0;
        c4m_utf8_t  *path     = c4m_xlist_get(to_recurse, i, NULL);
        c4m_xlist_t *files    = c4m_path_walk(path,
                                           c4m_kw("follow_links",
                                                  c4m_ka(true)));

        int walk_len = c4m_xlist_len(files);
        for (int j = 0; j < walk_len; j++) {
            c4m_utf8_t *one = c4m_xlist_get(files, j, NULL);
            if (c4m_str_ends_with(one, ext)) {
                kat = c4m_extract_kat(one);
                // When scanning dirs, if we have test cases that span
                // multiple files, we don't want to process multiple
                // times redundantly, so we only add ones w/ kat info.
                if (kat == NULL) {
                    continue;
                }

                num_hits++;
                hatrack_dict_put(result, one, kat);
            }
        }

        if (num_hits == 0) {
            c4m_printf("[yellow]warning[/]: No con4m files found in dir: {}",
                       path);
        }
    }

    return result;
}

static void
show_err_diffs(c4m_utf8_t *fname, c4m_xlist_t *expected, c4m_xlist_t *actual)
{
    c4m_compile_error_t err;
    c4m_utf8_t         *errstr;

    c4m_printf("[red]FAIL[/]: test [i]{}[/]: error mismatch.", fname);

    if (!expected || c4m_xlist_len(expected) == 0) {
        c4m_printf("[h1]Expected no errors.");
    }
    else {
        c4m_printf("[h1]Expected errors:");

        int n = c4m_xlist_len(expected);

        for (int i = 0; i < n; i++) {
            errstr = c4m_xlist_get(expected, i, NULL);
            c4m_printf("[b]{}:[/] [em]{}", c4m_box_u64(i + 1), errstr);
        }
    }

    if (!actual || c4m_xlist_len(actual) == 0) {
        c4m_printf("[h2]Got no errors.");
    }
    else {
        c4m_printf("[h2]Actual errors:");

        int n = c4m_xlist_len(actual);

        for (int i = 0; i < n; i++) {
            uint64_t u64     = (uint64_t)c4m_xlist_get(actual, i, NULL);
            err              = (c4m_compile_error_t)u64;
            c4m_utf8_t *code = c4m_err_code_to_str(err);
            c4m_printf("[b]{}:[/] [em]{}", c4m_box_u64(i + 1), code);
        }
    }
}

static bool
compare_results(c4m_utf8_t      *fname,
                c4m_test_kat    *kat,
                c4m_compile_ctx *ctx,
                c4m_buf_t       *outbuf)
{
    bool ret = true;

    if (kat == NULL) {
        return ret;
    }

    if (kat->expected_output) {
        if (!outbuf || c4m_buffer_len(outbuf) == 0) {
            if (!c4m_str_codepoint_len(kat->expected_output)) {
                goto next_comparison;
            }
empty_err:
            ret = false;
            c4m_printf(
                "[red]FAIL[/]: test [i]{}[/]: program expected output "
                "but did not compile. Expected output:\n {}",
                fname,
                kat->expected_output);
        }
        else {
            c4m_utf8_t *output = c4m_buf_to_utf8_string(outbuf);
            output             = c4m_to_utf8(c4m_str_strip(output));

            if (c4m_str_codepoint_len(output) == 0) {
                goto empty_err;
            }
            if (!c4m_str_eq(output, kat->expected_output)) {
                ret = false;

                c4m_printf(
                    "[red]FAIL[/]: test [i]{}[/]: output mismatch.",
                    fname);
                c4m_printf(
                    "[h1]Expected output[/]\n{}\n[h1]Actual[/]\n{}\n",
                    kat->expected_output,
                    output);
                c4m_printf(
                    "[h2]Expected (Hex)[/]\n{}\n[h2]Actual (Hex)[/]\n{}\n",
                    c4m_hex_dump(kat->expected_output->data,
                                 c4m_str_byte_len(kat->expected_output)),
                    c4m_hex_dump(output->data, c4m_str_byte_len(output)));
            }
        }
    }

next_comparison:;
    c4m_xlist_t *actual_errs  = c4m_compile_extract_all_error_codes(ctx);
    int          num_expected = 0;
    int          num_actual   = c4m_xlist_len(actual_errs);

    if (kat->expected_errors != NULL) {
        num_expected = c4m_xlist_len(kat->expected_errors);
    }

    if (num_expected != num_actual) {
        ret = false;
        show_err_diffs(fname, kat->expected_errors, actual_errs);
    }
    else {
        for (int i = 0; i < num_expected; i++) {
            c4m_compile_error_t c1;
            c4m_utf8_t         *c2;

            c1 = (uint64_t)c4m_xlist_get(actual_errs, i, NULL);
            c2 = c4m_xlist_get(kat->expected_errors, i, NULL);
            c2 = c4m_to_utf8(c4m_str_strip(c2));

            if (!c4m_str_eq(c4m_err_code_to_str(c1), c2)) {
                ret = false;
                show_err_diffs(fname, kat->expected_errors, actual_errs);
                break;
            }
        }
    }

    return ret;
}

bool
test_compiler(c4m_utf8_t *fname, c4m_test_kat *kat)
{
    c4m_compile_ctx *ctx;
    c4m_gc_show_heap_stats_on();
    c4m_printf("[atomic lime]info:[/] Compiling: {}", fname);

    ctx = c4m_compile_from_entry_point(fname);

    show_dev_compile_info(ctx);

    c4m_grid_t *err_output = c4m_format_errors(ctx);

    if (err_output != NULL) {
        c4m_print(err_output);
    }

    c4m_printf("[atomic lime]info:[/] Done processing: {}", fname);

    if (c4m_got_fatal_compiler_error(ctx)) {
        return compare_results(fname, kat, ctx, NULL);
    }

    c4m_vm_t *vm = c4m_generate_code(ctx);

    if (dev_mode) {
        for (int i = 0; i < c4m_xlist_len(ctx->module_ordering); i++) {
            c4m_zmodule_info_t *m;
            m = c4m_xlist_get(vm->obj->module_contents, i, NULL);
            show_dev_disasm(vm, m);
        }
    }

    c4m_printf("[h6]****STARTING PROGRAM EXECUTION*****[/]");
    c4m_vmthread_t *thread = c4m_vmthread_new(vm);
    c4m_vmthread_run(thread);
    c4m_printf("[h6]****PROGRAM EXECUTION FINISHED*****[/]\n");
    // TODO: We need to mark unlocked types with sub-variables at some point,
    // so they don't get clobbered.
    //
    // E.g.,  (dict[`x, list[int]]) -> int

    // c4m_clean_environment();
    // c4m_print(c4m_format_global_type_environment());
    bool result = compare_results(fname, kat, ctx, vm->print_buf);
    vm          = NULL;
    return result;
}

void
add_static_symbols()
{
    c4m_add_static_function(c4m_new_utf8("strndup"), strndup);
}

int
main(int argc, char **argv, char **envp)
{
    c4m_init(argc, argv, envp);
    add_static_symbols();

    int num_errs     = 0;
    int num_tests    = 0;
    int no_exception = 1;

    if (c4m_get_env(c4m_new_utf8("CON4M_DEV"))) {
        dev_mode = true;
    }

    //    C4M_TRY
    {
        c4m_install_default_styles();
        c4m_terminal_dimensions(&term_width, NULL);
        c4m_dict_t          *targets = build_file_list();
        uint64_t             n;
        hatrack_dict_item_t *items = hatrack_dict_items_sort(targets, &n);

        for (uint64_t i = 0; i < n; i++) {
            c4m_utf8_t   *fname = items[i].key;
            c4m_test_kat *kat   = items[i].value;

            if (kat != NULL) {
                num_tests++;
            }

            if (!test_compiler(fname, kat)) {
                num_errs++;
            }
        }
    }
    //    C4M_EXCEPT
    //    {
    //        no_exception = -1;
    //        printf("An exception was raised before exit:\n");
    //        c4m_print(c4m_repr_exception_stack_no_vm(c4m_new_utf8("Error: ")));
    //        C4M_JUMP_TO_TRY_END();
    //    }
    //    C4M_TRY_END;

    c4m_printf("Passed [em]{}[/] out of [em]{}[/] run tests.",
               c4m_box_u64(num_tests - num_errs),
               c4m_box_u64(num_tests));

    c4m_gc_thread_collect();

    if (!num_errs && !no_exception) {
        exit(-127);
    }

    if (num_errs && no_exception) {
        exit(-1);
    }
    if (num_errs) {
        exit(-2);
    }
    exit(0);
}

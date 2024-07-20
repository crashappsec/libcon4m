#define C4M_USE_INTERNAL_API
#include "con4m/test_harness.h"

static inline void
no_input()
{
    c4m_printf("[red b]NO VALID INPUT FILES.[/] Exiting.");
    exit(-1);
}

void
c4m_show_err_diffs(c4m_utf8_t *fname, c4m_list_t *expected, c4m_list_t *actual)
{
    c4m_compile_error_t err;
    c4m_utf8_t         *errstr;

    c4m_printf("[red]FAIL[/]: test [i]{}[/]: error mismatch.", fname);

    if (!expected || c4m_list_len(expected) == 0) {
        c4m_printf("[h1]Expected no errors.");
    }
    else {
        c4m_printf("[h1]Expected errors:");

        int n = c4m_list_len(expected);

        for (int i = 0; i < n; i++) {
            errstr = c4m_list_get(expected, i, NULL);
            c4m_printf("[b]{}:[/] [em]{}", c4m_box_u64(i + 1), errstr);
        }
    }

    if (!actual || c4m_list_len(actual) == 0) {
        c4m_printf("[h2]Got no errors.");
    }
    else {
        c4m_printf("[h2]Actual errors:");

        int n = c4m_list_len(actual);

        for (int i = 0; i < n; i++) {
            uint64_t u64     = (uint64_t)c4m_list_get(actual, i, NULL);
            err              = (c4m_compile_error_t)u64;
            c4m_utf8_t *code = c4m_err_code_to_str(err);
            c4m_printf("[b]{}:[/] [em]{}", c4m_box_u64(i + 1), code);
        }
    }
}

void
c4m_show_dev_compile_info(c4m_compile_ctx *ctx)
{
    if (!ctx->entry_point->path) {
        return;
    }

    c4m_printf("[h2]Module Source Code for {}", ctx->entry_point->path);
    c4m_print(ctx->entry_point->raw);
    c4m_printf("[h2]Module Tokens for {}", ctx->entry_point->path);
    c4m_print(c4m_format_tokens(ctx->entry_point));
    if (ctx->entry_point->parse_tree) {
        c4m_print(c4m_format_parse_tree(ctx->entry_point));
    }
    if (ctx->entry_point->cfg) {
        c4m_printf("[h1]Toplevel CFG for {}", ctx->entry_point->path);
        c4m_print(c4m_cfg_repr(ctx->entry_point->cfg));
    }

    for (int j = 0; j < c4m_list_len(ctx->entry_point->fn_def_syms); j++) {
        c4m_symbol_t  *sym  = c4m_list_get(ctx->entry_point->fn_def_syms,
                                         j,
                                         NULL);
        c4m_fn_decl_t *decl = sym->value;
        c4m_printf("[h1]CFG for Function {}{}", sym->name, sym->type);
        c4m_print(c4m_cfg_repr(decl->cfg));
        c4m_printf("[h2]Function Scope for {}{}", sym->name, sym->type);
        c4m_print(c4m_format_scope(decl->signature_info->fn_scope));
    }

    c4m_printf("[h2]Module Scope");
    c4m_print(c4m_format_scope(ctx->entry_point->module_scope));
    c4m_printf("[h2]Global Scope");
    c4m_print(c4m_format_scope(ctx->final_globals));

    c4m_printf("[h2]Loaded Modules");
    c4m_print(c4m_get_module_summary_info(ctx));
}

void
c4m_show_dev_disasm(c4m_vm_t *vm, c4m_zmodule_info_t *m)
{
    c4m_grid_t *g = c4m_disasm(vm, m);
    c4m_print(g);
    c4m_printf("Module [em]{}[/] disassembly done.", m->path);
}

static void
show_gridview(void)
{
    c4m_grid_t *success_grid = c4m_new(c4m_type_grid(),
                                       c4m_kw("start_cols",
                                              c4m_ka(2),
                                              "header_rows",
                                              c4m_ka(1),
                                              "container_tag",
                                              c4m_ka("error_grid")));
    c4m_grid_t *fail_grid    = c4m_new(c4m_type_grid(),
                                    c4m_kw("start_cols",
                                           c4m_ka(2),
                                           "header_rows",
                                           c4m_ka(1),
                                           "container_tag",
                                           c4m_ka("error_grid")));

    c4m_list_t *row = c4m_list(c4m_type_utf8());

    c4m_list_append(row, c4m_new_utf8("Test #"));
    c4m_list_append(row, c4m_new_utf8("Test File"));

    c4m_grid_add_row(success_grid, row);
    c4m_grid_add_row(fail_grid, row);

    c4m_set_column_style(success_grid, 0, "full_snap");
    c4m_set_column_style(success_grid, 1, "snap");
    c4m_set_column_style(fail_grid, 0, "full_snap");
    c4m_set_column_style(fail_grid, 1, "snap");

    for (int i = 0; i < c4m_test_total_items; i++) {
        c4m_test_kat *item = &c4m_test_info[i];

        if (!item->is_test) {
            continue;
        }

        row = c4m_list(c4m_type_utf8());

        c4m_utf8_t *num = c4m_cstr_format("[em]{}",
                                          c4m_box_u64(item->case_number));
        c4m_list_append(row, num);
        c4m_list_append(row, item->path);

        if (item->run_ok && !item->exit_code) {
            c4m_grid_add_row(success_grid, row);
        }
        else {
            c4m_grid_add_row(fail_grid, row);
        }
    }

    c4m_printf("[h5]Passed Tests:[/]");
    c4m_print(success_grid);

    c4m_printf("[h4]Failed Tests:[/]");
    c4m_print(fail_grid);
}

static int
possibly_warn()
{
    if (!c4m_give_malformed_warning) {
        return 0;
    }

    int result = 0;

    c4m_grid_t *oops_grid = c4m_new(c4m_type_grid(),
                                    c4m_kw("start_cols",
                                           c4m_ka(1),
                                           "header_rows",
                                           c4m_ka(0),
                                           "container_tag",
                                           c4m_ka("error_grid")));

    for (int i = 0; i < c4m_test_total_items; i++) {
        c4m_test_kat *item = &c4m_test_info[i];
        if (!item->is_malformed) {
            continue;
        }

        result++;

        c4m_list_t *row = c4m_list(c4m_type_utf8());

        c4m_list_append(row, item->path);
        c4m_grid_add_row(oops_grid, row);
    }

    c4m_printf("\[yellow]warning:[/] Bad test case format for file(s):");
    c4m_print(oops_grid);
    c4m_printf(
        "The second doc string may have 0 or 1 [em]$output[/] "
        "sections and 0 or 1 [em]$errors[/] sections ONLY.");
    c4m_printf(
        "If neither are provided, then the harness expects no "
        "errors and ignores output. There may be nothing else "
        "in the doc string except whitespace.");
    c4m_printf(
        "Also, instead of [em]$output[/] you may add a [em]$hex[/] "
        "section, where the contents must be raw hex bytes.");
    c4m_printf(
        "\n[i inv]Note: If you want to explicitly test for no output, then "
        "provide `$output:` with nothing following.");

    return result;
}

void
c4m_report_results_and_exit(void)
{
    if (!c4m_test_total_items) {
        no_input();
    }

    int malformed_items = possibly_warn();

    if (c4m_test_total_tests == 0) {
        if (c4m_test_total_items == malformed_items) {
            no_input();
        }

        exit(0);
    }

    if (c4m_test_number_passed == 0) {
        c4m_printf("[red b]Failed ALL TESTS.");
        exit(c4m_test_total_tests + 1);
    }

    c4m_printf("Passed [em]{}[/] out of [em]{}[/] run tests.",
               c4m_box_u64(c4m_test_number_passed),
               c4m_box_u64(c4m_test_total_tests));

    if (c4m_test_number_failed != 0) {
        show_gridview();
        c4m_printf("[h5] Con4m testing [b red]FAILED![/]");
        exit(c4m_test_number_failed);
    }

    c4m_printf("[h5] Con4m testing [b navy blue]PASSED.[/]");

    exit(0);
}

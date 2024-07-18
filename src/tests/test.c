#define C4M_USE_INTERNAL_API
#include "con4m.h"

typedef enum {
    c4m_tec_success = 0,
    c4m_tec_no_compile,
    c4m_tec_output_mismatch,
    c4m_tec_err_mismatch,
    c4m_tec_memcheck,
    c4m_tec_exception,
} c4m_test_exit_code;

typedef struct {
    c4m_utf8_t   *path;
    c4m_str_t    *raw_docstring;
    c4m_utf8_t   *expected_output;
    c4m_list_t   *expected_errors;
    struct rusage usage;
    int           case_number;
    bool          ignore_output;
    bool          is_hex;
    bool          is_test;
    bool          is_malformed;
    bool          run_ok;    // True if it ran successfully.
    bool          timeout;   // True if failed due to a timeout (not a crash)
    bool          stopped;   // Process was suspended via signal.
    int           exit_code; // Exit code of process if successfully run.
    int           signal;
    int           err_value; // Non-zero on a failure.
} c4m_test_kat;

int           c4m_test_total_items   = 0;
int           c4m_test_total_tests   = 0;
int           c4m_non_tests          = 0;
_Atomic int   c4m_test_number_passed = 0;
_Atomic int   c4m_test_number_failed = 0;
_Atomic int   c4m_test_next_test     = 0;
c4m_test_kat *c4m_test_info          = NULL;
static bool   give_malformed_warning = false;
static bool   dev_mode               = false;
int           c4m_current_test_case  = 0;
int           c4m_watch_case         = 5;
size_t        term_width;

#ifdef C4M_FULL_MEMCHECK
extern bool c4m_definite_memcheck_error;
#else
bool c4m_definite_memcheck_error = false;
#endif

static c4m_utf8_t *process_hex(c4m_utf32_t *s);

static inline bool
add_or_warn(c4m_list_t *flist, c4m_utf8_t *s, c4m_utf8_t *ext)
{
    s = c4m_to_utf8(s);
    if (c4m_str_ends_with(s, ext)) {
        c4m_list_append(flist, s);
        return true;
    }
    else {
        c4m_list_t *parts = c4m_str_split(s, c4m_new_utf8("/"));
        int         n     = c4m_list_len(parts);
        c4m_utf8_t *file  = c4m_to_utf8(c4m_list_get(parts, n - 1, NULL));
        if (!c4m_str_starts_with(file, c4m_new_utf8("."))) {
            c4m_printf(
                "[yellow]warning:[/] Skipping file w/o [em].c4m[/]"
                " file extension: {}",
                s);
        }
        return false;
    }
}

static void
no_input()
{
    c4m_printf("[red b]NO VALID INPUT FILES.[/] Exiting.");
    exit(-1);
}

static int
fname_sort(const c4m_utf8_t **s1, const c4m_utf8_t **s2)
{
    return strcmp((*s1)->data, (*s2)->data);
}

static c4m_list_t *
identify_test_files(void)
{
    c4m_list_t *argv       = c4m_get_program_arguments();
    int         n          = c4m_list_len(argv);
    c4m_list_t *to_recurse = c4m_list(c4m_type_utf8());
    c4m_utf8_t *test_dir   = c4m_get_env(c4m_new_utf8("CON4M_TEST_DIR"));
    c4m_utf8_t *cur_dir    = c4m_get_current_directory();
    c4m_utf8_t *ext        = c4m_new_utf8(".c4m");
    c4m_list_t *all_files  = c4m_list(c4m_type_utf8());

    if (test_dir == NULL) {
        test_dir = c4m_cstr_format("{}/tests/", c4m_con4m_root());
    }
    else {
        test_dir = c4m_resolve_path(test_dir);
    }

    if (!n) {
        n    = 1;
        argv = c4m_list(c4m_type_utf8());
        c4m_list_append(argv, test_dir);
    }

    for (int i = 0; i < n; i++) {
one_retry:;

        c4m_utf8_t *fname = c4m_to_utf8(c4m_list_get(argv, i, NULL));
        c4m_utf8_t *s     = c4m_path_simple_join(test_dir, fname);

        switch (c4m_get_file_kind(s)) {
        case C4M_FK_IS_REG_FILE:
        case C4M_FK_IS_FLINK:
            // Don't worry about the extension if they explicitly
            // named a file on the command line.
            c4m_list_append(all_files, c4m_to_utf8(s));
            continue;
        case C4M_FK_IS_DIR:
        case C4M_FK_IS_DLINK:
            c4m_list_append(to_recurse, c4m_to_utf8(s));
            continue;
        case C4M_FK_NOT_FOUND:
            if (!c4m_str_ends_with(s, ext)) {
                // We only attempt to add the file extension if
                // it's something on the command line.
                s = c4m_to_utf8(c4m_str_concat(s, ext));
                goto one_retry;
            }
            s = c4m_path_simple_join(cur_dir, fname);
            switch (c4m_get_file_kind(s)) {
            case C4M_FK_IS_REG_FILE:
            case C4M_FK_IS_FLINK:
                c4m_list_append(all_files, c4m_to_utf8(s));
                continue;
            case C4M_FK_IS_DIR:
            case C4M_FK_IS_DLINK:
                c4m_list_append(to_recurse, c4m_to_utf8(s));
                continue;
            default:
                break;
            }
            c4m_printf("[red]error:[/] No such file or directory: {}", s);
            continue;
        default:
            c4m_printf("[red]error:[/] Cannot process special file: {}", s);
            continue;
        }
    }

    n = c4m_list_len(to_recurse);

    for (int i = 0; i < n; i++) {
        c4m_utf8_t *path  = c4m_list_get(to_recurse, i, NULL);
        c4m_list_t *files = c4m_path_walk(path,
                                          c4m_kw("follow_links",
                                                 c4m_ka(true),
                                                 "yield_dirs",
                                                 c4m_ka(false)));

        int  walk_len                 = c4m_list_len(files);
        bool found_file_in_this_batch = false;

        for (int j = 0; j < walk_len; j++) {
            c4m_utf8_t *one = c4m_list_get(files, j, NULL);

            if (add_or_warn(all_files, one, ext)) {
                found_file_in_this_batch = true;
            }
        }

        if (!found_file_in_this_batch) {
            c4m_printf("[yellow]warning:[/] Nothing added from: {}", path);
        }
    }

    if (c4m_list_len(all_files) > 1) {
        c4m_list_sort(all_files, (c4m_sort_fn)fname_sort);
    }

    return all_files;
}

static void
extract_output(c4m_test_kat *kat,
               int64_t       start,
               int64_t       end)
{
    c4m_str_t *s         = c4m_str_slice(kat->raw_docstring, start, end);
    s                    = c4m_str_strip(s);
    s                    = c4m_to_utf8(s);
    kat->expected_output = kat->is_hex ? process_hex(s) : s;
}

static void
extract_errors(c4m_test_kat *kat, int64_t start, int64_t end)
{
    c4m_utf32_t *s       = c4m_str_slice(kat->raw_docstring, start, end);
    kat->expected_errors = c4m_list(c4m_type_utf8());
    c4m_list_t *split    = c4m_str_split(s, c4m_new_utf8("\n"));
    int         l        = c4m_list_len(split);

    for (int i = 0; i < l; i++) {
        s = c4m_str_strip(c4m_list_get(split, i, NULL));

        if (!c4m_str_codepoint_len(s)) {
            continue;
        }

        c4m_list_append(kat->expected_errors, c4m_to_utf8(s));
    }
}

#define kat_is_bad(x)              \
    give_malformed_warning = true; \
    x->is_malformed        = true; \
    return

static void
try_to_load_kat(c4m_test_kat *kat)
{
    kat->raw_docstring = c4m_to_utf32(c4m_str_strip(kat->raw_docstring));

    c4m_utf8_t *output = c4m_new_utf8("$output:");
    c4m_utf8_t *errors = c4m_new_utf8("$errors:");
    c4m_utf8_t *hex    = c4m_new_utf8("$hex:");
    int64_t     outix  = c4m_str_find(kat->raw_docstring, output);
    int64_t     errix  = c4m_str_find(kat->raw_docstring, errors);
    int64_t     hexix  = c4m_str_find(kat->raw_docstring, hex);

    if (hexix != -1) {
        kat->is_hex = true;
        outix       = hexix;
    }

    if (outix == -1 && errix == -1) {
        if (c4m_str_codepoint_len(kat->raw_docstring) != 0) {
            kat_is_bad(kat);
        }
        return;
    }

    if (outix == -1) {
        if (errix != 0) {
            kat_is_bad(kat);
        }

        extract_errors(kat, 9, c4m_str_codepoint_len(kat->raw_docstring));
        kat->ignore_output = true;
        return;
    }

    if (errix == -1) {
        if (outix != 0) {
            kat_is_bad(kat);
        }
        extract_output(kat, 9, c4m_str_codepoint_len(kat->raw_docstring));
        return;
    }

    if (outix != 0 && errix != 0) {
        kat_is_bad(kat);
    }

    if (errix != 0) {
        extract_output(kat, 9, errix);
        extract_errors(kat,
                       errix + 9,
                       c4m_str_codepoint_len(kat->raw_docstring));
    }
    else {
        extract_errors(kat, 9, outix);
        extract_output(kat,
                       outix + 9,
                       c4m_str_codepoint_len(kat->raw_docstring));
    }
}

static bool
find_docstring(c4m_test_kat *kat)
{
    // In this context, anything with a second doc string counts.

    c4m_file_compile_ctx *ctx = c4m_new_file_compile_ctx();
    c4m_stream_t         *s   = c4m_file_instream(kat->path, C4M_T_UTF8);

    c4m_lex(ctx, s);

    bool have_doc1 = false;
    int  l         = c4m_list_len(ctx->tokens);

    for (int i = 0; i < l; i++) {
        c4m_token_t *t = c4m_list_get(ctx->tokens, i, NULL);

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
            kat->raw_docstring = c4m_token_raw_content(t);
            kat->is_test       = true;
            kat->case_number   = ++c4m_test_total_tests;
            return true;
        default:
            ++c4m_non_tests;
            return false;
        }
    }

    return false;
}

static void
prep_tests(void)
{
    c4m_gc_register_root(&c4m_test_info, 1);

    c4m_list_t *all_files = identify_test_files();
    c4m_test_total_items  = c4m_list_len(all_files);
    c4m_test_info         = c4m_gc_array_alloc(c4m_test_kat,
                                       c4m_test_total_items);

    for (int i = 0; i < c4m_test_total_items; i++) {
        c4m_test_info[i].path = c4m_list_get(all_files, i, NULL);
        if (find_docstring(&c4m_test_info[i])) {
            try_to_load_kat(&c4m_test_info[i]);
        }
    }
}

static inline void
announce_test_start(c4m_test_kat *item)
{
    c4m_printf("[h4]Running test [atomic lime]{}[/atomic lime]: [i]{}",
               c4m_box_u64(item->case_number),
               item->path);
}

static inline void
announce_test_end(c4m_test_kat *kat)
{
    if (kat->run_ok && !kat->exit_code) {
        c4m_test_number_passed++;
        c4m_printf(
            "[h4]Finished test [atomic lime]{}[/atomic lime]: ({}). "
            "[i atomic lime]PASSED.",
            c4m_box_u64(kat->case_number),
            kat->path);
    }
    else {
        c4m_test_number_failed++;
        c4m_printf(
            "[h4]Finished test [atomic lime]{}[/atomic lime]: ({}). "
            "[i b navy blue]FAILED.",
            c4m_box_u64(kat->case_number),
            kat->path);
    }
    // TODO: format rusage data here.
}

static void
monitor_test(c4m_test_kat *kat, int readfd, pid_t pid)
{
    struct timeval timeout = (struct timeval){
        .tv_sec  = C4M_TEST_SUITE_TIMEOUT_SEC,
        .tv_usec = C4M_TEST_SUITE_TIMEOUT_USEC,
    };

    fd_set select_ctx;
    int    status = 0;

    FD_ZERO(&select_ctx);
    FD_SET(readfd, &select_ctx);

    switch (select(readfd + 1, &select_ctx, NULL, NULL, &timeout)) {
    case 0:
        kat->timeout = true;
        c4m_print(c4m_callout(c4m_cstr_format("{} TIMED OUT.",
                                              kat->path)));
        wait4(pid, &status, WNOHANG | WUNTRACED, &kat->usage);
        break;
    case -1:
        c4m_print(c4m_callout(c4m_cstr_format("{} CRASHED.", kat->path)));
        kat->err_value = errno;
        break;
    default:
        kat->run_ok = true;
        close(readfd);
        wait4(pid, &status, 0, &kat->usage);
        break;
    }

    if (WIFEXITED(status)) {
        kat->exit_code = WEXITSTATUS(status);

        c4m_printf("[h4]{}[/h4] exited with return code: [em]{}[/].",
                   kat->path,
                   c4m_box_u64(kat->exit_code));

        announce_test_end(kat);
        return;
    }

    if (WIFSIGNALED(status)) {
        kat->signal = WTERMSIG(status);
        announce_test_end(kat);
        return;
    }

    if (WIFSTOPPED(status)) {
        kat->stopped = true;
        kat->signal  = WSTOPSIG(status);
    }

    // If we got here, the test needs to be cleaned up.
    kill(pid, SIGKILL);
    announce_test_end(kat);
}

static c4m_test_exit_code run_one_item(c4m_test_kat *kat);

static void
run_tests(void)
{
    // For now, since the GC isn't working w/ cross-thread accesses yet,
    // we are just going to spawn fork and communicate via exist status.

    for (int i = 0; i < c4m_test_total_items; i++) {
        c4m_test_kat *item = &c4m_test_info[i];

        if (!item->is_test) {
            continue;
        }

        announce_test_start(item);

        // We never write to this file descriptor; if the child dies
        // the select will fire, and if it doesn't, it still allows us
        // to time out, where waitpid() and friends do not.
        int pipefds[2];
        if (pipe(pipefds) == -1) {
            abort();
        }

#ifndef C4M_TEST_WITHOUT_FORK
        pid_t pid = fork();

        if (!pid) {
            close(pipefds[0]);
            exit(run_one_item(item));
        }

        close(pipefds[1]);
        monitor_test(item, pipefds[0], pid);
#else
        item->exit_code = run_one_item(item);

        announce_test_end(item);
#endif
    }
}

static void
run_other_files(void)
{
    for (int i = 0; i < c4m_test_total_items; i++) {
        c4m_test_kat *item = &c4m_test_info[i];

        if (item->is_test) {
            continue;
        }

        c4m_printf("[h4]Running non-test case:[i] {}", item->path);

        int pipefds[2];

        if (pipe(pipefds) == -1) {
            abort();
        }

        pid_t pid = fork();
        if (!pid) {
            close(pipefds[0]);
            exit(run_one_item(item));
        }

        close(pipefds[1]);

        struct timeval timeout = (struct timeval){
            .tv_sec  = C4M_TEST_SUITE_TIMEOUT_SEC,
            .tv_usec = C4M_TEST_SUITE_TIMEOUT_USEC,
        };

        fd_set select_ctx;
        int    status;

        FD_ZERO(&select_ctx);
        FD_SET(pipefds[0], &select_ctx);

        switch (select(pipefds[0] + 1, &select_ctx, NULL, NULL, &timeout)) {
        case 0:
            c4m_print(c4m_callout(c4m_cstr_format("{} TIMED OUT.",
                                                  item->path)));
            kill(pid, SIGKILL);
            continue;
        case -1:
            c4m_print(c4m_callout(c4m_cstr_format("{} CRASHED.", item->path)));
            continue;
        default:
            waitpid(pid, &status, WNOHANG);
            c4m_printf("[h4]{}[/h4] exited with return code: [em]{}[/].",
                       WEXITSTATUS(status));
            continue;
        }
    }
}

static c4m_utf8_t *
process_hex(c4m_utf32_t *s)
{
    c4m_utf8_t *res = c4m_to_utf8(s);

    int n = 0;

    for (int i = 0; i < res->byte_len; i++) {
        char c = res->data[i];

        switch (c) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
            res->data[n++] = c;
            continue;
        case 'A':
            res->data[n++] = 'a';
            continue;
        case 'B':
            res->data[n++] = 'b';
            continue;
        case 'C':
            res->data[n++] = 'c';
            continue;
        case 'D':
            res->data[n++] = 'd';
            continue;
        case 'E':
            res->data[n++] = 'e';
            continue;
        case 'F':
            res->data[n++] = 'f';
            continue;
        default:
            continue;
        }
    }
    res->data[n]    = 0;
    res->byte_len   = n;
    res->codepoints = n;

    return res;
}

static void
show_err_diffs(c4m_utf8_t *fname, c4m_list_t *expected, c4m_list_t *actual)
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

static c4m_utf8_t *
line_strip(c4m_str_t *s)
{
    c4m_list_t *parts = c4m_str_split(s, c4m_new_utf8("\n"));

    for (int i = 0; i < c4m_list_len(parts); i++) {
        c4m_utf8_t *line = c4m_to_utf8(c4m_list_get(parts, i, NULL));
        c4m_utf8_t *item = c4m_to_utf8(c4m_str_strip(line));
        c4m_list_set(parts, i, item);
    }

    return c4m_str_join(parts, c4m_new_utf8("\n"));
}

static c4m_test_exit_code
compare_results(c4m_test_kat    *kat,
                c4m_compile_ctx *ctx,
                c4m_buf_t       *outbuf)
{
    if (kat->expected_output) {
        if (!outbuf || c4m_buffer_len(outbuf) == 0) {
            if (!c4m_str_codepoint_len(kat->expected_output)) {
                goto next_comparison;
            }
empty_err:
            c4m_printf(
                "[red]FAIL[/]: test [i]{}[/]: program expected output "
                "but did not compile. Expected output:\n {}",
                kat->path,
                kat->expected_output);
            return c4m_tec_no_compile;
        }
        else {
            c4m_utf8_t *output = c4m_buf_to_utf8_string(outbuf);
            output             = c4m_to_utf8(c4m_str_strip(output));

            if (c4m_str_codepoint_len(output) == 0) {
                goto empty_err;
            }

            if (kat->is_hex) {
                output = c4m_str_to_hex(output, false);
            }

            c4m_utf8_t *expected = line_strip(kat->expected_output);
            c4m_utf8_t *actual   = line_strip(output);

            if (!c4m_str_eq(expected, actual)) {
                c4m_printf(
                    "[red]FAIL[/]: test [i]{}[/]: output mismatch.",
                    kat->path);
                c4m_printf(
                    "[h1]Expected output[/]\n{}\n[h1]Actual[/]\n{}\n",
                    expected,
                    actual);
                return c4m_tec_output_mismatch;
            }
        }
    }

next_comparison:;
    c4m_list_t *actual_errs  = c4m_compile_extract_all_error_codes(ctx);
    int         num_expected = 0;
    int         num_actual   = c4m_list_len(actual_errs);

    if (kat->expected_errors != NULL) {
        num_expected = c4m_list_len(kat->expected_errors);
    }

    if (num_expected != num_actual) {
        show_err_diffs(kat->path, kat->expected_errors, actual_errs);
        return c4m_tec_err_mismatch;
    }
    else {
        for (int i = 0; i < num_expected; i++) {
            c4m_compile_error_t c1;
            c4m_utf8_t         *c2;

            c1 = (uint64_t)c4m_list_get(actual_errs, i, NULL);
            c2 = c4m_list_get(kat->expected_errors, i, NULL);
            c2 = c4m_to_utf8(c4m_str_strip(c2));

            if (!c4m_str_eq(c4m_err_code_to_str(c1), c2)) {
                show_err_diffs(kat->path,
                               kat->expected_errors,
                               actual_errs);
                return c4m_tec_err_mismatch;
            }
        }
    }

    return c4m_tec_success;
    ;
}

static void
show_dev_compile_info(c4m_compile_ctx *ctx)
{
    c4m_printf("[h2]Module Source Code for {}", ctx->entry_point->path);
    c4m_print(ctx->entry_point->raw);
    c4m_printf("[h2]Module Source Code for {}", ctx->entry_point->path);
    c4m_print(c4m_format_tokens(ctx->entry_point));
    if (ctx->entry_point->parse_tree) {
        c4m_print(c4m_format_parse_tree(ctx->entry_point));
    }
    if (ctx->entry_point->cfg) {
        c4m_print(c4m_cstr_format("[h1]Toplevel CFG for {}",
                                  ctx->entry_point->path));
        c4m_print(c4m_cfg_repr(ctx->entry_point->cfg));
    }

    for (int j = 0; j < c4m_list_len(ctx->entry_point->fn_def_syms); j++) {
        c4m_symbol_t  *sym  = c4m_list_get(ctx->entry_point->fn_def_syms,
                                         j,
                                         NULL);
        c4m_fn_decl_t *decl = sym->value;
        c4m_print(c4m_cstr_format("[h1]CFG for Function {}{}",
                                  sym->name,
                                  sym->type));
        c4m_print(c4m_cfg_repr(decl->cfg));
        c4m_print(c4m_cstr_format("[h2]Function Scope for {}{}",
                                  sym->name,
                                  sym->type));
        c4m_print(c4m_format_scope(decl->signature_info->fn_scope));
    }

    c4m_print(c4m_rich_lit("[h2]Module Scope"));
    c4m_print(c4m_format_scope(ctx->entry_point->module_scope));
    c4m_print(c4m_rich_lit("[h2]Global Scope"));
    c4m_print(c4m_format_scope(ctx->final_globals));
}

static void
show_dev_disasm(c4m_vm_t *vm, c4m_zmodule_info_t *m)
{
    c4m_grid_t *g = c4m_disasm(vm, m);
    c4m_print(g);
    c4m_print(c4m_cstr_format("Module [em]{}[/] disassembly done.",
                              m->path));
}

static c4m_test_exit_code
execute_test(c4m_test_kat *kat)
{
    c4m_compile_ctx *ctx;
    c4m_gc_show_heap_stats_on();
    c4m_print(c4m_cstr_format("[h1]Processing module {}", kat->path));

    ctx = c4m_compile_from_entry_point(kat->path);

    if (dev_mode) {
        show_dev_compile_info(ctx);
    }

    c4m_grid_t *err_output = c4m_format_errors(ctx);

    if (err_output != NULL) {
        c4m_print(err_output);
    }

    c4m_printf("[atomic lime]info:[/] Done processing: {}", kat->path);

    if (c4m_got_fatal_compiler_error(ctx)) {
        if (kat->is_test) {
            return compare_results(kat, ctx, NULL);
        }
        return c4m_tec_no_compile;
    }

    c4m_vm_t *vm = c4m_generate_code(ctx);

    if (dev_mode) {
        int                 n = vm->obj->entrypoint;
        c4m_zmodule_info_t *m = c4m_list_get(vm->obj->module_contents, n, NULL);

        show_dev_disasm(vm, m);
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
    if (kat->is_test) {
        return compare_results(kat, ctx, vm->print_buf);
    }

    return c4m_tec_success;
}

static c4m_test_exit_code
run_one_item(c4m_test_kat *kat)
{
    c4m_test_exit_code ec;

    C4M_TRY
    {
        ec = execute_test(kat);
    }
    C4M_EXCEPT
    {
        ec = c4m_tec_exception;
        C4M_JUMP_TO_TRY_END();
    }
    C4M_TRY_END;

    return ec;
}

void
add_static_symbols()
{
    c4m_add_static_function(c4m_new_utf8("strndup"), strndup);
    c4m_add_static_function(c4m_new_utf8("c4m_list_append"), c4m_list_append);
    c4m_add_static_function(c4m_new_utf8("c4m_join"), c4m_wrapper_join);
    c4m_add_static_function(c4m_new_utf8("c4m_str_upper"), c4m_str_upper);
    c4m_add_static_function(c4m_new_utf8("c4m_str_lower"), c4m_str_lower);
    c4m_add_static_function(c4m_new_utf8("c4m_str_split"), c4m_str_xsplit);
    c4m_add_static_function(c4m_new_utf8("c4m_str_pad"), c4m_str_pad);
    c4m_add_static_function(c4m_new_utf8("c4m_hostname"), c4m_wrapper_hostname);
    c4m_add_static_function(c4m_new_utf8("c4m_osname"), c4m_wrapper_os);
    c4m_add_static_function(c4m_new_utf8("c4m_arch"), c4m_wrapper_arch);
    c4m_add_static_function(c4m_new_utf8("c4m_repr"), c4m_wrapper_repr);
    c4m_add_static_function(c4m_new_utf8("c4m_to_str"), c4m_wrapper_to_str);
    c4m_add_static_function(c4m_new_utf8("c4m_len"), c4m_len);
    c4m_add_static_function(c4m_new_utf8("c4m_snap_column"), c4m_snap_column);
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
    if (!give_malformed_warning) {
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

static void
report_results_and_exit(void)
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

int
main(int argc, char **argv, char **envp)
{
    c4m_init(argc, argv, envp);
    add_static_symbols();
    c4m_install_default_styles();
    c4m_terminal_dimensions(&term_width, NULL);

    if (c4m_get_env(c4m_new_utf8("CON4M_DEV"))) {
        dev_mode = true;
    }

    prep_tests();
    run_tests();
    run_other_files();
    report_results_and_exit();
    c4m_unreachable();
    return 0;
}

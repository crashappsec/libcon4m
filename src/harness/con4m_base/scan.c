// Scan for test files, and populate the test matrix, including
// expected results.

#define C4M_USE_INTERNAL_API
#include "con4m/test_harness.h"

#define kat_is_bad(x)                  \
    c4m_give_malformed_warning = true; \
    x->is_malformed            = true; \
    return

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

static void
try_to_load_kat(c4m_test_kat *kat)
{
    int64_t len = c4m_str_codepoint_len(kat->raw_docstring);
    int64_t nl  = c4m_str_find(kat->raw_docstring, c4m_new_utf8("\n"));

    if (nl != 0) {
        c4m_str_t *line    = c4m_str_slice(kat->raw_docstring, 0, nl);
        kat->raw_docstring = c4m_str_slice(kat->raw_docstring, nl, len);

        if (line->data[0] == '#') {
            kat->save = true;
        }
    }

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

static int
fname_sort(const c4m_utf8_t **s1, const c4m_utf8_t **s2)
{
    return strcmp((*s1)->data, (*s2)->data);
}

static bool
find_docstring(c4m_test_kat *kat)
{
    c4m_compile_ctx *ctx = c4m_new_compile_context(NULL);
    c4m_module_t    *m   = c4m_init_module_from_loc(ctx, kat->path);

    if (!m || !m->ct->tokens) {
        return false;
    }

    bool have_doc1 = false;
    int  l         = c4m_list_len(m->ct->tokens);

    for (int i = 0; i < l; i++) {
        c4m_token_t *t = c4m_list_get(m->ct->tokens, i, NULL);

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

static c4m_list_t *
identify_test_files(void)
{
    c4m_list_t *argv      = c4m_get_program_arguments();
    int         n         = c4m_list_len(argv);
    c4m_list_t *to_load   = c4m_list(c4m_type_utf8());
    c4m_utf8_t *test_dir  = c4m_get_env(c4m_new_utf8("CON4M_TEST_DIR"));
    c4m_utf8_t *cur_dir   = c4m_get_current_directory();
    c4m_utf8_t *ext       = c4m_new_utf8(".c4m");
    c4m_list_t *all_files = c4m_list(c4m_type_utf8());

    if (test_dir == NULL) {
        test_dir = c4m_cstr_format("{}/tests/", c4m_con4m_root());
    }
    else {
        test_dir = c4m_resolve_path(test_dir);
    }

    c4m_list_append(con4m_path, test_dir);

    if (!n) {
        n    = 1;
        argv = c4m_list(c4m_type_utf8());
        c4m_list_append(argv, test_dir);
    }

    for (int i = 0; i < n; i++) {
        c4m_utf8_t *fname = c4m_to_utf8(c4m_list_get(argv, i, NULL));
one_retry:;
        c4m_utf8_t *s = c4m_path_simple_join(test_dir, fname);

        switch (c4m_get_file_kind(s)) {
        case C4M_FK_IS_REG_FILE:
        case C4M_FK_IS_FLINK:
            // Don't worry about the extension if they explicitly
            // named a file on the command line.
            c4m_list_append(all_files, c4m_to_utf8(s));
            continue;
        case C4M_FK_IS_DIR:
        case C4M_FK_IS_DLINK:
            c4m_list_append(to_load, c4m_to_utf8(s));
            continue;
        case C4M_FK_NOT_FOUND:
            if (!c4m_str_ends_with(s, ext)) {
                // We only attempt to add the file extension if
                // it's something on the command line.
                fname = c4m_to_utf8(c4m_str_concat(fname, ext));
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
                c4m_list_append(to_load, c4m_to_utf8(s));
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

    n = c4m_list_len(to_load);

    for (int i = 0; i < n; i++) {
        c4m_utf8_t *path  = c4m_list_get(to_load, i, NULL);
        c4m_list_t *files = c4m_path_walk(path,
                                          c4m_kw("follow_links",
                                                 c4m_ka(true),
                                                 "recurse",
                                                 c4m_ka(false),
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

void
c4m_scan_and_prep_tests(void)
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

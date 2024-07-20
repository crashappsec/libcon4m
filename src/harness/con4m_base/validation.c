#define C4M_USE_INTERNAL_API
#include "con4m/test_harness.h"

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

c4m_test_exit_code
c4m_compare_results(c4m_test_kat    *kat,
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
        c4m_show_err_diffs(kat->path, kat->expected_errors, actual_errs);
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
                c4m_show_err_diffs(kat->path,
                                   kat->expected_errors,
                                   actual_errs);
                return c4m_tec_err_mismatch;
            }
        }
    }

    return c4m_tec_success;
}

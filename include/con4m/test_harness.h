#ifdef C4M_USE_INTERNAL_API
#pragma once
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
    c4m_str_t    *second_entry;
    c4m_utf8_t   *expected_output;
    c4m_list_t   *expected_errors;
    struct rusage usage;
    int           case_number;
    int           second_entry_executions;
    bool          ignore_output;
    bool          is_hex;
    bool          is_test;
    bool          is_malformed;
    bool          run_ok;    // True if it ran successfully.
    bool          timeout;   // True if failed due to a timeout (not a crash)
    bool          save;
    bool          stopped;   // Process was suspended via signal.
    int           exit_code; // Exit code of process if successfully run.
    int           signal;
    int           err_value; // Non-zero on a failure.
} c4m_test_kat;

extern int           c4m_test_total_items;
extern int           c4m_test_total_tests;
extern int           c4m_non_tests;
extern _Atomic int   c4m_test_number_passed;
extern _Atomic int   c4m_test_number_failed;
extern _Atomic int   c4m_test_next_test;
extern c4m_test_kat *c4m_test_info;
extern int           c4m_current_test_case;
extern int           c4m_watch_case;
extern bool          c4m_dev_mode;
extern bool          c4m_give_malformed_warning;
extern size_t        c4m_term_width;

extern void               c4m_scan_and_prep_tests(void);
extern void               c4m_run_expected_value_tests(void);
extern void               c4m_run_other_test_files(void);
extern c4m_test_exit_code c4m_compare_results(c4m_test_kat *,
                                              c4m_compile_ctx *,
                                              c4m_buf_t *);
extern void               c4m_report_results_and_exit(void);
extern void               c4m_show_err_diffs(c4m_utf8_t *,
                                             c4m_list_t *,
                                             c4m_list_t *);
extern void               c4m_show_dev_compile_info(c4m_compile_ctx *);
extern void               c4m_show_dev_disasm(c4m_vm_t *, c4m_module_t *);

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
#endif

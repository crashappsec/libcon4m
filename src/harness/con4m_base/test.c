#define C4M_USE_INTERNAL_API
#include "con4m/test_harness.h"

int           c4m_test_total_items       = 0;
int           c4m_test_total_tests       = 0;
int           c4m_non_tests              = 0;
_Atomic int   c4m_test_number_passed     = 0;
_Atomic int   c4m_test_number_failed     = 0;
_Atomic int   c4m_test_next_test         = 0;
c4m_test_kat *c4m_test_info              = NULL;
bool          c4m_give_malformed_warning = false;
bool          c4m_dev_mode               = false;
int           c4m_current_test_case      = 0;
int           c4m_watch_case             = 5;
size_t        c4m_term_width;

#ifdef C4M_FULL_MEMCHECK
extern bool c4m_definite_memcheck_error;
#else
bool c4m_definite_memcheck_error = false;
#endif

void
add_static_test_symbols()
{
    c4m_add_static_symbols();
    c4m_add_static_function(c4m_new_utf8("strndup"),
                            strndup);
}

int
main(int argc, char **argv, char **envp)
{
    c4m_init(argc, argv, envp);
    add_static_test_symbols();
    c4m_install_default_styles();
    c4m_terminal_dimensions(&c4m_term_width, NULL);

    if (c4m_get_env(c4m_new_utf8("CON4M_DEV"))) {
        c4m_dev_mode = true;
    }

    c4m_scan_and_prep_tests();
    c4m_run_expected_value_tests();
    c4m_run_other_test_files();
    c4m_report_results_and_exit();
    c4m_unreachable();
    return 0;
}

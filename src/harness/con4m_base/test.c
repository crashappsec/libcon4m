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

void
test_automarshal()
{
    c4m_utf8_t     *str  = c4m_rich_lit("[h4]Hello, [h6]world!");
    c4m_base_obj_t *base = ((c4m_base_obj_t *)str) - 1;
    c4m_alloc_hdr  *hdr  = ((c4m_alloc_hdr *)base) - 1;
    hdr->cached_hash     = 0xffffffffffffffffULL;
    hdr->cached_hash <<= 64;
    hdr->cached_hash |= 0xffffffffffffffffUll;
    c4m_buf_t *buf      = c4m_automarshal(str);
    void      *addr     = buf->data;
    int        dump_len = c4m_buffer_len(buf);
    c4m_printf("[h2]Here's your temporary test");
    c4m_print(c4m_hex_dump(addr, dump_len));
    c4m_printf("[h2]Try unmarshalling LOL:");
    c4m_utf8_t *ums = c4m_autounmarshal(buf);
    c4m_mem_ptr p   = {.v = ums};
    p.object -= 1;
    p.alloc -= 1;
    c4m_print(c4m_hex_dump(addr, dump_len));
    c4m_printf("[h2]If this prints, we win:");
    c4m_print(ums);

    // c4m_buf_t    *compressed = c4m_buffer_empty();
    // c4m_stream_t *zstream    = c4m_buffer_outstream(compressed, true);

    // c4m_printf("[h2]Here's your compressed shiznit");
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

    test_automarshal();

    c4m_report_results_and_exit();
    c4m_unreachable();
    return 0;
}

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

#if 0
void
test_automarshal()
{
    c4m_utf8_t    *str = c4m_rich_lit("[h4]Hello, [h6]world!");
    c4m_alloc_hdr *hdr = ((c4m_alloc_hdr *)str) - 1;
    hdr->cached_hash   = 0xffffffffffffffffULL;
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
    p.alloc -= 1;
    c4m_print(c4m_hex_dump(addr, dump_len));
    c4m_printf("[h2]If this prints, we win:");
    c4m_print(ums);

    // c4m_buf_t    *compressed = c4m_buffer_empty();
    // c4m_stream_t *zstream    = c4m_buffer_outstream(compressed, true);
}
#endif

void
test_parsing()
{
    c4m_grammar_t *grammar = c4m_new(c4m_type_grammar());
    c4m_pitem_t   *add     = c4m_pitem_nonterm_raw(grammar,
                                             c4m_new_utf8("Add"));
    c4m_pitem_t   *mul     = c4m_pitem_nonterm_raw(grammar,
                                             c4m_new_utf8("Mul"));
    c4m_pitem_t   *paren   = c4m_pitem_nonterm_raw(grammar,
                                               c4m_new_utf8("Paren"));
    c4m_pitem_t   *digit   = c4m_pitem_builtin_raw(C4M_P_BIC_DIGIT);
    c4m_nonterm_t *nt_a    = c4m_pitem_get_ruleset(grammar, add);
    c4m_nonterm_t *nt_m    = c4m_pitem_get_ruleset(grammar, mul);
    c4m_nonterm_t *nt_p    = c4m_pitem_get_ruleset(grammar, paren);
    c4m_list_t    *rule1a  = c4m_list(c4m_type_ref());
    c4m_list_t    *rule1b  = c4m_list(c4m_type_ref());
    c4m_list_t    *rule2a  = c4m_list(c4m_type_ref());
    c4m_list_t    *rule2b  = c4m_list(c4m_type_ref());
    c4m_list_t    *rule3a  = c4m_list(c4m_type_ref());
    c4m_list_t    *rule3b  = c4m_list(c4m_type_ref());
    c4m_list_t    *plmi    = c4m_list(c4m_type_ref());
    c4m_list_t    *mudv    = c4m_list(c4m_type_ref());
    c4m_list_t    *lgrp    = c4m_list(c4m_type_ref());

    c4m_list_append(plmi, c4m_pitem_terminal_cp('+'));
    c4m_list_append(plmi, c4m_pitem_terminal_cp('-'));
    c4m_list_append(mudv, c4m_pitem_terminal_cp('*'));
    c4m_list_append(mudv, c4m_pitem_terminal_cp('/'));

    // Add -> Add [+-] Mul
    c4m_list_append(rule1a, add);
    c4m_list_append(rule1a, c4m_pitem_choice_raw(grammar, plmi));
    c4m_list_append(rule1a, mul);

    // Add -> Mul
    c4m_list_append(rule1b, mul);

    // Mul -> Mul [*/] Paren
    c4m_list_append(rule2a, mul);
    c4m_list_append(rule2a, c4m_pitem_choice_raw(grammar, mudv));
    c4m_list_append(rule2a, paren);

    // Mul -> Paren
    c4m_list_append(rule2b, paren);

    // Paren '(' Add ')'
    c4m_list_append(rule3a, c4m_pitem_terminal_cp('('));
    c4m_list_append(rule3a, add);
    c4m_list_append(rule3a, c4m_pitem_terminal_cp(')'));

    // Paren -> [0-9]+
    c4m_list_append(lgrp, digit);
    c4m_list_append(rule3b, c4m_group_items(grammar, lgrp, 1, 0));

    c4m_ruleset_add_rule(grammar, nt_a, rule1a);
    c4m_ruleset_add_rule(grammar, nt_a, rule1b);
    c4m_ruleset_add_rule(grammar, nt_m, rule2a);
    c4m_ruleset_add_rule(grammar, nt_m, rule2b);
    c4m_ruleset_add_rule(grammar, nt_p, rule3a);
    c4m_ruleset_add_rule(grammar, nt_p, rule3b);
    c4m_print(c4m_grammar_to_grid(grammar));

    c4m_parser_t *parser = c4m_new(c4m_type_parser(), grammar);

    c4m_parse_string(parser, c4m_new_utf8("1+(2*3-4321)"), NULL);
    c4m_print(c4m_parse_to_grid(parser, true));
    c4m_tree_node_t *forest = c4m_parse_get_parses(parser);

    c4m_print(c4m_forest_format(forest));
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

    test_parsing();

    c4m_report_results_and_exit();
    c4m_unreachable();
    return 0;
}

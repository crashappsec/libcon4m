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

static void
one_parse(c4m_parser_t *parser, char *s)
{
    c4m_utf8_t *input = c4m_new_utf8(s);
    c4m_utf8_t *ctext = c4m_cstr_format("'{}'", input);

    c4m_print(c4m_callout(ctext));

    c4m_parse_string(parser, input, NULL);

    c4m_list_t *l = c4m_parse_get_parses(parser);
    int         n = c4m_list_len(l);

    c4m_print(c4m_forest_format(l));
}

void
test_parsing(void)
{
    c4m_grammar_t *grammar = c4m_new(c4m_type_grammar(),
                                     c4m_kw("detect_errors",
                                            c4m_ka(true)));
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

    c4m_ruleset_add_rule(grammar, nt_a, rule1a, 0);
    c4m_ruleset_add_rule(grammar, nt_a, rule1b, 0);
    c4m_ruleset_add_rule(grammar, nt_m, rule2a, 0);
    c4m_ruleset_add_rule(grammar, nt_m, rule2b, 0);
    c4m_ruleset_add_rule(grammar, nt_p, rule3a, 0);
    c4m_ruleset_add_rule(grammar, nt_p, rule3b, 0);

    c4m_grid_t   *unmunged_grammar = c4m_grammar_format(grammar);
    c4m_parser_t *parser           = c4m_new(c4m_type_parser(), grammar);

    one_parse(parser, "21");
    one_parse(parser, "1+2");
    one_parse(parser, "(1)");
    one_parse(parser, "(1+2)");
    one_parse(parser, "1+(2+3)");
    one_parse(parser, "1+(2*3+4567)");
    one_parse(parser, " (1)");
    one_parse(parser, " 1");
    one_parse(parser, "1)");
    one_parse(parser, " 1)");

    c4m_grid_t *munged_grammar = c4m_grammar_format(grammar);

    // OOPS, seem to have an off-by-1; lower->upper, upper->title, title->lower
    c4m_print(c4m_callout(c4m_rich_lit(
        "[lower white]Grammar used for above parses")));
    c4m_print(unmunged_grammar);
    c4m_print(munged_grammar);
}

static void
show_gopt_results(c4m_gopt_ctx *gopt, c4m_list_t *all_parses)
{
    int num_parses = c4m_list_len(all_parses);
    c4m_printf("[em]{}[/] successful parses.", num_parses);

    for (int i = 0; i < num_parses; i++) {
        c4m_gopt_result_t *res = c4m_list_get(all_parses, i, NULL);
        c4m_printf("[h2]Parse {} Command:[/] [em]{}", i + 1, res->cmd);
        int64_t n;

        c4m_utf8_t **arg_keys = hatrack_dict_keys_sort(res->args, &n);
        bool         got_args = false;

        for (int i = 0; i < n; i++) {
            c4m_utf8_t *s    = arg_keys[i];
            c4m_list_t *args = hatrack_dict_get(res->args, arg_keys[i], NULL);

            if (args != NULL && c4m_list_len(args)) {
                got_args = true;

                if (!c4m_str_codepoint_len(s)) {
                    s = c4m_new_utf8("Root chalk command");
                }

                c4m_obj_t obj = c4m_clean_internal_list(args);

                c4m_printf("[h3]{} args for [reverse]{}[/]: [em]{}[/]",
                           c4m_list_len(args),
                           s,
                           obj);
            }
        }
        if (!got_args) {
            c4m_printf("[h4]No subcommands took arguments.");
        }

        int64_t *flag_info = hatrack_dict_keys_sort(res->flags, &n);
        for (int i = 0; i < n; i++) {
            int64_t          key    = flag_info[i];
            c4m_rt_option_t *option = hatrack_dict_get(res->flags,
                                                       (void *)key,
                                                       NULL);
            c4m_printf("[h3]Flag {}: [/] {}",
                       option->spec->name,
                       option->value);
        }
    }
}

static void
_gopt_test(c4m_gopt_ctx *gopt, c4m_list_t *args)
{
    c4m_list_t *res;

    res = c4m_gopt_parse(gopt, c4m_new_utf8("chalk"), args);
    show_gopt_results(gopt, res);
    c4m_printf("[h1]Run command was: chalk {}", args);
}

#define gopt_test(g, ...)                       \
    {                                           \
        c4m_list_t *l = c4m_c_map(__VA_ARGS__); \
        _gopt_test(g, l);                       \
    }

c4m_gopt_ctx *
setup_gopt_test(void)
{
    c4m_gopt_ctx   *gopt  = c4m_new(c4m_type_gopt_parser(),
                                 C4M_TOPLEVEL_IS_ARGV0);
    c4m_gopt_cspec *chalk = c4m_new(c4m_type_gopt_command(),
                                    c4m_kw("context", c4m_ka(gopt)));

    // gopt->show_debug = true;

    c4m_new(c4m_type_gopt_option(),
            c4m_kw("name",
                   c4m_new_utf8("color"),
                   "linked_command",
                   chalk,
                   "opt_type",
                   C4M_GOAT_BOOL_T_DEFAULT));

    c4m_new(c4m_type_gopt_option(),
            c4m_kw("name",
                   c4m_new_utf8("no-color"),
                   "linked_command",
                   chalk,
                   "opt_type",
                   C4M_GOAT_BOOL_F_DEFAULT));

    c4m_new(c4m_type_gopt_option(),
            c4m_kw("name",
                   c4m_new_utf8("testflag"),
                   "linked_command",
                   chalk,
                   "opt_type",
                   C4M_GOAT_WORD,
                   "max_args",
                   c4m_ka(0)));

    c4m_gopt_cspec *insert = c4m_new(c4m_type_gopt_command(),
                                     c4m_kw("context",
                                            c4m_ka(gopt),
                                            "name",
                                            c4m_ka(c4m_new_utf8("insert")),
                                            "parent",
                                            c4m_ka(chalk)));

    c4m_gopt_add_subcommand(insert, c4m_new_utf8("(STR)*"));

    c4m_gopt_cspec *extract = c4m_new(c4m_type_gopt_command(),
                                      c4m_kw("context",
                                             c4m_ka(gopt),
                                             "name",
                                             c4m_ka(c4m_new_utf8("extract")),
                                             "parent",
                                             c4m_ka(chalk)));
    // c4m_gopt_add_subcommand(extract, c4m_new_utf8("(str str)*"));
    c4m_gopt_add_subcommand(extract, c4m_new_utf8("(STR)*"));

    c4m_gopt_cspec *images     = c4m_new(c4m_type_gopt_command(),
                                     c4m_kw("context",
                                            c4m_ka(gopt),
                                            "name",
                                            c4m_ka(c4m_new_utf8("images")),
                                            "parent",
                                            c4m_ka(extract)));
    c4m_gopt_cspec *containers = c4m_new(c4m_type_gopt_command(),
                                         c4m_kw("context",
                                                c4m_ka(gopt),
                                                "name",
                                                c4m_ka(c4m_new_utf8("containers")),
                                                "parent",
                                                c4m_ka(extract)));

    c4m_gopt_cspec *extract_all = c4m_new(c4m_type_gopt_command(),
                                          c4m_kw("context",
                                                 c4m_ka(gopt),
                                                 "name",
                                                 c4m_ka(c4m_new_utf8("all")),
                                                 "parent",
                                                 c4m_ka(extract)));
    c4m_gopt_add_subcommand(extract_all, c4m_new_utf8("(str)*"));

    c4m_gopt_cspec *del = c4m_new(c4m_type_gopt_command(),
                                  c4m_kw("context",
                                         c4m_ka(gopt),
                                         "name",
                                         c4m_ka(c4m_new_utf8("delete")),
                                         "parent",
                                         c4m_ka(chalk)));
    c4m_gopt_add_subcommand(del, c4m_new_utf8("(str)*"));

    c4m_gopt_cspec *env = c4m_new(c4m_type_gopt_command(),
                                  c4m_kw("context",
                                         c4m_ka(gopt),
                                         "name",
                                         c4m_ka(c4m_new_utf8("env")),
                                         "parent",
                                         c4m_ka(chalk)));
    c4m_gopt_add_subcommand(env, c4m_new_utf8("(str)*"));

    c4m_gopt_cspec *exec = c4m_new(c4m_type_gopt_command(),
                                   c4m_kw("context",
                                          c4m_ka(gopt),
                                          "name",
                                          c4m_ka(c4m_new_utf8("exec")),
                                          "bad_opt_passthrough",
                                          c4m_ka(true),
                                          "parent",
                                          c4m_ka(chalk)));
    c4m_gopt_add_subcommand(exec, c4m_new_utf8("(str)*"));

    c4m_gopt_cspec *config = c4m_new(c4m_type_gopt_command(),
                                     c4m_kw("context",
                                            c4m_ka(gopt),
                                            "name",
                                            c4m_ka(c4m_new_utf8("config")),
                                            "parent",
                                            c4m_ka(chalk)));

    c4m_gopt_cspec *dump = c4m_new(c4m_type_gopt_command(),
                                   c4m_kw("context",
                                          c4m_ka(gopt),
                                          "name",
                                          c4m_ka(c4m_new_utf8("dump")),
                                          "parent",
                                          c4m_ka(chalk)));
    c4m_gopt_add_subcommand(dump, c4m_new_utf8("(str)?"));

    c4m_gopt_cspec *params = c4m_new(c4m_type_gopt_command(),
                                     c4m_kw("context",
                                            c4m_ka(gopt),
                                            "name",
                                            c4m_ka(c4m_new_utf8("params")),
                                            "parent",
                                            c4m_ka(dump)));

    c4m_gopt_cspec *cache    = c4m_new(c4m_type_gopt_command(),
                                    c4m_kw("context",
                                           c4m_ka(gopt),
                                           "name",
                                           c4m_ka(c4m_new_utf8("cache")),
                                           "parent",
                                           c4m_ka(dump)));
    c4m_gopt_cspec *dump_all = c4m_new(c4m_type_gopt_command(),
                                       c4m_kw("context",
                                              c4m_ka(gopt),
                                              "name",
                                              c4m_ka(c4m_new_utf8("all")),
                                              "parent",
                                              c4m_ka(dump)));
    c4m_gopt_cspec *load     = c4m_new(c4m_type_gopt_command(),
                                   c4m_kw("context",
                                          c4m_ka(gopt),
                                          "name",
                                          c4m_ka(c4m_new_utf8("load")),
                                          "parent",
                                          c4m_ka(chalk)));
    c4m_gopt_add_subcommand(load, c4m_new_utf8("(str)?"));

    c4m_gopt_cspec *version = c4m_new(c4m_type_gopt_command(),
                                      c4m_kw("context",
                                             c4m_ka(gopt),
                                             "name",
                                             c4m_ka(c4m_new_utf8("version")),
                                             "parent",
                                             c4m_ka(chalk)));

    c4m_gopt_cspec *docker = c4m_new(c4m_type_gopt_command(),
                                     c4m_kw("context",
                                            c4m_ka(gopt),
                                            "name",
                                            c4m_ka(c4m_new_utf8("docker")),
                                            "parent",
                                            c4m_ka(chalk)));
    c4m_gopt_add_subcommand(docker, c4m_new_utf8("raw"));

    c4m_gopt_cspec *setup = c4m_new(c4m_type_gopt_command(),
                                    c4m_kw("context",
                                           c4m_ka(gopt),
                                           "name",
                                           c4m_ka(c4m_new_utf8("setup")),
                                           "parent",
                                           c4m_ka(chalk)));

    c4m_gopt_cspec *docgen = c4m_new(c4m_type_gopt_command(),
                                     c4m_kw("context",
                                            c4m_ka(gopt),
                                            "name",
                                            c4m_ka(c4m_new_utf8("docgen")),
                                            "parent",
                                            c4m_ka(chalk)));
    c4m_gopt_cspec *__     = c4m_new(c4m_type_gopt_command(),
                                 c4m_kw("context",
                                        c4m_ka(gopt),
                                        "name",
                                        c4m_ka(c4m_new_utf8("__")),
                                        "parent",
                                        c4m_ka(chalk)));

    c4m_gopt_cspec *numtest = c4m_new(c4m_type_gopt_command(),
                                      c4m_kw("context",
                                             c4m_ka(gopt),
                                             "name",
                                             c4m_ka(c4m_new_utf8("num")),
                                             "parent",
                                             c4m_ka(chalk)));
    c4m_gopt_add_subcommand(numtest, c4m_new_utf8("(float int) *"));

    return gopt;
}

void
test_getopt(void)
{
    c4m_gopt_ctx *gopt = setup_gopt_test();

    gopt_test(gopt, "--color", "False", "version");
    gopt_test(gopt, "--color", "f", "--color", "False", "version");
    gopt_test(gopt, "extract", "foo", "--color", "true");
    gopt_test(gopt, "load", "foo", "--color", "true");
    gopt_test(gopt, "--color", "load", "foo");
    gopt_test(gopt, "--color", "num", "1209238", "37");
    gopt_test(gopt,
              "extract",
              "--color=",
              "true",
              "foo",
              "bar",
              "bleep",
              "74");

    gopt_test(gopt,
              "--color",
              "extract",
              "--no-color",
              "containers",
              "--color=",
              "true");
    gopt_test(gopt,
              "--color",
              "extract",
              "--no-color",
              "containers",
              "--testflag=x,y ",
              ",z");

    c4m_print(c4m_callout(c4m_rich_lit(
        "[lower white]Grammar used for above parses")));

    c4m_print(c4m_grammar_format(gopt->grammar));
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
    test_getopt();

    c4m_report_results_and_exit();
    c4m_unreachable();
    return 0;
}

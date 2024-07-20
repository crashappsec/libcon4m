#define C4M_USE_INTERNAL_API
#include "con4m/test_harness.h"

static c4m_test_exit_code
execute_test(c4m_test_kat *kat)
{
    c4m_compile_ctx *ctx;
    c4m_gc_show_heap_stats_on();
    c4m_printf("[h1]Processing module {}", kat->path);

    ctx = c4m_compile_from_entry_point(kat->path);

    if (c4m_dev_mode) {
        c4m_show_dev_compile_info(ctx);
    }

    c4m_grid_t *err_output = c4m_format_errors(ctx);

    if (err_output != NULL) {
        c4m_print(err_output);
    }

    c4m_printf("[atomic lime]info:[/] Done processing: {}", kat->path);

    if (c4m_got_fatal_compiler_error(ctx)) {
        if (kat->is_test) {
            return c4m_compare_results(kat, ctx, NULL);
        }
        return c4m_tec_no_compile;
    }

    c4m_vm_t *vm = c4m_generate_code(ctx);

    if (c4m_dev_mode) {
        int                 n = vm->obj->entrypoint;
        c4m_zmodule_info_t *m = c4m_list_get(vm->obj->module_contents, n, NULL);

        c4m_show_dev_disasm(vm, m);
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
        return c4m_compare_results(kat, ctx, vm->print_buf);
    }

    return c4m_tec_success;
}
static c4m_test_exit_code
run_one_item(c4m_test_kat *kat)
{
    c4m_test_exit_code ec = execute_test(kat);
    return ec;
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

void
c4m_run_expected_value_tests(void)
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
        item->run_ok    = true;
        announce_test_end(item);
#endif
    }
}

void
c4m_run_other_test_files(void)
{
    if (c4m_test_total_items == c4m_test_total_tests) {
        return;
    }

    c4m_print(c4m_callout(c4m_new_utf8("RUNNING TESTS LACKING TEST SPECS.")));
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
                       item->path,
                       c4m_box_u64(WEXITSTATUS(status)));
            continue;
        }
    }
}

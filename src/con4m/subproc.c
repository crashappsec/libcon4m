/*
 * Currently, we're using select() here, not epoll(), etc.
 */

#include "con4m.h"

/*
 * Initializes a `subprocess` context, setting the process to spawn.
 * By default, it will *not* be run on a pty; call `c4m_subproc_use_pty()`
 * before calling `c4m_subproc_run()` in order to turn that on.
 *
 * By default, the process will run QUIETLY, without any capture or
 * passthrough of IO.  See `c4m_subproc_set_passthrough()` for routing IO
 * between the subprocess and the parent, and `c4m_subproc_set_capture()`
 * for capturing output from the subprocess (or from your terminal).
 *
 * This does not take ownership of the strings passed in, and doesn't
 * use them until you call c4m_subproc_run(). In general, don't free
 * anything passed into this API until the process is done.
 */
void
c4m_subproc_init(c4m_subproc_t *ctx,
                 char          *cmd,
                 char          *argv[],
                 bool           proxy_stdin_close)
{
    memset(ctx, 0, sizeof(c4m_subproc_t));
    c4m_sb_init(&ctx->sb, C4M_IO_HEAP_SZ);
    ctx->cmd               = cmd;
    ctx->argv              = argv;
    ctx->capture           = 0;
    ctx->passthrough       = 0;
    ctx->proxy_stdin_close = proxy_stdin_close;

    c4m_sb_init_party_fd(&ctx->sb,
                         &ctx->parent_stdin,
                         0,
                         O_RDONLY,
                         false,
                         false,
                         false);
    c4m_sb_init_party_fd(&ctx->sb,
                         &ctx->parent_stdout,
                         1,
                         O_WRONLY,
                         false,
                         false,
                         false);
    c4m_sb_init_party_fd(&ctx->sb,
                         &ctx->parent_stderr,
                         2,
                         O_WRONLY,
                         false,
                         false,
                         false);
}

/*
 * By default, we pass through the environment. Use this to set your own
 * environment.
 */
bool
c4m_subproc_set_envp(c4m_subproc_t *ctx, char *envp[])
{
    if (ctx->run) {
        return false;
    }

    ctx->envp = envp;

    return true;
}

/*
 * This function passes the given string to the subprocess via
 * stdin. You can set this once before calling `c4m_subproc_run()`; but
 * after you've called `c4m_subproc_run()`, you can call this as many
 * times as you like, as long as the subprocess is open and its stdin
 * file descriptor hasn't been closed.
 */
bool
c4m_subproc_pass_to_stdin(c4m_subproc_t *ctx,
                          char          *str,
                          size_t         len,
                          bool           close_fd)
{
    if (ctx->str_waiting || ctx->sb.done) {
        return false;
    }

    if (ctx->run && close_fd) {
        return false;
    }

    c4m_sb_init_party_input_buf(&ctx->sb,
                                &ctx->str_stdin,
                                str,
                                len,
                                true,
                                true,
                                close_fd);

    if (ctx->run) {
        return c4m_sb_route(&ctx->sb, &ctx->str_stdin, &ctx->subproc_stdin);
    }
    else {
        ctx->str_waiting = true;

        if (close_fd) {
            ctx->pty_stdin_pipe = true;
        }

        return true;
    }
}

/*
 * This controls whether I/O gets proxied between the parent process
 * and the subprocess.
 *
 * The `which` parameter should be some combination of the following
 * flags:
 *
 * C4M_SP_IO_STDIN   (what you type goes to subproc stdin)
 * C4M_SP_IO_STDOUT  (subproc's stdout gets written to your stdout)
 * C4M_SP_IO_STDERR
 *
 * C4M_SP_IO_ALL proxies everything.
 * It's fine to use this even if no pty is used.
 *
 * If `combine` is true, then all subproc output for any proxied streams
 * will go to STDOUT.
 */
bool
c4m_subproc_set_passthrough(c4m_subproc_t *ctx,
                            unsigned char  which,
                            bool           combine)
{
    if (ctx->run || which > C4M_SP_IO_ALL) {
        return false;
    }

    ctx->passthrough      = which;
    ctx->pt_all_to_stdout = combine;

    return true;
}
/*
 * This controls whether input from a file descriptor is captured into
 * a string that is available when the process ends.
 *
 * You can capture any stream, including what the user's typing on stdin.
 *
 * The `which` parameter should be some combination of the following
 * flags:
 *
 * C4M_SP_IO_STDIN   (what you type); reference for string is "stdin"
 * C4M_SP_IO_STDOUT  reference for string is "stdout"
 * C4M_SP_IO_STDERR  reference for string is "stderr"
 *
 * C4M_SP_IO_ALL captures everything.
 It's fine to use this even if no pty is used.
 *
 * If `combine` is true, then all subproc output for any streams will
 * be combined into "stdout".  Retrieve from the `c4m_capture_result_t` object
 * returned from `c4m_subproc_run()`, using the sp_result_...() api.
 */
bool
c4m_subproc_set_capture(c4m_subproc_t *ctx, unsigned char which, bool combine)
{
    if (ctx->run || which > C4M_SP_IO_ALL) {
        return false;
    }

    ctx->capture          = which;
    ctx->pt_all_to_stdout = combine;

    return true;
}

bool
c4m_subproc_set_io_callback(c4m_subproc_t *ctx,
                            unsigned char  which,
                            c4m_sb_cb_t    cb)
{
    if (ctx->run || which > C4M_SP_IO_ALL) {
        return false;
    }

    c4m_deferred_cb_t *cbinfo = malloc(sizeof(c4m_deferred_cb_t));

    cbinfo->next  = ctx->deferred_cbs;
    cbinfo->which = which;
    cbinfo->cb    = cb;

    ctx->deferred_cbs = cbinfo;

    return true;
}

/*
 * This sets how long to wait in `select()` for file-descriptors to be
 * ready with data to read. If you don't set this, there will be no
 * timeout, and it's possible for the process to die and for the file
 * descriptors associated with them to never return ready.
 *
 * If you have a timeout, a progress callback can be called.
 *
 * Also, when the process is not blocked on the select(), right before
 * the next select we check the status of the subprocess. If it's
 * returned and all its descriptors are marked as closed, and no
 * descriptors that are open are waiting to write, then the subprocess
 * switchboard will exit.
 */
void
c4m_subproc_set_timeout(c4m_subproc_t *ctx, struct timeval *timeout)
{
    c4m_sb_set_io_timeout(&ctx->sb, timeout);
}

/*
 * Removes any set timeout.
 */
void
c4m_subproc_clear_timeout(c4m_subproc_t *ctx)
{
    c4m_sb_clear_io_timeout(&ctx->sb);
}

/*
 * When called before c4m_subproc_run(), will spawn the child process on
 * a pseudo-terminal.
 */
bool
c4m_subproc_use_pty(c4m_subproc_t *ctx)
{
    if (ctx->run) {
        return false;
    }
    ctx->use_pty = true;
    return true;
}

bool
c4m_subproc_set_startup_callback(c4m_subproc_t *ctx, void (*cb)(void *))
{
    ctx->startup_callback = cb;
    return true;
}

int
c4m_subproc_get_pty_fd(c4m_subproc_t *ctx)
{
    return ctx->pty_fd;
}

void
c4m_subproc_pause_passthrough(c4m_subproc_t *ctx, unsigned char which)
{
    /*
     * Since there's no real consequence to trying to pause a
     * subscription that doesn't exist, we'll just try to pause every
     * subscription implied by `which`. Note that if we see stderr, we
     * try to unsubscribe it from both the parent's stdout and the
     * parent's stderr; no strong need to care whether they were
     * combined or not here..
     */

    if (which & C4M_SP_IO_STDIN) {
        if (ctx->pty_fd) {
            c4m_sb_pause_route(&ctx->sb,
                               &ctx->parent_stdin,
                               &ctx->subproc_stdout);
        }
        else {
            c4m_sb_pause_route(&ctx->sb,
                               &ctx->parent_stdin,
                               &ctx->subproc_stdin);
        }
    }
    if (which & C4M_SP_IO_STDOUT) {
        c4m_sb_pause_route(&ctx->sb,
                           &ctx->subproc_stdout,
                           &ctx->parent_stdout);
    }
    if (!ctx->pty_fd && (which & C4M_SP_IO_STDERR)) {
        c4m_sb_pause_route(&ctx->sb,
                           &ctx->subproc_stderr,
                           &ctx->parent_stdout);
        c4m_sb_pause_route(&ctx->sb,
                           &ctx->subproc_stderr,
                           &ctx->parent_stderr);
    }
}

void
c4m_subproc_resume_passthrough(c4m_subproc_t *ctx, unsigned char which)
{
    /*
     * Since there's no real consequence to trying to pause a
     * subscription that doesn't exist, we'll just try to pause every
     * subscription implied by `which`. Note that if we see stderr, we
     * try to unsubscribe it from both the parent's stdout and the
     * parent's stderr; no strong need to care whether they were
     * combined or not here..
     */

    if (which & C4M_SP_IO_STDIN) {
        if (ctx->pty_fd) {
            c4m_sb_resume_route(&ctx->sb,
                                &ctx->parent_stdin,
                                &ctx->subproc_stdout);
        }
        else {
            c4m_sb_resume_route(&ctx->sb,
                                &ctx->parent_stdin,
                                &ctx->subproc_stdin);
        }
    }
    if (which & C4M_SP_IO_STDOUT) {
        c4m_sb_resume_route(&ctx->sb,
                            &ctx->subproc_stdout,
                            &ctx->parent_stdout);
    }
    if (!ctx->pty_fd && (which & C4M_SP_IO_STDERR)) {
        c4m_sb_resume_route(&ctx->sb,
                            &ctx->subproc_stderr,
                            &ctx->parent_stdout);
        c4m_sb_resume_route(&ctx->sb,
                            &ctx->subproc_stderr,
                            &ctx->parent_stderr);
    }
}

void
c4m_subproc_pause_capture(c4m_subproc_t *ctx, unsigned char which)
{
    if (which & C4M_SP_IO_STDIN) {
        c4m_sb_pause_route(&ctx->sb,
                           &ctx->parent_stdin,
                           &ctx->capture_stdin);
    }

    if (which & C4M_SP_IO_STDOUT) {
        c4m_sb_pause_route(&ctx->sb,
                           &ctx->subproc_stdout,
                           &ctx->capture_stdout);
    }

    if ((which & C4M_SP_IO_STDERR) && !ctx->pty_fd) {
        c4m_sb_pause_route(&ctx->sb,
                           &ctx->subproc_stderr,
                           &ctx->capture_stdout);
        c4m_sb_pause_route(&ctx->sb,
                           &ctx->subproc_stderr,
                           &ctx->capture_stderr);
    }
}

void
c4m_subproc_resume_capture(c4m_subproc_t *ctx, unsigned char which)
{
    if (which & C4M_SP_IO_STDIN) {
        c4m_sb_resume_route(&ctx->sb,
                            &ctx->parent_stdin,
                            &ctx->capture_stdin);
    }

    if (which & C4M_SP_IO_STDOUT) {
        c4m_sb_resume_route(&ctx->sb,
                            &ctx->subproc_stdout,
                            &ctx->capture_stdout);
    }

    if ((which & C4M_SP_IO_STDERR) && !ctx->pty_fd) {
        c4m_sb_resume_route(&ctx->sb,
                            &ctx->subproc_stderr,
                            &ctx->capture_stdout);
        c4m_sb_resume_route(&ctx->sb,
                            &ctx->subproc_stderr,
                            &ctx->capture_stderr);
    }
}

static void
setup_subscriptions(c4m_subproc_t *ctx, bool pty)
{
    c4m_party_t *stderr_dst = &ctx->parent_stderr;

    if (ctx->pt_all_to_stdout) {
        stderr_dst = &ctx->parent_stdout;
    }

    if (ctx->passthrough) {
        if (ctx->passthrough & C4M_SP_IO_STDIN) {
            if (pty) {
                // in pty, ctx->subproc_stdout is the same FD used for stdin
                // as its the same r/w FD for both
                c4m_sb_route(&ctx->sb,
                             &ctx->parent_stdin,
                             &ctx->subproc_stdout);
            }
            else {
                c4m_sb_route(&ctx->sb,
                             &ctx->parent_stdin,
                             &ctx->subproc_stdin);
            }
        }
        if (ctx->passthrough & C4M_SP_IO_STDOUT) {
            c4m_sb_route(&ctx->sb,
                         &ctx->subproc_stdout,
                         &ctx->parent_stdout);
        }
        if (!pty && ctx->passthrough & C4M_SP_IO_STDERR) {
            c4m_sb_route(&ctx->sb,
                         &ctx->subproc_stderr,
                         stderr_dst);
        }
    }

    if (ctx->capture) {
        if (ctx->capture & C4M_SP_IO_STDIN) {
            c4m_sb_init_party_output_buf(&ctx->sb,
                                         &ctx->capture_stdin,
                                         "stdin",
                                         C4M_CAP_ALLOC);
        }
        if (ctx->capture & C4M_SP_IO_STDOUT) {
            c4m_sb_init_party_output_buf(&ctx->sb,
                                         &ctx->capture_stdout,
                                         "stdout",
                                         C4M_CAP_ALLOC);
        }

        if (ctx->combine_captures) {
            if (!(ctx->capture & C4M_SP_IO_STDOUT) && ctx->capture & C4M_SP_IO_STDERR) {
                if (ctx->capture & C4M_SP_IO_STDOUT) {
                    c4m_sb_init_party_output_buf(&ctx->sb,
                                                 &ctx->capture_stdout,
                                                 "stdout",
                                                 C4M_CAP_ALLOC);
                }
            }

            stderr_dst = &ctx->capture_stdout;
        }
        else {
            if (!pty && ctx->capture & C4M_SP_IO_STDERR) {
                c4m_sb_init_party_output_buf(&ctx->sb,
                                             &ctx->capture_stderr,
                                             "stderr",
                                             C4M_CAP_ALLOC);
            }

            stderr_dst = &ctx->capture_stderr;
        }

        if (ctx->capture & C4M_SP_IO_STDIN) {
            c4m_sb_route(&ctx->sb, &ctx->parent_stdin, &ctx->capture_stdin);
        }
        if (ctx->capture & C4M_SP_IO_STDOUT) {
            c4m_sb_route(&ctx->sb, &ctx->subproc_stdout, &ctx->capture_stdout);
        }
        if (!pty && ctx->capture & C4M_SP_IO_STDERR) {
            c4m_sb_route(&ctx->sb, &ctx->subproc_stderr, stderr_dst);
        }
    }

    if (ctx->str_waiting) {
        c4m_sb_route(&ctx->sb, &ctx->str_stdin, &ctx->subproc_stdin);
        ctx->str_waiting = false;
    }

    // Make sure calls to the API know we've started!
    ctx->run = true;
}

static void
c4m_subproc_do_exec(c4m_subproc_t *ctx)
{
    if (ctx->envp) {
        execve(ctx->cmd, ctx->argv, ctx->envp);
    }
    else {
        execv(ctx->cmd, ctx->argv);
    }
    // If we get past the exec, kill the subproc, which will
    // tear down the switchboard.
    abort();
}

c4m_party_t *
c4m_subproc_new_party_callback(c4m_switchboard_t *ctx, c4m_sb_cb_t cb)
{
    c4m_party_t *result = (c4m_party_t *)calloc(sizeof(c4m_party_t), 1);
    c4m_sb_init_party_callback(ctx, result, cb);

    return result;
}

static void
c4m_subproc_install_callbacks(c4m_subproc_t *ctx)
{
    c4m_deferred_cb_t *entry = ctx->deferred_cbs;

    while (entry) {
        entry->to_free = c4m_subproc_new_party_callback(&ctx->sb, entry->cb);
        if (entry->which & C4M_SP_IO_STDIN) {
            c4m_sb_route(&ctx->sb, &ctx->parent_stdin, entry->to_free);
        }
        if (entry->which & C4M_SP_IO_STDOUT) {
            c4m_sb_route(&ctx->sb, &ctx->subproc_stdout, entry->to_free);
        }
        if (entry->which & C4M_SP_IO_STDERR) {
            c4m_sb_route(&ctx->sb, &ctx->subproc_stderr, entry->to_free);
        }
        entry = entry->next;
    }
}

static void
run_startup_callback(c4m_subproc_t *ctx)
{
    if (ctx->startup_callback) {
        (*ctx->startup_callback)(ctx);
    }
}

static void
c4m_subproc_spawn_fork(c4m_subproc_t *ctx)
{
    pid_t pid;
    int   stdin_pipe[2];
    int   stdout_pipe[2];
    int   stderr_pipe[2];

    pipe(stdin_pipe);
    pipe(stdout_pipe);
    pipe(stderr_pipe);

    pid = fork();

    if (pid != 0) {
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        c4m_sb_init_party_fd(&ctx->sb,
                             &ctx->subproc_stdin,
                             stdin_pipe[1],
                             O_WRONLY,
                             false,
                             true,
                             ctx->proxy_stdin_close);
        c4m_sb_init_party_fd(&ctx->sb,
                             &ctx->subproc_stdout,
                             stdout_pipe[0],
                             O_RDONLY,
                             false,
                             true,
                             false);
        c4m_sb_init_party_fd(&ctx->sb,
                             &ctx->subproc_stderr,
                             stderr_pipe[0],
                             O_RDONLY,
                             false,
                             true,
                             false);

        c4m_sb_monitor_pid(&ctx->sb,
                           pid,
                           &ctx->subproc_stdin,
                           &ctx->subproc_stdout,
                           &ctx->subproc_stderr,
                           true);
        c4m_subproc_install_callbacks(ctx);
        setup_subscriptions(ctx, false);
        run_startup_callback(ctx);
    }
    else {
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        dup2(stdin_pipe[0], 0);
        dup2(stdout_pipe[1], 1);
        dup2(stderr_pipe[1], 2);

        c4m_subproc_do_exec(ctx);
    }
}

static void
c4m_subproc_spawn_forkpty(c4m_subproc_t *ctx)
{
    struct winsize  wininfo;
    struct termios *term_ptr = ctx->child_termcap;
    struct winsize *win_ptr  = &wininfo;
    pid_t           pid;
    int             pty_fd;
    int             stdin_pipe[2];

    tcgetattr(0, &ctx->saved_termcap);

    if (ctx->pty_stdin_pipe) {
        pipe(stdin_pipe);
    }

    // We're going to use a pipe for stderr to get a separate
    // stream. The tty FD will be stdin and stdout for the child
    // process.
    //
    // Also, if we want to close the subproc's stdin after an initial
    // write, we will dup2.
    //
    // Note that this means the child process will see isatty() return
    // true for stdin and stdout, but not stderr.
    if (!isatty(0)) {
        win_ptr = NULL;
    }
    else {
        ioctl(0, TIOCGWINSZ, win_ptr);
    }

    pid = forkpty(&pty_fd, NULL, term_ptr, win_ptr);

    if (pid != 0) {
        if (ctx->pty_stdin_pipe) {
            close(stdin_pipe[0]);
            c4m_sb_init_party_fd(&ctx->sb,
                                 &ctx->subproc_stdin,
                                 stdin_pipe[1],
                                 O_WRONLY,
                                 false,
                                 true,
                                 ctx->proxy_stdin_close);
        }

        ctx->pty_fd = pty_fd;

        c4m_sb_init_party_fd(&ctx->sb,
                             &ctx->subproc_stdout,
                             pty_fd,
                             O_RDWR,
                             true,
                             true,
                             false);

        c4m_sb_monitor_pid(&ctx->sb,
                           pid,
                           &ctx->subproc_stdout,
                           &ctx->subproc_stdout,
                           NULL,
                           true);
        c4m_subproc_install_callbacks(ctx);
        setup_subscriptions(ctx, true);

        if (!ctx->parent_termcap) {
            c4m_termcap_set_raw_mode(&ctx->saved_termcap);
        }
        else {
            tcsetattr(1, TCSAFLUSH, ctx->parent_termcap);
        }
        int flags = fcntl(pty_fd, F_GETFL, 0) | O_NONBLOCK;
        fcntl(pty_fd, F_SETFL, flags);
        run_startup_callback(ctx);
    }
    else {
        setvbuf(stdout, NULL, _IONBF, (size_t)0);
        setvbuf(stdin, NULL, _IONBF, (size_t)0);

        if (ctx->pty_stdin_pipe) {
            close(stdin_pipe[1]);
            dup2(stdin_pipe[0], 0);
        }

        signal(SIGHUP, SIG_DFL);
        signal(SIGINT, SIG_DFL);
        signal(SIGILL, SIG_DFL);
        signal(SIGABRT, SIG_DFL);
        signal(SIGFPE, SIG_DFL);
        signal(SIGKILL, SIG_DFL);
        signal(SIGSEGV, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        signal(SIGALRM, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
        signal(SIGCONT, SIG_DFL);
        signal(SIGSTOP, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGWINCH, SIG_DFL);
        c4m_subproc_do_exec(ctx);
    }
}

/*
 * Start a subprocess if you want to be responsible for making
 * sufficient calls to poll for IP, instead of having it run to
 * completion.
 *
 * If you use this, call c4m_subproc_poll() until it returns false
 */
void
c4m_subproc_start(c4m_subproc_t *ctx)
{
    if (ctx->use_pty) {
        c4m_subproc_spawn_forkpty(ctx);
    }
    else {
        c4m_subproc_spawn_fork(ctx);
    }
}

/*
 * Handle IO on the subprocess a single time. This is meant to be
 * called only when manually runnng the subprocess; if you call
 * c4m_subproc_run, don't use this interface!
 */
bool
c4m_subproc_poll(c4m_subproc_t *ctx)
{
    return c4m_sb_operate_switchboard(&ctx->sb, false);
}

/*
 * Spawns a process, and runs it until the process has ended. The
 * process must first be set up with `c4m_subproc_init()` and you may
 * configure it with other `c4m_subproc_*()` calls before running.
 *
 * The results can be queried via the `c4m_subproc_get_*()` API.
 */
void
c4m_subproc_run(c4m_subproc_t *ctx)
{
    c4m_subproc_start(ctx);
    c4m_sb_operate_switchboard(&ctx->sb, true);
}

void
c4m_subproc_reset_terminal(c4m_subproc_t *ctx)
{
    // Post-run cleanup.
    if (ctx->use_pty) {
        tcsetattr(0, TCSANOW, &ctx->saved_termcap);
    }
}
/*
 * This destroys any allocated memory inside a `subproc` object.  You
 * should *not* call this until you're done with the `c4m_capture_result_t`
 * object, as any dynamic memory (like string captures) that you
 * haven't taken ownership of will get freed when you call this.
 *
 * This call *will* destroy to c4m_capture_result_t object.
 *
 * However, this does *not* free the `c4m_subproc_t` object itself.
 */
void
c4m_subproc_close(c4m_subproc_t *ctx)
{
    c4m_subproc_reset_terminal(ctx);
    c4m_sb_destroy(&ctx->sb, false);

    c4m_deferred_cb_t *cbs = ctx->deferred_cbs;
    c4m_deferred_cb_t *next;

    while (cbs) {
        next = cbs->next;
        free(cbs->to_free);
        free(cbs);
        cbs = next;
    }
}

/*
 * Return the PID of the current subprocess.  Returns -1 if the
 * subprocess hasn't been launched.
 */
pid_t
c4m_subproc_get_pid(c4m_subproc_t *ctx)
{
    c4m_monitor_t *subproc = ctx->sb.pid_watch_list;

    if (!subproc) {
        return -1;
    }
    return subproc->pid;
}

/*
 * If you've got captures under the given tag name, then this will
 * return whatever was captured. If nothing was captured, it will
 * return a NULL pointer.
 *
 * But if a capture is returned, it will have been allocated via
 * `malloc()` and you will be responsible for calling `free()`.
 */
char *
c4m_sp_result_capture(c4m_capture_result_t *ctx, char *tag, size_t *outlen)
{
    for (int i = 0; i < ctx->num_captures; i++) {
        if (!strcmp(tag, ctx->captures[i].tag)) {
            *outlen = ctx->captures[i].len;
            return ctx->captures[i].contents;
        }
    }

    *outlen = 0;
    return NULL;
}

char *
c4m_subproc_get_capture(c4m_subproc_t *ctx, char *tag, size_t *outlen)
{
    c4m_sb_get_results(&ctx->sb, &ctx->result);
    return c4m_sp_result_capture(&ctx->result, tag, outlen);
}

int
c4m_subproc_get_exit(c4m_subproc_t *ctx, bool wait_for_exit)
{
    c4m_monitor_t *subproc = ctx->sb.pid_watch_list;

    if (!subproc) {
        return -1;
    }

    c4m_subproc_status_check(subproc, wait_for_exit);
    return subproc->exit_status;
}

int
c4m_subproc_get_errno(c4m_subproc_t *ctx, bool wait_for_exit)
{
    c4m_monitor_t *subproc = ctx->sb.pid_watch_list;

    if (!subproc) {
        return -1;
    }

    c4m_subproc_status_check(subproc, wait_for_exit);
    return subproc->found_errno;
}

int
c4m_subproc_get_signal(c4m_subproc_t *ctx, bool wait_for_exit)
{
    c4m_monitor_t *subproc = ctx->sb.pid_watch_list;

    if (!subproc) {
        return -1;
    }

    c4m_subproc_status_check(subproc, wait_for_exit);
    return subproc->term_signal;
}

void
c4m_subproc_set_parent_termcap(c4m_subproc_t *ctx, struct termios *tc)
{
    ctx->parent_termcap = tc;
}

void
c4m_subproc_set_child_termcap(c4m_subproc_t *ctx, struct termios *tc)
{
    ctx->child_termcap = tc;
}

void
c4m_subproc_set_extra(c4m_subproc_t *ctx, void *extra)
{
    c4m_sb_set_extra(&ctx->sb, extra);
}

void *
c4m_subproc_get_extra(c4m_subproc_t *ctx)
{
    return c4m_sb_get_extra(&ctx->sb);
}

#ifdef C4M_SB_TEST
void
c4m_capture_tty_data(c4m_switchboard_t *sb,
                     c4m_party_t       *party,
                     char              *data,
                     size_t             len)
{
    printf("Callback got %d bytes from fd %d\n", len, c4m_sb_party_fd(party));
}

int
test1()
{
    char                 *cmd    = "/bin/cat";
    char                 *args[] = {"/bin/cat", "../aes.nim", 0};
    c4m_subproc_t         ctx;
    c4m_capture_result_t *result;
    struct timeval        timeout = {.tv_sec = 0, .tv_usec = 1000};

    c4m_subproc_init(&ctx, cmd, args, true);
    c4m_subproc_use_pty(&ctx);
    c4m_subproc_set_passthrough(&ctx, C4M_SP_IO_ALL, false);
    c4m_subproc_set_capture(&ctx, C4M_SP_IO_ALL, false);
    c4m_subproc_set_timeout(&ctx, &timeout);
    c4m_subproc_set_io_callback(&ctx, C4M_SP_IO_STDOUT, capture_tty_data);

    result = c4m_subproc_run(&ctx);

    while (result) {
        if (result->tag) {
            print_hex(result->contents, result->content_len, result->tag);
        }
        else {
            printf("PID: %d\n", result->pid);
            printf("Exit status: %d\n", result->exit_status);
        }
        result = result->next;
    }
    return 0;
}

int
test2()
{
    char *cmd    = "/bin/cat";
    char *args[] = {"/bin/cat", "-", 0};

    c4m_subproc_t         ctx;
    c4m_capture_result_t *result;
    struct timeval        timeout = {.tv_sec = 0, .tv_usec = 1000};

    c4m_subproc_init(&ctx, cmd, args, true);
    c4m_subproc_set_passthrough(&ctx, C4M_SP_IO_ALL, false);
    c4m_subproc_set_capture(&ctx, C4M_SP_IO_ALL, false);
    c4m_subproc_pass_to_stdin(&ctx, test_txt, strlen(test_txt), true);
    c4m_subproc_set_timeout(&ctx, &timeout);
    c4m_subproc_set_io_callback(&ctx, C4M_SP_IO_STDOUT, capture_tty_data);

    result = c4m_subproc_run(&ctx);

    while (result) {
        if (result->tag) {
            print_hex(result->contents, result->content_len, result->tag);
        }
        else {
            printf("PID: %d\n", result->pid);
            printf("Exit status: %d\n", result->exit_status);
        }
        result = result->next;
    }
    return 0;
}

int
test3()
{
    char                 *cmd    = "/usr/bin/less";
    char                 *args[] = {"/usr/bin/less", "../aes.nim", 0};
    c4m_subproc_t         ctx;
    c4m_capture_result_t *result;
    struct timeval        timeout = {.tv_sec = 0, .tv_usec = 1000};

    c4m_subproc_init(&ctx, cmd, args, true);
    c4m_subproc_use_pty(&ctx);
    c4m_subproc_set_passthrough(&ctx, C4M_SP_IO_ALL, false);
    c4m_subproc_set_capture(&ctx, C4M_SP_IO_ALL, false);
    c4m_subproc_set_timeout(&ctx, &timeout);
    c4m_subproc_set_io_callback(&ctx, C4M_SP_IO_STDOUT, capture_tty_data);

    result = c4m_subproc_run(&ctx);

    while (result) {
        if (result->tag) {
            print_hex(result->contents, result->content_len, result->tag);
        }
        else {
            printf("PID: %d\n", result->pid);
            printf("Exit status: %d\n", result->exit_status);
        }
        result = result->next;
    }
    return 0;
}

int
test4()
{
    char *cmd    = "/bin/cat";
    char *args[] = {"/bin/cat", "-", 0};

    c4m_subproc_t         ctx;
    c4m_capture_result_t *result;
    struct timeval        timeout = {.tv_sec = 0, .tv_usec = 1000};

    c4m_subproc_init(&ctx, cmd, args, true);
    c4m_subproc_use_pty(&ctx);
    c4m_subproc_set_passthrough(&ctx, C4M_SP_IO_ALL, false);
    c4m_subproc_set_capture(&ctx, C4M_SP_IO_ALL, false);
    c4m_subproc_set_timeout(&ctx, &timeout);
    c4m_subproc_set_io_callback(&ctx, C4M_SP_IO_STDOUT, capture_tty_data);

    result = c4m_subproc_run(&ctx);

    while (result) {
        if (result->tag) {
            print_hex(result->contents, result->content_len, result->tag);
        }
        else {
            printf("PID: %d\n", result->pid);
            printf("Exit status: %d\n", result->exit_status);
        }
        result = result->next;
    }
    return 0;
}

int
main()
{
    test1();
    test2();
    test3();
    test4();
}
#endif

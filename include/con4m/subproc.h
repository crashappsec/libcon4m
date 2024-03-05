#pragma once

#include <con4m.h>

extern void subproc_init(subprocess_t *, char *, char *[], bool);
extern bool subproc_set_envp(subprocess_t *, char *[]);
extern bool subproc_pass_to_stdin(subprocess_t *, char *, size_t, bool);
extern bool subproc_set_passthrough(subprocess_t *, unsigned char, bool);
extern bool subproc_set_capture(subprocess_t *, unsigned char, bool);
extern bool subproc_set_io_callback(subprocess_t *, unsigned char,
                                    switchboard_cb_t);
extern void subproc_set_timeout(subprocess_t *, struct timeval *);
extern void subproc_clear_timeout(subprocess_t *);
extern bool subproc_use_pty(subprocess_t *);
extern bool subproc_set_startup_callback(subprocess_t *, void (*)(void *));
extern int  subproc_get_pty_fd(subprocess_t *);
extern void subproc_start(subprocess_t *);
extern bool subproc_poll(subprocess_t *);
extern void subproc_run(subprocess_t *);
extern void subproc_close(subprocess_t *);
extern pid_t subproc_get_pid(subprocess_t *);
extern char *sp_result_capture(sp_result_t *, char *, size_t *);
extern char *subproc_get_capture(subprocess_t *, char *, size_t *);
extern int subproc_get_exit(subprocess_t *, bool);
extern int subproc_get_errno(subprocess_t *, bool);
extern int subproc_get_signal(subprocess_t *, bool);
extern void subproc_set_parent_termcap(subprocess_t *, struct termios *);
extern void subproc_set_child_termcap(subprocess_t *, struct termios *);
extern void subproc_set_extra(subprocess_t *, void *);
extern void *subproc_get_extra(subprocess_t *);
extern int subproc_get_pty_fd(subprocess_t *);
extern void pause_passthrough(subprocess_t *, unsigned char);
extern void resume_passthrough(subprocess_t *, unsigned char);
extern void pause_capture(subprocess_t *, unsigned char);
extern void resume_capture(subprocess_t *, unsigned char);
extern void termcap_get(struct termios *);
extern void termcap_set(struct termios *);
extern void termcap_set_raw_mode(struct termios *);
extern void process_status_check(monitor_t *, bool);

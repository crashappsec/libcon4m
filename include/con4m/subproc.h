#pragma once

#include "con4m.h"

extern void  c4m_subproc_init(subprocess_t *, char *, char *[], bool);
extern bool  c4m_subproc_set_envp(subprocess_t *, char *[]);
extern bool  c4m_subproc_pass_to_stdin(subprocess_t *, char *, size_t, bool);
extern bool  c4m_subproc_set_passthrough(subprocess_t *, unsigned char, bool);
extern bool  c4m_subproc_set_capture(subprocess_t *, unsigned char, bool);
extern bool  c4m_subproc_set_io_callback(subprocess_t *,
                                         unsigned char,
                                         switchboard_cb_t);
extern void  c4m_subproc_set_timeout(subprocess_t *, struct timeval *);
extern void  c4m_subproc_clear_timeout(subprocess_t *);
extern bool  c4m_subproc_use_pty(subprocess_t *);
extern bool  c4m_subproc_set_startup_callback(subprocess_t *, void (*)(void *));
extern int   c4m_subproc_get_pty_fd(subprocess_t *);
extern void  c4m_subproc_start(subprocess_t *);
extern bool  c4m_subproc_poll(subprocess_t *);
extern void  c4m_subproc_run(subprocess_t *);
extern void  c4m_subproc_close(subprocess_t *);
extern pid_t c4m_subproc_get_pid(subprocess_t *);
extern char *c4m_sp_result_capture(sp_result_t *, char *, size_t *);
extern char *c4m_subproc_get_capture(subprocess_t *, char *, size_t *);
extern int   c4m_subproc_get_exit(subprocess_t *, bool);
extern int   c4m_subproc_get_errno(subprocess_t *, bool);
extern int   c4m_subproc_get_signal(subprocess_t *, bool);
extern void  c4m_subproc_set_parent_termcap(subprocess_t *, struct termios *);
extern void  c4m_subproc_set_child_termcap(subprocess_t *, struct termios *);
extern void  c4m_subproc_set_extra(subprocess_t *, void *);
extern void *c4m_subproc_get_extra(subprocess_t *);
extern int   c4m_subproc_get_pty_fd(subprocess_t *);
extern void  c4m_subproc_pause_passthrough(subprocess_t *, unsigned char);
extern void  c4m_subproc_resume_passthrough(subprocess_t *, unsigned char);
extern void  c4m_subproc_pause_capture(subprocess_t *, unsigned char);
extern void  c4m_subproc_resume_capture(subprocess_t *, unsigned char);
extern void  c4m_subproc_status_check(monitor_t *, bool);

#pragma once

#include "con4m.h"

extern void  c4m_subproc_init(c4m_subproc_t *, char *, char *[], bool);
extern bool  c4m_subproc_set_envp(c4m_subproc_t *, char *[]);
extern bool  c4m_subproc_pass_to_stdin(c4m_subproc_t *, char *, size_t, bool);
extern bool  c4m_subproc_set_passthrough(c4m_subproc_t *, unsigned char, bool);
extern bool  c4m_subproc_set_capture(c4m_subproc_t *, unsigned char, bool);
extern bool  c4m_subproc_set_io_callback(c4m_subproc_t *,
                                         unsigned char,
                                         c4m_sb_cb_t);
extern void  c4m_subproc_set_timeout(c4m_subproc_t *, struct timeval *);
extern void  c4m_subproc_clear_timeout(c4m_subproc_t *);
extern bool  c4m_subproc_use_pty(c4m_subproc_t *);
extern bool  c4m_subproc_set_startup_callback(c4m_subproc_t *,
                                              void (*)(void *));
extern int   c4m_subproc_get_pty_fd(c4m_subproc_t *);
extern void  c4m_subproc_start(c4m_subproc_t *);
extern bool  c4m_subproc_poll(c4m_subproc_t *);
extern void  c4m_subproc_run(c4m_subproc_t *);
extern void  c4m_subproc_close(c4m_subproc_t *);
extern pid_t c4m_subproc_get_pid(c4m_subproc_t *);
extern char *c4m_sp_result_capture(c4m_capture_result_t *, char *, size_t *);
extern char *c4m_subproc_get_capture(c4m_subproc_t *, char *, size_t *);
extern int   c4m_subproc_get_exit(c4m_subproc_t *, bool);
extern int   c4m_subproc_get_errno(c4m_subproc_t *, bool);
extern int   c4m_subproc_get_signal(c4m_subproc_t *, bool);
extern void  c4m_subproc_set_parent_termcap(c4m_subproc_t *, struct termios *);
extern void  c4m_subproc_set_child_termcap(c4m_subproc_t *, struct termios *);
extern void  c4m_subproc_set_extra(c4m_subproc_t *, void *);
extern void *c4m_subproc_get_extra(c4m_subproc_t *);
extern int   c4m_subproc_get_pty_fd(c4m_subproc_t *);
extern void  c4m_subproc_pause_passthrough(c4m_subproc_t *, unsigned char);
extern void  c4m_subproc_resume_passthrough(c4m_subproc_t *, unsigned char);
extern void  c4m_subproc_pause_capture(c4m_subproc_t *, unsigned char);
extern void  c4m_subproc_resume_capture(c4m_subproc_t *, unsigned char);
extern void  c4m_subproc_status_check(c4m_monitor_t *, bool);

#pragma once

#include <con4m.h>

extern ssize_t read_one(int, char *, size_t);
extern bool write_data(int, char *, size_t);
extern int party_fd(party_t *party);
extern void sb_init_party_listener(switchboard_t *, party_t *, int,
	 		        accept_cb_t, bool, bool);
extern party_t * sb_new_party_listener(switchboard_t *, int, accept_cb_t,
				       bool, bool);
extern void sb_init_party_fd(switchboard_t *, party_t *, int, int,
			     bool, bool, bool);
extern party_t *sb_new_party_fd(switchboard_t *, int, int, bool, bool, bool);
extern void sb_init_party_input_buf(switchboard_t *, party_t *, char *,
				    size_t, bool, bool, bool);
extern party_t *sb_new_party_input_buf(switchboard_t *, char *, size_t,
				       bool, bool, bool);
extern void sb_party_input_buf_new_string(party_t *, char *, size_t,
					  bool, bool);
extern void sb_init_party_output_buf(switchboard_t *, party_t *, char *,
				     size_t);
extern party_t *sb_new_party_output_buf(switchboard_t *, char *, size_t);
extern void sb_init_party_callback(switchboard_t *, party_t *,
				   switchboard_cb_t);
extern party_t *sb_new_party_callback(switchboard_t *, switchboard_cb_t);
extern void sb_monitor_pid(switchboard_t *, pid_t, party_t *, party_t *,
			   party_t *, bool);
extern void *sb_get_extra(switchboard_t *);
extern void sb_set_extra(switchboard_t *, void *);
extern void *sb_get_party_extra(party_t *);
extern void sb_set_party_extra(party_t *, void *);
extern bool sb_route(switchboard_t *, party_t *, party_t *);
extern bool sb_pause_route(switchboard_t *, party_t *, party_t *);
extern bool sb_resume_route(switchboard_t *, party_t *, party_t *);
extern bool sb_route_is_active(switchboard_t *, party_t *, party_t *);
extern bool sb_route_is_paused(switchboard_t *, party_t *, party_t *);
extern bool sb_route_is_subscribed(switchboard_t *, party_t *, party_t *);
extern void sb_init(switchboard_t *, size_t);
extern void sb_set_io_timeout(switchboard_t *, struct timeval *);
extern void sb_clear_io_timeout(switchboard_t *);
extern void sb_destroy(switchboard_t *, bool);
extern bool sb_operate_switchboard(switchboard_t *, bool);
extern void sb_get_results(switchboard_t *, sb_result_t *);
extern char *sb_result_get_capture(sb_result_t *, char *, bool);
extern void sb_result_destroy(sb_result_t *);
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

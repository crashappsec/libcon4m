#pragma once

#include "con4m.h"

extern ssize_t  c4m_sb_read_one(int, char *, size_t);
extern bool     c4m_sb_write_data(int, char *, size_t);
extern int      c4m_sb_party_fd(party_t *party);
extern void     c4m_sb_init_party_listener(switchboard_t *,
                                           party_t *,
                                           int,
                                           accept_cb_t,
                                           bool,
                                           bool);
extern party_t *c4m_sb_new_party_listener(switchboard_t *,
                                          int,
                                          accept_cb_t,
                                          bool,
                                          bool);
extern void     c4m_sb_init_party_fd(switchboard_t *,
                                     party_t *,
                                     int,
                                     int,
                                     bool,
                                     bool,
                                     bool);
extern party_t *c4m_sb_new_party_fd(switchboard_t *,
                                    int,
                                    int,
                                    bool,
                                    bool,
                                    bool);
extern void     c4m_sb_init_party_input_buf(switchboard_t *,
                                            party_t *,
                                            char *,
                                            size_t,
                                            bool,
                                            bool,
                                            bool);
extern party_t *c4m_sb_new_party_input_buf(switchboard_t *,
                                           char *,
                                           size_t,
                                           bool,
                                           bool,
                                           bool);
extern void     c4m_sb_party_input_buf_new_string(party_t *,
                                                  char *,
                                                  size_t,
                                                  bool,
                                                  bool);
extern void     c4m_sb_init_party_output_buf(switchboard_t *,
                                             party_t *,
                                             char *,
                                             size_t);
extern party_t *c4m_sb_new_party_output_buf(switchboard_t *,
                                            char *,
                                            size_t);
extern void     c4m_sb_init_party_callback(switchboard_t *,
                                           party_t *,
                                           switchboard_cb_t);
extern party_t *c4m_sb_new_party_callback(switchboard_t *, switchboard_cb_t);
extern void     c4m_sb_monitor_pid(switchboard_t *,
                                   pid_t,
                                   party_t *,
                                   party_t *,
                                   party_t *,
                                   bool);
extern void    *c4m_sb_get_extra(switchboard_t *);
extern void     c4m_sb_set_extra(switchboard_t *, void *);
extern void    *c4m_sb_get_party_extra(party_t *);
extern void     c4m_sb_set_party_extra(party_t *, void *);
extern bool     c4m_sb_route(switchboard_t *, party_t *, party_t *);
extern bool     c4m_sb_pause_route(switchboard_t *, party_t *, party_t *);
extern bool     c4m_sb_resume_route(switchboard_t *, party_t *, party_t *);
extern bool     c4m_sb_route_is_active(switchboard_t *, party_t *, party_t *);
extern bool     c4m_sb_route_is_paused(switchboard_t *, party_t *, party_t *);
extern bool     c4m_sb_route_is_subscribed(switchboard_t *,
                                           party_t *,
                                           party_t *);
extern void     c4m_sb_init(switchboard_t *, size_t);
extern void     c4m_sb_set_io_timeout(switchboard_t *, struct timeval *);
extern void     c4m_sb_clear_io_timeout(switchboard_t *);
extern void     c4m_sb_destroy(switchboard_t *, bool);
extern bool     c4m_sb_operate_switchboard(switchboard_t *, bool);
extern void     c4m_sb_get_results(switchboard_t *, sb_result_t *);
extern char    *c4m_sb_result_get_capture(sb_result_t *, char *, bool);
extern void     c4m_sb_result_destroy(sb_result_t *);

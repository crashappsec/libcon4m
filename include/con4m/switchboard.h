#pragma once

#include "con4m.h"

extern ssize_t      c4m_sb_read_one(int, char *, size_t);
extern bool         c4m_sb_write_data(int, char *, size_t);
extern int          c4m_sb_party_fd(c4m_party_t *party);
extern void         c4m_sb_init_party_listener(c4m_switchboard_t *,
                                               c4m_party_t *,
                                               int,
                                               c4m_accept_cb_t,
                                               bool,
                                               bool);
extern c4m_party_t *c4m_sb_new_party_listener(c4m_switchboard_t *,
                                              int,
                                              c4m_accept_cb_t,
                                              bool,
                                              bool);
extern void         c4m_sb_init_party_fd(c4m_switchboard_t *,
                                         c4m_party_t *,
                                         int,
                                         int,
                                         bool,
                                         bool,
                                         bool);
extern c4m_party_t *c4m_sb_new_party_fd(c4m_switchboard_t *,
                                        int,
                                        int,
                                        bool,
                                        bool,
                                        bool);
extern void         c4m_sb_init_party_input_buf(c4m_switchboard_t *,
                                                c4m_party_t *,
                                                char *,
                                                size_t,
                                                bool,
                                                bool);
extern c4m_party_t *c4m_sb_new_party_input_buf(c4m_switchboard_t *,
                                               char *,
                                               size_t,
                                               bool,
                                               bool);
extern void         c4m_sb_party_input_buf_new_string(c4m_party_t *,
                                                      char *,
                                                      size_t,
                                                      bool,
                                                      bool);
extern void         c4m_sb_init_party_output_buf(c4m_switchboard_t *,
                                                 c4m_party_t *,
                                                 char *,
                                                 size_t);
extern c4m_party_t *c4m_sb_new_party_output_buf(c4m_switchboard_t *,
                                                char *,
                                                size_t);
extern void         c4m_sb_init_party_callback(c4m_switchboard_t *,
                                               c4m_party_t *,
                                               c4m_sb_cb_t);
extern c4m_party_t *c4m_sb_new_party_callback(c4m_switchboard_t *, c4m_sb_cb_t);
extern void         c4m_sb_monitor_pid(c4m_switchboard_t *,
                                       pid_t,
                                       c4m_party_t *,
                                       c4m_party_t *,
                                       c4m_party_t *,
                                       bool);
extern void        *c4m_sb_get_extra(c4m_switchboard_t *);
extern void         c4m_sb_set_extra(c4m_switchboard_t *, void *);
extern void        *c4m_sb_get_party_extra(c4m_party_t *);
extern void         c4m_sb_set_party_extra(c4m_party_t *, void *);
extern bool         c4m_sb_route(c4m_switchboard_t *,
                                 c4m_party_t *,
                                 c4m_party_t *);
extern bool         c4m_sb_pause_route(c4m_switchboard_t *,
                                       c4m_party_t *,
                                       c4m_party_t *);
extern bool         c4m_sb_resume_route(c4m_switchboard_t *,
                                        c4m_party_t *,
                                        c4m_party_t *);
extern bool         c4m_sb_route_is_active(c4m_switchboard_t *,
                                           c4m_party_t *,
                                           c4m_party_t *);
extern bool         c4m_sb_route_is_paused(c4m_switchboard_t *,
                                           c4m_party_t *,
                                           c4m_party_t *);
extern bool         c4m_sb_route_is_subscribed(c4m_switchboard_t *,
                                               c4m_party_t *,
                                               c4m_party_t *);
extern void         c4m_sb_init(c4m_switchboard_t *, size_t);
extern void         c4m_sb_set_io_timeout(c4m_switchboard_t *,
                                          struct timeval *);
extern void         c4m_sb_clear_io_timeout(c4m_switchboard_t *);
extern void         c4m_sb_destroy(c4m_switchboard_t *, bool);
extern bool         c4m_sb_operate_switchboard(c4m_switchboard_t *, bool);
extern void         c4m_sb_get_results(c4m_switchboard_t *,
                                       c4m_capture_result_t *);
extern char        *c4m_sb_result_get_capture(c4m_capture_result_t *,
                                              char *,
                                              bool);
extern void         c4m_sb_result_destroy(c4m_capture_result_t *);

#pragma once
#include "con4m.h"

extern c4m_duration_t *c4m_now(void);
extern c4m_duration_t *c4m_timestamp(void);
extern c4m_duration_t *c4m_process_cpu(void);
extern c4m_duration_t *c4m_thread_cpu(void);
extern c4m_duration_t *c4m_uptime(void);
extern c4m_duration_t *c4m_program_clock(void);
extern void            c4m_init_program_timestamp(void);
extern c4m_duration_t *c4m_duration_diff(c4m_duration_t *,
                                         c4m_duration_t *);

#pragma once
#include "con4m.h"

#ifdef C4M_DEBUG

// Basic polling watchpoint support. May eventually flesh out more;
// may put it in the VM loop, and poll after any instructions that do
// memory writes.  For now, it's just a prototype to help me hunt down
// a Heisenbug that is afraid of debuggers.

#ifndef C4M_WATCH_SLOTS
#define C4M_WATCH_SLOTS 30
#endif

#ifndef C4M_WATCH_LOG_SZ
#define C4M_WATCH_LOG_SZ (1 << 14)
#endif

typedef enum : int8_t {
    c4m_wa_ignore,
    c4m_wa_log,
    c4m_wa_print,
    c4m_wa_abort,
} c4m_watch_action;

typedef struct {
    void            *address;
    uint64_t         value;
    c4m_watch_action read_action;
    c4m_watch_action write_action;
    int              last_write;
} c4m_watch_target;

typedef struct {
    char    *file;
    int      line;
    uint64_t start_value;
    uint64_t seen_value;
    int64_t  logid;
    bool     changed;
} c4m_watch_log_t;

extern void _c4m_watch_set(void            *addr,
                           int              ix,
                           c4m_watch_action ra,
                           c4m_watch_action wa,
                           bool             print,
                           char            *file,
                           int              line);

extern void _c4m_watch_scan(char *file, int line);

#define c4m_watch_set(p, ix, ra, wa, print) \
    _c4m_watch_set(p, ix, ra, wa, print, __FILE__, __LINE__)

#define c4m_watch_scan() _c4m_watch_scan(__FILE__, __LINE__)

#endif

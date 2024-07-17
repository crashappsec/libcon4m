#pragma once

#include "con4m.h"

extern void c4m_terminal_dimensions(size_t *cols, size_t *rows);
extern void c4m_termcap_set_raw_mode(struct termios *termcap);

static inline size_t
c4m_terminal_width()
{
    size_t result;

    c4m_terminal_dimensions(&result, NULL);

    return result;
}

static inline void
c4m_termcap_get(struct termios *termcap)
{
    tcgetattr(0, termcap);
}

static inline void
c4m_termcap_set(struct termios *termcap)
{
    tcsetattr(0, TCSANOW, termcap);
}

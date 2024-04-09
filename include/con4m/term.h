#pragma once

#include "con4m.h"

extern void terminal_dimensions(size_t *cols, size_t *rows);
extern void termcap_set_raw_mode(struct termios *termcap);

static inline size_t
terminal_width()
{
    size_t result;

    terminal_dimensions(&result, NULL);

    return result;
}

#pragma once

#include <con4m.h>

extern void terminal_dimensions(size_t *cols, size_t *rows);
extern void termcap_set_raw_mode(struct termios *termcap);

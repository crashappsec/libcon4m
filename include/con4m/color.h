#pragma once

#include "con4m.h"

#define use_truecolor() (1)

extern color_t lookup_color(utf8_t *);
extern color_t to_vga(color_t truecolor);

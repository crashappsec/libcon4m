#pragma once

#include <con4m.h>

#define use_truecolor() (1)


extern color_t lookup_color(char *);
extern color_t to_vga(int32_t truecolor);

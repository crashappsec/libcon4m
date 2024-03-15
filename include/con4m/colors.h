#pragma once

#include <con4m.h>
// Temporary.
typedef int32_t color_t;

#define use_truecolor() (1)

typedef struct {
    const char *name;
    int32_t rgb;
} color_info_t;

extern color_t lookup_color(char *);
extern color_t to_vga(int32_t truecolor);

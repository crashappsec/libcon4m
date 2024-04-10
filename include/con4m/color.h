#pragma once

#include "con4m.h"

#define c4m_use_truecolor() (1)

extern c4m_color_t c4m_lookup_color(utf8_t *);
extern c4m_color_t c4m_to_vga(c4m_color_t truecolor);

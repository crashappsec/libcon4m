#pragma once
#include "con4m.h"

typedef uint64_t c4m_style_t;

typedef struct {
    int32_t     start;
    int32_t     end;
    c4m_style_t info; // 16 bits of flags, 24 bits bg color, 24 bits fg color
} c4m_style_entry_t;

typedef struct {
    int64_t           num_entries;
    c4m_style_entry_t styles[];
} c4m_style_info_t;

#define C4M_BORDER_TOP          0x01
#define C4M_BORDER_BOTTOM       0x02
#define C4M_BORDER_LEFT         0x04
#define C4M_BORDER_RIGHT        0x08
#define C4M_INTERIOR_HORIZONTAL 0x10
#define C4M_INTERIOR_VERTICAL   0x20

typedef enum : int8_t {
    C4M_ALIGN_IGNORE = 0,
    C4M_ALIGN_LEFT   = 1,
    C4M_ALIGN_RIGHT  = 2,
    C4M_ALIGN_CENTER = 4,
    C4M_ALIGN_TOP    = 8,
    C4M_ALIGN_BOTTOM = 16,
    C4M_ALIGN_MIDDLE = 32,

    C4M_ALIGN_TOP_LEFT   = C4M_ALIGN_LEFT | C4M_ALIGN_TOP,
    C4M_ALIGN_TOP_RIGHT  = C4M_ALIGN_RIGHT | C4M_ALIGN_TOP,
    C4M_ALIGN_TOP_CENTER = C4M_ALIGN_CENTER | C4M_ALIGN_TOP,

    C4M_ALIGN_MID_LEFT   = C4M_ALIGN_LEFT | C4M_ALIGN_MIDDLE,
    C4M_ALIGN_MID_RIGHT  = C4M_ALIGN_RIGHT | C4M_ALIGN_MIDDLE,
    C4M_ALIGN_MID_CENTER = C4M_ALIGN_CENTER | C4M_ALIGN_MIDDLE,

    C4M_ALIGN_BOTTOM_LEFT   = C4M_ALIGN_LEFT | C4M_ALIGN_BOTTOM,
    C4M_ALIGN_BOTTOM_RIGHT  = C4M_ALIGN_RIGHT | C4M_ALIGN_BOTTOM,
    C4M_ALIGN_BOTTOM_CENTER = C4M_ALIGN_CENTER | C4M_ALIGN_BOTTOM,
} c4m_alignment_t;

#define C4M_HORIZONTAL_MASK \
    (C4M_ALIGN_LEFT | C4M_ALIGN_CENTER | C4M_ALIGN_RIGHT)
#define C4M_VERTICAL_MASK \
    (C4M_ALIGN_TOP | C4M_ALIGN_MIDDLE | C4M_ALIGN_BOTTOM)

typedef enum : uint8_t {
    C4M_DIM_UNSET,
    C4M_DIM_AUTO,
    C4M_DIM_PERCENT_TRUNCATE,
    C4M_DIM_PERCENT_ROUND,
    C4M_DIM_FLEX_UNITS,
    C4M_DIM_ABSOLUTE,
    C4M_DIM_ABSOLUTE_RANGE,
    C4M_DIM_FIT_TO_TEXT
} c4m_dimspec_kind_t;

typedef struct border_theme_t {
    struct border_theme_t *next_style;
    char                  *name;
    int32_t                horizontal_rule;
    int32_t                vertical_rule;
    int32_t                upper_left;
    int32_t                upper_right;
    int32_t                lower_left;
    int32_t                lower_right;
    int32_t                cross;
    int32_t                top_t;
    int32_t                bottom_t;
    int32_t                left_t;
    int32_t                right_t;
} c4m_border_theme_t;

typedef uint8_t c4m_border_set_t;

// This is a 'repository' for style info that can be applied to grids
// or strings. When we apply to strings, anything grid-related gets
// ignored.

// This compacts grid style info, but currently that is internal.
// Some of the items apply to text, some apply to renderables.

typedef struct {
    struct c4m_str_t   *name;
    c4m_border_theme_t *border_theme;
    c4m_color_t         pad_color;
    c4m_style_t         base_style;

    union {
        float    percent;
        uint64_t units;
        int32_t  range[2];
    } dims;

    int8_t top_pad;
    int8_t bottom_pad;
    int8_t left_pad;
    int8_t right_pad;
    int8_t wrap;

    int weight_fg;
    int weight_bg;
    int weight_flags;
    int weight_align;
    int weight_width;
    int weight_borders;

    // Eventually we'll add more in like z-ordering and transparency.
    c4m_alignment_t    alignment     : 7;
    c4m_dimspec_kind_t dim_kind      : 3;
    c4m_border_set_t   borders       : 6;
    uint8_t            pad_color_set : 1;
    uint8_t            disable_wrap  : 1;
    uint8_t            tpad_set      : 1; // These prevent overrides when
    uint8_t            bpad_set      : 1; // the pad value is 0. If it's not
    uint8_t            lpad_set      : 1; // 0, this gets ignored.
    uint8_t            rpad_set      : 1;
    uint8_t            hang_set      : 1;
} c4m_render_style_t;

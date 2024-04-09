#pragma once
#include <con4m.h>

typedef uint64_t style_t;

typedef struct {
    int32_t start;
    int32_t end;
    style_t info; // 16 bits of flags, 24 bits bg color, 24 bits fg color
} style_entry_t;

typedef struct {
    int64_t       num_entries;
    style_entry_t styles[];
} style_info_t;

#define BORDER_TOP          0x01
#define BORDER_BOTTOM       0x02
#define BORDER_LEFT         0x04
#define BORDER_RIGHT        0x08
#define INTERIOR_HORIZONTAL 0x10
#define INTERIOR_VERTICAL   0x20

typedef enum : int8_t {
    ALIGN_IGNORE = 0,
    ALIGN_LEFT   = 1,
    ALIGN_RIGHT  = 2,
    ALIGN_CENTER = 4,
    ALIGN_TOP    = 8,
    ALIGN_BOTTOM = 16,
    ALIGN_MIDDLE = 32,

    ALIGN_TOP_LEFT   = ALIGN_LEFT | ALIGN_TOP,
    ALIGN_TOP_RIGHT  = ALIGN_RIGHT | ALIGN_TOP,
    ALIGN_TOP_CENTER = ALIGN_CENTER | ALIGN_TOP,

    ALIGN_MID_LEFT   = ALIGN_LEFT | ALIGN_MIDDLE,
    ALIGN_MID_RIGHT  = ALIGN_RIGHT | ALIGN_MIDDLE,
    ALIGN_MID_CENTER = ALIGN_CENTER | ALIGN_MIDDLE,

    ALIGN_BOTTOM_LEFT   = ALIGN_LEFT | ALIGN_BOTTOM,
    ALIGN_BOTTOM_RIGHT  = ALIGN_RIGHT | ALIGN_BOTTOM,
    ALIGN_BOTTOM_CENTER = ALIGN_CENTER | ALIGN_BOTTOM,
} alignment_t;

#define HORIZONTAL_MASK (ALIGN_LEFT | ALIGN_CENTER | ALIGN_RIGHT)
#define VERTICAL_MASK   (ALIGN_TOP | ALIGN_MIDDLE | ALIGN_BOTTOM)

typedef enum : uint8_t {
    DIM_UNSET,
    DIM_AUTO,
    DIM_PERCENT_TRUNCATE,
    DIM_PERCENT_ROUND,
    DIM_FLEX_UNITS,
    DIM_ABSOLUTE,
    DIM_ABSOLUTE_RANGE,
    DIM_FIT_TO_TEXT
} dimspec_kind_t;

typedef struct border_theme_t {
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
    struct border_theme_t *next_style;
} border_theme_t;

typedef uint8_t border_set_t;

// This is a 'repository' for style info that can be applied to grids
// or strings. When we apply to strings, anything grid-related gets
// ignored.

// This compacts grid style info, but currently that is internal.
// Some of the items apply to text, some apply to renderables.

typedef struct {
    char           *name;
    border_theme_t *border_theme;
    style_t         base_style;
    color_t         pad_color;

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

    // Eventually we'll add more in like z-ordering and transparency.
    alignment_t    alignment     : 7;
    dimspec_kind_t dim_kind      : 3;
    border_set_t   borders       : 6;
    uint8_t        pad_color_set : 1;
    uint8_t        disable_wrap  : 1;
    uint8_t        tpad_set      : 1; // These prevent overrides when
    uint8_t        bpad_set      : 1; // the pad value is 0. If it's not
    uint8_t        lpad_set      : 1; // 0, this gets ignored.
    uint8_t        rpad_set      : 1;
    uint8_t        hang_set      : 1;
} render_style_t;

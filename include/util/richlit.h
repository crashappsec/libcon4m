#pragma once
#include "con4m.h"

typedef struct {
    c4m_utf8_t *name;
    union {
        c4m_style_t style;
        c4m_color_t color;
        int         kw_ix;
    } contents;
    uint32_t    flags;
    c4m_style_t prev_style;

} c4m_tag_item_t;

typedef struct c4m_fmt_frame_t {
    struct c4m_fmt_frame_t *next;
    c4m_utf8_t             *raw_contents;
    int32_t                 start;
    int32_t                 end;
    c4m_style_t             style; // Calculated incremental style from a tag.
} c4m_fmt_frame_t;

typedef struct {
    c4m_fmt_frame_t *start_frame;
    c4m_fmt_frame_t *cur_frame;
    c4m_list_t      *style_directions;
    c4m_utf8_t      *style_text;
    c4m_tag_item_t **stack;
    c4m_utf8_t      *raw;
    c4m_style_t      cur_style;
    int              stack_ix;
} c4m_style_ctx;

typedef struct {
    c4m_utf8_t      *raw;
    c4m_list_t      *tokens;
    c4m_fmt_frame_t *cur_frame;
    c4m_style_ctx   *style_ctx;
    c4m_utf8_t      *not_matched;
    int              num_toks;
    int              tok_ix;
    int              num_atoms;
    bool             negating;
    bool             at_start;
    bool             got_percent;
    c4m_style_t      cur_style;
} c4m_tag_parse_ctx;

#define C4M_F_NEG         (1 << 1)
#define C4M_F_BGCOLOR     (1 << 2)
#define C4M_F_STYLE_KW    (1 << 3)
#define C4M_F_STYLE_CELL  (1 << 4)
#define C4M_F_STYLE_COLOR (1 << 5)
#define C4M_F_TAG_START   (1 << 6)
#define C4M_F_POPPED      (1 << 7)

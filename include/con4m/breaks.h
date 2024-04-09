#pragma once

#include "con4m.h"

// break_info_t is in datatypes/strings.h

extern const int c4m_minimum_break_slots;

extern break_info_t *c4m_get_grapheme_breaks(const any_str_t *,
                                             int32_t,
                                             int32_t);
extern break_info_t *c4m_get_line_breaks(const any_str_t *);
extern break_info_t *c4m_get_all_line_break_ops(const any_str_t *);
extern break_info_t *c4m_wrap_text(const any_str_t *, int32_t, int32_t);

static inline break_info_t *
c4m_alloc_break_structure(const any_str_t *s, int shift)
{
    break_info_t *result;
    int32_t       alloc_slots = max(c4m_str_codepoint_len(s) >> shift,
                              c4m_minimum_break_slots);

    result = c4m_gc_flex_alloc(break_info_t, int32_t, alloc_slots, NULL);

    result->num_slots  = alloc_slots;
    result->num_breaks = 0;

    return result;
}

static inline break_info_t *
c4m_grow_break_structure(break_info_t *breaks)
{
    int32_t new_slots = breaks->num_slots * 2;

    break_info_t *res;

    res = c4m_gc_flex_alloc(break_info_t, int32_t, new_slots, NULL);

    res->num_slots  = new_slots;
    res->num_breaks = breaks->num_breaks;

    memcpy(res->breaks, breaks->breaks, breaks->num_slots * sizeof(int32_t));

    return res;
}

static inline void
c4m_add_break(break_info_t **listp, int32_t br)
{
    break_info_t *breaks = *listp;

    if (breaks->num_slots == breaks->num_breaks) {
        breaks = c4m_grow_break_structure(breaks);
        *listp = breaks;
    }

    breaks->breaks[breaks->num_breaks++] = br;
}

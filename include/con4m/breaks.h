#pragma once

#include <con4m/str.h>

extern const int minimum_break_slots;

typedef struct break_info_st {
    int32_t num_slots;
    int32_t num_breaks;
    int32_t breaks[];
} break_info_t;

extern break_info_t * get_grapheme_breaks(str_t *instr, int32_t start_ix,
					  int32_t end_ix);

extern break_info_t *get_line_breaks(str_t *instr);
extern break_info_t *get_all_line_break_ops(str_t *instr);
extern break_info_t * wrap_text(str_t *s, int32_t width, int32_t hang);

static inline break_info_t *
alloc_break_structure(real_str_t *s, int shift)
{
    break_info_t *result;
    int32_t       alloc_slots = max(internal_num_cp(s) >> shift,
				    minimum_break_slots);

    result = (break_info_t *)zalloc(alloc_slots * sizeof(int32_t) +
				    sizeof(break_info_t));

    result->num_slots  = alloc_slots;
    result->num_breaks = 0;

    return result;
}

static inline break_info_t *
grow_break_structure(break_info_t *breaks)
{
    int32_t new_slots = breaks->num_slots * 2;

    break_info_t *res = zalloc(new_slots * sizeof(int32_t) +
			       sizeof(break_info_t));

    res->num_slots  = new_slots;
    res->num_breaks = breaks->num_breaks;

    memcpy(res->breaks, breaks->breaks, breaks->num_slots * sizeof(int32_t));

    free(breaks);

    return res;
}

static inline void
add_break(break_info_t **listp, int32_t br)
{
    break_info_t *breaks = *listp;

    if (breaks->num_slots == breaks->num_breaks) {
	breaks = grow_break_structure(breaks);
        *listp = breaks;
    }

    breaks->breaks[breaks->num_breaks++] = br;
}

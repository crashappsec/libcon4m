#include <con4m.h>

// Assume a possible break every 2^n codepoints when allocating, but
// be prepared to alloc extra if needed.
//
// n will vary based on the operation; for a grapheme break list, we
// would expect almost 1:1 in most text, but word boundaries (for line
// breaks) n = 2 probably minimizes our need to realloc.
//
// And, if we do run out of room, we double the number of slots, from
// this initial estimate.
//
// For long text, this might suck a bit, but it's probably better than
// lots of little allocs.
//
// Over time, if it seems like we neeed to, we may make this smarter.

const int minimum_break_slots = 16;

break_info_t *
get_grapheme_breaks(str_t *instr, int32_t start_ix, int32_t end_ix)
{
    real_str_t   *s     = to_internal(instr);
    break_info_t *res   = alloc_break_structure(s, 1);
    int32_t       state = 0;
    int32_t       cps   = internal_num_cp(s);
    int32_t       len;

    if (start_ix < 0) {
	start_ix += cps;
    }

    if (end_ix <= 0) {
	end_ix += cps;
    }

    if (start_ix > cps || end_ix <= start_ix) {
	return res;
    }

    len = end_ix - start_ix;

    if (len < 2) {
	add_break(&res, end_ix);
	return res;
    }

    if (internal_is_u32(s)) {
	int32_t *p1 = ((int32_t *)(s->data)) + start_ix;
	int32_t *p2 = (int32_t *)(p1 + 1);
	for (int i = 1; i < len; i++) {
	    if (utf8proc_grapheme_break_stateful(*p1, *p2, &state)) {
		add_break(&res, start_ix + i);
	    }
	    p1++;
	    p2++;
	}
	return res;
    }
    else {
	int32_t  prev;
	int32_t  cur;
	int32_t  i   = 0;
	uint8_t *p   = (uint8_t *)s->data;

	while (i <= start_ix) {
	    p += utf8proc_iterate(p, 4, &prev);
	    i++;
	}

	while(i < end_ix) {
	    p += utf8proc_iterate(p, 4, &cur);
	    if (utf8proc_grapheme_break_stateful(prev, cur, &state)) {
		add_break(&res, i);
	    }
	    i   += 1;
	    prev = cur;
	}
	return res;
    }
}

static inline bool
internal_is_line_break(int32_t cp) {
    if (cp == '\n' || cp == '\r') {
	return true;
    }

    switch (utf8proc_category(cp)) {
    case CP_CATEGORY_ZL:
    case CP_CATEGORY_ZP:
	return true;
    default:
	return false;
    }
}

break_info_t *
get_line_breaks(str_t *instr)
{
    real_str_t   *s   = to_internal(instr);
    break_info_t *res = alloc_break_structure(s, 6); // 2^6 = 64.
    int32_t       l   = internal_num_cp(s);

    if (internal_is_u32(s)) {
	int32_t *p = (int32_t *)(s->data);
	for (int i = 0; i < l; i++) {
	    if (internal_is_line_break(p[i])) {
		add_break(&res, i);
	    }
	}
    }
    else {
	int32_t  cp;
	uint8_t *p  = (uint8_t *)s->data;

	for (int i = 0; i < l; i++) {
	    p += utf8proc_iterate(p, 4, &cp);
	    if (internal_is_line_break(cp)) {
		add_break(&res, i);
	    }
	}
    }
    return res;
}

break_info_t *
get_all_line_break_ops(str_t *instr)
{
    real_str_t   *s      = to_internal(instr);
    int32_t       l      = internal_num_cp(s);
    break_info_t *res    = alloc_break_structure(s, 0);
    char         *br_raw;

    if (internal_is_u32(s)) {
	br_raw = (char *)zalloc(l);
        assert (s != NULL);
	set_linebreaks_utf32((int32_t *)s->data, l, "en", br_raw);
    }
    else {
	br_raw = (char *)zalloc(s->byte_len);
	set_linebreaks_utf8_per_code_point((int8_t *)s->data, s->byte_len,
					   "en", br_raw);
    }

    for (int i = 0; i < l; i++) {
	if (br_raw[i] < LB_NOBREAK) {
	    add_break(&res, i);
	}
    }

    if (br_raw != NULL) {
        free(br_raw);
    }

    return res;
}

/**
 ** This doesn't directly change the string; it instead
 ** returns a list of codepoints that start lines, when
 ** wrapping to a given width.
 **
 ** This is a very simple best-fit algorithm right now.
 **
 ** If there is an actual newline in the contents, it is always
 ** returned, which can lead to short lines. This is done because we
 ** expect this to represent a paragraph break.
 **/
break_info_t *
wrap_text(str_t *s, int32_t width, int32_t hang)
{
    break_info_t *line_breaks  = get_line_breaks(s);
    break_info_t *break_ops    = get_all_line_break_ops(s);
    int32_t       n            = 32 - __builtin_clz(width);
    real_str_t   *r            = to_internal(s);
    int32_t       l            = internal_num_cp(r);
    break_info_t *res          = alloc_break_structure(r, n);
    int32_t       cur_start    = 0;
    int32_t       last_ok_br   = 0;
    int32_t       lb_ix        = 0;
    int32_t       bo_ix        = 0;
    int32_t       hard_wrap_ix = width;
    int32_t       hang_width   = width - hang;
    int32_t       next_lb;


    if (line_breaks->num_breaks == 0) {
	next_lb = l;
    }
    else {
	next_lb = line_breaks->breaks[0];
	lb_ix   = 1;
    }

    add_break(&res, 0);

    while (cur_start < l) {
    find_next_break:
	last_ok_br = cur_start;

	while (bo_ix < break_ops->num_breaks) {
	    int32_t cur_break = break_ops->breaks[bo_ix];

	    if (cur_break >= hard_wrap_ix) {
		if (last_ok_br == cur_start) {
		    // No valid break; hard wrap it.
		    add_break(&res, hard_wrap_ix);
		    cur_start    = hard_wrap_ix;
		    hard_wrap_ix = cur_start + hang_width;
		    goto find_next_break;
		}
		else {
		    add_break(&res, last_ok_br);
		    cur_start    = last_ok_br;
		    hard_wrap_ix = cur_start + hang_width;
		    goto find_next_break;
		}
	    }

	    if (next_lb == cur_break) {
		add_break(&res, next_lb + 1);
		cur_start = next_lb + 1;
		if (lb_ix == line_breaks->num_breaks) {
		    next_lb = l;
		}
		else {
		    next_lb = line_breaks->breaks[lb_ix++];
		}
		hard_wrap_ix = cur_start + hang_width;
		bo_ix++;
		goto find_next_break;
	    }

	    last_ok_br = cur_break + 1;
	    bo_ix++;
	}
	break;
    }

    while (true) {
	if (l == cur_start) {
	    break;
	}
	if (l <= hard_wrap_ix) {
	    break;
	}

	if (last_ok_br != cur_start) {
	    add_break(&res, last_ok_br);
	    cur_start    = last_ok_br;
	    hard_wrap_ix = cur_start + hang_width;
	}
	else {
	    add_break(&res, hard_wrap_ix);
	    cur_start    = hard_wrap_ix;
	    hard_wrap_ix = cur_start + hang_width;
	}
    }

    free(line_breaks);
    free(break_ops);

    if (internal_is_u32(r)) {
	return res;
    }

    uint8_t *start = (uint8_t *)r->data;
    uint8_t *p     = start;
    int32_t  z     = 0;
    int32_t  i     = 0;
    int32_t  cp;

    while (z < res->num_breaks) {
	if (i == res->breaks[z]) {
	    res->breaks[z] = p - start;
	    z++;
	}
	p += utf8proc_iterate(p, 4, &cp);
	i++;
    }

    return res;
}

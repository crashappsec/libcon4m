#include "con4m.h"

typedef struct {
    c4m_xlist_t     *captures;
    c4m_tree_node_t *tree_cur;
    c4m_tpat_node_t *pattern_cur;
    c4m_cmp_fn       cmp;
} search_ctx_t;

#undef C4M_DEBUG_PATTERNS
#ifdef C4M_DEBUG_PATTERNS
#define tpat_debug(ctx, txt) c4m_print(c4m_cstr_format( \
    "{:x} : [em]{}[/]",                                 \
    c4m_box_u64((uint64_t)ctx->pattern_cur),            \
    c4m_new_utf8(txt),                                  \
    0))
#else
#define tpat_debug(ctx, txt)
#endif

#define tpat_varargs(result)                                                 \
    va_list args;                                                            \
    int16_t num_kids = 0;                                                    \
                                                                             \
    va_start(args, capture);                                                 \
    while (va_arg(args, c4m_tpat_node_t *) != NULL) {                        \
        num_kids++;                                                          \
    }                                                                        \
                                                                             \
    va_end(args);                                                            \
                                                                             \
    result->num_kids = num_kids;                                             \
                                                                             \
    if (num_kids) {                                                          \
        result->children = c4m_gc_array_alloc(c4m_tpat_node_t **, num_kids); \
        va_start(args, capture);                                             \
                                                                             \
        for (int i = 0; i < num_kids; i++) {                                 \
            result->children[i] = va_arg(args, c4m_tpat_node_t *);           \
        }                                                                    \
                                                                             \
        va_end(args);                                                        \
    }

c4m_tree_node_t *
c4m_pat_repr(c4m_tpat_node_t   *pat,
             c4m_pattern_fmt_fn content_formatter)
{
    c4m_utf8_t *op       = NULL;
    c4m_utf8_t *contents = (*content_formatter)(pat->contents);
    c4m_utf8_t *capture;

    if (pat->walk) {
        op = c4m_new_utf8(">>>");
    }
    else {
        if (pat->min == 1 && pat->max == 1) {
            if (pat->ignore_kids) {
                op = c4m_new_utf8(".");
            }
            else {
                op = c4m_new_utf8("...");
            }
        }
        if (pat->min == 0 && pat->max == 1) {
            if (pat->ignore_kids) {
                op = c4m_new_utf8("?");
            }
            else {
                op = c4m_new_utf8("???");
            }
        }
        if (pat->min == 0 && pat->max == 0x7fff) {
            if (pat->ignore_kids) {
                op = c4m_new_utf8("*");
            }
            else {
                op = c4m_new_utf8("***");
            }
        }
        if (pat->min == 0 && pat->max == 0x7fff) {
            if (pat->ignore_kids) {
                op = c4m_new_utf8("*");
            }
            else {
                op = c4m_new_utf8("***");
            }
        }
        if (pat->min == 1 && pat->max == 0x7fff) {
            if (pat->ignore_kids) {
                op = c4m_new_utf8("+");
            }
            else {
                op = c4m_new_utf8("+++");
            }
        }
    }
    if (op == NULL) {
        if (pat->ignore_kids) {
            op = c4m_cstr_format("{}:{}", pat->min, pat->max);
        }
        else {
            op = c4m_cstr_format("{}:::{}", pat->min, pat->max);
        }
    }

    if (pat->capture) {
        capture = c4m_new_utf8(" !");
    }
    else {
        capture = c4m_new_utf8(" ");
    }

    c4m_utf8_t *txt = c4m_cstr_format("{:x} [em]{}{}[/] :",
                                      c4m_box_u64((uint64_t)pat),
                                      op,
                                      capture);

    txt = c4m_str_concat(txt, contents);

    c4m_tree_node_t *result = c4m_new(c4m_tspec_tree(c4m_tspec_utf8()),
                                      c4m_kw("contents", c4m_ka(txt)));

    for (int i = 0; i < pat->num_kids; i++) {
        c4m_tree_node_t *kid = c4m_pat_repr(pat->children[i],
                                            content_formatter);
        c4m_tree_adopt_node(result, kid);
    }

    return result;
}

static inline c4m_xlist_t *
merge_captures(c4m_xlist_t *l1, c4m_xlist_t *l2)
{
    c4m_xlist_t *result = c4m_xlist_plus(l1, l2);
    return result;
}

static inline c4m_tpat_node_t *
tpat_base(void *contents, int16_t min, int16_t max, bool walk, int capture)
{
    c4m_tpat_node_t *result = c4m_gc_alloc(c4m_tpat_node_t);
    result->min             = min;
    result->max             = max;
    result->contents        = contents;

    if (walk) {
        result->walk = 1;
    }
    if (capture) {
        result->capture = 1;
    }

    return result;
}

c4m_tpat_node_t *
_c4m_tpat_find(void *contents, int capture, ...)
{
    c4m_tpat_node_t *result = tpat_base(contents, 1, 1, true, capture);
    tpat_varargs(result);

    return result;
}

c4m_tpat_node_t *
_c4m_tpat_match(void *contents, int capture, ...)
{
    c4m_tpat_node_t *result = tpat_base(contents, 1, 1, false, capture);
    tpat_varargs(result);

    return result;
}

c4m_tpat_node_t *
_c4m_tpat_opt_match(void *contents, int capture, ...)
{
    c4m_tpat_node_t *result = tpat_base(contents, 0, 1, false, capture);
    tpat_varargs(result);

    return result;
}

c4m_tpat_node_t *
_c4m_tpat_n_m_match(void *contents, int16_t min, int16_t max, int capture, ...)
{
    c4m_tpat_node_t *result = tpat_base(contents, min, max, false, capture);
    tpat_varargs(result);

    return result;
}

c4m_tpat_node_t *
c4m_tpat_content_find(void *contents, int capture)
{
    c4m_tpat_node_t *result = tpat_base(contents, 1, 1, true, capture);
    result->ignore_kids     = 1;

    return result;
}

c4m_tpat_node_t *
c4m_tpat_content_match(void *contents, int capture)
{
    c4m_tpat_node_t *result = tpat_base(contents, 1, 1, false, capture);
    result->ignore_kids     = 1;

    return result;
}

c4m_tpat_node_t *
c4m_tpat_opt_content_match(void *contents, int capture)
{
    c4m_tpat_node_t *result = tpat_base(contents, 0, 1, false, capture);
    result->ignore_kids     = 1;

    return result;
}

c4m_tpat_node_t *
c4m_tpat_n_m_content_match(void   *contents,
                           int16_t min,
                           int16_t max,
                           int     capture)
{
    c4m_tpat_node_t *result = tpat_base(contents, min, max, false, capture);
    result->ignore_kids     = 1;

    return result;
}

// This is a naive implementation. I won't use patterns that trigger the
// non-linear cost, but if this gets more general purpose use, should probably
// implement a closer-to-optimal general purpose search.

static bool full_match(search_ctx_t *, void *);

bool
c4m_tree_match(c4m_tree_node_t *tree,
               c4m_tpat_node_t *pat,
               c4m_cmp_fn       cmp,
               c4m_xlist_t    **match_loc)
{
    search_ctx_t search_state = {
        .tree_cur    = tree,
        .pattern_cur = pat,
        .cmp         = cmp,
        .captures    = NULL,
    };

    tpat_debug((&search_state), "start");

    if (pat->min != 1 && pat->max != 1) {
        C4M_CRAISE("Pattern root must be a single node (non-optional) match.");
    }

    bool result = full_match(&search_state, pat->contents);

    if (match_loc != NULL) {
        *match_loc = search_state.captures;
    }

    if (result) {
        tpat_debug((&search_state), "end: success!");
    }
    else {
        tpat_debug((&search_state), "end: fail :(");
    }
    return result;
}

static inline bool
content_matches(search_ctx_t *ctx, void *contents)
{
    return (*ctx->cmp)(contents, ctx->tree_cur);
}

static inline void
capture(search_ctx_t *ctx, c4m_tree_node_t *node)
{
    if (ctx->captures == NULL) {
        ctx->captures = c4m_new(c4m_tspec_xlist(c4m_tspec_ref()));
    }

    c4m_xlist_append(ctx->captures, node);
}

static int
count_consecutive_matches(search_ctx_t    *ctx,
                          c4m_tree_node_t *parent,
                          int              next_child,
                          void            *contents,
                          int              max,
                          c4m_xlist_t    **captures)
{
    c4m_xlist_t *saved_captures     = ctx->captures;
    c4m_xlist_t *per_match_captures = NULL;
    int          result             = 0;

    ctx->captures = NULL;

    if (captures != NULL) {
        per_match_captures = c4m_new(c4m_tspec_xlist(c4m_tspec_ref()));
        *captures          = per_match_captures;
    }

    for (; next_child < parent->num_kids; next_child++) {
        ctx->tree_cur = parent->children[next_child];

        if (!full_match(ctx, contents)) {
            break;
        }

        if (++result == max) {
            break;
        }

        if (captures != NULL) {
            c4m_xlist_append(per_match_captures, ctx->captures);
        }
        ctx->captures = NULL;
    }

    if (captures != NULL) {
        c4m_xlist_append(per_match_captures, ctx->captures);
    }

    ctx->captures = saved_captures;
    return result;
}

static bool
kid_match_from(search_ctx_t    *ctx,
               c4m_tree_node_t *parent,
               c4m_tpat_node_t *parent_pattern,
               int              next_child,
               int              next_pattern,
               void            *contents)
{
    c4m_tpat_node_t *subpattern   = parent_pattern->children[next_pattern];
    c4m_xlist_t     *kid_captures = NULL;
    int              num_matches;

    ctx->pattern_cur = subpattern;
    // We start by seeing how many sequential matches we can find.
    // The call will limit itself to the pattern's 'max' value,
    // but we check the 'min' field in this function after.
    num_matches      = count_consecutive_matches(ctx,
                                            parent,
                                            next_child,
                                            contents,
                                            subpattern->max,
                                            &kid_captures);

    if (num_matches < subpattern->min) {
        return false;
    }

    int kid_capture_ix = 0;

    // Capture any nodes that are definitely part of this match.
    for (int i = next_child; i < subpattern->min; i++) {
        c4m_xlist_t *one_set = c4m_xlist_get(kid_captures,
                                             kid_capture_ix++,
                                             NULL);
        ctx->captures        = merge_captures(ctx->captures, one_set);
    }

    // Here we are looking to advance to the next pattern, but if
    // there isn't one, we just need to know if what we matched consumes
    // all possible child nodes. If it does, then we successfully matched,
    // and if there are leftover child nodes, we did not match.

    if (next_pattern + 1 >= parent_pattern->num_kids) {
        if (next_child + num_matches < parent->num_kids) {
            return false;
        }

        for (int i = kid_capture_ix; i < c4m_xlist_len(kid_captures); i++) {
            c4m_xlist_t *one_set = c4m_xlist_get(kid_captures,
                                                 kid_capture_ix,
                                                 NULL);
            ctx->captures        = merge_captures(ctx->captures, one_set);
        }
        return true;
    }

    // Here, we know we have more patterns to check.
    // If this pattern is an n:m match where n:m, we're going to
    // try every valid match until the rest of the siblings match or
    // we run out of options.
    //
    // We could do better, but it's much more complicated to do so.
    // As is, we do this essentially via recursion;
    //
    // MINIMAL munch... we always return the shortest path to a
    // sibling match.  Meaning, if we do: .(x?, <any_text>*, t) and ask
    // to capture node 1, it will never capture because we first accept
    // the null match, try the rest, and only accept the one-node match
    // if the null match fails.
    //
    // If it turns out there's a good reason for maximal munch, we
    // just need to run this loop backwards.

    next_pattern++;

    next_child += subpattern->min;

    c4m_tpat_node_t *pnew          = parent_pattern->children[next_pattern];
    void            *next_contents = pnew->contents;

    for (int i = subpattern->min; i <= num_matches + subpattern->min; i++) {
        c4m_xlist_t *copy = NULL;

        if (ctx->captures != NULL) {
            copy = c4m_xlist_shallow_copy(ctx->captures);
        }

        if (kid_match_from(ctx,
                           parent,
                           parent_pattern,
                           next_child + i,
                           next_pattern,
                           next_contents)) {
            return true;
        }
        else {
            ctx->captures = copy;

            // Since the next rule didn't work w/ this
            // node that DOES work for us, if there's a capture
            // it's time to stash it.
            c4m_xlist_t *one_set = c4m_xlist_get(kid_captures,
                                                 kid_capture_ix++,
                                                 NULL);
            ctx->captures        = merge_captures(ctx->captures, one_set);
        }
    }

    return false;
}

static inline bool
children_match(search_ctx_t *ctx)
{
    if (ctx->pattern_cur->ignore_kids == 1) {
        return true;
    }

    c4m_tree_node_t *saved_parent  = ctx->tree_cur;
    c4m_tpat_node_t *saved_pattern = ctx->pattern_cur;
    bool             result;

    if (saved_pattern->num_kids == 0) {
        if (saved_parent->num_kids == 0 || saved_pattern->ignore_kids) {
            return true;
        }
        return false;
    }

    void *contents = ctx->pattern_cur->children[0]->contents;

    result = kid_match_from(ctx,
                            saved_parent,
                            saved_pattern,
                            0,
                            0,
                            contents);

    ctx->tree_cur    = saved_parent;
    ctx->pattern_cur = saved_pattern;

    return result;
}

static bool
walk_match(search_ctx_t *ctx, void *contents, bool capture_match)
{
    // This only gets called from full_match, which saves/restores.
    // Since this function always goes down, and never needs to
    // ascend, we don't restore when we're done; full_match does it.

    c4m_tree_node_t *current_tree_node = ctx->tree_cur;

    if (content_matches(ctx, contents)) {
        if (children_match(ctx)) {
            if (capture_match) {
                capture(ctx, current_tree_node);
            }
            return true;
        }
    }

    for (int i = 0; i < current_tree_node->num_kids; i++) {
        ctx->tree_cur = c4m_tree_get_child(current_tree_node, i);

        if (walk_match(ctx, contents, capture_match)) {
            return true;
        }
    }

    return false;
}

static bool
full_match(search_ctx_t *ctx, void *contents)
{
    tpat_debug(ctx, "enter full");
    // Full match doesn't look at min/max; it checks content and
    // children only.

    c4m_tree_node_t *saved_tree_node = ctx->tree_cur;
    c4m_tpat_node_t *saved_pattern   = ctx->pattern_cur;
    c4m_xlist_t     *saved_captures  = ctx->captures;

    bool result = false;
    bool capture_result;

    if (ctx->pattern_cur->walk) {
        capture_result   = (bool)saved_pattern->capture;
        result           = walk_match(ctx, contents, capture_result);
        ctx->tree_cur    = saved_tree_node;
        ctx->pattern_cur = saved_pattern;
        ctx->captures    = merge_captures(saved_captures, ctx->captures);

        if (result == true && capture_result == true) {
            capture(ctx, saved_tree_node);
        }

        tpat_debug(ctx, "exit full 1");
        return result;
    }

    if (!(result = content_matches(ctx, contents))) {
        ctx->tree_cur    = saved_tree_node;
        ctx->pattern_cur = saved_pattern;
        ctx->captures    = saved_captures;
        tpat_debug(ctx, "exit full 2");
        return false;
    }

    capture_result   = (bool)saved_pattern->capture;
    result           = children_match(ctx);
    ctx->captures    = merge_captures(saved_captures, ctx->captures);
    ctx->tree_cur    = saved_tree_node;
    ctx->pattern_cur = saved_pattern;

    if (result == true && capture_result == true) {
        capture(ctx, saved_tree_node);
    }

    tpat_debug(ctx, "exit full 3");
    return result;
}

#include "con4m.h"

typedef struct {
    c4m_tree_node_t *tree_cur;
    c4m_tpat_node_t *pattern_cur;
    c4m_cmp_fn       cmp;
    c4m_xlist_t     *captures;
} search_ctx_t;

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

static inline c4m_xlist_t *
merge_captures(c4m_xlist_t *l1, c4m_xlist_t *l2)
{
    if (l1 == NULL) {
        return l2;
    }
    if (l2 == NULL) {
        return l1;
    }

    return c4m_xlist_plus(l1, l2);
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
    };

    if (pat->min != 1 && pat->max != 1) {
        C4M_CRAISE("Pattern root must be a single node (non-optional) match.");
    }

    bool result = full_match(&search_state, pat->contents);

    if (match_loc != NULL) {
        *match_loc = search_state.captures;
    }

    return result;
}

static inline bool
content_matches(search_ctx_t *ctx, void *contents)
{
    return (*ctx->cmp)(contents, ctx->tree_cur);
}

static inline void
capture(search_ctx_t *ctx)
{
    if (ctx->captures == NULL) {
        ctx->captures = c4m_new(c4m_tspec_xlist(c4m_tspec_ref()));
    }

    c4m_xlist_append(ctx->captures, ctx->tree_cur);
}

static int
count_consecutive_matches(search_ctx_t    *ctx,
                          c4m_tree_node_t *parent,
                          int              next_child,
                          void            *contents,
                          int              max)
{
    int result = 0;
    // pattern_cur is already properly set by our caller.
    for (; next_child < parent->num_kids; next_child++) {
        ctx->tree_cur = parent->children[next_child];

        if (!full_match(ctx, contents)) {
            break;
        }

        if (++result == max) {
            break;
        }
    }

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
    c4m_tpat_node_t *subpattern = parent_pattern->children[next_pattern];
    int              num_matches;

    ctx->pattern_cur = subpattern;
    // We start by seeing how many sequential matches we can find.
    // The call will limit itself to the pattern's 'max' value,
    // but we check the 'min' field in this function after.
    num_matches      = count_consecutive_matches(ctx,
                                            parent,
                                            next_child,
                                            contents,
                                            subpattern->max);

    if (num_matches < subpattern->min) {
        return false;
    }

    // Here we are looking to advance to the next pattern, but if
    // there isn't one, we just need to know if what we matched consumes
    // all possible child nodes. If it does, then we successfully matched,
    // and if there are leftover child nodes, we did not match.
    if (next_pattern + 1 == parent_pattern->num_kids) {
        if (next_child + num_matches < parent_pattern->num_kids) {
            return false;
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

    c4m_tpat_node_t *pnew           = parent_pattern->children[next_pattern];
    void            *next_contents  = pnew->contents;
    c4m_xlist_t     *saved_captures = ctx->captures;
    ctx->captures                   = NULL;

    for (int i = subpattern->min; i <= subpattern->min + num_matches; i++) {
        ctx->captures = NULL;
        if (kid_match_from(ctx,
                           parent,
                           parent_pattern,
                           next_child + i,
                           next_pattern,
                           next_contents)) {
            merge_captures(ctx->captures, saved_captures);
            return true;
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
        if (saved_parent->num_kids == 0) {
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
                capture(ctx);
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
    // Full match doesn't look at min/max; it checks content and
    // children only.

    c4m_tree_node_t *saved_tree_node;
    c4m_tpat_node_t *saved_pattern;
    bool             result;
    bool             capture_result;

    if (ctx->pattern_cur->walk) {
        saved_tree_node  = ctx->tree_cur;
        saved_pattern    = ctx->pattern_cur;
        capture_result   = (bool)saved_pattern->capture;
        result           = walk_match(ctx, contents, capture_result);
        ctx->tree_cur    = saved_tree_node;
        ctx->pattern_cur = saved_pattern;
        return result;
    }

    if (!content_matches(ctx, contents)) {
        return false;
    }

    saved_tree_node  = ctx->tree_cur;
    saved_pattern    = ctx->pattern_cur;
    capture_result   = (bool)saved_pattern->capture;
    result           = children_match(ctx);
    ctx->tree_cur    = saved_tree_node;
    ctx->pattern_cur = saved_pattern;

    if (result == true && capture_result == true) {
        capture(ctx);
    }

    return result;
}

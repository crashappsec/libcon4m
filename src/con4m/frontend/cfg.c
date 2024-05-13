#include "con4m.h"

static void
add_child(c4m_cfg_node_t *parent, c4m_cfg_node_t *child)
{
    c4m_cfg_branch_info_t *branch_info;

    switch (parent->kind) {
    case c4m_cfg_block_entrance:
        parent->contents.block_entrance.next_node = child;
        return;
    case c4m_cfg_block_exit:
        parent->contents.block_exit.next_node = child;
        return;
    case c4m_cfg_node_branch:
        branch_info = &parent->contents.branches;

        branch_info->branch_targets[branch_info->next_to_process++] = child;
        return;
    case c4m_cfg_jump:
        C4M_CRAISE("Reached supposedly unreachable code.");
    default:
        parent->contents.flow.next_node = child;
    }
}

c4m_cfg_node_t *
c4m_cfg_enter_block(c4m_cfg_node_t  *parent,
                    c4m_tree_node_t *treeloc)
{
    c4m_cfg_node_t *result = c4m_gc_alloc(c4m_cfg_node_t);
    c4m_cfg_node_t *exit   = c4m_gc_alloc(c4m_cfg_node_t);

    result->parent                            = parent;
    result->kind                              = c4m_cfg_block_entrance;
    result->reference_location                = treeloc;
    result->contents.block_entrance.exit_node = exit;

    result->contents.block_entrance.inbound_links = c4m_new(
        c4m_tspec_xlist(c4m_tspec_ref()));
    exit->contents.block_exit.inbound_links = c4m_new(
        c4m_tspec_xlist(c4m_tspec_ref()));

    exit->kind = c4m_cfg_block_exit;

    if (parent != NULL) {
        add_child(parent, result);
    }

    return result;
}

c4m_cfg_node_t *
c4m_cfg_exit_block(c4m_cfg_node_t *parent, c4m_tree_node_t *treeloc)
{
    if (parent->kind == c4m_cfg_jump) {
        return NULL;
    }

    c4m_cfg_node_t *cur = parent;

    // Search parents until we find the block start.
    while (cur->kind != c4m_cfg_block_entrance) {
        cur = cur->parent;
    }

    c4m_cfg_node_t *result = parent->contents.block_entrance.exit_node;
    c4m_xlist_append(result->contents.block_exit.inbound_links, parent);

    add_child(parent, result);

    return result;
}

c4m_cfg_node_t *
c4m_cfg_block_new_branch_node(c4m_cfg_node_t *parent,
                              int             num_branches,
                              c4m_utf8_t     *label,

                              c4m_tree_node_t *treeloc)
{
    c4m_cfg_node_t  *result  = c4m_gc_alloc(c4m_cfg_node_t);
    c4m_cfg_node_t **targets = c4m_gc_array_alloc(c4m_cfg_node_t *,
                                                  num_branches);

    result->parent                            = parent;
    result->kind                              = c4m_cfg_node_branch;
    result->reference_location                = treeloc;
    result->contents.branches.num_branches    = num_branches;
    result->contents.branches.branch_targets  = targets;
    result->contents.branches.next_to_process = 0;
    result->contents.branches.label           = label;

    add_child(parent, result);

    return result;
}

c4m_cfg_node_t *
c4m_cfg_add_return(c4m_cfg_node_t *parent, c4m_tree_node_t *treeloc)
{
    c4m_cfg_node_t *result = c4m_gc_alloc(c4m_cfg_node_t);

    add_child(parent, result);

    result->kind               = c4m_cfg_jump;
    result->reference_location = treeloc;
    result->parent             = parent;

    while (parent->parent != NULL) {
        parent = parent->parent;
    }

    c4m_cfg_node_t *target = parent->contents.block_entrance.exit_node;

    c4m_xlist_append(target->contents.block_exit.inbound_links, result);

    result->contents.jump.target = target;

    return result;
}

c4m_cfg_node_t *
c4m_cfg_add_continue(c4m_cfg_node_t  *parent,
                     c4m_tree_node_t *treeloc,
                     c4m_utf8_t      *label)
{
    // Loops are structured as:
    // pre-entry initialization def/use info
    // block-start
    // loop-condition checking def/use info
    // branch_info.
    //
    // But the block-start node doesn't hold the loop info, so we need
    // to check the branch_info node we pass, and when we see it, the
    // next block entrance we find is where continue statements should
    // link to.
    //
    // Note that if the 'continue' statement targets a loop label, and
    // we never find that loop label, we return NULL and the caller is
    // responsible for issuing the error.

    c4m_cfg_node_t *result             = c4m_gc_alloc(c4m_cfg_node_t);
    c4m_cfg_node_t *cur                = parent;
    bool            found_proper_block = false;

    result->kind               = c4m_cfg_jump;
    result->reference_location = treeloc;
    result->parent             = parent;

    add_child(parent, result);

    while (true) {
        switch (cur->kind) {
        case c4m_cfg_block_entrance:
            if (found_proper_block) {
                c4m_xlist_append(cur->contents.block_entrance.inbound_links,
                                 result);
                result->contents.jump.target = cur;
                return result;
            }
            break;
        case c4m_cfg_node_branch:
            if (label == NULL) {
                found_proper_block = true;
                break;
            }
            if (cur->contents.branches.label == NULL) {
                break;
            }

            if (!strcmp(cur->contents.branches.label->data, label->data)) {
                found_proper_block = true;
            }
            break;
        default:
            break;
        }
        if (cur->parent == NULL) {
            return NULL;
            cur = cur->parent;
        }
    }
}

// This should look EXACTLY like continue, except that, when we find the
// right block-enter node, we link to the associated EXIT node instead.
c4m_cfg_node_t *
c4m_cfg_add_break(c4m_cfg_node_t  *parent,
                  c4m_tree_node_t *treeloc,
                  c4m_utf8_t      *label)
{
    c4m_cfg_node_t *result             = c4m_gc_alloc(c4m_cfg_node_t);
    c4m_cfg_node_t *cur                = parent;
    bool            found_proper_block = false;

    add_child(parent, result);

    result->kind               = c4m_cfg_jump;
    result->reference_location = treeloc;
    result->parent             = parent;

    while (true) {
        switch (cur->kind) {
        case c4m_cfg_block_entrance:
            if (found_proper_block) {
                c4m_cfg_node_t *target = cur->contents.block_entrance.exit_node;

                c4m_xlist_append(target->contents.block_exit.inbound_links,
                                 result);
                result->contents.jump.target = target;
                return result;
            }
            break;
        case c4m_cfg_node_branch:
            if (label == NULL) {
                found_proper_block = true;
                break;
            }
            if (cur->contents.branches.label == NULL) {
                break;
            }

            if (!strcmp(cur->contents.branches.label->data, label->data)) {
                found_proper_block = true;
            }
            break;
        default:
            break;
        }
        if (cur->parent == NULL) {
            return NULL;
            cur = cur->parent;
        }
    }
}

c4m_cfg_node_t *
c4m_cfg_add_def(c4m_cfg_node_t    *parent,
                c4m_tree_node_t   *treeloc,
                c4m_scope_entry_t *symbol,
                c4m_xlist_t       *dependencies)
{
    c4m_cfg_node_t *result           = c4m_gc_alloc(c4m_cfg_node_t);
    result->kind                     = c4m_cfg_def;
    result->reference_location       = treeloc;
    result->parent                   = parent;
    result->contents.flow.dst_symbol = symbol;
    result->contents.flow.deps       = dependencies;

    add_child(parent, result);

    return result;
}

c4m_cfg_node_t *
c4m_cfg_add_call(c4m_cfg_node_t    *parent,
                 c4m_tree_node_t   *treeloc,
                 c4m_scope_entry_t *symbol,
                 c4m_xlist_t       *dependencies)
{
    c4m_cfg_node_t *result           = c4m_gc_alloc(c4m_cfg_node_t);
    result->kind                     = c4m_cfg_call;
    result->reference_location       = treeloc;
    result->parent                   = parent;
    result->contents.flow.dst_symbol = symbol;
    result->contents.flow.deps       = dependencies;

    add_child(parent, result);

    return result;
}

c4m_cfg_node_t *
c4m_cfg_add_use(c4m_cfg_node_t    *parent,
                c4m_tree_node_t   *treeloc,
                c4m_scope_entry_t *symbol)
{
    c4m_cfg_node_t *result           = c4m_gc_alloc(c4m_cfg_node_t);
    result->kind                     = c4m_cfg_use;
    result->reference_location       = treeloc;
    result->parent                   = parent;
    result->contents.flow.dst_symbol = symbol;

    add_child(parent, result);

    return result;
}

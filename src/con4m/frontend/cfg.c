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
    if (!parent || parent->kind == c4m_cfg_jump) {
        return NULL;
    }

    c4m_cfg_node_t *cur = parent;

    // Search parents until we find the block start.
    while (cur && cur->kind != c4m_cfg_block_entrance) {
        cur = cur->parent;
    }

    if (!cur) {
        return NULL;
    }

    c4m_cfg_node_t *result = cur->contents.block_entrance.exit_node;
    c4m_xlist_append(result->contents.block_exit.inbound_links, parent);

    add_child(parent, result);

    return result;
}

c4m_cfg_node_t *
c4m_cfg_block_new_branch_node(c4m_cfg_node_t  *parent,
                              int              num_branches,
                              c4m_utf8_t      *label,
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
        }
        cur = cur->parent;
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
        }
        cur = cur->parent;
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

static c4m_tree_node_t *
c4m_cfg_repr_internal(c4m_cfg_node_t  *node,
                      c4m_tree_node_t *tree_parent,
                      c4m_cfg_node_t  *cfg_parent,
                      c4m_utf8_t      *label)
{
    c4m_utf8_t      *str;
    c4m_tree_node_t *result;
    c4m_cfg_node_t  *link;
    uint64_t         node_addr = (uint64_t)(void *)node;
    uint64_t         link_addr;

    switch (node->kind) {
    case c4m_cfg_block_entrance:
        link      = node->contents.block_entrance.exit_node;
        link_addr = (uint64_t)(void *)link;
        str       = c4m_cstr_format("@{:x}: [em]Enter[/]",
                              c4m_box_i64(node_addr));
        break;
    case c4m_cfg_block_exit:
        if (cfg_parent) {
            return NULL;
        }
        str = c4m_cstr_format("@{:x}: [em]Exit",
                              c4m_box_i64(node_addr));
        break;
    case c4m_cfg_node_branch:
        str = c4m_cstr_format("@{:x}: [em]branch",
                              c4m_box_i64(node_addr));
        break;
    case c4m_cfg_use:
        str = c4m_cstr_format("@{:x}: [em]USE[/] {}",
                              c4m_box_i64(node_addr),
                              node->contents.flow.dst_symbol->name);
        break;
    case c4m_cfg_def:
        str = c4m_cstr_format("@{:x}: [em]DEF[/] {}",
                              c4m_box_i64(node_addr),
                              node->contents.flow.dst_symbol->name);
        break;
    case c4m_cfg_call:
        str = c4m_cstr_format("@{:x}: [em]call[/] {}",
                              c4m_box_i64(node_addr),
                              node->contents.flow.dst_symbol->name);
        break;
    case c4m_cfg_jump:
        link      = node->contents.jump.target;
        link_addr = (uint64_t)(void *)link;
        str       = c4m_cstr_format("@{:x}: [em]jmp[/] {:x}",
                              c4m_box_i64(node_addr),
                              c4m_box_i64(link_addr));
    }

    if (node->kind == c4m_cfg_block_entrance) {
        if (label == NULL) {
            label = c4m_new_utf8("block");
        }
        result                = c4m_new(c4m_tspec_tree(c4m_tspec_utf8()),
                         c4m_kw("contents", label));
        c4m_tree_node_t *sub1 = c4m_new(c4m_tspec_tree(c4m_tspec_utf8()),
                                        c4m_kw("contents", c4m_ka(str)));

        c4m_tree_adopt_node(tree_parent, result);
        c4m_tree_adopt_node(result, sub1);
        c4m_cfg_repr_internal(node->contents.flow.next_node, sub1, node, NULL);

        c4m_cfg_repr_internal(node->contents.block_entrance.exit_node,
                              result,
                              NULL,
                              NULL);

        return result;
    }

    result = c4m_new(c4m_tspec_tree(c4m_tspec_utf8()),
                     c4m_kw("contents", c4m_ka(str)));

    c4m_tree_adopt_node(tree_parent, result);

    switch (node->kind) {
    case c4m_cfg_block_entrance:
        unreachable();
    case c4m_cfg_node_branch:
        for (int i = 0; i < node->contents.branches.num_branches; i++) {
            c4m_cfg_node_t *sub = node->contents.branches.branch_targets[i];

            c4m_cfg_repr_internal(sub,
                                  result,
                                  node,
                                  c4m_cstr_format("b{}", c4m_box_i64(i)));
        }
        break;
    case c4m_cfg_jump:
        break;
    case c4m_cfg_block_exit:
        link = node->contents.flow.next_node;

        if (link) {
            c4m_cfg_repr_internal(link, result->parent->parent, node, NULL);
        }
        break;

    default:
        link = node->contents.flow.next_node;

        if (link) {
            c4m_cfg_repr_internal(link, tree_parent, node, NULL);
        }
        break;
    }

    return result;
}

c4m_grid_t *
c4m_cfg_repr(c4m_cfg_node_t *node)
{
    c4m_tree_node_t *root = c4m_new(
        c4m_tspec_tree(c4m_tspec_utf8()),
        c4m_kw("contents", c4m_ka(c4m_new_utf8("Root"))));

    c4m_cfg_repr_internal(node, root, NULL, NULL);
    return c4m_grid_tree(root);
}

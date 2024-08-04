#include "con4m.h"

void
c4m_cfg_gc_bits(uint64_t *bitmap, c4m_cfg_node_t *cfgnode)
{
    switch (cfgnode->kind) {
    case c4m_cfg_block_entrance:
        c4m_mark_raw_to_addr(bitmap,
                             cfgnode,
                             &cfgnode->contents.block_entrance.to_merge);
        return;
    case c4m_cfg_block_exit:
        c4m_mark_raw_to_addr(bitmap,
                             cfgnode,
                             &cfgnode->contents.block_exit.to_merge);
        return;
    case c4m_cfg_node_branch:
        c4m_mark_raw_to_addr(bitmap,
                             cfgnode,
                             &cfgnode->contents.branches.label);
        return;
    case c4m_cfg_jump:
        c4m_mark_raw_to_addr(bitmap, cfgnode, &cfgnode->contents.jump.target);
        return;
    case c4m_cfg_use:
    case c4m_cfg_def:
    case c4m_cfg_call:
        c4m_mark_raw_to_addr(bitmap, cfgnode, &cfgnode->contents.flow.deps);
        return;
    }
}

static c4m_cfg_node_t *
c4m_new_cfg_node()
{
    return c4m_gc_alloc_mapped(c4m_cfg_node_t, c4m_cfg_gc_bits);
}

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
        parent->contents.jump.dead_code = child;
        return;
    default:
        parent->contents.flow.next_node = child;
    }
}

c4m_cfg_node_t *
c4m_cfg_enter_block(c4m_cfg_node_t  *parent,
                    c4m_tree_node_t *treeloc)
{
    c4m_cfg_node_t *result = c4m_new_cfg_node();
    c4m_cfg_node_t *exit   = c4m_new_cfg_node();

    result->parent                            = parent;
    result->kind                              = c4m_cfg_block_entrance;
    result->reference_location                = treeloc;
    result->contents.block_entrance.exit_node = exit;

    result->contents.block_entrance.inbound_links = c4m_new(
        c4m_type_list(c4m_type_ref()));

    exit->reference_location                = treeloc;
    exit->parent                            = result;
    exit->starting_liveness_info            = NULL;
    exit->starting_sometimes                = NULL;
    exit->liveness_info                     = NULL;
    exit->sometimes_live                    = NULL;
    exit->kind                              = c4m_cfg_block_exit;
    exit->contents.block_exit.next_node     = NULL;
    exit->contents.block_exit.entry_node    = result;
    exit->contents.block_exit.to_merge      = NULL;
    exit->contents.block_exit.inbound_links = c4m_new(
        c4m_type_list(c4m_type_ref()));

    if (parent != NULL) {
        add_child(parent, result);
    }

    return result;
}

c4m_cfg_node_t *
c4m_cfg_exit_block(c4m_cfg_node_t  *parent,
                   c4m_cfg_node_t  *entry,
                   c4m_tree_node_t *treeloc)
{
    if (parent == NULL) {
        return NULL;
    }

    if (entry == NULL) {
        c4m_cfg_node_t *cur = parent;

        // Search parents until we find the block start.
        while (cur && cur->kind != c4m_cfg_block_entrance) {
            cur = cur->parent;
        }

        if (!cur) {
            return NULL;
        }
        entry = cur;
    }

    c4m_cfg_node_t *result = entry->contents.block_entrance.exit_node;

    if (parent->kind != c4m_cfg_jump) {
        c4m_list_append(result->contents.block_exit.inbound_links, parent);
        add_child(parent, result);
    }

    return result;
}

c4m_cfg_node_t *
c4m_cfg_block_new_branch_node(c4m_cfg_node_t  *parent,
                              int              num_branches,
                              c4m_utf8_t      *label,
                              c4m_tree_node_t *treeloc)
{
    c4m_cfg_node_t  *result  = c4m_new_cfg_node();
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

    while (parent->kind != c4m_cfg_block_entrance) {
        parent = parent->parent;
    }

    result->contents.branches.exit_node = parent->contents.block_entrance.exit_node;

    return result;
}

c4m_cfg_node_t *
c4m_cfg_add_return(c4m_cfg_node_t  *parent,
                   c4m_tree_node_t *treeloc,
                   c4m_cfg_node_t  *fn_exit_node)
{
    c4m_cfg_node_t *result = c4m_new_cfg_node();

    add_child(parent, result);

    result->kind               = c4m_cfg_jump;
    result->reference_location = treeloc;
    result->parent             = parent;

    c4m_list_append(fn_exit_node->contents.block_exit.inbound_links, result);

    result->contents.jump.target = fn_exit_node;

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

    c4m_cfg_node_t *result             = c4m_new_cfg_node();
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
                c4m_list_append(cur->contents.block_entrance.inbound_links,
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
    c4m_cfg_node_t *result             = c4m_new_cfg_node();
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

                c4m_list_append(target->contents.block_exit.inbound_links,
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
            return result;
        }
        cur = cur->parent;
    }
}

c4m_cfg_node_t *
c4m_cfg_add_def(c4m_cfg_node_t  *parent,
                c4m_tree_node_t *treeloc,
                c4m_symbol_t    *symbol,
                c4m_list_t      *dependencies)
{
    c4m_cfg_node_t *result           = c4m_new_cfg_node();
    result->kind                     = c4m_cfg_def;
    result->reference_location       = treeloc;
    result->parent                   = parent;
    result->contents.flow.dst_symbol = symbol;
    result->contents.flow.deps       = dependencies;

    add_child(parent, result);

    return result;
}

c4m_cfg_node_t *
c4m_cfg_add_call(c4m_cfg_node_t  *parent,
                 c4m_tree_node_t *treeloc,
                 c4m_symbol_t    *symbol,
                 c4m_list_t      *dependencies)
{
    c4m_cfg_node_t *result           = c4m_new_cfg_node();
    result->kind                     = c4m_cfg_call;
    result->reference_location       = treeloc;
    result->parent                   = parent;
    result->contents.flow.dst_symbol = symbol;
    result->contents.flow.deps       = dependencies;

    add_child(parent, result);

    return result;
}

c4m_cfg_node_t *
c4m_cfg_add_use(c4m_cfg_node_t  *parent,
                c4m_tree_node_t *treeloc,
                c4m_symbol_t    *symbol)
{
    if (symbol->kind == C4M_SK_ENUM_VAL) {
        return parent;
    }
    c4m_cfg_node_t *result           = c4m_new_cfg_node();
    result->kind                     = c4m_cfg_use;
    result->reference_location       = treeloc;
    result->parent                   = parent;
    result->contents.flow.dst_symbol = symbol;

    add_child(parent, result);

    return result;
}

static c4m_utf8_t *
du_format_node(c4m_cfg_node_t *n)
{
    c4m_utf8_t *result;

    c4m_dict_t *liveness_info;
    c4m_list_t *sometimes_live;

    switch (n->kind) {
    case c4m_cfg_block_entrance:
        liveness_info  = n->starting_liveness_info;
        sometimes_live = n->starting_sometimes;
        break;
    case c4m_cfg_block_exit:
    case c4m_cfg_use:
    case c4m_cfg_def:
    case c4m_cfg_call:
        liveness_info  = n->liveness_info;
        sometimes_live = n->sometimes_live;
        break;
    default:
        c4m_unreachable();
    }

    if (liveness_info == NULL) {
        return c4m_new_utf8("-");
    }

    uint64_t             num_syms;
    hatrack_dict_item_t *info  = hatrack_dict_items_sort(liveness_info,
                                                        &num_syms);
    c4m_list_t          *cells = c4m_new(c4m_type_list(c4m_type_utf8()));

    for (unsigned int i = 0; i < num_syms; i++) {
        c4m_symbol_t     *sym    = info[i].key;
        c4m_cfg_status_t *status = info[i].value;

        if (status->last_def) {
            c4m_list_append(cells, sym->name);
        }
        else {
            c4m_list_append(cells,
                            c4m_str_concat(sym->name,
                                           c4m_new_utf8(" (err)")));
        }
    }

    if (num_syms == 0) {
        result = c4m_new_utf8("-");
    }
    else {
        if (num_syms) {
            result = c4m_str_join(cells, c4m_new_utf8(", "));
        }
        else {
            result = c4m_rich_lit("[i](none)");
        }
    }

    if (sometimes_live == NULL) {
        return result;
    }

    int num_sometimes = c4m_list_len(sometimes_live);
    if (num_sometimes == 0) {
        return result;
    }

    c4m_list_t *l2 = c4m_new(c4m_type_list(c4m_type_utf8()));

    for (int i = 0; i < num_sometimes; i++) {
        c4m_symbol_t *sym = c4m_list_get(sometimes_live, i, NULL);

        c4m_list_append(l2, sym->name);
    }

    return c4m_cstr_format("{}; st: {}",
                           result,
                           c4m_str_join(l2, c4m_new_utf8(", ")));
}

static c4m_tree_node_t *
c4m_cfg_repr_internal(c4m_cfg_node_t  *node,
                      c4m_tree_node_t *tree_parent,
                      c4m_cfg_node_t  *cfg_parent,
                      c4m_utf8_t      *label)
{
    c4m_utf8_t      *str = NULL;
    c4m_tree_node_t *result;
    c4m_cfg_node_t  *link;
    uint64_t         node_addr = (uint64_t)(void *)node;
    uint64_t         link_addr;

    if (!node) {
        return NULL;
    }

    switch (node->kind) {
    case c4m_cfg_block_entrance:
        link      = node->contents.block_entrance.exit_node;
        link_addr = (uint64_t)(void *)link;
        str       = c4m_cstr_format("@{:x}: [em]Enter[/] [i]({})",
                              c4m_box_i64(node_addr),
                              du_format_node(node));
        if (label == NULL) {
            label = c4m_new_utf8("block");
        }
        result                = c4m_new(c4m_type_tree(c4m_type_utf8()),
                         c4m_kw("contents", label));
        c4m_tree_node_t *sub1 = c4m_new(c4m_type_tree(c4m_type_utf8()),
                                        c4m_kw("contents", c4m_ka(str)));

        c4m_tree_adopt_node(tree_parent, result);
        c4m_tree_adopt_node(result, sub1);
        c4m_cfg_repr_internal(node->contents.flow.next_node, sub1, node, NULL);

        c4m_cfg_repr_internal(node->contents.block_entrance.exit_node,
                              result,
                              NULL,
                              NULL);

        return result;
    case c4m_cfg_node_branch:
        if (node->contents.branches.label != NULL) {
            str = c4m_cstr_format("@{:x}: [em]branch[/] [h1]{}[/]",
                                  c4m_box_i64(node_addr),
                                  node->contents.branches.label);
        }
        else {
            str = c4m_cstr_format("@{:x}: [em]branch",
                                  c4m_box_i64(node_addr));
        }

        result = c4m_new(c4m_type_tree(c4m_type_utf8()),
                         c4m_kw("contents", str));
        c4m_tree_adopt_node(tree_parent, result);

        for (int i = 0; i < node->contents.branches.num_branches; i++) {
            c4m_utf8_t      *label = c4m_cstr_format("b{}", c4m_box_i64(i));
            c4m_tree_node_t *sub   = c4m_new(c4m_type_tree(c4m_type_utf8()),
                                           c4m_kw("contents", c4m_ka(label)));
            c4m_cfg_node_t  *kid   = node->contents.branches.branch_targets[i];

            assert(kid != NULL);

            c4m_tree_adopt_node(result, sub);
            c4m_cfg_repr_internal(kid, sub, node, NULL);
        }

        c4m_cfg_repr_internal(node->contents.branches.exit_node,
                              tree_parent,
                              node,
                              NULL);

        return result;

    case c4m_cfg_block_exit:
        if (cfg_parent) {
            return NULL;
        }
        c4m_cfg_node_t *nn = node->contents.block_exit.next_node;

        if (nn != NULL) {
            str = c4m_cstr_format("@{:x}: [em]Exit[/] (next @{:x})  [i]({})",
                                  c4m_box_i64(node_addr),
                                  c4m_box_i64((uint64_t)(void *)nn),
                                  du_format_node(node));
        }
        else {
            str = c4m_cstr_format("@{:x}: [em]Exit[/]  [i]({})",
                                  c4m_box_i64(node_addr),
                                  du_format_node(node));
        }
        break;
    case c4m_cfg_use:
        str = c4m_cstr_format("@{:x}: [em]USE[/] {} [i]({})",
                              c4m_box_i64(node_addr),
                              node->contents.flow.dst_symbol->name,
                              du_format_node(node));
        break;
    case c4m_cfg_def:
        str = c4m_cstr_format("@{:x}: [em]DEF[/] {} [i]({})",
                              c4m_box_i64(node_addr),
                              node->contents.flow.dst_symbol->name,
                              du_format_node(node));
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
        break;
    default:
        c4m_unreachable();
    }

    if (node->kind == c4m_cfg_block_entrance) {
    }

    result = c4m_new(c4m_type_tree(c4m_type_utf8()),
                     c4m_kw("contents", c4m_ka(str)));

    c4m_tree_adopt_node(tree_parent, result);

    switch (node->kind) {
    case c4m_cfg_block_entrance:
    case c4m_cfg_node_branch:
        c4m_unreachable();
    case c4m_cfg_block_exit:
        link = node->contents.flow.next_node;

        if (link) {
            c4m_cfg_repr_internal(link, result->parent, node, NULL);
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
        c4m_type_tree(c4m_type_utf8()),
        c4m_kw("contents", c4m_ka(c4m_new_utf8("Root"))));

    c4m_cfg_repr_internal(node, root, NULL, NULL);
    return c4m_grid_tree(root);
}

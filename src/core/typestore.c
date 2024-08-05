#define C4M_USE_INTERNAL_API
#include "con4m.h"

// This is the core type store interface.

void
c4m_universe_init(c4m_type_universe_t *u)
{
    // Start w/ 128 items.
    u->dict = c4m_new_unmanaged_dict(HATRACK_DICT_KEY_TYPE_OBJ_INT,
                                     true,
                                     true);

    atomic_store(&u->next_typeid, 1LLU << 63);
}

// The most significant bits stay 0.
static inline void
init_hv(hatrack_hash_t *hv, c4m_type_hash_t id)
{
#ifdef NO___INT128_T
    hv->w1 = 0ULL;
    hv->w2 = id;
#else
    *hv = id;
#endif
}

c4m_type_t *
c4m_universe_get(c4m_type_universe_t *u, c4m_type_hash_t typeid)
{
    hatrack_hash_t hv;

    init_hv(&hv, typeid);
    assert(typeid);

    return crown_get_mmm(&u->dict->crown_instance,
                         mmm_thread_acquire(),
                         hv,
                         NULL);
}

bool
c4m_universe_add(c4m_type_universe_t *u, c4m_type_t *t)
{
    hatrack_hash_t hv;

    init_hv(&hv, t->typeid);
    assert(t->typeid);

    return crown_add_mmm(&u->dict->crown_instance,
                         mmm_thread_acquire(),
                         hv,
                         t);
}

bool
c4m_universe_put(c4m_type_universe_t *u, c4m_type_t *t)
{
    hatrack_hash_t hv;

    init_hv(&hv, t->typeid);

    if (c4m_universe_add(u, t)) {
        return true;
    }

    crown_put_mmm(&u->dict->crown_instance,
                  mmm_thread_acquire(),
                  hv,
                  t,
                  NULL);

    return false;
}

c4m_type_t *
c4m_universe_attempt_to_add(c4m_type_universe_t *u, c4m_type_t *t)
{
    assert(t->typeid);

    if (c4m_universe_add(u, t)) {
        return t;
    }

    return c4m_universe_get(u, t->typeid);
}

void
c4m_universe_forward(c4m_type_universe_t *u, c4m_type_t *t1, c4m_type_t *t2)
{
    hatrack_hash_t hv;

    assert(t1->typeid);
    assert(t2->typeid);
    assert(!t2->fw);

    t1->fw = t2->typeid;
    init_hv(&hv, t1->typeid);
    crown_put_mmm(&u->dict->crown_instance,
                  mmm_thread_acquire(),
                  hv,
                  t2,
                  NULL);
}

c4m_grid_t *
c4m_format_global_type_environment(c4m_type_universe_t *u)
{
    uint64_t        len;
    hatrack_view_t *view;
    c4m_grid_t     *grid  = c4m_new(c4m_type_grid(),
                               c4m_kw("start_cols",
                                      c4m_ka(3),
                                      "header_rows",
                                      c4m_ka(1),
                                      "stripe",
                                      c4m_ka(true)));
    c4m_list_t     *row   = c4m_new_table_row();
    c4m_dict_t     *memos = c4m_dict(c4m_type_ref(),
                                 c4m_type_utf8());
    int64_t         n     = 0;

    view = crown_view(&u->dict->crown_instance, &len, true);

    c4m_list_append(row, c4m_new_utf8("Id"));
    c4m_list_append(row, c4m_new_utf8("Value"));
    c4m_list_append(row, c4m_new_utf8("Base Type"));
    c4m_grid_add_row(grid, row);

    for (uint64_t i = 0; i < len; i++) {
        c4m_type_t *t = (c4m_type_t *)view[i].item;

        // Skip forwarded nodes.
        if (t->fw) {
            continue;
        }

        c4m_utf8_t *base_name;

        switch (c4m_type_get_kind(t)) {
        case C4M_DT_KIND_nil:
            base_name = c4m_new_utf8("nil");
            break;
        case C4M_DT_KIND_primitive:
            base_name = c4m_new_utf8("primitive");
            break;
        case C4M_DT_KIND_internal: // Internal primitives.
            base_name = c4m_new_utf8("internal");
            break;
        case C4M_DT_KIND_type_var:
            base_name = c4m_new_utf8("var");
            break;
        case C4M_DT_KIND_list:
            base_name = c4m_new_utf8("list");
            break;
        case C4M_DT_KIND_dict:
            base_name = c4m_new_utf8("dict");
            break;
        case C4M_DT_KIND_tuple:
            base_name = c4m_new_utf8("tuple");
            break;
        case C4M_DT_KIND_func:
            base_name = c4m_new_utf8("func");
            break;
        case C4M_DT_KIND_maybe:
            base_name = c4m_new_utf8("maybe");
            break;
        case C4M_DT_KIND_object:
            base_name = c4m_new_utf8("object");
            break;
        case C4M_DT_KIND_oneof:
            base_name = c4m_new_utf8("oneof");
            break;
        case C4M_DT_KIND_box:
            base_name = c4m_new_utf8("box");
            break;
        default:
            c4m_unreachable();
        }

        row = c4m_new_table_row();
        c4m_list_append(row, c4m_cstr_format("{:x}", c4m_box_i64(t->typeid)));
        c4m_list_append(row,
                        c4m_internal_type_repr(t, memos, &n));
        c4m_list_append(row, base_name);
        c4m_grid_add_row(grid, row);
    }
    c4m_set_column_style(grid, 0, c4m_new_utf8("snap"));
    c4m_set_column_style(grid, 1, c4m_new_utf8("snap"));
    c4m_set_column_style(grid, 2, c4m_new_utf8("snap"));
    return grid;
}

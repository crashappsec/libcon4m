#include "con4m.h"

// This is the core type store interface.

void
c4m_universe_init(c4m_type_universe_t *u)
{
    // Start w/ 128 items.
    crown_init_size(&u->store, 7, C4M_GC_SCAN_ALL);
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

    return crown_get_mmm(&u->store, mmm_thread_acquire(), hv, NULL);
}

bool
c4m_universe_put(c4m_type_universe_t *u, c4m_type_t *t)
{
    bool           result;
    hatrack_hash_t hv;

    init_hv(&hv, t->typeid);
    assert(t->typeid);

    crown_put_mmm(&u->store, mmm_thread_acquire(), hv, t, &result);

    return result;
}

bool
c4m_universe_add(c4m_type_universe_t *u, c4m_type_t *t)
{
    hatrack_hash_t hv;

    init_hv(&hv, t->typeid);
    assert(t->typeid);

    return crown_add_mmm(&u->store, mmm_thread_acquire(), hv, t);
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
    crown_put_mmm(&u->store, mmm_thread_acquire(), hv, t2, NULL);
}

#include <con4m.h>

// Different queue/list types.

static void
con4m_list_init(flexarray_t *list, va_list args)
{
    DECLARE_KARGS(
	uint64_t length = 0;
	);

    method_kargs(args, length);
    flexarray_init(list, length);
}

static void
con4m_queue_init(queue_t *list, va_list args)
{
    DECLARE_KARGS(
	uint64_t length = 0;
	);
    method_kargs(args, length);
    queue_init_size(list, (char)(64 - __builtin_clzll(length)));
}

static void
con4m_ring_init(hatring_t *ring, va_list args)
{
    DECLARE_KARGS(
	uint64_t length = 0;
	);

    method_kargs(args, length);
    hatring_init(ring, length);
}

static void
con4m_logring_init(logring_t *ring, va_list args)
{
    DECLARE_KARGS(
	uint64_t num_entries  = 0;
	uint64_t entry_length = 1024;
	);

    method_kargs(args, num_entries, entry_length);
    logring_init(ring, num_entries, entry_length);
}

static void
con4m_stack_init(hatstack_t *stack, va_list args)
{
    DECLARE_KARGS(
	uint64_t prealloc = 128;
	method_kargs(args, prealloc);
	)
    hatstack_init(stack, prealloc);
}

static void
con4m_list_marshal(flexarray_t *r, FILE *f, dict_t *memos, int64_t *mid)
{
    // See the note in xlist; this isn't done yet, and won't
    // be until the type system is ported.
    flex_view_t *view = flexarray_view(r);
    uint64_t     len  = flexarray_view_len(view);

    marshal_u64(len, f);

    for (uint64_t i = 0; i < len; i++)
    {
	con4m_sub_marshal(flexarray_view_next(view, NULL), f, memos, mid);
    }
}

static void
con4m_list_unmarshal(flexarray_t *r, FILE *f, dict_t *memos)
{
    uint64_t len = unmarshal_u64(f);

    flexarray_init(r, len);

    for (uint64_t i = 0; i < len; i++) {
	flexarray_set(r, i, con4m_sub_unmarshal(f, memos));
    }
}

const con4m_vtable list_vtable = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	(con4m_vtable_entry)con4m_list_init,
	NULL,
	NULL,
	(con4m_vtable_entry)con4m_list_marshal,
	(con4m_vtable_entry)con4m_list_unmarshal
    }
};

const con4m_vtable queue_vtable = {
    .num_entries = 1,
    .methods     = {
	(con4m_vtable_entry)con4m_queue_init
    }
};

const con4m_vtable ring_vtable = {
    .num_entries = 1,
    .methods     = {
	(con4m_vtable_entry)con4m_ring_init
    }
};

const con4m_vtable logring_vtable = {
    .num_entries = 1,
    .methods     = {
	(con4m_vtable_entry)con4m_logring_init
    }
};

const con4m_vtable stack_vtable = {
    .num_entries = 1,
    .methods     = {
	(con4m_vtable_entry)con4m_stack_init
    }
};

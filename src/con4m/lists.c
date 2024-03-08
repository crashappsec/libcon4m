#include <con4m.h>

// Different queue/list types.

static void
con4m_list_init(flexarray_t *list, va_list args)
{
    // Begin keyword arguments
    uint64_t length = 0;

    method_kargs(args, length);
    // End keyword arguments.

    flexarray_init(list, length);
}

static void
con4m_queue_init(queue_t *list, va_list args)
{
    // Begin keyword arguments
    uint64_t length = 0;

    method_kargs(args, length);
    // End keyword arguments.

    queue_init_size(list, (char)(64 - __builtin_clzll(length)));
}

static void
con4m_ring_init(hatring_t *ring, va_list args)
{
    // Begin keyword arguments
    uint64_t length = 0;

    method_kargs(args, length);
    // End keyword arguments.

    hatring_init(ring, length);
}

static void
con4m_logring_init(logring_t *ring, va_list args)
{
    // Begin keyword arguments
    uint64_t num_entries  = 0;
    uint64_t entry_length = 1024;

    method_kargs(args, num_entries, entry_length);
    // End keyword arguments.

    logring_init(ring, num_entries, entry_length);
}

static void
con4m_stack_init(hatstack_t *stack, va_list args)
{
    // Begin keyword arguments
    uint64_t prealloc = 128;

    method_kargs(args, prealloc);
    // End keyword arguments.

    hatstack_init(stack, prealloc);
}

const con4m_vtable list_vtable = {
    .num_entries = 1,
    .methods     = {
	(con4m_vtable_entry)con4m_list_init
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

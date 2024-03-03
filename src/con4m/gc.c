#include <con4m/gc.h>
#include <con4m/random.h>

#ifdef __linux__
#include <thread.h>
#endif

uint64_t gc_sentinal = 0;

__attribute__((constructor)) void
initialize_sentinal()
{
    static bool once = false;

    if (!once) {
	gc_sentinal = con4m_rand64();
	once = true;
    }
}

_Atomic uint64_t num_arenas = 0;

thread_local con4m_arena_t *current_heap = NULL;

void
con4m_expand_arena(size_t num_words, con4m_arena_t **current)
{

    // Right now this is just using malloc; we'll change to use mmap()
    // soon; we will try to map to arena_id << 32 using MAP_ANON.
    con4m_arena_t *new_heap = zalloc_flex(con4m_arena_t, int64_t, num_words);

    new_heap->next_alloc = (con4m_alloc_hdr *)new_heap->data;
    new_heap->previous   = *current;
    new_heap->heap_end   = (uint64_t *)(&(new_heap->data[num_words]));
    new_heap->arena_id   = atomic_fetch_add(&num_arenas, 1);
    *current             = new_heap;
}

con4m_arena_t *
con4m_new_arena(size_t num_words)
{
    con4m_arena_t *result = NULL;

    con4m_expand_arena(num_words, &result);

    return result;
}

void *
con4m_gc_alloc(size_t len, uint64_t *ptr_map)
{
    return con4m_arena_alloc(&current_heap, len, ptr_map);
}

void
con4m_delete_arena(con4m_arena_t *arena)
{
    con4m_arena_t *prev_active;

    while (arena != NULL) {
	prev_active = arena->previous;
	zfree(arena);
	arena = prev_active;
    }
}

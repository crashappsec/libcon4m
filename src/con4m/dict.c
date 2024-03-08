#include <con4m.h>

static void
con4m_dict_init(hatrack_dict_t *dict, va_list args)
{
    // Until we push the type system to C, let's just have the
    // constructor require custom invocation I suppose.
    // I.e., for now, this is a C-only API.
    //
    // The constructor should only be called positionally and has
    // but one parameter, which should be one of:
    //  - HATRACK_DICT_KEY_TYPE_INT
    //  - HATRACK_DICT_KEY_TYPE_REAL
    //  - HATRACK_DICT_KEY_TYPE_CSTR
    //  - HATRACK_DICT_KEY_TYPE_PTR

    size_t key_type = (uint32_t)va_arg(args, size_t);
    assert(!(uint64_t)va_arg(args, uint64_t));

    hatrack_dict_init(dict, key_type);
}

const con4m_vtable dict_vtable = {
    .num_entries = 1,
    .methods     = {
	(con4m_vtable_entry)con4m_dict_init
    }
};

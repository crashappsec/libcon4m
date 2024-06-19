#include "con4m.h"

// At least for the time being, we will statically ensure that there
// is a function in the compilation context with the right name and
// signature. For extern stuff, we will not attempt to bind until
// runtime.
//
// Eventually (when we go the REPL) we might want to revise this
// depending on how hot reloading is handled for bindings.
//
// To that end, all callback objects should currently be statically
// bound and unmarshaled from const space.

static void
callback_init(c4m_callback_t *cb, va_list args)
{
    c4m_str_t  *symbol_name = va_arg(args, c4m_utf8_t *);
    c4m_type_t *type        = va_arg(args, c4m_type_t *);

    cb->target_symbol_name = c4m_to_utf8(symbol_name);
    cb->target_type        = type;
}

const c4m_vtable_t c4m_callback_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)callback_init,
        NULL,
    },
};

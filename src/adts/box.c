#include "con4m.h"

static void
box_init(c4m_box_t *box, va_list args)
{
    c4m_box_t b = va_arg(args, c4m_box_t);
    box->u64    = b.u64;
    return;
}

static c4m_utf8_t *
box_repr(c4m_box_t *box)
{
    return c4m_repr(box->v, c4m_type_unbox(c4m_get_my_type(box)));
}

static c4m_utf8_t *
box_to_str(c4m_box_t *box)
{
    return c4m_to_str(box->v, c4m_type_unbox(c4m_get_my_type(box)));
}

static c4m_utf8_t *
box_format(c4m_box_t *box, c4m_fmt_spec_t *spec)
{
    c4m_type_t    *t      = c4m_type_unbox(c4m_get_my_type(box));
    c4m_dt_info_t *info   = c4m_type_get_data_type_info(t);
    c4m_vtable_t  *vtable = (c4m_vtable_t *)info->vtable;
    c4m_format_fn  fn     = (c4m_format_fn)vtable->methods[C4M_BI_FORMAT];

    return (*fn)(box, spec);
}

static c4m_box_t
box_from_lit(c4m_box_t *b, void *i1, void *i2, void *i3)
{
    return c4m_unbox_obj(b);
}

const c4m_vtable_t c4m_box_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR]  = (c4m_vtable_entry)box_init,
        [C4M_BI_TO_STR]       = (c4m_vtable_entry)box_repr,
        [C4M_BI_REPR]         = (c4m_vtable_entry)box_to_str,
        [C4M_BI_FORMAT]       = (c4m_vtable_entry)box_format,
        [C4M_BI_FROM_LITERAL] = (c4m_vtable_entry)box_from_lit,
        [C4M_BI_GC_MAP]       = (c4m_vtable_entry)C4M_GC_SCAN_ALL,
        // Explicit because some compilers don't seem to always properly
        // zero it (Was sometimes crashing on a `c4m_stream_t` on my mac).
        [C4M_BI_FINALIZER]    = NULL,
    },
};

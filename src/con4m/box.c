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
    c4m_type_t *t = c4m_get_my_type(box);

    return c4m_repr(box->v, c4m_tspec_get_param(t, 0));
}

static void
box_marshal(c4m_box_t    *box,
            c4m_stream_t *out,
            c4m_dict_t   *memos,
            int64_t      *mid)
{
    c4m_marshal_u64(box->u64, out);
}

static void
box_unmarshal(c4m_box_t *box, c4m_stream_t *in, c4m_dict_t *memos)
{
    box->u64 = c4m_unmarshal_u64(in);
}

const c4m_vtable_t c4m_box_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)box_init,
        [C4M_BI_TO_STR]      = (c4m_vtable_entry)box_repr,
        [C4M_BI_REPR]        = (c4m_vtable_entry)box_repr,
        [C4M_BI_MARSHAL]     = (c4m_vtable_entry)box_marshal,
        [C4M_BI_UNMARSHAL]   = (c4m_vtable_entry)box_unmarshal,
    },
};

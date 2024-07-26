#include "con4m.h"

// At least for the time being, we will statically ensure that there
// is a function in the compilation context with the right name and
// signature. For extern stuff, we will not attempt to bind until
// runtime.
//
// Note, there are two different callback abstractions in this file, the
// compile-time info we keep on callbacks (`c4m_callback_info_t`), and
//  the bare bones runtime callback object, `c4m_zcallback_t`.
//
// The former is not a Con4m object, as it's not available at runtime.

c4m_callback_info_t *
c4m_callback_info_init()

{
    return c4m_gc_alloc_mapped(c4m_callback_info_t,
                               C4M_GC_SCAN_ALL);
    /*
    result->target_symbol_name = c4m_to_utf8(symbol_name);
    result->target_type        = type;

    return result;
    */
}

static void
zcallback_marshal(c4m_zcallback_t *cb,
                  c4m_stream_t    *s,
                  c4m_dict_t      *m,
                  int64_t         *i)
{
    c4m_sub_marshal(cb->name, s, m, i);
    c4m_sub_marshal(cb->tid, s, m, i);
    c4m_marshal_i64(cb->impl, s);
    c4m_marshal_i32(cb->mid, s);
    c4m_marshal_bool(cb->ffi, s);
    c4m_marshal_bool(cb->skip_boxes, s);
}

static void
zcallback_gc_bits(uint64_t *bitmap, c4m_base_obj_t *base)
{
    c4m_zcallback_t *cb = (void *)base->data;

    c4m_mark_raw_to_addr(bitmap, base, &cb->tid);
}

static void
zcallback_unmarshal(c4m_zcallback_t *cb, c4m_stream_t *s, c4m_dict_t *m)
{
    cb->name       = c4m_sub_unmarshal(s, m);
    cb->tid        = c4m_sub_unmarshal(s, m);
    cb->impl       = c4m_unmarshal_i64(s);
    cb->mid        = c4m_unmarshal_i32(s);
    cb->ffi        = c4m_unmarshal_bool(s);
    cb->skip_boxes = c4m_unmarshal_bool(s);
}

static c4m_utf8_t *
zcallback_repr(c4m_zcallback_t *cb)
{
    return c4m_cstr_format("func {}{}", cb->name, cb->tid);
}

const c4m_vtable_t c4m_callback_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_MARSHAL]     = (c4m_vtable_entry)zcallback_marshal,
        [C4M_BI_UNMARSHAL]   = (c4m_vtable_entry)zcallback_unmarshal,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)zcallback_gc_bits,
        [C4M_BI_REPR]        = (c4m_vtable_entry)zcallback_repr,
        [C4M_BI_FINALIZER]   = NULL,
    },
};

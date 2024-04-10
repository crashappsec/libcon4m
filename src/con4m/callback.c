#include "con4m.h"

static c4m_dict_t *bound_functions = NULL;

static void
callback_init(c4m_callback_t *cb, va_list args)
{
    c4m_type_t  *type        = NULL;
    void        *address     = NULL;
    c4m_str_t   *symbol_name = NULL;
    c4m_xlist_t *libraries   = NULL; // of c4m_str_t
    int32_t      static_link = 0;
    bool         ffi         = false;
    //    bool         bind_now    = false;

    c4m_karg_va_init(args);
    c4m_kw_ptr("type", type);
    c4m_kw_ptr("address", address);
    c4m_kw_ptr("symbol_name", symbol_name);
    c4m_kw_int32("static_linking", static_link);
    c4m_kw_bool("ffi", ffi);
    // c4m_kw_bool("bind_now", bind_now);

    c4m_funcinfo_t *info = NULL;

    if (bound_functions == NULL) {
        bound_functions = c4m_new(c4m_tspec_dict(c4m_tspec_ref(),
                                                 c4m_tspec_ref()));
        c4m_gc_register_root(&bound_functions, 1);
    }

    if (address == NULL) {
        if (!symbol_name) {
            C4M_CRAISE("Not enough information for callback.");
        }

        address = dlsym(RTLD_DEFAULT, symbol_name->data);

        if (!address && libraries != NULL) {
            for (int i = 0; i < c4m_xlist_len(libraries); i++) {
                c4m_utf8_t *s = c4m_to_utf8(c4m_xlist_get(libraries, i, NULL));
                address       = dlopen(s->data, RTLD_NOW | RTLD_GLOBAL);

                if (address != NULL) {
                    break;
                }
            }
        }
    }

    if (address != NULL) {
        info = hatrack_dict_get(bound_functions, address, NULL);
    }

    if (info == NULL) {
        info       = c4m_gc_alloc(c4m_funcinfo_t);
        info->fn   = address;
        info->name = symbol_name->data;
        info->type = type;

        if (ffi) {
            info->flags = C4M_CB_FLAG_FFI;
        }

        if (static_link) {
            info->flags |= C4M_CB_FLAG_STATIC;
        }
    }

    cb->info = info;
}

const c4m_vtable_t c4m_callback_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        (c4m_vtable_entry)callback_init,
        NULL,
        //(c4m_vtable_entry)callback_marshal,
        //(c4m_vtable_entry)callback_unmarshal,
        NULL,
    },
};

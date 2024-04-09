#include "con4m.h"

static dict_t *bound_functions = NULL;

static void
callback_init(callback_t *cb, va_list args)
{
    type_spec_t *type        = NULL;
    void        *address     = NULL;
    any_str_t   *symbol_name = NULL;
    xlist_t     *libraries   = NULL; // of any_str_t
    int32_t      static_link = 0;
    bool         ffi         = false;
    //    bool         bind_now    = false;

    karg_va_init(args);

    kw_ptr("type", type);
    kw_ptr("address", address);
    kw_ptr("symbol_name", symbol_name);
    kw_int32("static_linking", static_link);
    kw_bool("ffi", ffi);
    // kw_bool("bind_now", bind_now);

    funcinfo_t *info = NULL;

    if (bound_functions == NULL) {
        bound_functions = con4m_new(tspec_dict(tspec_ref(), tspec_ref()));
        con4m_gc_register_root(&bound_functions, 1);
    }

    if (address == NULL) {
        if (!symbol_name) {
            CRAISE("Not enough information for callback.");
        }

        address = dlsym(RTLD_DEFAULT, symbol_name->data);

        if (!address && libraries != NULL) {
            for (int i = 0; i < xlist_len(libraries); i++) {
                utf8_t *s = force_utf8(xlist_get(libraries, i, NULL));
                address   = dlopen(s->data, RTLD_NOW | RTLD_GLOBAL);

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
        info       = gc_alloc(funcinfo_t);
        info->fn   = address;
        info->name = symbol_name->data;
        info->type = type;

        if (ffi) {
            info->flags = CB_FLAG_FFI;
        }

        if (static_link) {
            info->flags |= CB_FLAG_STATIC;
        }
    }

    cb->info = info;
}

const con4m_vtable callback_vtable = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
        (con4m_vtable_entry)callback_init,
        NULL,
        //(con4m_vtable_entry)callback_marshal,
        //(con4m_vtable_entry)callback_unmarshal,
        NULL,
    }};

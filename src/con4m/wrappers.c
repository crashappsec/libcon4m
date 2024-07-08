#include "con4m.h"

// Currently not supporting the keyword argument, so need to add the
// null terminator.
c4m_utf32_t *
c4m_wrapper_join(c4m_list_t *l, const c4m_str_t *joiner)
{
    return c4m_str_join(l, joiner);
}

c4m_str_t *
c4m_wrapper_repr(c4m_obj_t obj)
{
    c4m_type_t *t = c4m_type_resolve(c4m_get_my_type(obj));
    return c4m_repr(obj, t);
}

c4m_str_t *
c4m_wrapper_to_str(c4m_obj_t obj)
{
    c4m_type_t *t = c4m_type_resolve(c4m_get_my_type(obj));
    return c4m_to_str(obj, t);
}

c4m_str_t *
c4m_wrapper_hostname()
{
    struct utsname info;

    uname(&info);

    return c4m_new_utf8(info.nodename);
}

c4m_str_t *
c4m_wrapper_os()
{
    struct utsname info;

    uname(&info);

    return c4m_new_utf8(info.sysname);
}

c4m_str_t *
c4m_wrapper_arch()
{
    struct utsname info;

    uname(&info);

    return c4m_new_utf8(info.machine);
}

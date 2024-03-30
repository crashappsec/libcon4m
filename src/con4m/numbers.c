#include <con4m.h>

static any_str_t *
signed_repr(int64_t item, to_str_use_t how)
{
    // TODO, add hex as an option in how.
    char buf[21] = {0, };
    bool negative = false;

    if (item < 0) {
	negative = true;
	item *= -1;
    }

    if (item == 0) {
	return con4m_new(tspec_utf8(), kw("cstring", ka("0")));
    }

    int i = 20;

    while (item) {
	buf[--i] = (item % 10) + '0';
	item /= 10;
    }

    if (negative) {
	buf[--i] = '-';
    }

    return con4m_new(tspec_utf8(), kw("cstring", ka(&buf[i])));
}

static any_str_t *
unsigned_repr(int64_t item, to_str_use_t how)
{
    // TODO, add hex as an option in how.
    char buf[21] = {0, };

    if (item == 0) {
	return con4m_new(tspec_utf8(), kw("cstring", ka("0")));
    }

    int i = 20;

    while (item) {
	buf[--i] = (item % 10) + '0';
	item /= 10;
    }

    return con4m_new(tspec_utf8(), kw("cstring", ka(&buf[i])));
}

static any_str_t *
i8_repr(i8_box *i8, to_str_use_t how)
{
    int64_t n = *i8;
    return signed_repr(n, how);
}

static any_str_t *
u8_repr(u8_box *u8, to_str_use_t how)
{
    uint64_t n = *u8;
    return unsigned_repr(n, how);
}

static any_str_t *
i32_repr(i32_box *i32, to_str_use_t how)
{
    int64_t n = *i32;
    return signed_repr(n, how);
}

static any_str_t *
u32_repr(u32_box *u32, to_str_use_t how)
{
    uint64_t n = *u32;
    return unsigned_repr(n, how);
}

static any_str_t *
i64_repr(i64_box *i64, to_str_use_t how)
{
    return signed_repr(*i64, how);
}

static any_str_t *
u64_repr(u64_box *u64, to_str_use_t how)
{
    return unsigned_repr(*u64, how);
}

static any_str_t *true_repr = NULL;
static any_str_t *false_repr = NULL;

static any_str_t *
bool_repr(bool *b, to_str_use_t how)
{
    if (*b == false) {
	if (false_repr == NULL) {
	    false_repr = con4m_new(tspec_utf8(), kw("cstring", ka("false")));
	    con4m_gc_register_root(&false_repr, 1);
	}
	return false_repr;
    }
    if (true_repr == NULL) {
	true_repr = con4m_new(tspec_utf8(), kw("cstring", ka("true")));
	con4m_gc_register_root(&true_repr, 1);
    }

    return true_repr;
}

bool
any_number_can_coerce_to(type_spec_t *my_type, type_spec_t *target_type)
{
    switch (tspec_get_data_type_info(target_type)->typeid) {
    case T_BOOL:
    case T_I8:
    case T_BYTE:
    case T_I32:
    case T_CHAR:
    case T_U32:
    case T_INT:
    case T_UINT:
    case T_F32:
    case T_F64:
	return true;
    default:
	return false;
    }
}

void *
any_int_coerce_to(const int64_t data, type_spec_t *target_type)
{
    double d;

    switch (tspec_get_data_type_info(target_type)->typeid) {
    case T_BOOL:
    case T_I8:
    case T_BYTE:
    case T_I32:
    case T_CHAR:
    case T_U32:
    case T_INT:
    case T_UINT:
	return (void *)data;
    case T_F32:
    case T_F64:
	d = (double)(data);
	return double_to_ptr(d);
    default:
	CRAISE("Invalid type conversion.");
    }
}

void *
bool_coerce_to(const int64_t data, type_spec_t *target_type)
{
    switch (tspec_get_data_type_info(target_type)->typeid) {
    case T_BOOL:
    case T_I8:
    case T_BYTE:
    case T_I32:
    case T_CHAR:
    case T_U32:
    case T_INT:
    case T_UINT:
	if (data) {
	    return (void *)NULL;
	}
	else {
	    return NULL;
	}
    case T_F32:
    case T_F64:
	if (data) {
	    return double_to_ptr(1.0);
	}
	else {
	    return double_to_ptr(0.0);
	}
    default:
	CRAISE("Invalid type conversion.");
    }
}

any_str_t *
float_repr(const double *dp, to_str_use_t how)
{
    double     d = *dp;
    char buf[20] = {0,};

    // snprintf includes null terminator in its count.
    snprintf(buf, 20, "%g", d);

    return con4m_new(tspec_utf8(), kw("cstring", ka(buf)));
}

void *
float_coerce_to(const double d, type_spec_t *target_type)
{
    int64_t i;

    switch (tspec_get_data_type_info(target_type)->typeid) {
    case T_BOOL:
    case T_I8:
    case T_BYTE:
    case T_I32:
    case T_CHAR:
    case T_U32:
    case T_INT:
    case T_UINT:
	i = (int64_t)d;

	return (void *)i;
    case T_F32:
    case T_F64:
	return double_to_ptr(d);
    default:
	CRAISE("Invalid type conversion.");
    }
}

const con4m_vtable u8_type = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	NULL, // You have to get it through a reference or mixed.
	(con4m_vtable_entry)u8_repr,
	NULL, // finalizer
	NULL, // Not used for ints.
	NULL, // Not used for ints.
	(con4m_vtable_entry)any_number_can_coerce_to,
	(con4m_vtable_entry)any_int_coerce_to,
	NULL, // From lit,
	NULL, // The rest are not implemented for value types.
    }
};

const con4m_vtable i8_type = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	NULL, // You have to get it through a reference or mixed.
	(con4m_vtable_entry)i8_repr,
	NULL, // finalizer
	NULL, // Not used for ints.
	NULL, // Not used for ints.
	(con4m_vtable_entry)any_number_can_coerce_to,
	(con4m_vtable_entry)any_int_coerce_to,
	NULL, // From lit,
	NULL, // The rest are not implemented for value types.
    }
};

const con4m_vtable u32_type = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	NULL, // You have to get it through a reference or mixed.
	(con4m_vtable_entry)u32_repr,
	NULL, // finalizer
	NULL, // Not used for ints.
	NULL, // Not used for ints.
	(con4m_vtable_entry)any_number_can_coerce_to,
	(con4m_vtable_entry)any_int_coerce_to,
	NULL, // From lit,
	NULL, // The rest are not implemented for value types.
    }
};

const con4m_vtable i32_type = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	NULL, // You have to get it through a reference or mixed.
	(con4m_vtable_entry)i32_repr,
	NULL, // finalizer
	NULL, // Not used for ints.
	NULL, // Not used for ints.
	(con4m_vtable_entry)any_number_can_coerce_to,
	(con4m_vtable_entry)any_int_coerce_to,
	NULL, // From lit,
	NULL, // The rest are not implemented for value types.
    }
};

const con4m_vtable u64_type = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	NULL, // You have to get it through a reference or mixed.
	(con4m_vtable_entry)u64_repr,
	NULL, // finalizer
	NULL, // Not used for ints.
	NULL, // Not used for ints.
	(con4m_vtable_entry)any_number_can_coerce_to,
	(con4m_vtable_entry)any_int_coerce_to,
	NULL, // From lit,
	NULL, // The rest are not implemented for value types.
    }
};

const con4m_vtable i64_type = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	NULL, // You have to get it through a reference or mixed.
	(con4m_vtable_entry)i64_repr,
	NULL, // finalizer
	NULL, // Not used for ints.
	NULL, // Not used for ints.
	(con4m_vtable_entry)any_number_can_coerce_to,
	(con4m_vtable_entry)any_int_coerce_to,
	NULL, // From lit,
	NULL, // The rest are not implemented for value types.
    }
};

const con4m_vtable bool_type = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	NULL, // You have to get it through a reference or mixed.
	(con4m_vtable_entry)bool_repr,
	NULL, // finalizer
	NULL, // Not used for ints.
	NULL, // Not used for ints.
	(con4m_vtable_entry)any_number_can_coerce_to,
	(con4m_vtable_entry)bool_coerce_to,
	NULL, // From lit,
	NULL, // The rest are not implemented for value types.
    }
};

const con4m_vtable float_type = {
    .num_entries = CON4M_BI_NUM_FUNCS,
    .methods     = {
	NULL, // You have to get it through a reference or mixed.
	(con4m_vtable_entry)float_repr,
	NULL, // finalizer
	NULL, // Not used for ints.
	NULL, // Not used for ints.
	(con4m_vtable_entry)any_number_can_coerce_to,
	(con4m_vtable_entry)float_coerce_to,
	NULL, // From lit,
	NULL, // The rest are not implemented for value types.
    }
};

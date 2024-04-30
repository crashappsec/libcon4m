#pragma once

#include "con4m.h"

// reset a vm's volatile state. this should normally only be done before the
// first run of a vm, but it may be done successively as well, as long as there
// are no vm thread states active for the vm.
extern void
c4m_vm_reset(c4m_vm_t *vm);

// create a new thread state attached to the specified vm. Multiple threads may
// run code from the same VM simultaneously, but each one needs its own thread
// state.
extern c4m_vmthread_t *
c4m_vmthread_new(c4m_vm_t *vm);

// set the specified thread state running. evaluation of instructions at the
// location previously set into the tstate will proceed.
extern int
c4m_vmthread_run(c4m_vmthread_t *tstate);

// reset the specified thread state, leaving the state as if it was newly
// return from c4m_vmthread_new.
extern void
c4m_vmthread_reset(c4m_vmthread_t *tstate);

// retrieve the attribute specified by key. if expected_type is not NULL, raise
// an error if the attribute's type does not match.
extern c4m_value_t *
c4m_vm_attr_get(c4m_vmthread_t *tstate,
                c4m_str_t      *key,
                c4m_type_t     *expected_type);

extern void
c4m_vm_attr_set(c4m_vmthread_t *tstate,
                c4m_str_t      *key,
                c4m_value_t    *value,
                bool            lock,
                bool            override,
                bool            internal);

// lock an attribute immediately of on_write is false; otherwise, lock it when
// it is set.
extern void
c4m_vm_attr_lock(c4m_vmthread_t *tstate, c4m_str_t *key, bool on_write);

extern void
c4m_vm_marshal(c4m_vm_t *vm, c4m_stream_t *out, c4m_dict_t *memos, int64_t *mid);

extern void
c4m_vm_unmarshal(c4m_vm_t *vm, c4m_stream_t *in, c4m_dict_t *memos);

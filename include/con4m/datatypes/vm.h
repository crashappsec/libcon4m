#pragma once

#include "con4m.h"

// This is mostly a straight port of the original Nim code. The overriding goal
// here is to keep things simple and easy. To that end, we always want to store
// primitives naturally (unboxed) alongside con4m objects whenever possible,
// because that's how the rest of the con4m API works. These points inform the
// design that is implemented here.

// Instructions are 32 bytes each and are defined as c4m_zinstruction_t. Types
// named with a z are marshalled. Each instruction requires a single byte opcode
// that defines what the instruction does. These opcodes are described below
// with information about what they do and what other components of the
// instruction structure they use. Most instructions do not use all possible
// instruction fields, but all fields are present for all instructions to keep
// things simple.

// Each element of the stack consists of a value and a type. The type is largely
// redundant in most cases, because it ought to match the type information that
// is stored in the value object itself, but if an unboxed primitive (e.g., an
// an integer or a float) is pushed onto the stack, the type information is
// needed to be stored along with it. It must be reiterated that the core con4m
// API expects primitives to be unboxed and so the VM does as well. A suggested
// alternative to the current implementation is to use two value stacks: one
// with type information and one without type information. Code generation would
// always know which stack to reference. But ultimately the savings are minimal,
// because the type information needs to be managed in other locations that need
// to store (return register, attributes, etc.). I think it's much simpler to
// just maintain a single value stack, even though it means storing redundant
// type information.

// Some instructions encode addressing information for various types of values.
// For these instructions, a 16-bit module id specifies which module the value
// belongs to. A module id >= 0 refers to that module's global variables, and
// the instruction's arg specifies the slot in the module's table of global
// variables. If the module id is -1, the value is a local variable stored on
// the stack, and the instruction's arg specifies the offset from the frame
// pointer. There is also a special case where a module id of -2 means that the
// instruction's arg refers to an offset in the current module's static data
// table, but this appears to be a dead end in the Nim code and has not been
// implemented in C.

// For all call instructions, arguments are pushed onto the stack in reverse
// order. It is up to the caller to pop the arguments from the stack after the
// call returns. This calling convention causes some difficulty in a strictly
// stack-based virtual machine, so there is a special register used for return
// values. The typical order of operations is:
//
//   * Push arguments onto the stack for the call in reverse order
//   * Call instruction (C4M_ZTCall, C4M_Z0Call, C4M_ZCallModule, etc.)
//   * Callee sets return value via C4M_ZSetRes
//   * Callee returns via C4M_ZRet or C4M_ZRetModule, as appropriate
//   * Caller pops argumnts from the stack via C4M_ZPop or C4M_ZMoveSp
//   * Caller pushes the return value via C4M_ZPushRes
//
// Code generation should maintain an implicit result local variable that is
// always the first offset from the frame pointer. The C4M_ZSetRes instruction
// sets the return value register from this value slot on the value stack.
//
// The C4M_ZTCall instruction used for calling builtin vtable functions differs
// from this. All arguments are still expected to be pushed onto the stack in
// reverse order; however, the call instruction itself pops the arguments and
// pushes the result onto the stack.
//
// The C4M_ZFFICall instruction used for making non-native FFI calls also
// differs similarly, though not identically. Arguments are popped from the
// stack, but the return value is stored in the return register, and so the
// caller must push the return register onto the stack if it's interested in
// its value.

#define STACK_SIZE     (1 << 20)
#define MAX_CALL_DEPTH 200

typedef enum : uint8_t {
    // Push an rvalue by value. At this time, this is just used by constant
    // values that are immutable. The value to push is determined as described
    // in the above comment about address encodings.
    C4M_ZPushVal       = 0x10,
    // Push an rvalue by reference. At this time, this is used by any value that
    // is not immutable. The value to push is determined as described in the
    // above comment about address encodings.
    C4M_ZPushPtr       = 0x11,
    // Push an offset from the start of the current module's static string table
    // onto the stack. It is the offset itself that is pushed onto the stack.
    // Instructions that rely on this know what to do with it. The offset to
    // push is stored in the instruction's arg field.
    C4M_ZPushStaticPtr = 0x12,
    // Push an immediate value onto the stack. The value to push is encoded in
    // the instruction's immediate field and may be an integer or floating point
    // value. The type of the immediate value is encoded in the instruction's
    // type_info.
    C4M_ZPushImm       = 0x13,
    // Push the type of a value onto the stack. The value to operate on is
    // determined as described in the above comment about address encodings.
    C4M_ZPushSType     = 0x14,
    // Push an lvalue. The lvalue to push is determined as described in the
    // above comment about address encodings. This should be paired with
    // C4M_ZAssignToLoc.
    C4M_ZPushAddr      = 0x15,
    // Duplicate the value at the top of the stack by pushing it again.
    C4M_ZDupTop        = 0x16,
    // Retrieves an attribute value and pushes it onto the stack. The attribute
    // is the top stack value pushed by C4M_ZPushStaticPtr and is replaced by
    // the attribute value. If the instruction's arg is non-zero, an lvalue is
    // pushed instead of an rvalue. Note that in the case where an lvalue is
    // pushed and subsequently stored to via C4M_ZAssignToLoc, no lock checking
    // for the attribute is done, including lock on write.
    C4M_ZLoadFromAttr  = 0x17,
    // Create a callback and push it onto the stack. The instruction's arg,
    // immediate, and type_info fields are encoded into the callback as the
    // implementation (ffi function index), name offset, and type info,
    // respectively. The ZRunCallback instruction is used to run the callback,
    // which is run as an FFI function.
    C4M_ZPushFfiPtr    = 0x18,
    // Create a callback and push it onto the stack. The instrcution's arg,
    // immediate, and type_info fields are encoded into the callback as the
    // implementation (function index), name offset, and type info,
    // respectively. The ZRunCallback instruction is used to run the callback,
    // which is run as a native function via C4M_Z0Call, but using a separate
    // VM state.
    C4M_ZPushVmPtr     = 0x19,
    // Swap the two top values on the stack.
    C4M_ZSwap          = 0x1A,

    // Pops the top value from the stack. This is the same as C4M_ZMoveSp with
    // an adjustment of -1.
    C4M_ZPop        = 0x20,
    // Stores the value at the top of the stack to the value address encoded in
    // the instruction. The storage address is determined as described in the
    // above comment about address encodings. The value assigned from the stack
    // is not popped.
    C4M_ZStoreTop   = 0x21,
    // Stores the encoded immediate value into the value address encoded in the
    // instruction. The storage address is determined as described in the above
    // comment about address encodings.
    C4M_ZStoreImm   = 0x22,
    // Stashes the tuple at the top of the stack into the unpack register that's
    // used by C4M_ZUnpack. The value at the top of the stack is popped.
    C4M_ZTupleStash = 0x23,
    // Unpack the elements of a tuple, storing each one into the lvalue on the
    // stack, popping each lvalue as its assigned. The number of assignments to
    // perform is encoded in the instruction's arg field. The tuple to unpack is
    // stored in the special unpack register. The unpack register is not cleared
    // upon completion of the unpacking operation.
    C4M_ZUnpack     = 0x24,

    // Jump if the top value on the stack is zero. The pc is adjusted by the
    // number of bytes encoded in the instruction's arg field, which is always
    // a multiple of the size of an instruction. A negative value jumps
    // backward. If the comparison triggers a jump, the stack is left as-is,
    // but the top value is popped if no jump occurs.
    C4M_ZJz          = 0x30,
    // Jump if the top value on the stack is not zero. The pc is adjusted by the
    // number of bytes encoded in the instruction's arg field, which is always
    // a multiple of the size of an instruction. A negative value jumps
    // backward. If the comparison triggers a jump, the stack is left as-is,
    // but the top value is popped if no jump occurs.
    C4M_ZJnz         = 0x31,
    // Unconditional jump. Adjust the pc by the number of bytes encoded in the
    // instruction's arg field, which is always a multiple of the size of an
    // instruction. A negative value jumps backward.
    C4M_ZJ           = 0x32,
    // Call one of con4m's builtin functions via vtable for the object on the
    // top of the stack. The index of the builtin function to call is encoded
    // in the instruction's arg field and should be treated as one of the values
    // in the c4m_builtin_type_fn enum type. The number of arguments expected to
    // be on the stack varies for each function. In all cases, contrary to how
    // other calls are handled, the arguments are popped from the stack and the
    // result is pushed onto the stack.
    C4M_ZTCall       = 0x33,
    // Call a "native" function, one which is defined in bytecode from the same
    // object file. The index of the function to call is encoded in the
    // instruction's arg parameter, adjusted up by 1 (0 is not a valid index).
    // The index is used to lookup the function from the object file's
    // func_info table.
    C4M_Z0Call       = 0x34,
    // Call an external "non-native" function via FFI. The index of the function
    // to call is encoded in the instruction's arg parameter. This index is not
    // adjusted as it is for other, similar instructions. The index is used to
    // lookup the function from the object file's ffi_info table. The arguments
    // are popped from the stack. The return value is stored in the return value
    // register.
    C4M_ZFFICall     = 0x35,
    // Call a module's initialization code. This corresponds with a "use"
    // statement. The module index of the module to call is encoded in the
    // instruction's arg parameter, adjusted up by 1 (0 is not a valid index).
    // The index is used to lookup the module from the object file's
    // module_contents table.
    C4M_ZCallModule  = 0x36,
    // Pops a callback from the stack (pushed via either C4M_ZPushFfiPtr or
    // C4M_ZPushVmPtr) and runs it. If the callback is an FFI callback, the
    // action is basically the same as C4M_ZFFICalll, except it uses the index
    // from the callback. Otherwise, the callback is the same as a native call
    // via C4M_Z0Call, except it uses the index from the callback.
    C4M_ZRunCallback = 0x37,

    // Perform a logical not operation on the top stack value. If the value is
    // zero, it will be replaced with a one value of the same type. If the value
    // is non-zero, it will be replaced with a zero value of the same type.
    C4M_ZNot = 0x50,

    // Unmarshals the data stored in the static data area beginning at the
    // offset encoded into the instruction's immediate field. The length of the
    // marhsalled data is encoded in the instruction's arg field. The resulting
    // object is pushed onto the stack.
    C4M_ZSObjNew = 0x60,

    // Perform an assignment into the lvalue at the top of the stack of the
    // value just below it and pops both items from the stack. This should be
    // paired with C4M_ZPushAddr or C4M_ZLoadFromAttr with a non-zero arg.
    C4M_ZAssignToLoc = 0x70,
    // Stores a value to the attribute named by the top value on the stack. The
    // value to store is the stack value just below it. Both values are popped
    // from the stack. If the instruction's arg is non-zero, the attribute
    // will be locked when it's set. This instruction expects that the attribute
    // is stored on the stack via C4M_ZPushStaticPtr.
    C4M_ZAssignAttr  = 0x71,
    // Exits the current call frame, returning the current state back to the
    // originating location, which is the instruction immediately following the
    // C4M_Z0Call instruction that created this frame.
    C4M_ZRet         = 0x80,
    // Exits the current stack frame, returning the current state back to the
    // originating location, which is the instruction immediately following the
    // C4M_ZCallModule instruction that created this frame.
    C4M_ZModuleRet   = 0x81,
    // Halt the current program immediately.
    C4M_ZHalt        = 0x82,
    // Initialze module parameters. The number of parameters is encoded in the
    // instruction's arg field. This is only used during module initialization.
    C4M_ZModuleEnter = 0x83,
    // Pops the top stack value and tests it. The value is expected to be either
    // a string or NULL. If it is a string and is not an empty string, it will
    // be used as an error message and evalutation will stop. This is basically
    // a specialized assert used during module initialization to validate
    // module parameters.
    C4M_ZParamCheck  = 0x84,

    // Load the return value register from the stack value at fp - 1.
    C4M_ZSetRes  = 0x90,
    // Pushes the value stored in the return register onto the stack. The return
    // register is not cleared.
    C4M_ZPushRes = 0x91,
    // Adjust the stack pointer down by the amount encoded in the instruction's
    // arg field. This means specifically that the arg field is subtracted from
    // sp, so a single pop would encode -1 as the adjustment.
    C4M_ZMoveSp  = 0x92,

    // Test the top stack value. If it is non-zero, pop it and continue running
    // the program. Otherwise, print an assertion failure and stop running the
    // program.
    C4M_ZAssert = 0xA0,

    // Set the specified attribute to be "lock on write". Triggers an error if
    // the attribute is already set to lock on write. This instruction expects
    // the top stack value to be loaded via ZPushStaticPtr and does not pop it.
    // The attribute to lock is named according to the top stack value.
    C4M_ZLockOnWrite = 0xB0,

    // Print the error message that is the top value on the stack and stop
    // running the program.
    C4M_ZBail = 0xEE,

    // Nop does nothing.
    C4M_ZNop = 0xFF,
} c4m_zop_t;

// We'll make the main VM container, c4m_vm_t an object type (C4M_T_VM) so that
// we can have an entry point into the core marshalling functionality, but we
// won't make other internal types object types. Only those structs that have a
// z prefix on their name get marshalled. For lists we'll use c4m_tspec_xlist
// with a c4m_tspec_ref base type. But this means that we have to marshal these
// lists manually. As of right now at least, all dicts used have object keys
// and values (ints, strings, etc).

typedef struct {
    c4m_zop_t   op;
    uint8_t     pad;
    int16_t     module_id;
    int32_t     line_no;
    int32_t     arg;
    int64_t     immediate;
    c4m_type_t *type_info;
} c4m_zinstruction_t;

// FIXME this does not appear to be marshalled. Rename to c4m_vmcallback_t
typedef struct {
    // Nim casts this around as a pointer, but it's always used as an integer
    // index into an array
    int64_t     impl;
    int64_t     nameoffset;
    c4m_type_t *tid;
    int16_t     mid;
    bool        ffi;
} c4m_zcallback_t;

// this is an arbitrary value that combines the value itself with its type
// information. this is needed because we want to keep ints and floats unboxed,
// but to do that we always need to store the values with type information
// attached. a boxed int/float should never be stored in a value, but unboxed
// before storing in the value, because there's otherwise no way to be able to
// know whether the number value is boxed or not.
typedef struct c4m_value_t {
    c4m_obj_t   obj;
    c4m_type_t *type_info;
} c4m_value_t;

// stack values have no indicator of what's actually stored, instead relying on
// instructions to assume correctly what's present.
typedef union c4m_stack_value_t {
    c4m_value_t             *lvalue;
    c4m_value_t              rvalue;
    uint64_t                 static_ptr; // offset into static_data
    c4m_zcallback_t         *callback;
    uint64_t                 uint;       // saved pc / module_id
    union c4m_stack_value_t *fp;         // saved fp
} c4m_stack_value_t;

typedef struct {
    // whether passing a pointer to the thing causes it to hold the pointer,
    // in which case decref must be explicit.
    bool       held;
    // this passes a value back that was allocated in the FFI.
    bool       alloced;
    // an index into the CTypeNames data structure in ffi.nim.
    int16_t    arg_type;
    // To look up any FFI processing we do for the type.
    int32_t    our_type;
    c4m_str_t *name;
} c4m_zffi_arg_info_t;

typedef struct {
    int64_t      nameoffset;
    int64_t      localname;
    int32_t      mid; // module_id
    c4m_type_t  *tid;
    bool         va;
    c4m_xlist_t *dlls;     // int64_t
    c4m_xlist_t *arg_info; // tspec_ref: c4m_zffi_arg_info_t
    c4m_str_t   *shortdoc;
    c4m_str_t   *longdoc;
} c4m_zffi_info_t;

typedef struct {
    int64_t     offset;
    c4m_type_t *tid;
} c4m_zsymbol_t;

typedef struct {
    c4m_str_t  *funcname;
    c4m_dict_t *syms; // int64_t, string

    // sym_types maps offset to type ids at compile time. Particularly
    // for debugging info, but will be useful if we ever want to, from
    // the object file, create optimized instances of functions based
    // on the calling type, etc. Parameters are held separately from
    // stack symbols, and like w/ syms, we only capture variables
    // scoped to the entire function. We'll probably address that
    // later.
    //
    // At run-time, the type will always need to be concrete.
    c4m_xlist_t *sym_types; // tspec_ref: c4m_zsymbol_t

    c4m_type_t *tid;
    int32_t     mid;    // module_id
    int32_t     offset; // offset to start of instructions in module
    int32_t     size;
    c4m_str_t  *shortdoc;
    c4m_str_t  *longdoc;
} c4m_zfn_info_t;

typedef struct {
    c4m_str_t  *attr;
    int64_t     offset;
    c4m_value_t default_value;
    c4m_type_t *tid;
    bool        have_default;
    bool        is_private;
    int32_t     v_fn_ix;
    bool        v_native;
    int32_t     i_fn_ix;
    bool        i_native;
    c4m_value_t userparam;
    c4m_str_t  *shortdoc;
    c4m_str_t  *longdoc;
} c4m_zparam_info_t;

typedef struct {
    c4m_str_t   *modname;
    c4m_str_t   *location;
    c4m_str_t   *key;
    c4m_str_t   *ext;
    c4m_str_t   *url;
    c4m_str_t   *version;
    c4m_xlist_t *sym_types; // tspec_ref: c4m_zsymbol_t
    c4m_dict_t  *codesyms;  // int64_t, string
    c4m_dict_t  *datasyms;  // int64_t, string
    c4m_str_t   *source;
    c4m_str_t   *shortdoc;
    c4m_str_t   *longdoc;
    int64_t      module_id;
    int64_t      module_var_size;
    int64_t      init_size;    // size of init code before functions begin
    c4m_xlist_t *parameters;   // tspec_ref: c4m_zparam_info_t
    c4m_xlist_t *instructions; // tspec_ref: c4m_zinstruction_t
} c4m_zmodule_info_t;

typedef enum : uint8_t {
    C4M_FS_FIELD,
    C4M_FS_OBJECT_TYPE,
    C4M_FS_SINGLETON,
    C4M_FS_USER_DEF_FIELD,
    C4M_FS_OBJECT_INSTANCE,
    C4M_FS_ERROR_NO_SPEC,
    C4M_FS_ERROR_SEC_UNDER_FIELD,
    C4M_FS_ERROR_NO_SUCH_SEC,
    C4M_FS_ERROR_SEC_NOT_ALLOWED,
    C4M_FS_ERROR_FIELD_NOT_ALLOWED,
} c4m_field_spec_kind_t;

typedef struct {
    c4m_str_t            *name;
    c4m_type_t           *tid;
    c4m_field_spec_kind_t field_kind;
    bool                  lock_on_write; // enforced at runtime.
    bool                  hidden;
    bool                  required;
    bool                  have_default;
    c4m_value_t           default_value;
    // TODO    c4m_xlist_t          *validators; // tspec_ref: c4m_zvalidator_t
    c4m_str_t            *doc;
    c4m_str_t            *shortdoc;
    int64_t               err_ix;
    c4m_xlist_t          *exclusions; // string
    // name of the field that's going to contain our type.
    c4m_str_t            *deferred_type;
} c4m_zfield_spec_t;

typedef struct {
    c4m_str_t   *name;
    int64_t      min_allowed; // this is useless actually
    int64_t      max_allowed; // And this can be a bool for singleton.
    c4m_dict_t  *fields;      // string, tspec_ref: c4m_zfield_spec_t
    bool         user_def_ok;
    bool         hidden;
    bool         cycle; // Private, used to avoid populating cyclic defs.
    // TODO c4m_xlist_t *validators; // tspec_ref: c4m_zvalidator_t
    c4m_str_t   *doc;
    c4m_str_t   *shortdoc;
    c4m_xlist_t *allowed_sections;  // string
    c4m_xlist_t *required_sections; // string
} c4m_zsection_spec_t;

typedef struct {
    c4m_zsection_spec_t *root_spec;
    c4m_dict_t          *sec_specs; // string, tspec_ref: c4m_zsection_spec_t
    bool                 used;
    bool                 locked;
} c4m_zvalidation_spec_t;

typedef struct {
    uint64_t                zero_magic;
    uint16_t                zc_object_vers;
    c4m_buf_t              *static_data;
    c4m_dict_t             *t_info;          // c4m_type_t *, int64_t (index into static data for repr)
    c4m_dict_t             *globals;         // int64_t, string
    c4m_xlist_t            *sym_types;       // tspec_ref: c4m_zsymbol_t
    int64_t                 global_scope_sz;
    c4m_xlist_t            *module_contents; // tspec_ref: c4m_zmodule_info_t
    int32_t                 entrypoint;
    int32_t                 next_entrypoint;
    c4m_xlist_t            *func_info; // tspec_ref: c4m_zfn_info_t
    c4m_xlist_t            *ffi_info;  // tspec_ref: c4m_zffi_info_t
    c4m_zvalidation_spec_t *spec;
} c4m_zobject_file_t;

typedef struct {
    c4m_zmodule_info_t *call_module;
    int32_t             calllineno;
    int32_t             targetline;
    c4m_zmodule_info_t *targetmodule;
    c4m_zfn_info_t     *targetfunc;
} c4m_vmframe_t;

typedef struct {
    c4m_value_t         contents;
    bool                is_set;
    bool                locked;
    bool                lock_on_write;
    int32_t             module_lock;
    bool                override;
    c4m_zinstruction_t *lastset; // (not marshaled)
} c4m_attr_contents_t;

typedef struct {
    c4m_str_t *shortdoc;
    c4m_str_t *longdoc;
} c4m_docs_container_t;

// C4M_T_VM
typedef struct {
    c4m_zobject_file_t *obj;

    // The following fields represent saved execution state on top of
    // the base object file.
    c4m_value_t **module_allocations;
    c4m_dict_t   *attrs;        // string, c4m_attr_contents_t (tspec_ref)
    c4m_set_t    *all_sections; // string
    c4m_dict_t   *section_docs; // string, c4m_docs_container_t (tspec_ref)
    bool          using_attrs;
} c4m_vm_t;

typedef struct {
    // vm is a pointer to the global vm state shared by various threads.
    c4m_vm_t *vm;

    // sp is the current stack pointer. The stack grows down, so sp starts out
    // as &stack[STACK_SIZE] (stack bottom)
    c4m_stack_value_t *sp;

    // fp points to the start of the current frame on the stack.
    c4m_stack_value_t *fp;

    // pc is the current program counter, which is an index into current_module
    // instructions array.
    uint32_t pc;

    // num_frames is the number of active frames in the call stack. the current
    // frame is always frame_stack[num_frames - 1]
    int32_t num_frames;

    // rr is the return register, used to communicate function return values
    // to the caller. This is needed because the caller is responsible for
    // popping arguments to calls from the stack, which doesn't allow for
    // ordering to pass the return value back on the stack
    c4m_value_t rr;

    // tr is the tuple stash register, used to by C4M_ZUnpack to know which
    // tuple to unpack.
    c4m_value_t tr;

    // current_module is the module to which currently executing instructions
    // belong.
    c4m_zmodule_info_t *current_module;

    // running is true if this thread state is currently running VM code.
    bool running;

    // error is true if this thread state raised an error during evaluation.
    bool error;

    c4m_xlist_t *module_lock_stack;

    // frame_stack is the base address of the call stack
    c4m_vmframe_t frame_stack[MAX_CALL_DEPTH];

    // stack is the base address of the stack for this thread of execution.
    // the stack grows down, so the stack bottom is &stack[STACK_SIZE]
    c4m_stack_value_t stack[STACK_SIZE];
} c4m_vmthread_t;

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
//   * Callee returns via C4M_ZRet or C4M_ZRetModule, as appropriate
//   * Caller pops arguments from the stack via C4M_ZPop or C4M_ZMoveSp
//   * Caller pushes the return value via pushing to R0.
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

typedef enum : uint8_t {
    // Push from const storage onto the stack.
    // Const values that are not reference objects can be pushed as immediates
    // and not put into const storage, so this only ever is expected to
    // contain read-only pointers to objects in the read-only heap.
    //
    // The type field should be redundant, and is not pushed.
    C4M_ZPushConstObj  = 0x01,
    // Push the *address* of the constant object. I wouldn't expect
    // this to get used until we add references.
    C4M_ZPushConstRef  = 0x02,
    // Push a copy of a local variable from its frame pointer offset to
    // the top of the stack.
    C4M_ZPushLocalObj  = 0x03,
    // Do we need this??
    C4M_ZPushLocalRef  = 0x04,
    // Push an rvalue from static space.
    C4M_ZPushStaticObj = 0x05,
    // Push an lvalue from static space.
    C4M_ZPushStaticRef = 0x06,
    // Push an immediate value onto the stack. The value to push is encoded in
    // the instruction's immediate field and may be an integer or floating point
    // value. The type of the immediate value is encoded in the instruction's
    // type_info.
    C4M_ZPushImm       = 0x07,
    // For an object on top of the stack, will retrieve and push the object's
    // type. Note that the top of the stack must not be a raw value type;
    // if it's not a by-reference type, box it first.
    C4M_ZPushObjType   = 0x09,
    // Duplicate the value at the top of the stack by pushing it again.
    C4M_ZDupTop        = 0x0A,
    // Replaces the top item on the stack with its dereference.
    C4M_ZDeref         = 0x0B,
    // Retrieves an attribute value and pushes it onto the stack. The
    // attribute is the top stack value pushed by C4M_ZPushConstObj
    // and is replaced by the attribute value. If the instruction's
    // arg is non-zero, an lvalue is pushed instead of an rvalue. Note
    // that in the case where an lvalue is pushed and subsequently
    // stored to via C4M_ZAssignToLoc, no lock checking for the
    // attribute is done, including lock on write.  If the immediate
    // field has the `1` bit set, then there is also a test for
    // whether the attribute is found... if it is, a non-zero value is
    // pushed after the result. If not, then only a zero is pushed.
    // If that non-zero field also has the `2` bit set, then the
    // actual value will not be pushed.
    C4M_ZLoadFromAttr  = 0x0C,
    C4M_ZLoadFromView  = 0x0D,
    // Create a callback and push it onto the stack. The instruction's
    // arg, immediate, and type_info fields are encoded into the
    // callback as the implementation (ffi function index), whether to
    // skip needed boxing,, and type info, respectively. The func name
    // is expected to be on the top of the stack, and get will
    // replaced with a callback object when this instruction runs. The
    // ZRunCallback instruction is used to run the callback, which is
    // run as an FFI function.
    //
    C4M_ZPushFfiPtr    = 0x0E,
    // Create a callback and push it onto the stack. The instruction's
    // arg, and type_info fields are encoded into the callback as the
    // implementation (function index), and type info,
    // respectively. The func name is expected to be on the top of the
    // stack, and get will replaced with a callback object when this
    // instruction runs. The ZRunCallback instruction is used to run
    // the callback, which is run as a native function via C4M_Z0Call,
    // but using a separate VM state.
    C4M_ZPushVmPtr     = 0x0F,
    // Stores a value to the attribute named by the top value on the stack. The
    // value to store is the stack value just below it. Both values are popped
    // from the stack. If the instruction's arg is non-zero, the attribute
    // will be locked when it's set. This instruction expects that the attribute
    // is stored on the stack via C4M_ZPushStaticPtr.
    C4M_ZAssignAttr    = 0x1D,
    // Pops the top value from the stack. This is the same as C4M_ZMoveSp with
    // an adjustment of -1.
    C4M_ZPop           = 0x20,
    // Stores the encoded immediate value into the value address encoded in the
    // instruction. The storage address is determined as described in the above
    // comment about address encodings.
    C4M_ZStoreImm      = 0x22,
    // Unpack the elements of a tuple, storing each one into the lvalue on the
    // stack, popping each lvalue as its assigned. The number of assignments to
    // perform is encoded in the instruction's arg field.
    C4M_ZUnpack        = 0x23,
    // Swap the two top values on the stack.
    C4M_ZSwap          = 0x24,
    // Perform an assignment into the lvalue at the top of the stack of the
    // value just below it and pops both items from the stack. This should be
    // paired with C4M_ZPushAddr or C4M_ZLoadFromAttr with a non-zero arg.
    C4M_ZAssignToLoc   = 0x25,
    // Jump if the top value on the stack is zero. The pc is adjusted by the
    // number of bytes encoded in the instruction's arg field, which is always
    // a multiple of the size of an instruction. A negative value jumps
    // backward. If the comparison triggers a jump, the stack is left as-is,
    // but the top value is popped if no jump occurs.
    C4M_ZJz            = 0x30,
    // Jump if the top value on the stack is not zero. The pc is adjusted by the
    // number of bytes encoded in the instruction's arg field, which is always
    // a multiple of the size of an instruction. A negative value jumps
    // backward. If the comparison triggers a jump, the stack is left as-is,
    // but the top value is popped if no jump occurs.
    C4M_ZJnz           = 0x31,
    // Unconditional jump. Adjust the pc by the number of bytes encoded in the
    // instruction's arg field, which is always a multiple of the size of an
    // instruction. A negative value jumps backward.
    C4M_ZJ             = 0x32,
    // Call one of con4m's builtin functions via vtable for the object on the
    // top of the stack. The index of the builtin function to call is encoded
    // in the instruction's arg field and should be treated as one of the values
    // in the c4m_builtin_type_fn enum type. The number of arguments expected to
    // be on the stack varies for each function. In all cases, contrary to how
    // other calls are handled, the arguments are popped from the stack and the
    // result is pushed onto the stack.
    C4M_ZTCall         = 0x33,
    // Call a "native" function, one which is defined in bytecode from the same
    // object file. The index of the function to call is encoded in the
    // instruction's arg parameter, adjusted up by 1 (0 is not a valid index).
    // The index is used to lookup the function from the object file's
    // func_info table.
    C4M_Z0Call         = 0x34,
    // Call an external "non-native" function via FFI. The index of the function
    // to call is encoded in the instruction's arg parameter. This index is not
    // adjusted as it is for other, similar instructions. The index is used to
    // lookup the function from the object file's ffi_info table. The arguments
    // are popped from the stack. The return value is stored in the return value
    // register.
    C4M_ZFFICall       = 0x35,
    // Call a module's initialization code. This corresponds with a "use"
    // statement. The module index of the module to call is encoded in the
    // instruction's arg parameter, adjusted up by 1 (0 is not a valid index).
    // The index is used to lookup the module from the object file's
    // module_contents table.
    C4M_ZCallModule    = 0x36,
    // Pops a callback from the stack (pushed via either C4M_ZPushFfiPtr or
    // C4M_ZPushVmPtr) and runs it. If the callback is an FFI callback, the
    // action is basically the same as C4M_ZFFICalll, except it uses the index
    // from the callback. Otherwise, the callback is the same as a native call
    // via C4M_Z0Call, except it uses the index from the callback.
    C4M_ZRunCallback   = 0x37,
    // Unused; will redo when adding objects.
    C4M_ZSObjNew       = 0x38,
    // Box a literal, which requires supplying the type for the object.
    C4M_ZBox           = 0x3e,
    // Unbox a value literal into its actual value.
    C4M_ZUnbox         = 0x3f,
    // Compare (and pop) two types to see if they're comptable.
    C4M_ZTypeCmp       = 0x40,
    // Compare (and pop) two values to see if they're equal. Note that
    // this is not the same as checking for object equality; this assumes
    // primitive type or reference.
    C4M_ZCmp           = 0x41,
    C4M_ZLt            = 0x42,
    C4M_ZULt           = 0x43,
    C4M_ZLte           = 0x44,
    C4M_ZULte          = 0x45,
    C4M_ZGt            = 0x46,
    C4M_ZUGt           = 0x47,
    C4M_ZGte           = 0x48,
    C4M_ZUGte          = 0x49,
    C4M_ZNeq           = 0x4A,
    // Do a GTE comparison without popping.
    C4M_ZGteNoPop      = 0x4D,
    // Do an equality comparison without popping.
    C4M_ZCmpNoPop      = 0x4E,
    // Mask out 3 bits from the top stack value; push them onto the stack.
    // Remove the bits from the pointer.
    //
    // This is meant to remove the low bits of pointers (pointer
    // stealing), so we can communicate info through them at
    // runtime. The pointer gets those bits cleared, but they are
    // pushed onto the stack. Currently, we use this to indicate how
    // much space is required per-item if we are iterating over a
    // type.
    //
    // Often we could know at compile time and generate code specific
    // to the size, but as a first pass this is easier.
    C4M_ZUnsteal       = 0x4F,
    // Begin register ops.
    C4M_ZPopToR0       = 0x50,
    // Pushes the value in R1 onto the stack.
    C4M_ZPushFromR0    = 0x51,
    // Zero out R0
    C4M_Z0R0c00l       = 0x52,
    C4M_ZPopToR1       = 0x53,
    // Pushes the value in R1 onto the stack.
    C4M_ZPushFromR1    = 0x54,
    C4M_ZPopToR2       = 0x56,
    C4M_ZPushFromR2    = 0x57,
    C4M_ZPopToR3       = 0x59,
    C4M_ZPushFromR3    = 0x5A,
    // Exits the current call frame, returning the current state back to the
    // originating location, which is the instruction immediately following the
    // C4M_Z0Call instruction that created this frame.
    C4M_ZRet           = 0x60,
    // Exits the current stack frame, returning the current state back to the
    // originating location, which is the instruction immediately following the
    // C4M_ZCallModule instruction that created this frame.
    C4M_ZModuleRet     = 0x61,
    // Halt the current program immediately.
    C4M_ZHalt          = 0x62,
    // Initialze module parameters. The number of parameters is encoded in the
    // instruction's arg field. This is only used during module initialization.
    C4M_ZModuleEnter   = 0x63,
    // Adjust the stack pointer down by the amount encoded in the instruction's
    // arg field. This means specifically that the arg field is subtracted from
    // sp, so a single pop would encode -1 as the adjustment.
    C4M_ZMoveSp        = 0x65,
    // Test the top stack value. If it is non-zero, pop it and continue running
    // the program. Otherwise, print an assertion failure and stop running the
    // program.
    // Math operations have signed and unsigned variants. We can go from
    // signed to unsigned where it makes sense by adding 0x10.
    // Currently, we do not do this for bit ops, they are just all unsigned.
    C4M_ZAdd           = 0x70,
    C4M_ZSub           = 0x71,
    C4M_ZMul           = 0x72,
    C4M_ZDiv           = 0x73,
    C4M_ZMod           = 0x74,
    C4M_ZBXOr          = 0x75,
    C4M_ZShl           = 0x76,
    C4M_ZShr           = 0x77,
    C4M_ZBOr           = 0x78,
    C4M_ZBAnd          = 0x79,
    C4M_ZBNot          = 0x7A,
    C4M_ZUAdd          = 0x80, // Same as ZAdd
    C4M_ZUSub          = 0x81, // Same as ZSub
    C4M_ZUMul          = 0x82,
    C4M_ZUDiv          = 0x83,
    C4M_ZUMod          = 0x84,
    C4M_ZFAdd          = 0x90,
    C4M_ZFSub          = 0x91,
    C4M_ZFMul          = 0x92,
    C4M_ZFDiv          = 0x93,
    C4M_ZAssert        = 0xA0,
    // Set the specified attribute to be "lock on write". Triggers an error if
    // the attribute is already set to lock on write. This instruction expects
    // the top stack value to be loaded via ZPushStaticPtr and does not pop it.
    // The attribute to lock is named according to the top stack value.
    C4M_ZLockOnWrite   = 0xB0,
    // Given a static offset as an argument, locks the mutex passed in
    // the argument.
    C4M_ZLockMutex     = 0xB1,
    C4M_ZUnlockMutex   = 0xB2,
    // Arithmetic and bitwise operators on 64-bit values; the two-arg
    // ones conceptually pop the right operand, then the left operand,
    // perform the operation, then push. But generally after the RHS
    // pop, the left operand will get replaced without additional
    // movement.
    //
    // Perform a logical not operation on the top stack value. If the value is
    // zero, it will be replaced with a one value of the same type. If the value
    // is non-zero, it will be replaced with a zero value of the same type.
    C4M_ZNot           = 0xE0,
    C4M_ZAbs           = 0xE1,
    // The rest of these mathy operators are less general purpose, and are
    // just used to make our lives easier when generating code, so
    // there's currently a lack of symmetry here.
    //
    // This version explicitly takes an immediate on the RHS so we
    // don't have to test for it.
    C4M_ZShlI          = 0xF0,
    C4M_ZSubNoPop      = 0xF1,
    // This one mostly computes abs(x) / x.
    // The only difference is that it's well defined for 0, it returns
    // 1 (0 is not a negative number).
    // This is mostly used to generate the step for ranged loops; you
    // can do for x in 10 to 0 to get 10 down to 1 (inclusive).
    C4M_ZGetSign       = 0xF2,
    // Print the error message that is the top value on the stack and stop
    // running the program.
    C4M_ZBail          = 0xFE,
    // Nop does nothing.
    C4M_ZNop           = 0xFF,
#ifdef C4M_DEV
    C4M_ZPrint = 0xFD,
    C4M_ZDebug = 0xFC,
#endif
} c4m_zop_t;

typedef struct c4m_static_memory {
    c4m_mem_ptr *items;
    uint32_t     num_items;
    uint32_t     alloc_len;
} c4m_static_memory;

// We'll make the main VM container, c4m_vm_t an object type (C4M_T_VM) so that
// we can have an entry point into the core marshalling functionality, but we
// won't make other internal types object types. Only those structs that have a
// z prefix on their name get marshalled. For lists we'll use c4m_type_list
// with a c4m_type_ref base type. But this means that we have to marshal these
// lists manually. As of right now at least, all dicts used have object keys
// and values (ints, strings, etc).
typedef struct {
    c4m_zop_t   op;
    uint8_t     pad;
    int32_t     module_id;
    int32_t     line_no;
    int32_t     arg;
    int64_t     immediate;
    c4m_type_t *type_info;
} c4m_zinstruction_t;

typedef struct {
    c4m_utf8_t *name;
    c4m_type_t *tid;
    // Nim casts this around as a pointer, but it's always used as an integer
    // index into an array.
    int64_t     impl;
    int32_t     mid;
    bool        ffi;
    bool        skip_boxes;
} c4m_zcallback_t;

// stack values have no indicator of what's actually stored, instead relying on
// instructions to assume correctly what's present.
typedef union c4m_value_t {
    void              *vptr;
    c4m_zcallback_t   *callback;
    c4m_obj_t         *lvalue;
    char              *cptr;
    union c4m_value_t *fp;         // saved fp
    c4m_obj_t          rvalue;
    uint64_t           static_ptr; // offset into static_data
    // saved pc / module_id, along with unsigned int values where
    // we don't care about the type field for the operation.
    uint64_t           uint;
    int64_t            sint; // signed int values.
    c4m_box_t          box;
    double             dbl;
    bool               boolean;
} c4m_value_t;

// Might want to trim a bit out of it, but for right now, an going to not.
typedef struct c4m_ffi_decl_t c4m_zffi_info_t;

typedef struct {
    c4m_type_t *tid;
    int64_t     offset;
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
    c4m_list_t *sym_types; // tspec_ref: c4m_zsymbol_t
    c4m_type_t *tid;
    c4m_str_t  *shortdoc;
    c4m_str_t  *longdoc;
    int32_t     mid;    // module_id
    int32_t     offset; // offset to start of instructions in module
    int32_t     size;   // Stack frame size.
    int32_t     static_lock;
} c4m_zfn_info_t;

enum {
    // The 'ORIG' fields all map to the state we want if we do a 'reset'.
    // The rest are the current saved state due to incremental compilation.
    C4M_CCACHE_ORIG_SCONSTS,
    C4M_CCACHE_ORIG_OCONSTS,
    C4M_CCACHE_ORIG_VCONSTS,
    C4M_CCACHE_ORIG_STATIC,
    C4M_CCACHE_ORIG_ASCOPE,
    C4M_CCACHE_ORIG_GSCOPE,
    C4M_CCACHE_ORIG_MODULES,
    C4M_CCACHE_ORIG_SPEC,
    C4M_CCACHE_ORIG_SORT,
    C4M_CCACHE_ORIG_ATTR,
    C4M_CCACHE_ORIG_SECTIONS,
    C4M_CCACHE_ORIG_TYPES,
    C4M_CCACHE_CUR_SCONSTS,
    C4M_CCACHE_CUR_OCONSTS,
    C4M_CCACHE_CUR_VCONSTS,
    C4M_CCACHE_CUR_ASCOPE,
    C4M_CCACHE_CUR_GSCOPE,
    C4M_CCACHE_CUR_MODULES,
    C4M_CCACHE_CUR_TYPES,
    C4M_CCACHE_LEN,
};

typedef union {
    struct {
        uint8_t preview;
        uint8_t patch;
        uint8_t minor;
        uint8_t major;
    } dotted;
    uint64_t number;
} c4m_version_t;

typedef struct {
    void              *ccache[C4M_CCACHE_LEN];
    struct c4m_spec_t *attr_spec;
    c4m_static_memory *static_contents;
    c4m_list_t        *module_contents; // tspec_ref: c4m_zmodule_info_t
    c4m_list_t        *func_info;       // tspec_ref: c4m_zfn_info_t
    c4m_list_t        *ffi_info;        // tspec_ref: c4m_zffi_info_t
    int                ffi_info_entries;
    // CCACHE == 'compilation cache', which means stuff we use to
    // re-start incremental compilation; we *could* rebuild this stuff
    // when needed, but these things shouldn't be too large.
    //
    // The items are in the enum above.
    //

    c4m_version_t con4m_version;
    int32_t       first_entry;    // Initial entry point.
    int32_t       default_entry;  // The module to whitch we reset.
    bool          using_attrs;    // Should move into object.
    bool          root_populated; // This too.
} c4m_zobject_file_t;

typedef struct {
    struct c4m_module_t *call_module;
    struct c4m_module_t *targetmodule;
    c4m_zfn_info_t      *targetfunc;
    int32_t              calllineno;
    int32_t              targetline;
    uint32_t             pc;
} c4m_vmframe_t;

typedef struct {
    c4m_zinstruction_t *lastset;
    void               *contents;
    c4m_type_t         *type; // Value types will not generally be boxed.
    bool                is_set;
    bool                locked;
    bool                lock_on_write;
    int32_t             module_lock;
    bool                override;
} c4m_attr_contents_t;

typedef struct {
    // The stuff in this struct isn't saved out; it needs to be
    // reinitialized on each startup.
#ifdef C4M_DEV
    c4m_buf_t    *print_buf;
    c4m_stream_t *print_stream;
#endif
} c4m_zrun_state_t;

// C4M_T_VM
typedef struct {
    c4m_zobject_file_t *obj;
    c4m_dict_t         *attrs; // string, c4m_attr_contents_t (tspec_ref)
    c4m_zrun_state_t   *run_state;
    c4m_obj_t         **module_allocations;
    c4m_set_t          *all_sections; // string
    c4m_duration_t      creation_time;
    c4m_duration_t      first_saved_run_time;
    c4m_duration_t      last_saved_run_time;
    uint32_t            num_saved_runs;
    int32_t             entry_point;
} c4m_vm_t;

typedef struct {
    // vm is a pointer to the global vm state shared by various threads.
    c4m_vm_t *vm;

    // sp is the current stack pointer. The stack grows down, so sp starts out
    // as &stack[STACK_SIZE] (stack bottom)
    c4m_value_t *sp;

    // fp points to the start of the current frame on the stack.
    c4m_value_t *fp;

    // current_module is the module to which currently executing instructions
    // belong.
    struct c4m_module_t *current_module;

    // The arena this allocation is from.
    c4m_arena_t *thread_arena;

    // frame_stack is the base address of the call stack
    c4m_vmframe_t frame_stack[C4M_MAX_CALL_DEPTH];

    // stack is the base address of the stack for this thread of execution.
    // the stack grows down, so the stack bottom is &stack[STACK_SIZE]
    c4m_value_t stack[C4M_STACK_SIZE];

    // General purpose registers.
    // R0 should generally only be used for passing return values.
    c4m_obj_t r0;
    c4m_obj_t r1;
    c4m_obj_t r2;
    c4m_obj_t r3;

    // pc is the current program counter, which is an index into current_module
    // instructions array.
    uint32_t pc;

    // num_frames is the number of active frames in the call stack. the current
    // frame is always frame_stack[num_frames - 1]
    int32_t num_frames;

    // running is true if this thread state is currently running VM code.
    bool running;

    // error is true if this thread state raised an error during evaluation.
    bool error;

} c4m_vmthread_t;

#define C4M_F_ATTR_PUSH_FOUND 1
#define C4M_F_ATTR_SKIP_LOAD  2

#pragma once

#include "con4m.h"

typedef struct c4m_marshal_wl_item_t {
    c4m_alloc_hdr                *header;
    uint64_t                     *data;
    uint64_t                      dataoffset;
    struct c4m_marshal_wl_item_t *next;
} c4m_marshal_wl_item_t;

// This needs to stay in sync w/ c4m_alloc_hdr
typedef struct c4m_marshal_ctx {
    c4m_stream_t          *s;
    c4m_dict_t            *memos;
    c4m_dict_t            *needed_patches;
    c4m_marshal_wl_item_t *worklist_start;
    c4m_marshal_wl_item_t *worklist_end;
    c4m_marshal_wl_item_t *cur;
    uint64_t               base;
    uint64_t               memo;
    uint64_t               write_offset;
} c4m_marshal_ctx;

typedef struct {
    c4m_mem_ptr    base_ptr;
    c4m_mem_ptr    end;
    uint64_t       fake_heap_base;
    c4m_alloc_hdr *this_alloc;

} c4m_unmarshal_ctx;

typedef struct c4m_marshaled_hdr {
    uint64_t empty_guard;
    uint64_t next_offset;
    uint64_t empty_fw;
    uint64_t empty_arena;
    uint32_t alloc_len;
    uint32_t request_len;
    uint64_t scan_fn_id; // Only marshaled for non-object types.
#if defined(C4M_FULL_MEMCHECK)
    uint64_t end_guard_loc;
#endif
#if defined(C4M_ADD_ALLOC_LOC_INFO)
    char *alloc_file;
    int   alloc_line;
#endif
    uint32_t    finalize  : 1;
    uint32_t    con4m_obj : 1;
    uint32_t    type_obj  : 1;
    __uint128_t cached_hash;
    alignas(C4M_FORCED_ALIGNMENT) uint64_t data[];
} c4m_marshaled_hdr;

// This is going to be the same layout as c4m_alloc_hdr_t because we
// are going to get unmarshalling down to the point where data can
// be placed statically with relocations. Unmarshalling will only
// involve fixing up pointers and filling in any necessary bits of
// alloc headers.

typedef struct {
    uint32_t scan_fn;
    uint32_t alloc_len : 31;
} c4m_marshal_record_t;

extern void    c4m_marshal_cstring(char *, c4m_stream_t *);
extern char   *c4m_unmarshal_cstring(c4m_stream_t *);
extern void    c4m_marshal_i64(int64_t, c4m_stream_t *);
extern int64_t c4m_unmarshal_i64(c4m_stream_t *);
extern void    c4m_marshal_i32(int32_t, c4m_stream_t *);
extern int32_t c4m_unmarshal_i32(c4m_stream_t *);
extern void    c4m_marshal_i16(int16_t, c4m_stream_t *);
extern int16_t c4m_unmarshal_i16(c4m_stream_t *);

// These functions need to either be exported, because they're needed wherever
// we allocate, and they're needed centrally to support auto-marshaling.
//
// con4m object types are first here.
extern void c4m_buffer_set_gc_bits(uint64_t *, c4m_base_obj_t *);
extern void c4m_zcallback_gc_bits(uint64_t *, c4m_base_obj_t *);
extern void c4m_dict_gc_bits_obj(uint64_t *, c4m_base_obj_t *);
extern void c4m_dict_gc_bits_raw(uint64_t *, c4m_base_obj_t *);
extern void c4m_grid_set_gc_bits(uint64_t *, c4m_base_obj_t *);
extern void c4m_renderable_set_gc_bits(uint64_t *, c4m_base_obj_t *);
extern void c4m_list_set_gc_bits(uint64_t *, c4m_base_obj_t *);
extern void c4m_set_set_gc_bits(uint64_t *, c4m_base_obj_t *);
extern void c4m_stream_set_gc_bits(uint64_t *, c4m_base_obj_t *);
extern void c4m_str_set_gc_bits(uint64_t *, c4m_base_obj_t *);
extern void c4m_tree_node_set_gc_bits(uint64_t *, c4m_base_obj_t *);
extern void c4m_exception_gc_bits(uint64_t *, c4m_base_obj_t *);
extern void c4m_type_set_gc_bits(uint64_t *, c4m_base_obj_t *);
extern void c4m_pnode_set_gc_bits(uint64_t *, c4m_base_obj_t *);

// Adt / IO related, etc.
extern void c4m_dict_gc_bits_bucket_full(uint64_t *, mmm_header_t *);
extern void c4m_dict_gc_bits_bucket_key(uint64_t *, mmm_header_t *);
extern void c4m_dict_gc_bits_bucket_value(uint64_t *, mmm_header_t *);
extern void c4m_dict_gc_bits_bucket_hdr_only(uint64_t *, mmm_header_t *);
extern void c4m_cookie_gc_bits(uint64_t *, c4m_cookie_t *);
extern void c4m_basic_http_set_gc_bits(uint64_t *, c4m_basic_http_t *);
extern void c4m_party_gc_bits(uint64_t *, c4m_party_t *);
extern void c4m_monitor_gc_bits(uint64_t *, c4m_monitor_t *);
extern void c4m_subscription_gc_bits(uint64_t *, c4m_subscription_t *);
extern void c4m_fmt_gc_bits(uint64_t *, c4m_fmt_info_t *);
extern void c4m_tag_gc_bits(uint64_t *, c4m_tag_item_t *);
extern void c4m_rs_gc_bits(uint64_t *, c4m_render_style_t *);
extern void c4m_tpat_gc_bits(uint64_t *, c4m_tpat_node_t *);

// Compiler.
extern void c4m_cctx_gc_bits(uint64_t *, c4m_compile_ctx *);
extern void c4m_smem_gc_bits(uint64_t *, c4m_static_memory *);
extern void c4m_module_ctx_gc_bits(uint64_t *, c4m_module_compile_ctx *);
extern void c4m_zmodule_gc_bits(uint64_t *, c4m_zmodule_info_t *);
extern void c4m_token_set_gc_bits(uint64_t *, void *);
extern void c4m_checkpoint_gc_bits(uint64_t *, c4m_checkpoint_t *);
extern void c4m_comment_node_gc_bits(uint64_t *, c4m_comment_node_t *);
extern void c4m_sym_gc_bits(uint64_t *, c4m_symbol_t *);
extern void c4m_scope_gc_bits(uint64_t *, c4m_scope_t *);

extern void c4m_cfg_gc_bits(uint64_t *, c4m_cfg_node_t *);
extern void c4m_cfg_status_gc_bits(uint64_t *, c4m_cfg_status_t *);
extern void c4m_zinstr_gc_bits(uint64_t *, c4m_zinstruction_t *);
extern void c4m_zfn_gc_bits(uint64_t *, c4m_zfn_info_t *);
extern void c4m_jump_info_gc_bits(uint64_t *, c4m_jump_info_t *);
extern void c4m_backpatch_gc_bits(uint64_t *, c4m_call_backpatch_info_t *);
extern void c4m_module_param_gc_bits(uint64_t *, c4m_module_param_info_t *);
extern void c4m_sig_info_gc_bits(uint64_t *, c4m_sig_info_t *);
extern void c4m_fn_decl_gc_bits(uint64_t *, c4m_fn_decl_t *);
extern void c4m_module_info_gc_bits(uint64_t *, c4m_module_info_t *);
extern void c4m_err_set_gc_bits(uint64_t *, c4m_compile_error *);
extern void c4m_objfile_gc_bits(uint64_t *, c4m_zobject_file_t *);

// Con4m specific runtime stuff
extern void c4m_spec_gc_bits(uint64_t *, c4m_spec_t *);
extern void c4m_attr_info_gc_bits(uint64_t *, c4m_attr_info_t *);
extern void c4m_section_gc_bits(uint64_t *, c4m_spec_section_t *);
extern void c4m_spec_field_gc_bits(uint64_t *, c4m_spec_field_t *);
extern void c4m_type_info_gc_bits(uint64_t *, c4m_type_info_t *);
extern void c4m_vm_gc_bits(uint64_t *, c4m_vm_t *);
extern void c4m_vmthread_gc_bits(uint64_t *, c4m_vmthread_t *);
extern void c4m_deferred_cb_gc_bits(uint64_t *, c4m_deferred_cb_t *);
extern void c4m_frame_gc_bits(uint64_t *, c4m_fmt_frame_t *);

static inline void
c4m_marshal_i8(int8_t c, c4m_stream_t *s)
{
    c4m_stream_raw_write(s, 1, (char *)&c);
}

static inline int8_t
c4m_unmarshal_i8(c4m_stream_t *s)
{
    int8_t ret;

    c4m_stream_raw_read(s, 1, (char *)&ret);

    return ret;
}

static inline void
c4m_marshal_u8(uint8_t n, c4m_stream_t *s)
{
    c4m_marshal_i8((int8_t)n, s);
}

static inline uint8_t
c4m_unmarshal_u8(c4m_stream_t *s)
{
    return (uint8_t)c4m_unmarshal_i8(s);
}

static inline void
c4m_marshal_bool(bool value, c4m_stream_t *s)
{
    c4m_marshal_i8(value ? 1 : 0, s);
}

static inline bool
c4m_unmarshal_bool(c4m_stream_t *s)
{
    return (bool)c4m_unmarshal_i8(s);
}

static inline void
c4m_marshal_u64(uint64_t n, c4m_stream_t *s)
{
    c4m_marshal_i64((int64_t)n, s);
}

static inline uint64_t
c4m_unmarshal_u64(c4m_stream_t *s)
{
    return (uint64_t)c4m_unmarshal_i64(s);
}

static inline void
c4m_marshal_u32(uint32_t n, c4m_stream_t *s)
{
    c4m_marshal_i32((int32_t)n, s);
}

static inline uint32_t
c4m_unmarshal_u32(c4m_stream_t *s)
{
    return (uint32_t)c4m_unmarshal_i32(s);
}

static inline void
c4m_marshal_u16(uint16_t n, c4m_stream_t *s)
{
    c4m_marshal_i16((int16_t)n, s);
}

static inline uint16_t
c4m_unmarshal_u16(c4m_stream_t *s)
{
    return (uint16_t)c4m_unmarshal_i16(s);
}

static inline c4m_alloc_hdr *
c4m_find_allocation_record(void *addr)
{
    // Align the pointer for scanning back to the header.
    // It should be aligned, but people do the craziest things.
    void **p = (void **)(((uint64_t)addr) & ~0x0000000000000007);

    while (*(uint64_t *)p != c4m_gc_guard) {
        --p;
    }

    return (c4m_alloc_hdr *)p;
}

extern void       c4m_automarshal_stream(void *, c4m_stream_t *);
extern c4m_buf_t *c4m_automarshal(void *);
extern void      *c4m_autounmarshal_stream(c4m_stream_t *);
extern void      *c4m_autounmarshal(c4m_buf_t *);

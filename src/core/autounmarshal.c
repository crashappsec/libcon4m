#include "con4m.h"

#define SCAN_FN_END (void *)0xffffffffffffffff

#define FIRST_BAD_SCAN_IX \
    (int)((sizeof(c4m_builtin_gc_bit_fns) / sizeof(void *)) - 1)

extern const uint64_t c4m_marshal_magic;
extern c4m_type_t    *c4m_type_node_for_list_of_type_objects;
extern uint64_t       c4m_end_guard;
const void           *c4m_builtin_gc_bit_fns[] = {
    NULL,
    c4m_buffer_set_gc_bits,
    c4m_zcallback_gc_bits,
    c4m_dict_gc_bits_obj,
    c4m_dict_gc_bits_raw,
    c4m_grid_set_gc_bits,
    c4m_renderable_set_gc_bits,
    c4m_list_set_gc_bits,
    c4m_set_set_gc_bits,
    c4m_stream_set_gc_bits,
    c4m_str_set_gc_bits,
    c4m_tree_node_set_gc_bits,
    c4m_exception_gc_bits,
    c4m_type_set_gc_bits,
    c4m_pnode_set_gc_bits,
    c4m_dict_gc_bits_bucket_full,
    c4m_dict_gc_bits_bucket_key,
    c4m_dict_gc_bits_bucket_value,
    c4m_dict_gc_bits_bucket_hdr_only,
    c4m_cookie_gc_bits,
    c4m_basic_http_set_gc_bits,
    c4m_party_gc_bits,
    c4m_monitor_gc_bits,
    c4m_subscription_gc_bits,
    c4m_fmt_gc_bits,
    c4m_tag_gc_bits,
    c4m_rs_gc_bits,
    c4m_tpat_gc_bits,
    c4m_cctx_gc_bits,
    c4m_module_ctx_gc_bits,
    c4m_zmodule_gc_bits,
    c4m_token_set_gc_bits,
    c4m_checkpoint_gc_bits,
    c4m_comment_node_gc_bits,
    c4m_sym_gc_bits,
    c4m_scope_gc_bits,
    c4m_cfg_gc_bits,
    c4m_cfg_status_gc_bits,
    c4m_zinstr_gc_bits,
    c4m_zfn_gc_bits,
    c4m_jump_info_gc_bits,
    c4m_backpatch_gc_bits,
    c4m_module_param_gc_bits,
    c4m_sig_info_gc_bits,
    c4m_fn_decl_gc_bits,
    c4m_module_info_gc_bits,
    c4m_err_set_gc_bits,
    c4m_objfile_gc_bits,
    c4m_spec_gc_bits,
    c4m_attr_info_gc_bits,
    c4m_section_gc_bits,
    c4m_spec_field_gc_bits,
    c4m_type_info_gc_bits,
    c4m_vm_gc_bits,
    c4m_vmthread_gc_bits,
    c4m_deferred_cb_gc_bits,
    c4m_frame_gc_bits,
    c4m_smem_gc_bits,
    SCAN_FN_END,
};

static inline uint64_t
validate_header(c4m_unmarshal_ctx *ctx)
{
    c4m_mem_ptr p = ctx->base_ptr;

    uint64_t found_magic = *p.u64;
    p.u64++;

    little_64(found_magic);

    if (found_magic != c4m_marshal_magic) {
        c4m_utf8_t *err = NULL;

        if ((found_magic & ~03ULL) == (c4m_marshal_magic & ~03ULL)) {
            err = c4m_cstr_format(
                "Input was marshaled using different memory options. Expected "
                "[i]a Con4m marshal header value of[/] of [em]{:x}[/], but "
                "got [em]{:x}[/]",
                c4m_box_u64(c4m_marshal_magic),
                c4m_box_u64(found_magic));
        }
        else {
            if ((found_magic & 0xffff) == (c4m_marshal_magic & 0xffff)) {
                err = c4m_cstr_format(
                    "Con4m version used for marshaling is not known to this "
                    "version of con4m [em]({:x})[/]",
                    c4m_box_u64(found_magic));
            }
            else {
                err = c4m_cstr_format(
                    "First 8 bytes [em]({:x})[/] is not a valid Con4m marshal "
                    "header.",
                    c4m_box_u64(found_magic));
            }
        }
        C4M_RAISE(err);
    }

    uint64_t value = *p.u64;
    little_64(value);

    if (value) {
        ctx->fake_heap_base = value & 0xffffffff00000000ULL;
        return value & 0x00000000ffffffffULL;
    }

    return 0;
}

static void __attribute__((noreturn))
unmarshal_invalid()
{
    C4M_CRAISE("Invalid or corrupt marshal data.");
}

static void __attribute__((noreturn))
future_c4m_version()
{
    C4M_CRAISE(
        "Marshaled data provides information that "
        "does not exist in this version of Con4m, thus cannot "
        "be unmarshaled.");
}

// Make sure the value already went through little_64() before
// calling.
static inline bool
is_fake_heap_ptr(c4m_unmarshal_ctx *ctx, uint64_t value)
{
    return (value & 0xffffffff00000000ULL) == ctx->fake_heap_base;
}

static inline bool
is_fake_early_heap_ptr(c4m_unmarshal_ctx *ctx, uint64_t value)
{
    uint64_t fake_base = (~ctx->fake_heap_base) & 0xffffffff00000000ULL;

    return (value & 0xffffffff00000000ULL) == fake_base;
}

static uint64_t
translate_pointer(c4m_unmarshal_ctx *ctx, uint64_t value)
{
    c4m_mem_ptr p;
    uint64_t    offset = value & 0x00000000ffffffffUL;
    p.c                = ctx->base_ptr.c + offset;

    if (p.c >= ctx->end.c) {
        unmarshal_invalid();
    }

    return (uint64_t)p.c;
}

static void *
translate_early_pointer(c4m_unmarshal_ctx *ctx, uint64_t value)
{
    uint64_t offset = value & 0x00000000ffffffffUL;

    if (offset == 0xfffffffe) {
        return (void *)c4m_type_node_for_list_of_type_objects;
    }
    if (offset & 0x80000000) {
        uint64_t typeid = offset & 0x7fffffff;
        if (typeid >= C4M_NUM_BUILTIN_DTS) {
            unmarshal_invalid();
        }
        return (void *)(c4m_bi_types[typeid]);
    }

    c4m_mem_ptr p = (c4m_mem_ptr){.v = c4m_early_type_arena};
    p.u64         = p.u64 + offset;

    if (p.u64 >= c4m_early_type_arena->heap_end) {
        unmarshal_invalid();
    }

    return (void *)(p.u64);
}

static void *
lookup_scan_fn(void *index_as_void)
{
    int64_t index = (int64_t)index_as_void;
    if (!index || index == -1) {
        return index_as_void;
    }

    if (index < 0 || index >= FIRST_BAD_SCAN_IX) {
        future_c4m_version();
    }

    return (void *)c4m_builtin_gc_bit_fns[index];
}

static inline void
setup_header(c4m_unmarshal_ctx *ctx, c4m_marshaled_hdr *marshaled_hdr)
{
    little_32(marshaled_hdr->alloc_len);
    little_32(marshaled_hdr->request_len);
    little_64(marshaled_hdr->scan_fn_id);
    little_64(marshaled_hdr->next_offset);

    uint64_t next = marshaled_hdr->next_offset;

    if (next) {
        if (c4m_unlikely(!is_fake_heap_ptr(ctx, next))) {
            unmarshal_invalid();
        }
        next = translate_pointer(ctx, next);
    }

    c4m_alloc_hdr *hdr = (c4m_alloc_hdr *)marshaled_hdr;

    hdr->guard     = c4m_gc_guard;
    hdr->fw_addr   = NULL;
    hdr->next_addr = (void *)next;
    hdr->arena     = ctx->this_alloc->arena;
    hdr->scan_fn   = lookup_scan_fn(hdr->scan_fn);

#if defined(C4M_FULL_MEMCHECK)
    uint64_t  word_len       = hdr->alloc_len / sizeof(uint64_t);
    uint64_t *end_guard_addr = &hdr->data[word_len - 2];
    *end_guard_addr          = c4m_end_guard;
    hdr->end_guard_loc       = end_guard_addr;
#endif
#if defined(C4M_ADD_ALLOC_LOC_INFO)
    hdr->alloc_file = "marshaled data";
    hdr->alloc_line = (int)(ctx->fake_heap_base >> 32);
#endif
}

static inline c4m_marshaled_hdr *
validate_record(c4m_unmarshal_ctx *ctx, void *ptr)
{
    c4m_mem_ptr hdr = (c4m_mem_ptr){.v = ptr};

    setup_header(ctx, hdr.v);

    // This shouldn't get called on the end record,
    // so the 'next_alloc' field should always look right.
    // Worst case, the stream could end in a zero-byte alloc and
    // still be valid.

    if (!hdr.alloc->next_addr) {
        return hdr.v;
    }

    if (hdr.alloc->next_addr < &hdr.u64[hdr.alloc->alloc_len / 8]) {
        unmarshal_invalid();
    }
    uint64_t *last_possible = (uint64_t *)(ctx->end.c - sizeof(c4m_alloc_hdr));

    if (hdr.alloc->next_addr > last_possible) {
        unmarshal_invalid();
    }

    return hdr.v;
}

static inline void
replace_type_base(c4m_unmarshal_ctx *ctx, c4m_mem_ptr p)
{
    uint64_t typeid = *p.u64;

    if (typeid >= C4M_NUM_BUILTIN_DTS) {
        unmarshal_invalid();
    }
    *p.lvalue = (void *)&c4m_base_type_info[typeid];
}

static inline void
process_pointer_location(c4m_unmarshal_ctx *ctx, c4m_mem_ptr ptr)
{
    uint64_t value = *ptr.u64;

    if (is_fake_heap_ptr(ctx, value)) {
        *ptr.u64 = translate_pointer(ctx, value);
    }
    if (is_fake_early_heap_ptr(ctx, value)) {
        *ptr.u64 = (uint64_t)translate_early_pointer(ctx, value);
    }
}

static inline void
process_all_words(c4m_unmarshal_ctx *ctx, c4m_alloc_hdr *hdr)
{
    c4m_mem_ptr cur = (c4m_mem_ptr){.v = hdr->data};
    c4m_mem_ptr end = (c4m_mem_ptr){.v = hdr->next_addr};

    if (hdr->con4m_obj || hdr->type_obj) {
        replace_type_base(ctx, cur);
        cur.u64++;
    }

    while (cur.u64 < end.u64) {
        process_pointer_location(ctx, cur);
        cur.u64++;
    }
}

static inline void
process_one_record(c4m_unmarshal_ctx *ctx, c4m_alloc_hdr *hdr)
{
    process_all_words(ctx, hdr);
}

#if 0
static inline void
process_marked_addresses(c4m_unmarshal_ctx *ctx, c4m_alloc_hdr *hdr)
{
    c4m_mem_scan_fn scanner     = hdr->scan_fn;
    uint32_t        numwords    = hdr->alloc_len / 8;
    uint32_t        bf_byte_len = ((numwords / 64) + 1) * sizeof(uint64_t);
    uint64_t       *map         = alloca(bf_byte_len);

    memset(map, 0, bf_byte_len);

    // TODO: Need to pass heap bounds, because some data structures
    //       might have data-dependent scanners that do their own
    //       checking. Easy thing is to convert the second parameter
    //       into a context object (or add a 3rd parameter for the rest).
    (*scanner)(map, hdr->data);

    int         last_cell = numwords / 64;
    c4m_mem_ptr base      = (c4m_mem_ptr){.v = hdr->data};
    c4m_mem_ptr p;

    if (hdr->con4m_obj || hdr->type_obj) {
        p.u64 = base.u64;
        replace_type_base(ctx, p);
        map[0] &= ~1;
    }

    if (hdr->con4m_obj) {
        p.u64 = base.u64 + 1;
        process_pointer_location(ctx, p);
        map[0] &= ~2;
    }

    for (int i = 0; i <= last_cell; i++) {
        uint64_t w = map[i];

        while (w) {
            int ix = 63 - __builtin_clzll(w);
            w &= ~(1ULL << ix);
            p.u64 = base.u64 + ix;
            process_pointer_location(ctx, p);
        }
    }
}
#endif

static inline void
finish_unmarshaling(c4m_unmarshal_ctx *ctx, c4m_buf_t *buf, c4m_alloc_hdr *hdr)
{
    // First, process any patches, even though there should never
    // be any.

    int         num_patches = hdr->alloc_len;
    c4m_mem_ptr bufdata;
    c4m_mem_ptr p;
    c4m_mem_ptr patch_loc;

    bufdata.c = buf->data;

    if (hdr->data + (2 * num_patches) >= ctx->end.u64) {
        unmarshal_invalid();
    }

    p.u64 = hdr->data;

    for (int i = 0; i < num_patches; i++) {
        uint64_t offset = *p.u64;
        p.u64++;
        uint64_t value = *p.u64;
        p.u64++;

        if (!is_fake_heap_ptr(ctx, offset)) {
            unmarshal_invalid();
        }

        patch_loc.u64  = (uint64_t *)translate_pointer(ctx, offset);
        *patch_loc.u64 = value;
    }

    // Now, make the parent allocation header and the last allocation
    // header all look as if we alloc'd in small chunks.
    hdr->next_addr = ctx->this_alloc->next_addr;

#if defined(C4M_FULL_MEMCHECK)
    hdr->end_guard_loc             = ctx->this_alloc->end_guard_loc;
    ctx->this_alloc->end_guard_loc = (uint64_t *)buf->data;
    bufdata.u64[0]                 = c4m_end_guard;
#else
    bufdata.u64[0] = 0;
#endif

    // Make sure there cannot be an attempt to unmarshal the same heap
    // memory a second time.
    //
    // If the data is on the heap and you want to keep it around,
    // copy the buffer first, before calling unmarshal.
    // Note that, even if you managed to keep around a pointer to
    // the data location, we overwrote the header to signal that
    // we were done...
    bufdata.u64[1]               = ~0ULL;
    buf->byte_len                = 0;
    buf->alloc_len               = 0;
    buf->data                    = NULL;
    buf->flags                   = 0;
    ctx->this_alloc->request_len = 0;
    ctx->this_alloc->next_addr   = ctx->base_ptr.u64 + 2;
}

void *
c4m_autounmarshal(c4m_buf_t *buf)
{
    if (!buf || !buf->data) {
        unmarshal_invalid();
    }

    c4m_alloc_hdr *hdr = c4m_find_allocation_record(buf->data);

    if (!c4m_in_heap(buf->data) || ((char *)hdr->data) != buf->data) {
        buf = c4m_copy_object(buf);
        hdr = c4m_find_allocation_record(buf->data);
    }

    if (buf->byte_len < 16) {
        C4M_CRAISE("Missing full con4m marshal header.");
    }

    c4m_unmarshal_ctx ctx = {
        .base_ptr.c = buf->data,
        .end.c      = buf->data + buf->byte_len,
        .this_alloc = hdr,
    };

    uint64_t result_offset = validate_header(&ctx);

    // What was marshaled wasn't a pointer, just a single value.
    // Don't know why you'd do that, but you did...
    if (result_offset == 0) {
        uint64_t result_as_i64 = ctx.base_ptr.u64[1];
        little_64(result_as_i64);

        return (void *)result_as_i64;
    }

    // First record is 16 bytes into the data object.
    c4m_marshaled_hdr *mhdr = validate_record(&ctx, buf->data + 16);

    while (mhdr->next_offset) {
        if (ctx.end.v <= (void *)mhdr) {
            unmarshal_invalid();
        }
        process_one_record(&ctx, (c4m_alloc_hdr *)mhdr);
        mhdr = validate_record(&ctx, (void *)mhdr->next_offset);
    }

    void *result = (void *)ctx.base_ptr.c + result_offset;

    finish_unmarshaling(&ctx, buf, (c4m_alloc_hdr *)mhdr);

    return result;

#if 0 // For now, process everything.
    while (ctx.cur.c + sizeof(c4m_alloc_hdr) < ctx.end.c) {
        c4m_marshaled_hdr *mhdr = validate_record(&ctx);
        // TODO: If it's a type object, install the static pointer.
        // TODO: Prefix stuff to make it clear if it's in marshal or unmarshal.

        // Advance to the end of the alloc.
        ctx.cur.v = mhdr;
        ctx.cur.alloc += 1;

        if (mhdr->scan_fn_id == (uint64_t)C4M_GC_SCAN_ALL) {
            process_all_words(&ctx, (c4m_alloc_hdr *)mhdr);
        }
        else {
            if (mhdr->scan_fn_id != (uint64_t)C4M_GC_SCAN_NONE) {
                process_marked_addresses(&ctx, (c4m_alloc_hdr *)mhdr);
            }
        }
        process_all_words(&ctx, (c4m_alloc_hdr *)mhdr);

        if (no_more_records(&ctx)) {
        }

        process_all_words(&ctx, mhdr);
    }
#endif
}

void *
c4m_autounmarshal_stream(c4m_stream_t *stream)
{
    c4m_buf_t *b = (c4m_buf_t *)c4m_stream_read_all(stream);

    if (c4m_base_type((void *)b) != C4M_T_BUFFER) {
        C4M_CRAISE("Automarshal requires a binary buffer.");
    }

    return c4m_autounmarshal(b);
}

c4m_buf_t *
c4m_automarshal(void *ptr)
{
    c4m_buf_t    *result = c4m_buffer_empty();
    c4m_stream_t *stream = c4m_buffer_outstream(result, true);

    c4m_automarshal_stream(ptr, stream);
    c4m_stream_close(stream);
    return result;
}

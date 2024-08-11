#pragma once
#include "con4m.h"

#pragma once
#include "con4m.h"

typedef enum {
    c4m_compile_status_struct_allocated,
    c4m_compile_status_tokenized,
    c4m_compile_status_code_parsed,
    c4m_compile_status_code_loaded,     // parsed w/ declarations processed.
    c4m_compile_status_scopes_merged,   // Merged info global scope,
    c4m_compile_status_tree_typed,      // full symbols and parsing.
    c4m_compile_status_applied_folding, // Skippable and not done yet.
    c4m_compile_status_generated_code
} c4m_module_compile_status;

typedef struct c4m_ct_module_info_t {
#ifdef C4M_DEV
    // Cache all the print nodes to type check before running.
    // This is a temporary measure until we add varargs functions.
    c4m_list_t *print_nodes;
#endif
    // Information we always throw away after compilation. Things like
    // source code where we might want to keep it don't live here.
    c4m_list_t               *tokens;          // xlist: c4m_token_t
    c4m_tree_node_t          *parse_tree;
    c4m_list_t               *errors;          // xlist: c4m_compile_error
    c4m_scope_t              *global_scope;    // Symbols w/ global scope
    c4m_scope_t              *attribute_scope; // Declared or used attrs
    c4m_scope_t              *imports;         // via 'use' statements.
    c4m_cfg_node_t           *cfg;             // CFG for the module top-level.
    c4m_list_t               *callback_lits;
    c4m_spec_t               *local_specs;
    c4m_module_compile_status status;
    unsigned int              fatal_errors : 1;
} c4m_ct_module_info_t;

extern bool          c4m_validate_module_info(c4m_module_t *);
extern c4m_module_t *c4m_init_module_from_loc(c4m_compile_ctx *,
                                              c4m_str_t *);
extern c4m_module_t *c4m_new_module_compile_ctx();
extern c4m_grid_t   *c4m_get_module_summary_info(c4m_compile_ctx *);
extern bool          c4m_add_module_to_worklist(c4m_compile_ctx *,
                                                c4m_module_t *);
extern c4m_utf8_t   *c4m_package_from_path_prefix(c4m_utf8_t *,
                                                  c4m_utf8_t **);
extern c4m_utf8_t   *c4m_format_module_location(c4m_module_t *ctx,
                                                c4m_token_t *);

static inline void
c4m_module_set_status(c4m_module_t *ctx, c4m_module_compile_status status)
{
    if (ctx->ct->status < status) {
        ctx->ct->status = status;
    }
}

#define c4m_set_package_search_path(x, ...) \
    _c4m_set_package_search_path(x, C4M_VA(__VA_ARGS__))

extern c4m_module_t *
c4m_find_module(c4m_compile_ctx *ctx,
                c4m_str_t       *path,
                c4m_str_t       *module,
                c4m_str_t       *package,
                c4m_str_t       *relative_package,
                c4m_str_t       *relative_path,
                c4m_list_t      *fext);

static inline c4m_utf8_t *
c4m_module_fully_qualified(c4m_module_t *f)
{
    if (f->package) {
        return c4m_cstr_format("{}.{}", f->package, f->name);
    }

    return f->name;
}

static inline bool
c4m_path_is_url(c4m_str_t *path)
{
    if (c4m_str_starts_with(path, c4m_new_utf8("https:"))) {
        return true;
    }

    if (c4m_str_starts_with(path, c4m_new_utf8("http:"))) {
        return true;
    }

    return false;
}

#define C4M_INDEX_FN  "$index"
#define C4M_SLICE_FN  "$slice"
#define C4M_PLUS_FN   "$plus"
#define C4M_MINUS_FN  "$minus"
#define C4M_MUL_FN    "$mul"
#define C4M_MOD_FN    "$mod"
#define C4M_DIV_FN    "$div"
#define C4M_FDIV_FN   "$fdiv"
#define C4M_SHL_FN    "$shl"
#define C4M_SHR_FN    "$shr"
#define C4M_BAND_FN   "$bit_and"
#define C4M_BOR_FN    "$bit_or"
#define C4M_BXOR_FN   "$bit_xor"
#define C4M_CMP_FN    "$cmp"
#define C4M_SET_INDEX "$set_index"
#define C4M_SET_SLICE "$set_slice"

void c4m_vm_remove_compile_time_data(c4m_vm_t *);

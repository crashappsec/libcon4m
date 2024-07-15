#pragma once

typedef struct hatrack_set_st c4m_set_t;

#include "adts/dt_box.h"
#include "core/dt_alloc.h"
#include "core/dt_kargs.h"
#include "core/dt_objects.h"
#include "core/dt_literals.h"
#include "util/dt_colors.h"
#include "adts/dt_codepoints.h"
#include "util/dt_styles.h"
#include "adts/dt_strings.h"
#include "adts/dt_flags.h"
#include "adts/dt_lists.h"
#include "adts/dt_trees.h"
#include "util/dt_tree_patterns.h"
#include "core/dt_types.h"
#include "adts/dt_grids.h"
#include "adts/dt_buffers.h"
#include "io/dt_io.h"
#include "crypto/dt_crypto.h"
#include "core/dt_exceptions.h"
#include "adts/dt_mixed.h"
#include "adts/dt_tuples.h"
#include "adts/dt_streams.h"
#include "util/dt_format.h"
#include "compiler/dt_lex.h"
#include "compiler/dt_errors.h"
#include "compiler/dt_parse.h"
#include "compiler/dt_scopes.h"
#include "core/dt_ffi.h"
#include "core/dt_ufi.h"
#include "core/dt_vm.h"
#include "adts/dt_callbacks.h"
#include "compiler/dt_nodeinfo.h"
#include "compiler/dt_specs.h"
#include "compiler/dt_cfgs.h"
#include "compiler/dt_files.h"
#include "compiler/dt_compile.h"

typedef c4m_str_t *(*c4m_repr_fn)(c4m_obj_t);
typedef void (*c4m_marshal_fn)(c4m_obj_t,
                               c4m_stream_t *,
                               c4m_dict_t *,
                               int64_t *);
typedef void (*c4m_unmarshal_fn)(c4m_obj_t, c4m_stream_t *, c4m_dict_t *);
typedef c4m_obj_t (*c4m_copy_fn)(c4m_obj_t);
typedef c4m_obj_t (*c4m_binop_fn)(c4m_obj_t, c4m_obj_t);
typedef int64_t (*c4m_len_fn)(c4m_obj_t);
typedef c4m_obj_t (*c4m_index_get_fn)(c4m_obj_t, c4m_obj_t);
typedef void (*c4m_index_set_fn)(c4m_obj_t, c4m_obj_t, c4m_obj_t);
typedef c4m_obj_t (*c4m_slice_get_fn)(c4m_obj_t, int64_t, int64_t);
typedef void (*c4m_slice_set_fn)(c4m_obj_t, int64_t, int64_t, c4m_obj_t);
typedef bool (*c4m_can_coerce_fn)(c4m_type_t *, c4m_type_t *);
typedef void *(*c4m_coerce_fn)(void *, c4m_type_t *);
typedef bool (*c4m_cmp_fn)(c4m_obj_t, c4m_obj_t);
typedef c4m_obj_t (*c4m_literal_fn)(c4m_utf8_t *,
                                    c4m_lit_syntax_t,
                                    c4m_utf8_t *,
                                    c4m_compile_error_t *);
typedef c4m_obj_t (*c4m_container_lit_fn)(c4m_type_t *,
                                          c4m_list_t *,
                                          c4m_utf8_t *);
typedef c4m_str_t *(*c4m_format_fn)(c4m_obj_t, c4m_fmt_spec_t *);
typedef c4m_type_t *(*c4m_ix_item_ty_fn)(c4m_type_t *);
typedef void *(*c4m_view_fn)(c4m_obj_t, uint64_t *);

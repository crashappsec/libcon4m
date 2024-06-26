#pragma once

typedef struct hatrack_set_st c4m_set_t;

#include "con4m/datatypes/box.h"
#include "con4m/datatypes/memory.h"
#include "con4m/datatypes/kargs.h"
#include "con4m/datatypes/objects.h"
#include "con4m/datatypes/literals.h"
#include "con4m/datatypes/colors.h"
#include "con4m/datatypes/codepoints.h"
#include "con4m/datatypes/styles.h"
#include "con4m/datatypes/strings.h"
#include "con4m/datatypes/flags.h"
#include "con4m/datatypes/lists.h"
#include "con4m/datatypes/trees.h"
#include "con4m/datatypes/tree_pattern.h"
#include "con4m/datatypes/types.h"
#include "con4m/datatypes/grids.h"
#include "con4m/datatypes/buffers.h"
#include "con4m/datatypes/io.h"
#include "con4m/datatypes/crypto.h"
#include "con4m/datatypes/exceptions.h"
#include "con4m/datatypes/mixed.h"
#include "con4m/datatypes/tuples.h"
#include "con4m/datatypes/streams.h"
#include "con4m/datatypes/format.h"
#include "compiler/datatypes/lex.h"
#include "compiler/datatypes/error.h"
#include "compiler/datatypes/parse.h"
#include "compiler/datatypes/scope.h"
#include "con4m/datatypes/ffi.h"
#include "con4m/datatypes/ufi.h"
#include "con4m/datatypes/vm.h"
#include "con4m/datatypes/callbacks.h"
#include "compiler/datatypes/nodeinfo.h"
#include "compiler/datatypes/spec.h"
#include "compiler/datatypes/cfg.h"
#include "compiler/datatypes/file.h"
#include "compiler/datatypes/compile.h"

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
                                          c4m_xlist_t *,
                                          c4m_utf8_t *);
typedef c4m_str_t *(*c4m_format_fn)(c4m_obj_t, c4m_fmt_spec_t *);
typedef c4m_type_t *(*c4m_ix_item_ty_fn)(c4m_type_t *);
typedef void *(*c4m_view_fn)(c4m_obj_t, uint64_t *);

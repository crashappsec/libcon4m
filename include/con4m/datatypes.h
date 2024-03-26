#pragma once

#include <con4m/datatypes/memory.h>
#include <con4m/datatypes/kargs.h>
#include <con4m/datatypes/objects.h>
#include <con4m/datatypes/colors.h>
#include <con4m/datatypes/codepoints.h>
#include <con4m/datatypes/styles.h>
#include <con4m/datatypes/strings.h>
#include <con4m/datatypes/lists.h>
#include <con4m/datatypes/trees.h>
#include <con4m/datatypes/types.h>
#include <con4m/datatypes/grids.h>
#include <con4m/datatypes/buffers.h>
#include <con4m/datatypes/io.h>
#include <con4m/datatypes/crypto.h>
#include <con4m/datatypes/exceptions.h>
#include <con4m/datatypes/mixed.h>
#include <con4m/datatypes/tuples.h>
#include <con4m/datatypes/callbacks.h>
#include <con4m/datatypes/streams.h>

typedef any_str_t *(*repr_fn)(object_t, to_str_use_t);
typedef void (*marshal_fn)(object_t, stream_t *, struct dict_t *, int64_t *);
typedef void (*unmarshal_fn)(object_t, stream_t *, struct dict_t *);
typedef object_t (*copy_fn)(object_t);
typedef object_t (*binop_fn)(object_t, object_t);
typedef int64_t (*len_fn)(object_t);
typedef object_t (*index_get_fn)(object_t, object_t);
typedef void (*index_set_fn)(object_t, object_t, object_t);
typedef object_t (*slice_get_fn)(object_t, int64_t, int64_t);
typedef void (*slice_set_fn)(object_t, int64_t, int64_t, object_t);
typedef bool (*can_coerce_fn)(type_spec_t *, type_spec_t *);
typedef void *(*coerce_fn)(void *, type_spec_t *);

#pragma once

#include <con4m/datatypes/memory.h>
#include <con4m/datatypes/kargs.h>
#include <con4m/datatypes/objects.h>
#include <con4m/datatypes/colors.h>
#include <con4m/datatypes/codepoints.h>
#include <con4m/datatypes/styles.h>
#include <con4m/datatypes/strings.h>
#include <con4m/datatypes/lists.h>
#include <con4m/datatypes/types.h>
#include <con4m/datatypes/grids.h>
#include <con4m/datatypes/buffers.h>
#include <con4m/datatypes/io.h>
#include <con4m/datatypes/crypto.h>
#include <con4m/datatypes/exceptions.h>

typedef any_str_t *(*repr_fn)(object_t, to_str_use_t);
typedef void (*marshal_fn)(object_t, FILE *, struct dict_t *, int64_t *);
typedef void (*unmarshal_fn)(object_t, FILE *, struct dict_t *);

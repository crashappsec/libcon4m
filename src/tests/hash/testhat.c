/*
 * Copyright © 2021-2022 John Viega
 *
 * See LICENSE.txt for licensing info.
 *
 *  Name:           testhat.c
 *
 *  Description:    A wrapper to provide a single interface to all
 *                  the implementations, for ease of testing.
 *
 *                  Note that this interface isn't particularly high
 *                  level:
 *
 *                  1) You need to do the hashing yourself, and pass in
 *                     the value.
 *
 *                  2) You just pass in a pointer to an "item" that's
 *                     expected to represent the key/item pair.
 *
 *                  3) You need to do your own memory management for
 *                     the key / item pairs, if appropriate.
 *
 *                  Most of the implementation is inlined in the header
 *                  file, since it merely dispatches to individual
 *                  implementations.
 *
 *                  This file mainly consists of algorithm
 *                  registration / management and instantiation.
 *
 *  Author:         John Viega, john@zork.org
 *
 */

#include <test/testhat.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int32_t    next_table_type_id = 0;
static alg_info_t implementation_info[HATRACK_MAX_HATS];

static void testhat_init_default_algorithms() __attribute__((constructor));

uint32_t
algorithm_register(char             *name,
                   hatrack_vtable_t *vtable,
                   size_t            size,
                   int               hashbytes,
                   bool              threadsafe)
{
    uint64_t ret;

    if (algorithm_id_by_name(name) != -1) {
        fprintf(stderr, "Error: table entry '%s' added twice.\n", name);
        abort();
    }

    ret                                 = next_table_type_id++;
    implementation_info[ret].name       = name;
    implementation_info[ret].vtable     = vtable;
    implementation_info[ret].size       = size;
    implementation_info[ret].threadsafe = threadsafe;
    implementation_info[ret].hashbytes  = hashbytes;

    return ret;
}

uint32_t
get_num_algorithms(void)
{
    return next_table_type_id;
}

alg_info_t *
get_all_algorithm_info(void)
{
    return &implementation_info[0];
}

// We assume this is all single-threaded.
int32_t
algorithm_id_by_name(char *name)
{
    int32_t i;

    for (i = 0; i < next_table_type_id; i++) {
        if (!strcmp(implementation_info[i].name, name)) {
            return i;
        }
    }

    return -1;
}

alg_info_t *
algorithm_info(char *name)
{
    int64_t i;

    i = algorithm_id_by_name(name);

    if (i == -1) {
        fprintf(stderr, "Error: table type named '%s' not registered.\n", name);
        abort();
    }

    return &implementation_info[i];
}

testhat_t *
testhat_new(char *name)
{
    testhat_t *ret;
    int64_t    i;

    i = algorithm_id_by_name(name);

    if (i == -1) {
        fprintf(stderr, "Error: table type named '%s' not registered.\n", name);
        abort();
    }

    ret         = (testhat_t *)calloc(1, sizeof(testhat_t));
    ret->htable = (void *)calloc(1, implementation_info[i].size);

    memcpy(ret, implementation_info[i].vtable, sizeof(hatrack_vtable_t));

    (*ret->vtable.init)(ret->htable);

    return ret;
}

testhat_t *
testhat_new_size(char *name, char sz)
{
    testhat_t *ret;
    int64_t    i;

    i = algorithm_id_by_name(name);

    if (i == -1) {
        fprintf(stderr, "Error: table type named '%s' not registered.\n", name);
        abort();
    }

    ret         = (testhat_t *)calloc(1, sizeof(testhat_t));
    ret->htable = (void *)calloc(1, implementation_info[i].size);

    memcpy(ret, implementation_info[i].vtable, sizeof(hatrack_vtable_t));

    (*ret->vtable.init_sz)(ret->htable, sz);

    return ret;
}

// clang-format off
hatrack_vtable_t refhat_vtable = {
    .init    = (hatrack_init_func)refhat_init,
    .init_sz = (hatrack_init_sz_func)refhat_init_size,
    .get     = (hatrack_get_func)refhat_get,
    .put     = (hatrack_put_func)refhat_put,
    .replace = (hatrack_replace_func)refhat_replace,
    .add     = (hatrack_add_func)refhat_add,
    .remove  = (hatrack_remove_func)refhat_remove,
    .delete  = (hatrack_delete_func)refhat_delete,
    .len     = (hatrack_len_func)refhat_len,
    .view    = (hatrack_view_func)refhat_view
};

hatrack_vtable_t dcap_vtable = {
    .init    = (hatrack_init_func)duncecap_init,
    .init_sz = (hatrack_init_sz_func)duncecap_init_size,
    .get     = (hatrack_get_func)duncecap_get,
    .put     = (hatrack_put_func)duncecap_put,
    .replace = (hatrack_replace_func)duncecap_replace,
    .add     = (hatrack_add_func)duncecap_add,
    .remove  = (hatrack_remove_func)duncecap_remove,
    .delete  = (hatrack_delete_func)duncecap_delete,
    .len     = (hatrack_len_func)duncecap_len,
    .view    = (hatrack_view_func)duncecap_view
};

hatrack_vtable_t swimcap_vtable = {
    .init    = (hatrack_init_func)swimcap_init,
    .init_sz = (hatrack_init_sz_func)swimcap_init_size,
    .get     = (hatrack_get_func)swimcap_get,
    .put     = (hatrack_put_func)swimcap_put,
    .replace = (hatrack_replace_func)swimcap_replace,
    .add     = (hatrack_add_func)swimcap_add,
    .remove  = (hatrack_remove_func)swimcap_remove,
    .delete  = (hatrack_delete_func)swimcap_delete,
    .len     = (hatrack_len_func)swimcap_len,
    .view    = (hatrack_view_func)swimcap_view
};

hatrack_vtable_t newshat_vtable = {
    .init    = (hatrack_init_func)newshat_init,
    .init_sz = (hatrack_init_sz_func)newshat_init_size,
    .get     = (hatrack_get_func)newshat_get,
    .put     = (hatrack_put_func)newshat_put,
    .replace = (hatrack_replace_func)newshat_replace,
    .add     = (hatrack_add_func)newshat_add,
    .remove  = (hatrack_remove_func)newshat_remove,
    .delete  = (hatrack_delete_func)newshat_delete,
    .len     = (hatrack_len_func)newshat_len,
    .view    = (hatrack_view_func)newshat_view
};

hatrack_vtable_t ballcap_vtable = {
    .init    = (hatrack_init_func)ballcap_init,
    .init_sz = (hatrack_init_sz_func)ballcap_init_size,
    .get     = (hatrack_get_func)ballcap_get,
    .put     = (hatrack_put_func)ballcap_put,
    .replace = (hatrack_replace_func)ballcap_replace,
    .add     = (hatrack_add_func)ballcap_add,
    .remove  = (hatrack_remove_func)ballcap_remove,
    .delete  = (hatrack_delete_func)ballcap_delete,
    .len     = (hatrack_len_func)ballcap_len,
    .view    = (hatrack_view_func)ballcap_view
};

hatrack_vtable_t hihat_vtable = {
    .init    = (hatrack_init_func)hihat_init,
    .init_sz = (hatrack_init_sz_func)hihat_init_size,
    .get     = (hatrack_get_func)hihat_get,
    .put     = (hatrack_put_func)hihat_put,
    .replace = (hatrack_replace_func)hihat_replace,
    .add     = (hatrack_add_func)hihat_add,
    .remove  = (hatrack_remove_func)hihat_remove,
    .delete  = (hatrack_delete_func)hihat_delete,
    .len     = (hatrack_len_func)hihat_len,
    .view    = (hatrack_view_func)hihat_view
};

hatrack_vtable_t hihat_a_vtable = {
    .init    = (hatrack_init_func)hihat_a_init,
    .init_sz = (hatrack_init_sz_func)hihat_a_init_size,
    .get     = (hatrack_get_func)hihat_a_get,
    .put     = (hatrack_put_func)hihat_a_put,
    .replace = (hatrack_replace_func)hihat_a_replace,
    .add     = (hatrack_add_func)hihat_a_add,
    .remove  = (hatrack_remove_func)hihat_a_remove,
    .delete  = (hatrack_delete_func)hihat_a_delete,
    .len     = (hatrack_len_func)hihat_a_len,
    .view    = (hatrack_view_func)hihat_a_view
};

hatrack_vtable_t lohat_vtable = {
    .init    = (hatrack_init_func)lohat_init,
    .init_sz = (hatrack_init_sz_func)lohat_init_size,
    .get     = (hatrack_get_func)lohat_get,
    .put     = (hatrack_put_func)lohat_put,
    .replace = (hatrack_replace_func)lohat_replace,
    .add     = (hatrack_add_func)lohat_add,
    .remove  = (hatrack_remove_func)lohat_remove,
    .delete  = (hatrack_delete_func)lohat_delete,
    .len     = (hatrack_len_func)lohat_len,
    .view    = (hatrack_view_func)lohat_view
};

hatrack_vtable_t lohat_a_vtable = {
    .init    = (hatrack_init_func)lohat_a_init,
    .init_sz = (hatrack_init_sz_func)lohat_a_init_size,
    .get     = (hatrack_get_func)lohat_a_get,
    .put     = (hatrack_put_func)lohat_a_put,
    .replace = (hatrack_replace_func)lohat_a_replace,
    .add     = (hatrack_add_func)lohat_a_add,
    .remove  = (hatrack_remove_func)lohat_a_remove,
    .delete  = (hatrack_delete_func)lohat_a_delete,
    .len     = (hatrack_len_func)lohat_a_len,
    .view    = (hatrack_view_func)lohat_a_view
};

hatrack_vtable_t witch_vtable = {
    .init    = (hatrack_init_func)witchhat_init,
    .init_sz = (hatrack_init_sz_func)witchhat_init_size,
    .get     = (hatrack_get_func)witchhat_get,
    .put     = (hatrack_put_func)witchhat_put,
    .replace = (hatrack_replace_func)witchhat_replace,
    .add     = (hatrack_add_func)witchhat_add,
    .remove  = (hatrack_remove_func)witchhat_remove,
    .delete  = (hatrack_delete_func)witchhat_delete,
    .len     = (hatrack_len_func)witchhat_len,
    .view    = (hatrack_view_func)witchhat_view
};

hatrack_vtable_t woolhat_vtable = {
    .init    = (hatrack_init_func)woolhat_init,
    .init_sz = (hatrack_init_sz_func)woolhat_init_size,
    .get     = (hatrack_get_func)woolhat_get,
    .put     = (hatrack_put_func)woolhat_put,
    .replace = (hatrack_replace_func)woolhat_replace,
    .add     = (hatrack_add_func)woolhat_add,
    .remove  = (hatrack_remove_func)woolhat_remove,
    .delete  = (hatrack_delete_func)woolhat_delete,
    .len     = (hatrack_len_func)woolhat_len,
    .view    = (hatrack_view_func)woolhat_view
};

hatrack_vtable_t oldhat_vtable = {
    .init    = (hatrack_init_func)oldhat_init,
    .init_sz = (hatrack_init_sz_func)oldhat_init_size,
    .get     = (hatrack_get_func)oldhat_get,
    .put     = (hatrack_put_func)oldhat_put,
    .replace = (hatrack_replace_func)oldhat_replace,
    .add     = (hatrack_add_func)oldhat_add,
    .remove  = (hatrack_remove_func)oldhat_remove,
    .delete  = (hatrack_delete_func)oldhat_delete,
    .len     = (hatrack_len_func)oldhat_len,
    .view    = (hatrack_view_func)oldhat_view
};

hatrack_vtable_t thfmx_vtable = {
    .init    = (hatrack_init_func)tophat_init_fast_mx,
    .init_sz = (hatrack_init_sz_func)tophat_init_fast_mx_size,
    .get     = (hatrack_get_func)tophat_get,
    .put     = (hatrack_put_func)tophat_put,
    .replace = (hatrack_replace_func)tophat_replace,
    .add     = (hatrack_add_func)tophat_add,
    .remove  = (hatrack_remove_func)tophat_remove,
    .delete  = (hatrack_delete_func)tophat_delete,
    .len     = (hatrack_len_func)tophat_len,
    .view    = (hatrack_view_func)tophat_view
};

hatrack_vtable_t thfwf_vtable = {
    .init    = (hatrack_init_func)tophat_init_fast_wf,
    .init_sz = (hatrack_init_sz_func)tophat_init_fast_wf_size,
    .get     = (hatrack_get_func)tophat_get,
    .put     = (hatrack_put_func)tophat_put,
    .replace = (hatrack_replace_func)tophat_replace,
    .add     = (hatrack_add_func)tophat_add,
    .remove  = (hatrack_remove_func)tophat_remove,
    .delete  = (hatrack_delete_func)tophat_delete,
    .len     = (hatrack_len_func)tophat_len,
    .view    = (hatrack_view_func)tophat_view
};

hatrack_vtable_t thcmx_vtable = {
    .init    = (hatrack_init_func)tophat_init_cst_mx,
    .init_sz = (hatrack_init_sz_func)tophat_init_cst_mx_size,
    .get     = (hatrack_get_func)tophat_get,
    .put     = (hatrack_put_func)tophat_put,
    .replace = (hatrack_replace_func)tophat_replace,
    .add     = (hatrack_add_func)tophat_add,
    .remove  = (hatrack_remove_func)tophat_remove,
    .delete  = (hatrack_delete_func)tophat_delete,
    .len     = (hatrack_len_func)tophat_len,
    .view    = (hatrack_view_func)tophat_view
};

hatrack_vtable_t thcwf_vtable = {
    .init    = (hatrack_init_func)tophat_init_cst_wf,
    .init_sz = (hatrack_init_sz_func)tophat_init_cst_wf_size,
    .get     = (hatrack_get_func)tophat_get,
    .put     = (hatrack_put_func)tophat_put,
    .replace = (hatrack_replace_func)tophat_replace,
    .add     = (hatrack_add_func)tophat_add,
    .remove  = (hatrack_remove_func)tophat_remove,
    .delete  = (hatrack_delete_func)tophat_delete,
    .len     = (hatrack_len_func)tophat_len,
    .view    = (hatrack_view_func)tophat_view
};

hatrack_vtable_t crown_vtable = {
    .init    = (hatrack_init_func)crown_init,
    .init_sz = (hatrack_init_sz_func)crown_init_size,
    .get     = (hatrack_get_func)crown_get,
    .put     = (hatrack_put_func)crown_put,
    .replace = (hatrack_replace_func)crown_replace,
    .add     = (hatrack_add_func)crown_add,
    .remove  = (hatrack_remove_func)crown_remove,
    .delete  = (hatrack_delete_func)crown_delete,
    .len     = (hatrack_len_func)crown_len,
    .view    = (hatrack_view_func)crown_view
};

hatrack_vtable_t tiara_vtable = {
    .init    = (hatrack_init_func)tiara_init,
    .init_sz = (hatrack_init_sz_func)tiara_init_size,
    .get     = (hatrack_get_func)tiara_get,
    .put     = (hatrack_put_func)tiara_put,
    .replace = (hatrack_replace_func)tiara_replace,
    .add     = (hatrack_add_func)tiara_add,
    .remove  = (hatrack_remove_func)tiara_remove,
    .delete  = (hatrack_delete_func)tiara_delete,
    .len     = (hatrack_len_func)tiara_len,
    .view    = (hatrack_view_func)tiara_view
};

// clang-format on

static void
testhat_init_default_algorithms(void)
{
    algorithm_register("refhat", &refhat_vtable, sizeof(refhat_t), 16, false);
    algorithm_register("duncecap", &dcap_vtable, sizeof(duncecap_t), 16, true);
    algorithm_register("swimcap", &swimcap_vtable, sizeof(swimcap_t), 16, true);
    algorithm_register("newshat", &newshat_vtable, sizeof(newshat_t), 16, true);
    algorithm_register("ballcap", &ballcap_vtable, sizeof(ballcap_t), 16, true);
    algorithm_register("hihat", &hihat_vtable, sizeof(hihat_t), 16, true);
    algorithm_register("hihat-a", &hihat_a_vtable, sizeof(hihat_t), 16, true);
    algorithm_register("witchhat", &witch_vtable, sizeof(witchhat_t), 16, true);
    algorithm_register("crown", &crown_vtable, sizeof(crown_t), 16, true);
    algorithm_register("oldhat", &oldhat_vtable, sizeof(oldhat_t), 16, true);
    algorithm_register("lohat", &lohat_vtable, sizeof(lohat_t), 16, true);
    algorithm_register("lohat-a", &lohat_a_vtable, sizeof(lohat_a_t), 16, true);
    algorithm_register("woolhat", &woolhat_vtable, sizeof(woolhat_t), 16, true);
    algorithm_register("tophat-fmx", &thfmx_vtable, sizeof(tophat_t), 16, true);
    algorithm_register("tophat-fwf", &thfwf_vtable, sizeof(tophat_t), 16, true);
    algorithm_register("tophat-cmx", &thcmx_vtable, sizeof(tophat_t), 16, true);
    algorithm_register("tophat-cwf", &thcwf_vtable, sizeof(tophat_t), 16, true);
    algorithm_register("tiara", &tiara_vtable, sizeof(tiara_t), 8, true);
    return;
}

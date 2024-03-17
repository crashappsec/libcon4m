#pragma once
#include <con4m.h>

typedef void *EVP_MD_CTX;
typedef void *EVP_MD;
typedef void *OSSL_PARAM;

typedef struct {
    EVP_MD_CTX openssl_ctx;
    buffer_t  *digest;
} sha_ctx;

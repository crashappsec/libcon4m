#pragma once
#include "con4m.h"

typedef void *EVP_MD_CTX;
typedef void *EVP_MD;
typedef void *OSSL_PARAM;

typedef struct {
    EVP_MD_CTX openssl_ctx;
    c4m_buf_t *digest;
} c4m_sha_t;

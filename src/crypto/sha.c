#include "con4m.h"

void
c4m_gc_openssl()
{
    // Nothing now.
}

static const EVP_MD init_map[2][3] = {
    {
        EVP_sha256,
        EVP_sha384,
        EVP_sha512,
    },
    {
        EVP_sha3_256,
        EVP_sha3_384,
        EVP_sha3_512,
    },
};

static void
c4m_sha_cleanup(c4m_sha_t *ctx)
{
    EVP_MD_CTX_free(ctx->openssl_ctx);
}

void
c4m_sha_init(c4m_sha_t *ctx, va_list args)
{
    int64_t version = 2;
    int64_t bits    = 256;

    if (args != NULL) {
        c4m_karg_va_init(args);
        c4m_kw_int64("version", version);
        c4m_kw_int64("bits", bits);
    }

    if (bits != 256 && bits != 384 && bits != 512) {
        abort();
    }
    if (version != 2 && version != 3) {
        abort();
    }

    ctx->digest = c4m_new(c4m_type_buffer(),
                          c4m_kw("length", c4m_ka(bits / 8)));

    version -= 2;
    bits               = (bits >> 7) - 2; // Maps the bit sizes to 0, 1 and 2,
                                          // by dividing by 128, then - 2.
    ctx->openssl_ctx   = EVP_MD_CTX_new();
    EVP_MD *(*f)(void) = init_map[version][bits];

    EVP_DigestInit_ex2(ctx->openssl_ctx, f(), NULL);
}

void
c4m_sha_cc4m_str_update(c4m_sha_t *ctx, char *str)
{
    size_t len = strlen(str);
    if (len > 0) {
        EVP_DigestUpdate(ctx->openssl_ctx, str, len);
    }
}

void
c4m_sha_int_update(c4m_sha_t *ctx, uint64_t n)
{
    little_64(n);
    EVP_DigestUpdate(ctx->openssl_ctx, &n, sizeof(uint64_t));
}

// Note; we should probably go back and correct 'byte_length' whenever
// we overestimate so that this doesn't seem nondeterministic when it
// hashes extra 0's.
void
c4m_sha_string_update(c4m_sha_t *ctx, c4m_str_t *str)
{
    int64_t len = c4m_str_byte_len(str);

    if (len > 0) {
        EVP_DigestUpdate(ctx->openssl_ctx, str->data, len);
    }
}

void
c4m_sha_buffer_update(c4m_sha_t *ctx, c4m_buf_t *buffer)
{
    int32_t len = buffer->byte_len;
    if (len > 1) {
        EVP_DigestUpdate(ctx->openssl_ctx, buffer->data, len);
    }
}

c4m_buf_t *
c4m_sha_finish(c4m_sha_t *ctx)
{
    EVP_DigestFinal_ex(ctx->openssl_ctx, ctx->digest->data, NULL);
    c4m_buf_t *result = ctx->digest;
    ctx->digest       = NULL;

    return result;
}

const c4m_vtable_t c4m_sha_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR] = (c4m_vtable_entry)c4m_sha_init,
        [C4M_BI_FINALIZER]   = (c4m_vtable_entry)c4m_sha_cleanup,
        [C4M_BI_GC_MAP]      = (c4m_vtable_entry)C4M_GC_SCAN_ALL,
    },
};

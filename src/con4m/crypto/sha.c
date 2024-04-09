#include "con4m.h"

static void *
openssl_m_proxy(size_t sz, const char *file, int line)
{
    return con4m_gc_alloc(sz, GC_SCAN_ALL);
}

static void *
openssl_r_proxy(void *p, size_t sz, const char *file, int line)
{
    return con4m_gc_resize(p, sz);
}

static void
openssl_f_proxy(void *p, const char *file, int line)
{
    ; // Intentionally blank.
}

__attribute__((constructor)) void
gc_openssl()
{
    CRYPTO_set_mem_functions(openssl_m_proxy, openssl_r_proxy, openssl_f_proxy);
}

static const EVP_MD init_map[2][3] = {
    {EVP_sha256, EVP_sha384, EVP_sha512},
    {EVP_sha3_256, EVP_sha3_384, EVP_sha3_512}};

void
sha_init(sha_ctx *ctx, va_list args)
{
    int64_t version = 2;
    int64_t bits    = 256;

    karg_va_init(args);
    kw_int64("version", version);
    kw_int64("bits", bits);

    if (bits != 256 && bits != 384 && bits != 512) {
        abort();
    }
    if (version != 2 && version != 3) {
        abort();
    }

    ctx->digest = con4m_new(tspec_buffer(), kw("length", ka(bits / 8)));
    version -= 2;
    bits               = (bits >> 7) - 2; // This maps the bit sizes to 0, 1 and 2,
                                          // by dividing by 128, then subtracting by 2.
    ctx->openssl_ctx   = EVP_MD_CTX_new();
    EVP_MD *(*f)(void) = init_map[version][bits];

    EVP_DigestInit_ex2(ctx->openssl_ctx, f(), NULL);
}

void
sha_cstring_update(sha_ctx *ctx, char *str)
{
    size_t len = strlen(str);
    if (len > 0) {
        EVP_DigestUpdate(ctx->openssl_ctx, str, len);
    }
}

void
sha_int_update(sha_ctx *ctx, uint64_t n)
{
    little_64(result);
    EVP_DigestUpdate(ctx->openssl_ctx, &n, sizeof(uint64_t));
}

// Note; we should probably go back and correct 'byte_length' whenever
// we overestimate so that this doesn't seem nondeterministic when it
// hashes extra 0's.
void
sha_string_update(sha_ctx *ctx, any_str_t *str)
{
    int64_t len = string_byte_len(str);

    if (len > 0) {
        EVP_DigestUpdate(ctx->openssl_ctx, str->data, len);
    }
}

void
sha_buffer_update(sha_ctx *ctx, buffer_t *buffer)
{
    int32_t len = buffer->byte_len;
    if (len > 1) {
        EVP_DigestUpdate(ctx->openssl_ctx, buffer->data, len);
    }
}

buffer_t *
sha_finish(sha_ctx *ctx)
{
    EVP_DigestFinal_ex(ctx->openssl_ctx, ctx->digest->data, NULL);
    buffer_t *result = ctx->digest;
    ctx->digest      = NULL;

    return result;
}

const con4m_vtable sha_vtable = {
    .num_entries = 1,
    .methods     = {
        (con4m_vtable_entry)sha_init}};

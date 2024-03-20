#pragma once
#include <con4m.h>

 int CRYPTO_set_mem_functions(
         void *(*m)(size_t, const char *, int),
         void *(*r)(void *, size_t, const char *, int),
         void (*f)(void *, const char *, int));

extern EVP_MD_CTX EVP_MD_CTX_new();
extern int        EVP_DigestInit_ex2(EVP_MD_CTX, const EVP_MD, void *);
extern int        EVP_DigestUpdate(EVP_MD_CTX *, const void *, size_t);
extern int        EVP_DigestFinal_ex(EVP_MD_CTX *, char *, unsigned int *);

extern EVP_MD     *EVP_sha256(void);
extern EVP_MD     *EVP_sha384(void);
extern EVP_MD     *EVP_sha512(void);
extern EVP_MD     *EVP_sha3_256(void);
extern EVP_MD     *EVP_sha3_384(void);
extern EVP_MD     *EVP_sha3_512(void);
extern uint8_t   *SHA1(const uint8_t *data, size_t count, uint8_t *md_buf);
extern uint8_t   *SHA224(const uint8_t *data, size_t count, uint8_t *md_buf);
extern uint8_t   *SHA256(const uint8_t *data, size_t count, uint8_t *md_buf);
extern uint8_t   *SHA512(const uint8_t *data, size_t count, uint8_t *md_buf);

extern void      init_sha(sha_ctx *, va_list);
extern void      sha_cstring_update(sha_ctx *, char *);
extern void      sha_int_update(sha_ctx *, uint64_t);
extern void      sha_string_update(sha_ctx *, any_str_t *);
extern void      sha_buffer_update(sha_ctx *, buffer_t *);
extern buffer_t *sha_finish(sha_ctx *);

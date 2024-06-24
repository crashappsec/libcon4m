#pragma once
#include "con4m.h"

int CRYPTO_set_mem_functions(
    void *(*m)(size_t, const char *, int),
    void *(*r)(void *, size_t, const char *, int),
    void (*f)(void *, const char *, int));

extern EVP_MD_CTX EVP_MD_CTX_new();
extern int        EVP_DigestInit_ex2(EVP_MD_CTX, const EVP_MD, void *);
extern int        EVP_DigestUpdate(EVP_MD_CTX *, const void *, size_t);
extern int        EVP_DigestFinal_ex(EVP_MD_CTX *, char *, unsigned int *);
extern void       EVP_MD_CTX_free(EVP_MD_CTX *);

extern EVP_MD  *EVP_sha256(void);
extern EVP_MD  *EVP_sha384(void);
extern EVP_MD  *EVP_sha512(void);
extern EVP_MD  *EVP_sha3_256(void);
extern EVP_MD  *EVP_sha3_384(void);
extern EVP_MD  *EVP_sha3_512(void);
extern uint8_t *SHA1(const uint8_t *data, size_t count, uint8_t *md_buf);
extern uint8_t *SHA224(const uint8_t *data, size_t count, uint8_t *md_buf);
extern uint8_t *SHA256(const uint8_t *data, size_t count, uint8_t *md_buf);
extern uint8_t *SHA512(const uint8_t *data, size_t count, uint8_t *md_buf);

extern void       c4m_sha_init(c4m_sha_t *, va_list);
extern void       c4m_sha_cstring_update(c4m_sha_t *, char *);
extern void       c4m_sha_int_update(c4m_sha_t *, uint64_t);
extern void       c4m_sha_string_update(c4m_sha_t *, c4m_str_t *);
extern void       c4m_sha_buffer_update(c4m_sha_t *, c4m_buf_t *);
extern c4m_buf_t *c4m_sha_finish(c4m_sha_t *);
extern void       c4m_gc_openssl();

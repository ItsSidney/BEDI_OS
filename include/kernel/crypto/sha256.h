#ifndef SHA256_H
#define SHA256_H

#include <stdint.h>
#include <stddef.h>

#define SHA256_BLOCK_SIZE 64
#define SHA256_HASH_SIZE  32

typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} sha256_ctx;

void sha256_init(sha256_ctx* ctx);
void sha256_update(sha256_ctx* ctx, const uint8_t* data, size_t len);
void sha256_final(sha256_ctx* ctx, uint8_t* hash);

/* Convenience function to hash a single buffer */
void sha256(const uint8_t* data, size_t len, uint8_t* hash);

/* HMAC-SHA256 */
void hmac_sha256(const uint8_t* key, size_t keylen, const uint8_t* data, size_t datalen, uint8_t* out);

/* TLS PRF using HMAC-SHA256 (P_hash function for TLS 1.2) */
void tls_prf_sha256(const uint8_t* secret, size_t secret_len, 
                    const char* label, 
                    const uint8_t* seed, size_t seed_len, 
                    uint8_t* out, size_t out_len);

#endif

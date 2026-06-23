#ifndef AES_GCM_H
#define AES_GCM_H

#include <stdint.h>
#include <stddef.h>
#include "kernel/crypto/aes.h"

/* AES-128 GCM Context */
typedef struct {
    aes128_ctx aes;
    uint8_t H[16]; /* Hash subkey */
} aes128_gcm_ctx;

/* Initialize AES-128-GCM with a 16-byte key */
void aes128_gcm_init(aes128_gcm_ctx* ctx, const uint8_t* key);

/* 
 * Encrypt and authenticate data.
 * iv_len should be 12 for TLS.
 * tag_out must be 16 bytes.
 */
void aes128_gcm_encrypt(aes128_gcm_ctx* ctx,
                        const uint8_t* iv, size_t iv_len,
                        const uint8_t* aad, size_t aad_len,
                        const uint8_t* pt, size_t pt_len,
                        uint8_t* ct, uint8_t* tag_out);

/* 
 * Decrypt and authenticate data.
 * Returns 0 on success, -1 on authentication failure.
 * tag_in must be 16 bytes.
 */
int aes128_gcm_decrypt(aes128_gcm_ctx* ctx,
                       const uint8_t* iv, size_t iv_len,
                       const uint8_t* aad, size_t aad_len,
                       const uint8_t* ct, size_t ct_len,
                       const uint8_t* tag_in, uint8_t* pt);

#endif

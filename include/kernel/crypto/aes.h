#ifndef AES_H
#define AES_H

#include <stdint.h>
#include <stddef.h>

#define AES_BLOCK_SIZE 16

typedef struct {
    uint32_t round_key[44];
} aes128_ctx;

/* Initialize AES-128 context with a 16-byte key */
void aes128_init(aes128_ctx* ctx, const uint8_t* key);

/* Encrypt a single 16-byte block */
void aes128_encrypt_block(const aes128_ctx* ctx, const uint8_t* in, uint8_t* out);

/* Decrypt a single 16-byte block */
void aes128_decrypt_block(const aes128_ctx* ctx, const uint8_t* in, uint8_t* out);

/* CBC mode encryption (updates iv) */
void aes128_cbc_encrypt(const aes128_ctx* ctx, uint8_t* iv, const uint8_t* in, uint8_t* out, size_t length);

/* CBC mode decryption (updates iv) */
void aes128_cbc_decrypt(const aes128_ctx* ctx, uint8_t* iv, const uint8_t* in, uint8_t* out, size_t length);

#endif

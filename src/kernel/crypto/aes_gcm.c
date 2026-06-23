#include "kernel/crypto/aes_gcm.h"
#include <string.h>

static void ghash_multiply(uint8_t* x, const uint8_t* y) {
    uint8_t z[16] = {0};
    uint8_t v[16];
    memcpy(v, y, 16);

    for (int i = 0; i < 128; i++) {
        // If x[i] is 1, z ^= v
        if ((x[i / 8] >> (7 - (i % 8))) & 1) {
            for (int j = 0; j < 16; j++) z[j] ^= v[j];
        }

        // v >>= 1
        int lsb = v[15] & 1;
        for (int j = 15; j > 0; j--) {
            v[j] = (v[j] >> 1) | ((v[j - 1] & 1) << 7);
        }
        v[0] >>= 1;

        // If lsb was 1, v ^= R
        if (lsb) {
            v[0] ^= 0xE1;
        }
    }
    memcpy(x, z, 16);
}

static void increment_counter(uint8_t* ctr) {
    for (int i = 15; i >= 12; i--) {
        if (++ctr[i] != 0) break;
    }
}

void aes128_gcm_init(aes128_gcm_ctx* ctx, const uint8_t* key) {
    aes128_init(&ctx->aes, key);
    uint8_t zero[16] = {0};
    aes128_encrypt_block(&ctx->aes, zero, ctx->H);
}

static void ghash_update(uint8_t* hash, const uint8_t* data, size_t len, const uint8_t* H) {
    size_t blocks = len / 16;
    for (size_t i = 0; i < blocks; i++) {
        for (int j = 0; j < 16; j++) hash[j] ^= data[i * 16 + j];
        ghash_multiply(hash, H);
    }
    
    size_t rem = len % 16;
    if (rem > 0) {
        for (size_t j = 0; j < rem; j++) hash[j] ^= data[blocks * 16 + j];
        ghash_multiply(hash, H);
    }
}

static void aes_gcm_process(aes128_gcm_ctx* ctx, const uint8_t* iv, size_t iv_len,
                            const uint8_t* aad, size_t aad_len,
                            const uint8_t* in, size_t in_len, uint8_t* out,
                            uint8_t* tag_out, int encrypt) {
    uint8_t J0[16] = {0};
    uint8_t hash[16] = {0};

    // Calculate J0
    if (iv_len == 12) {
        memcpy(J0, iv, 12);
        J0[15] = 1;
    } else {
        ghash_update(hash, iv, iv_len, ctx->H);
        uint8_t len_block[16] = {0};
        uint64_t iv_bits = (uint64_t)iv_len * 8;
        for (int i = 0; i < 8; i++) len_block[8 + i] = (iv_bits >> (56 - i * 8)) & 0xFF;
        ghash_update(hash, len_block, 16, ctx->H);
        memcpy(J0, hash, 16);
    }

    uint8_t ctr[16];
    memcpy(ctr, J0, 16);
    increment_counter(ctr);

    uint8_t stream[16];
    size_t blocks = in_len / 16;

    // Process AAD for auth tag
    memset(hash, 0, 16);
    ghash_update(hash, aad, aad_len, ctx->H);

    // Process plaintext/ciphertext
    if (encrypt) {
        for (size_t i = 0; i < blocks; i++) {
            aes128_encrypt_block(&ctx->aes, ctr, stream);
            increment_counter(ctr);
            for (int j = 0; j < 16; j++) out[i * 16 + j] = in[i * 16 + j] ^ stream[j];
            ghash_update(hash, out + i * 16, 16, ctx->H);
        }
        size_t rem = in_len % 16;
        if (rem > 0) {
            aes128_encrypt_block(&ctx->aes, ctr, stream);
            for (size_t j = 0; j < rem; j++) out[blocks * 16 + j] = in[blocks * 16 + j] ^ stream[j];
            ghash_update(hash, out + blocks * 16, rem, ctx->H);
        }
    } else {
        // Decrypt: hash ciphertext first, then XOR
        ghash_update(hash, in, in_len, ctx->H);
        
        for (size_t i = 0; i < blocks; i++) {
            aes128_encrypt_block(&ctx->aes, ctr, stream);
            increment_counter(ctr);
            for (int j = 0; j < 16; j++) out[i * 16 + j] = in[i * 16 + j] ^ stream[j];
        }
        size_t rem = in_len % 16;
        if (rem > 0) {
            aes128_encrypt_block(&ctx->aes, ctr, stream);
            for (size_t j = 0; j < rem; j++) out[blocks * 16 + j] = in[blocks * 16 + j] ^ stream[j];
        }
    }

    // Finalize authentication tag
    uint8_t len_block[16] = {0};
    uint64_t aad_bits = (uint64_t)aad_len * 8;
    uint64_t pt_bits = (uint64_t)in_len * 8;
    for (int i = 0; i < 8; i++) {
        len_block[i] = (aad_bits >> (56 - i * 8)) & 0xFF;
        len_block[8 + i] = (pt_bits >> (56 - i * 8)) & 0xFF;
    }
    ghash_update(hash, len_block, 16, ctx->H);

    aes128_encrypt_block(&ctx->aes, J0, stream);
    for (int i = 0; i < 16; i++) tag_out[i] = hash[i] ^ stream[i];
}

void aes128_gcm_encrypt(aes128_gcm_ctx* ctx,
                        const uint8_t* iv, size_t iv_len,
                        const uint8_t* aad, size_t aad_len,
                        const uint8_t* pt, size_t pt_len,
                        uint8_t* ct, uint8_t* tag_out) {
    aes_gcm_process(ctx, iv, iv_len, aad, aad_len, pt, pt_len, ct, tag_out, 1);
}

int aes128_gcm_decrypt(aes128_gcm_ctx* ctx,
                       const uint8_t* iv, size_t iv_len,
                       const uint8_t* aad, size_t aad_len,
                       const uint8_t* ct, size_t ct_len,
                       const uint8_t* tag_in, uint8_t* pt) {
    uint8_t calc_tag[16];
    aes_gcm_process(ctx, iv, iv_len, aad, aad_len, ct, ct_len, pt, calc_tag, 0);
    
    int diff = 0;
    for (int i = 0; i < 16; i++) diff |= (calc_tag[i] ^ tag_in[i]);
    
    if (diff != 0) {
        memset(pt, 0, ct_len); // Clear plaintext on failure
        return -1;
    }
    return 0;
}

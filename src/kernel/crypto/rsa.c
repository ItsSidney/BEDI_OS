#include "kernel/crypto/rsa.h"
#include <string.h>

// Simple PRNG for PKCS#1 padding since we don't have /dev/urandom
static uint32_t rsa_seed = 0x12345678;
static uint8_t random_nonzero_byte() {
    rsa_seed = rsa_seed * 1664525 + 1013904223;
    uint8_t r = (rsa_seed >> 16) & 0xFF;
    if (r == 0) r = 0x42;
    return r;
}

int rsa_public_encrypt(const uint8_t* modulus, size_t mod_len,
                       const uint8_t* exponent, size_t exp_len,
                       const uint8_t* in, size_t in_len,
                       uint8_t* out) {
    if (mod_len == 0 || mod_len < in_len + 11 || mod_len > 512) return -1;

    uint8_t padded[512]; // Max 4096-bit RSA
    memset(padded, 0, mod_len);

    // Format: 0x00 0x02 [non-zero padding] 0x00 [in]
    padded[0] = 0x00;
    padded[1] = 0x02;
    
    size_t pad_len = mod_len - in_len - 3;
    for (size_t i = 0; i < pad_len; ++i) {
        padded[2 + i] = random_nonzero_byte();
    }
    
    padded[2 + pad_len] = 0x00;
    memcpy(padded + 3 + pad_len, in, in_len);

    bignum_t N, E, M, C;
    bignum_from_bytes(&N, modulus, mod_len);
    bignum_from_bytes(&E, exponent, exp_len);
    bignum_from_bytes(&M, padded, mod_len);

    bignum_modexp(&C, &M, &E, &N);

    bignum_to_bytes(&C, out, mod_len);
    return 0;
}

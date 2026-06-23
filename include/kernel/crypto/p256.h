#ifndef P256_H
#define P256_H

#include <stdint.h>
#include <stddef.h>
#include "kernel/crypto/bignum.h"

/* Secp256r1 (P-256) point in affine coordinates */
typedef struct {
    bignum_t x;
    bignum_t y;
} p256_point_t;

/* Initialize the P-256 subsystem (sets up constants) */
void p256_init(void);

/* Point addition: r = a + b */
void p256_point_add(p256_point_t* r, const p256_point_t* a, const p256_point_t* b);

/* Point doubling: r = 2 * a */
void p256_point_double(p256_point_t* r, const p256_point_t* a);

/* Scalar multiplication: r = k * p */
void p256_scalar_mult(p256_point_t* r, const bignum_t* k, const p256_point_t* p);

/* Base point multiplication: r = k * G */
void p256_base_mult(p256_point_t* r, const bignum_t* k);

/* Parse an uncompressed point from bytes (0x04 || X || Y) */
int p256_from_bytes(p256_point_t* p, const uint8_t* bytes, size_t len);

/* Serialize point to bytes (uncompressed: 0x04 || X || Y) */
void p256_to_bytes(const p256_point_t* p, uint8_t* bytes);

#endif

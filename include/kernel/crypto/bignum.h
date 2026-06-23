#ifndef BIGNUM_H
#define BIGNUM_H

#include <stdint.h>
#include <stddef.h>

/* Max limbs for 2048 bits */
#define BIGNUM_LIMBS 32

typedef struct {
    uint64_t limbs[BIGNUM_LIMBS];
} bignum_t;

/* Initialize to zero */
void bignum_init(bignum_t* n);

/* Import from big-endian bytes (e.g. from network/cert) */
void bignum_from_bytes(bignum_t* n, const uint8_t* bytes, size_t len);

/* Export to big-endian bytes */
void bignum_to_bytes(const bignum_t* n, uint8_t* bytes, size_t len);

/* Compare: Returns 1 if a > b, -1 if a < b, 0 if a == b */
int bignum_cmp(const bignum_t* a, const bignum_t* b);

/* Modular Exponentiation: result = (base ^ exp) % mod */
void bignum_modexp(bignum_t* result, const bignum_t* base, const bignum_t* exp, const bignum_t* mod);

void bignum_add(bignum_t* r, const bignum_t* a, const bignum_t* b);
void bignum_sub(bignum_t* r, const bignum_t* a, const bignum_t* b);
void bignum_mod(bignum_t* r, const bignum_t* a, const bignum_t* m);
void bignum_mul_mod(bignum_t* r, const bignum_t* a, const bignum_t* b, const bignum_t* m);
void bignum_inv_mod(bignum_t* r, const bignum_t* a, const bignum_t* m);

#endif

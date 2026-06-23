#ifndef RSA_H
#define RSA_H

#include <stdint.h>
#include <stddef.h>
#include "kernel/crypto/bignum.h"

/*
 * rsa_public_encrypt - Encrypts data using RSA PKCS#1 v1.5 padding.
 * 
 * modulus: Big-endian RSA modulus (N) bytes
 * mod_len: Length of modulus in bytes
 * exponent: Big-endian RSA public exponent (e) bytes
 * exp_len: Length of exponent in bytes
 * in: Data to encrypt (e.g., pre-master secret)
 * in_len: Length of data
 * out: Buffer for encrypted data (must be mod_len bytes long)
 * 
 * Returns 0 on success, -1 on error.
 */
int rsa_public_encrypt(const uint8_t* modulus, size_t mod_len,
                       const uint8_t* exponent, size_t exp_len,
                       const uint8_t* in, size_t in_len,
                       uint8_t* out);

#endif

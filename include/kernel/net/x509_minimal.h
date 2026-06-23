#ifndef X509_MINIMAL_H
#define X509_MINIMAL_H

#include <stdint.h>
#include <stddef.h>

/* 
 * Extracts RSA public key modulus and exponent from a DER-encoded X.509 certificate.
 * Returns 0 on success, -1 on failure.
 */
int x509_extract_rsa_pubkey(const uint8_t* cert, size_t cert_len,
                            const uint8_t** mod_out, size_t* mod_len_out,
                            const uint8_t** exp_out, size_t* exp_len_out);

#endif

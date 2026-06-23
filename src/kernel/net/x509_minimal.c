#include "kernel/net/x509_minimal.h"

// ASN.1 DER parser helpers
static int read_length(const uint8_t* ptr, size_t* offset, size_t max_len, size_t* len_out) {
    if (*offset >= max_len) return -1;
    uint8_t first = ptr[(*offset)++];
    if (first < 0x80) {
        *len_out = first;
        return 0;
    }
    int num_bytes = first & 0x7F;
    if (num_bytes == 0 || num_bytes > 4 || *offset + num_bytes > max_len) return -1;
    
    size_t len = 0;
    for (int i = 0; i < num_bytes; ++i) {
        len = (len << 8) | ptr[(*offset)++];
    }
    *len_out = len;
    return 0;
}

int x509_extract_rsa_pubkey(const uint8_t* cert, size_t cert_len,
                            const uint8_t** mod_out, size_t* mod_len_out,
                            const uint8_t** exp_out, size_t* exp_len_out) {
    // RSA OID: 1.2.840.113549.1.1.1
    const uint8_t rsa_oid[] = {0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01};
    
    size_t i = 0;
    int found = 0;
    for (i = 0; i < cert_len - sizeof(rsa_oid); ++i) {
        int match = 1;
        for (size_t j = 0; j < sizeof(rsa_oid); ++j) {
            if (cert[i+j] != rsa_oid[j]) { match = 0; break; }
        }
        if (match) {
            found = 1;
            i += sizeof(rsa_oid);
            break;
        }
    }
    if (!found) return -1;

    // Skip NULL parameters if present (0x05 0x00)
    if (i + 1 < cert_len && cert[i] == 0x05 && cert[i+1] == 0x00) {
        i += 2;
    }

    // Expect BIT STRING (0x03)
    if (i >= cert_len || cert[i++] != 0x03) return -1;
    size_t bit_string_len = 0;
    if (read_length(cert, &i, cert_len, &bit_string_len) < 0) return -1;
    
    // Skip unused bits byte (must be 0x00 for our purpose)
    if (i >= cert_len || cert[i++] != 0x00) return -1;

    // Expect SEQUENCE (0x30)
    if (i >= cert_len || cert[i++] != 0x30) return -1;
    size_t seq_len = 0;
    if (read_length(cert, &i, cert_len, &seq_len) < 0) return -1;

    // Expect INTEGER (0x02) - Modulus
    if (i >= cert_len || cert[i++] != 0x02) return -1;
    size_t mod_len = 0;
    if (read_length(cert, &i, cert_len, &mod_len) < 0) return -1;
    
    if (i + mod_len > cert_len) return -1;
    const uint8_t* mod_ptr = cert + i;
    size_t orig_mod_len = mod_len;
    // Strip leading zero byte from modulus if present
    if (mod_len > 0 && mod_ptr[0] == 0x00) {
        mod_ptr++;
        mod_len--;
    }
    *mod_out = mod_ptr;
    *mod_len_out = mod_len;
    i += orig_mod_len;

    // Expect INTEGER (0x02) - Exponent
    if (i >= cert_len || cert[i++] != 0x02) return -1;
    size_t exp_len = 0;
    if (read_length(cert, &i, cert_len, &exp_len) < 0) return -1;
    
    if (i + exp_len > cert_len) return -1;
    *exp_out = cert + i;
    *exp_len_out = exp_len;
    
    return 0;
}

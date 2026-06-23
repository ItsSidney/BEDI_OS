#ifndef TLS_H
#define TLS_H

#include <stdint.h>
#include <stddef.h>
#include "kernel/crypto/aes.h"
#include "kernel/crypto/aes_gcm.h"

typedef struct {
    int sock;
    int tx_encrypted;
    int rx_encrypted;
    
    // Cipher suite negotiated
    int is_gcm;

    // Crypto state (CBC)
    aes128_ctx client_aes;
    aes128_ctx server_aes;
    
    // Crypto state (GCM)
    aes128_gcm_ctx client_gcm;
    aes128_gcm_ctx server_gcm;
    
    uint8_t client_iv[16];
    uint8_t server_iv[16];
    uint8_t client_mac_key[32];
    uint8_t server_mac_key[32];
    
    uint64_t client_seq;
    uint64_t server_seq;

    // Handshake hash
    uint8_t handshake_messages[16384];
    size_t handshake_len;
    
    // Buffers to prevent stack overflow
    uint8_t tx_buf[16384];
    uint8_t rx_buf[16384];
} tls_ctx_t;

/* Initialize TLS connection on an already connected TCP socket */
tls_ctx_t* tls_connect(int sock, const char* hostname);

/* Send application data over TLS */
int tls_send(tls_ctx_t* ctx, const void* data, size_t len);

/* Receive application data over TLS */
int tls_recv(tls_ctx_t* ctx, void* buf, size_t max_len);

/* Close TLS connection and free resources */
void tls_close(tls_ctx_t* ctx);

#endif

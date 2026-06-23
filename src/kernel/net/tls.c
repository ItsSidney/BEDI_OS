#include "kernel/net/tls.h"
#include "kernel/net/socket.h"
#include "kernel/crypto/sha256.h"
#include "kernel/crypto/rsa.h"
#include "kernel/crypto/p256.h"
#include "kernel/net/x509_minimal.h"
#include "kernel/mem/kheap.h"
#include <string.h>

#define TLS_HANDSHAKE           22
#define TLS_ALERT               21
#define TLS_CHANGE_CIPHER_SPEC  20
#define TLS_APPLICATION_DATA    23

extern void sleep_task(int ms);
extern void print_string(const char* s);
extern void itoa(uint64_t n, char* s);

static uint32_t tls_rng_state = 0x87654321;
static void get_random_bytes(uint8_t* buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        tls_rng_state = tls_rng_state * 1664525 + 1013904223;
        buf[i] = (tls_rng_state >> 16) & 0xFF;
    }
}

static int tcp_read_exact(int sock, uint8_t* buf, size_t len) {
    extern int sys_socket_closed(int s);
    size_t total = 0;
    int empty_loops = 0;
    while (total < len) {
        int r = sys_recv(sock, buf + total, len - total, 0);
        if (r < 0) return -1;
        if (r == 0) {
            if (sys_socket_closed(sock)) return -1;
            empty_loops++;
            if (empty_loops > 5) {
                print_string("  TLS: tcp_read_exact timeout (");
                extern void itoa(uint64_t n, char* s);
                char buf2[16]; itoa(len, buf2); print_string(buf2);
                print_string(" bytes needed, ");
                itoa((uint64_t)total, buf2); print_string(buf2);
                print_string(" received)\n");
                return -1;
            }
            sleep_task(10);
            continue;
        }
        total += r;
    }
    return 0;
}

static void hs_log(tls_ctx_t* ctx, const uint8_t* data, size_t len) {
    if (ctx->handshake_len + len <= sizeof(ctx->handshake_messages)) {
        memcpy(ctx->handshake_messages + ctx->handshake_len, data, len);
        ctx->handshake_len += len;
    }
}

static int send_record(tls_ctx_t* ctx, uint8_t type, const uint8_t* data, size_t len) {
    uint8_t* rec = ctx->tx_buf;
    rec[0] = type;
    rec[1] = 0x03;
    rec[2] = 0x03;

    if (!ctx->tx_encrypted) {
        rec[3] = (len >> 8) & 0xFF;
        rec[4] = len & 0xFF;
        memcpy(rec + 5, data, len);
        {
            char dbg[128]; int dp=0; const char* hex="0123456789ABCDEF";
            dbg[dp++]='P';dbg[dp++]='L';dbg[dp++]='N';dbg[dp++]=':';
            for(size_t k=0;k<5+len&&k<20;k++){dbg[dp++]=hex[(rec[k]>>4)&0xF];dbg[dp++]=hex[rec[k]&0xF];}
            dbg[dp]=0; print_string(dbg); print_string("\n");
        }
        sys_send(ctx->sock, rec, 5 + len, 0);
    } else if (ctx->is_gcm) {
        uint8_t aad[13];
        for (int i = 0; i < 8; i++) aad[i] = (ctx->client_seq >> (56 - i * 8)) & 0xFF;
        aad[8] = type; aad[9] = 0x03; aad[10] = 0x03;
        aad[11] = (len >> 8) & 0xFF; aad[12] = len & 0xFF;

        {
            char dbg[128]; int dp=0; const char* hex="0123456789ABCDEF";
            dbg[dp++]='A';dbg[dp++]='D';dbg[dp++]=':';
            for(int k=0;k<13;k++){dbg[dp++]=hex[(aad[k]>>4)&0xF];dbg[dp++]=hex[aad[k]&0xF];}
            dbg[dp]=0; print_string(dbg); print_string("\n");
        }

        uint8_t explicit_iv[8];
        get_random_bytes(explicit_iv, 8);
        
        uint8_t iv[12];
        memcpy(iv, ctx->client_iv, 4);
        memcpy(iv + 4, explicit_iv, 8);

        {
            char dbg[128]; int dp=0; const char* hex="0123456789ABCDEF";
            dbg[dp++]='I';dbg[dp++]='V';dbg[dp++]='F';dbg[dp++]=':';
            for(int k=0;k<12;k++){dbg[dp++]=hex[(iv[k]>>4)&0xF];dbg[dp++]=hex[iv[k]&0xF];}
            dbg[dp]=0; print_string(dbg); print_string("\n");
        }
        
        uint8_t* ct = kmalloc(len);
        uint8_t tag[16];
        if (!ct) return -1;
        
        aes128_gcm_encrypt(&ctx->client_gcm, iv, 12, aad, 13, data, len, ct, tag);
        {
            char dbg[128]; int dp=0; const char* hex="0123456789ABCDEF";
            dbg[dp++]='E';dbg[dp++]='N';dbg[dp++]='C';dbg[dp++]=':';
            for(int k=0;k<4;k++){dbg[dp++]=hex[(ct[k]>>4)&0xF];dbg[dp++]=hex[ct[k]&0xF];}
            dbg[dp++]=' ';dbg[dp++]='T';dbg[dp++]=':';
            for(int k=0;k<4;k++){dbg[dp++]=hex[(tag[k]>>4)&0xF];dbg[dp++]=hex[tag[k]&0xF];}
            dbg[dp]=0; print_string(dbg); print_string("\n");
        }
        
        size_t total_payload = 8 + len + 16;
        rec[3] = (total_payload >> 8) & 0xFF;
        rec[4] = total_payload & 0xFF;
        memcpy(rec + 5, explicit_iv, 8);
        memcpy(rec + 5 + 8, ct, len);
        memcpy(rec + 5 + 8 + len, tag, 16);
        kfree(ct);
        
        ctx->client_seq++;
        {
            char dbg[256]; int dp=0; const char* hex="0123456789ABCDEF";
            dbg[dp++]='S';dbg[dp++]='E';dbg[dp++]='N';dbg[dp++]='D';dbg[dp++]=':';
            for(size_t k=0;k<5+total_payload;k++){dbg[dp++]=hex[(rec[k]>>4)&0xF];dbg[dp++]=hex[rec[k]&0xF];}
            dbg[dp]=0; print_string(dbg); print_string("\n");
        }
        int sres = sys_send(ctx->sock, rec, 5 + total_payload, 0);
        {
            char dbg[32]; int dp=0; const char* hex="0123456789ABCDEF";
            dbg[dp++]='R';dbg[dp++]='=';char sn[16];itoa(sres,sn);int si=0;while(sn[si])dbg[dp++]=sn[si++];
            dbg[dp]=0; print_string(dbg); print_string("\n");
        }
    } else {
        uint8_t* mac_in = kmalloc(13 + len);
        if (!mac_in) return -1;
        for (int i = 0; i < 8; i++) mac_in[i] = (ctx->client_seq >> (56 - i * 8)) & 0xFF;
        mac_in[8]  = type;
        mac_in[9]  = 0x03; mac_in[10] = 0x03;
        mac_in[11] = (len >> 8) & 0xFF;
        mac_in[12] = len & 0xFF;
        memcpy(mac_in + 13, data, len);

        uint8_t mac[32];
        hmac_sha256(ctx->client_mac_key, 32, mac_in, 13 + len, mac);
        kfree(mac_in);

        size_t content_len = len + 32;
        size_t pad_len = 16 - (content_len % 16);
        size_t enc_len = content_len + pad_len;

        uint8_t* pt = kmalloc(enc_len);
        uint8_t* ct = kmalloc(enc_len);
        if (!pt || !ct) { kfree(pt); kfree(ct); return -1; }

        memcpy(pt, data, len);
        memcpy(pt + len, mac, 32);
        for (size_t i = 0; i < pad_len; ++i) pt[content_len + i] = (uint8_t)(pad_len - 1);

        uint8_t explicit_iv[16];
        get_random_bytes(explicit_iv, 16);
        uint8_t iv_copy[16];
        memcpy(iv_copy, explicit_iv, 16);

        aes128_cbc_encrypt(&ctx->client_aes, iv_copy, pt, ct, enc_len);
        kfree(pt);

        size_t total_payload = 16 + enc_len;
        rec[3] = (total_payload >> 8) & 0xFF;
        rec[4] = total_payload & 0xFF;
        memcpy(rec + 5,      explicit_iv, 16);
        memcpy(rec + 5 + 16, ct,          enc_len);
        kfree(ct);

        ctx->client_seq++;
        sys_send(ctx->sock, rec, 5 + total_payload, 0);
    }

    if (type == TLS_HANDSHAKE && !ctx->tx_encrypted) hs_log(ctx, data, len);
    return 0;
}

static int recv_record(tls_ctx_t* ctx, uint8_t* type_out, uint8_t* out, size_t* out_len) {
    uint8_t hdr[5];
    if (tcp_read_exact(ctx->sock, hdr, 5) < 0) return -1;

    *type_out = hdr[0];
    size_t len = ((size_t)hdr[3] << 8) | hdr[4];
    if (len > 16384) return -1;

    uint8_t* buf = ctx->rx_buf;
    if (tcp_read_exact(ctx->sock, buf, len) < 0) return -1;

    if (!ctx->rx_encrypted) {
        if (len > 16384) return -1;
        memcpy(out, buf, len);
        *out_len = len;
    } else if (ctx->is_gcm) {
        if (len < 8 + 16) return -1;
        
        uint8_t explicit_iv[8];
        memcpy(explicit_iv, buf, 8);
        size_t ct_len = len - 8 - 16;
        
        uint8_t tag_in[16];
        memcpy(tag_in, buf + 8 + ct_len, 16);
        
        uint8_t iv[12];
        memcpy(iv, ctx->server_iv, 4);
        memcpy(iv + 4, explicit_iv, 8);
        
        uint8_t aad[13];
        for (int i = 0; i < 8; i++) aad[i] = (ctx->server_seq >> (56 - i * 8)) & 0xFF;
        aad[8] = *type_out; aad[9] = 0x03; aad[10] = 0x03;
        aad[11] = (ct_len >> 8) & 0xFF; aad[12] = ct_len & 0xFF;
        
        if (aes128_gcm_decrypt(&ctx->server_gcm, iv, 12, aad, 13, buf + 8, ct_len, tag_in, out) < 0) {
            return -1;
        }
        *out_len = ct_len;
        ctx->server_seq++;
    } else {
        if (len < 16 + 1 + 32) return -1;

        uint8_t explicit_iv[16];
        memcpy(explicit_iv, buf, 16);
        size_t enc_len = len - 16;

        uint8_t* pt = kmalloc(enc_len + 1);
        if (!pt) return -1;
        aes128_cbc_decrypt(&ctx->server_aes, explicit_iv, buf + 16, pt, enc_len);

        uint8_t pad_byte = pt[enc_len - 1];
        if ((size_t)(pad_byte + 1) > enc_len) { kfree(pt); return -1; }

        if (enc_len < 32 + (size_t)(pad_byte + 1)) { kfree(pt); return -1; }
        size_t pt_len = enc_len - 32 - (size_t)(pad_byte + 1);

        memcpy(out, pt, pt_len);
        *out_len = pt_len;
        kfree(pt);

        ctx->server_seq++;
    }

    if (*type_out == TLS_HANDSHAKE && !ctx->rx_encrypted) hs_log(ctx, out, *out_len);
    return 0;
}

tls_ctx_t* tls_connect(int sock, const char* hostname) {
    p256_init();
    tls_ctx_t* ctx = kmalloc(sizeof(tls_ctx_t));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(tls_ctx_t));
    ctx->sock = sock;

    uint8_t client_random[32];
    get_random_bytes(client_random, 32);

    /* ---- 1. ClientHello ---- */
    size_t host_len = strlen(hostname);
    size_t sni_ext_data_len = 2 + 1 + 2 + host_len;
    size_t sni_ext_total    = 2 + 2 + sni_ext_data_len;
    
    // Supported Groups - only secp256r1 (we don't support x25519)
    size_t sg_ext_total = 4 + 4;
    // EC Point Formats ext: uncompressed (0x00)
    size_t ec_ext_total = 4 + 2;
    // Signature Algorithms: RSA+SHA256
    size_t sa_ext_total = 4 + 4;

    size_t ext_total = sni_ext_total + sg_ext_total + ec_ext_total + sa_ext_total;
    
    // Cipher suites: C02F (ECDHE-RSA-AES128-GCM-SHA256), 003C (RSA-AES128-CBC-SHA256)
    // + SCSV for secure renegotiation (0x00, 0xFF)
    size_t ch_body_len = 2 + 32 + 1 + 2 + 6 + 1 + 1 + 2 + ext_total;
    size_t ch_total = 4 + ch_body_len;

    uint8_t* ch = kmalloc(ch_total);
    if (!ch) { kfree(ctx); return NULL; }
    memset(ch, 0, ch_total);

    size_t i = 0;
    ch[i++] = 1;
    ch[i++] = (ch_body_len >> 16) & 0xFF;
    ch[i++] = (ch_body_len >>  8) & 0xFF;
    ch[i++] = ch_body_len & 0xFF;

    ch[i++] = 0x03; ch[i++] = 0x03;
    memcpy(ch + i, client_random, 32); i += 32;
    ch[i++] = 0;
    ch[i++] = 0x00; ch[i++] = 0x06;
    ch[i++] = 0xC0; ch[i++] = 0x2F; // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
    ch[i++] = 0x00; ch[i++] = 0x3C; // TLS_RSA_WITH_AES_128_CBC_SHA256
    ch[i++] = 0x00; ch[i++] = 0xFF; // SCSV for renegotiation
    ch[i++] = 1;
    ch[i++] = 0;

    ch[i++] = (ext_total >> 8) & 0xFF;
    ch[i++] = ext_total & 0xFF;

    // SNI
    ch[i++] = 0x00; ch[i++] = 0x00;
    ch[i++] = (sni_ext_data_len >> 8) & 0xFF; ch[i++] = sni_ext_data_len & 0xFF;
    ch[i++] = ((host_len + 3) >> 8) & 0xFF; ch[i++] = (host_len + 3) & 0xFF;
    ch[i++] = 0x00;
    ch[i++] = (host_len >> 8) & 0xFF; ch[i++] = host_len & 0xFF;
    memcpy(ch + i, hostname, host_len); i += host_len;

    // Supported Groups - only secp256r1
    ch[i++] = 0x00; ch[i++] = 0x0A;
    ch[i++] = 0x00; ch[i++] = 0x04;
    ch[i++] = 0x00; ch[i++] = 0x02;
    ch[i++] = 0x00; ch[i++] = 0x17; // secp256r1
    
    // EC Point Formats
    ch[i++] = 0x00; ch[i++] = 0x0B;
    ch[i++] = 0x00; ch[i++] = 0x02;
    ch[i++] = 0x01; ch[i++] = 0x00;
    
    // Signature Algorithms
    ch[i++] = 0x00; ch[i++] = 0x0D;
    ch[i++] = 0x00; ch[i++] = 0x04;
    ch[i++] = 0x00; ch[i++] = 0x02;
    ch[i++] = 0x04; ch[i++] = 0x01; // SHA256+RSA

    send_record(ctx, TLS_HANDSHAKE, ch, ch_total);
    kfree(ch);

    uint8_t server_random[32];
    memset(server_random, 0, 32);
    uint8_t cert_modulus[512];
    size_t  cert_mod_len = 0;
    uint8_t cert_exponent[8];
    size_t  cert_exp_len = 0;
    
    int use_ecdhe = 0;
    p256_point_t server_pub_ec;
    
    while (1) {
        uint8_t type;
        uint8_t* rec = kmalloc(16384);
        if (!rec) { kfree(ctx); return NULL; }
        size_t rec_len;

        if (recv_record(ctx, &type, rec, &rec_len) < 0) {
            print_string("  TLS: recv_record failed\n");
            kfree(rec); kfree(ctx); return NULL;
        }

        if (type == TLS_ALERT) {
            print_string("  TLS: Received alert from server\n");
            kfree(rec); kfree(ctx); return NULL;
        }

        if (type != TLS_HANDSHAKE) {
            {
                char dbg[64];
                extern void itoa(uint64_t n, char* s);
                dbg[0] = 'T'; dbg[1] = 'L'; dbg[2] = 'S'; dbg[3] = ':'; dbg[4] = ' ';
                dbg[5] = 'r'; dbg[6] = 'e'; dbg[7] = 'c'; dbg[8] = 'v'; dbg[9] = ' ';
                dbg[10] = 't'; dbg[11] = 'y'; dbg[12] = 'p'; dbg[13] = 'e'; dbg[14] = '=';
                dbg[15] = 0;
                print_string(dbg); itoa(type, dbg); print_string(dbg);
                print_string(" (non-handshake), skipping\n");
            }
            kfree(rec); continue;
        }

        size_t p = 0;
        while (p + 4 <= rec_len) {
            uint8_t  ht    = rec[p];
            size_t   hlen  = ((size_t)rec[p+1] << 16) | ((size_t)rec[p+2] << 8) | rec[p+3];

            if (p + 4 + hlen > rec_len) break;

            if (ht == 2) {          // ServerHello
                print_string("  TLS: Got ServerHello\n");
                if (hlen >= 34) memcpy(server_random, rec + p + 4 + 2, 32);
                uint8_t sid_len = rec[p+4+34];
                uint16_t cipher = (rec[p+4+35+sid_len] << 8) | rec[p+4+35+sid_len+1];
                {
                    char cbuf[32]; cbuf[0]='C';cbuf[1]='S';cbuf[2]='=';cbuf[3]=0;
                    print_string(cbuf); itoa(cipher, cbuf); print_string(cbuf); print_string("\n");
                }
                if (cipher == 0xC02B || cipher == 0xC02F) {
                    ctx->is_gcm = 1;
                    use_ecdhe = 1;
                    print_string("  TLS: Using ECDHE-RSA-AES128-GCM\n");
                } else if (cipher == 0x003C) {
                    ctx->is_gcm = 0;
                    use_ecdhe = 0;
                    print_string("  TLS: Using RSA-AES128-CBC-SHA256\n");
                } else {
                    print_string("  TLS: Unknown cipher suite\n");
                    kfree(rec); kfree(ctx); return NULL;
                }
            } else if (ht == 11) {  // Certificate
                print_string("  TLS: Got Certificate\n");
                if (hlen >= 7) {
                    size_t c1_len = ((size_t)rec[p+4+3] << 16) | ((size_t)rec[p+4+4] << 8) | rec[p+4+5];
                    const uint8_t* c1_ptr = rec + p + 4 + 6;
                    if (p + 4 + 6 + c1_len <= rec_len) {
                        const uint8_t *mod, *exp;
                        if (x509_extract_rsa_pubkey(c1_ptr, c1_len, &mod, &cert_mod_len, &exp, &cert_exp_len) == 0) {
                            memcpy(cert_modulus, mod, cert_mod_len);
                            memcpy(cert_exponent, exp, cert_exp_len);
                            print_string("  TLS: RSA key extracted from cert\n");
                        }
                    }
                }
            } else if (ht == 12) {  // ServerKeyExchange
                print_string("  TLS: Got ServerKeyExchange\n");
                size_t sp = p + 4;
                if (hlen >= 4 && rec[sp] == 3) { // named_curve
                    uint16_t curve = (rec[sp+1] << 8) | rec[sp+2];
                    {
                        char cbuf[32]; cbuf[0]='C';cbuf[1]='=';cbuf[2]=0;
                        print_string(cbuf);
                        itoa(curve, cbuf); print_string(cbuf);
                        print_string("\n");
                    }
                    if (curve == 23) { // secp256r1
                        size_t pk_len = rec[sp+3];
                        if (pk_len == 65) {
                            p256_from_bytes(&server_pub_ec, rec + sp + 4, 65);
                            print_string("  TLS: ECDHE params parsed\n");
                        }
                    } else {
                        print_string("  TLS: Unsupported curve\n");
                        kfree(rec); kfree(ctx); return NULL;
                    }
                }
            } else if (ht == 14) {  // ServerHelloDone
                print_string("  TLS: Got ServerHelloDone\n");
                kfree(rec);
                goto send_cke;
            } else {
                {
                    char dbg[64];
                    extern void itoa(uint64_t n, char* s);
                    dbg[0] = 'T'; dbg[1] = 'L'; dbg[2] = 'S'; dbg[3] = ':'; dbg[4] = ' ';
                    dbg[5] = 'u'; dbg[6] = 'n'; dbg[7] = 'k'; dbg[8] = 'n'; dbg[9] = 'o';
                    dbg[10] = 'w'; dbg[11] = 'n'; dbg[12] = ' '; dbg[13] = 'h'; dbg[14] = 't';
                    dbg[15] = '='; dbg[16] = 0;
                    print_string(dbg); itoa(ht, dbg); print_string(dbg);
                    print_string("\n");
                }
            }
            p += 4 + hlen;
        }
        kfree(rec);
    }

send_cke:
    uint8_t pre_master[48];
    uint8_t master_secret[48];
    
    if (use_ecdhe) {
        print_string("  TLS: Generating ECDHE keypair...\n");
        // Generate ephemeral key
        bignum_t priv_key;
        uint8_t priv_bytes[32];
        get_random_bytes(priv_bytes, 32);
        bignum_from_bytes(&priv_key, priv_bytes, 32);
        
        p256_point_t my_pub, shared;
        p256_base_mult(&my_pub, &priv_key);
        p256_scalar_mult(&shared, &priv_key, &server_pub_ec);
        print_string("  TLS: ECDHE keypair done\n");
        
        uint8_t pms[32];
        bignum_to_bytes(&shared.x, pms, 32);
        {
            char dbg[64]; int dp=0;
            dbg[dp++]='P';dbg[dp++]='M';dbg[dp++]=':';dbg[dp++]=' ';
            for(int k=0;k<4;k++){char h[3]="00";const char* hex="0123456789ABCDEF";h[0]=hex[(pms[k]>>4)&0xF];h[1]=hex[pms[k]&0xF];dbg[dp++]=h[0];dbg[dp++]=h[1];}
            dbg[dp]=0; print_string(dbg); print_string("\n");
        }
        
        uint8_t cke[4 + 1 + 65];
        cke[0] = 16;
        cke[1] = 0; cke[2] = 0; cke[3] = 66;
        cke[4] = 65;
        p256_to_bytes(&my_pub, cke + 5);
        print_string("  TLS: Sending ClientKeyExchange...\n");
        send_record(ctx, TLS_HANDSHAKE, cke, sizeof(cke));
        print_string("  TLS: ClientKeyExchange sent\n");
        
        // Master secret for ECDHE: PRF(pre_master(32), "master secret", client_random + server_random)
        uint8_t seed[64];
        memcpy(seed, client_random, 32);
        memcpy(seed + 32, server_random, 32);
        tls_prf_sha256(pms, 32, "master secret", seed, 64, master_secret, 48);
        
    } else {
        if (cert_mod_len == 0) { kfree(ctx); return NULL; }
        pre_master[0] = 0x03; pre_master[1] = 0x03;
        get_random_bytes(pre_master + 2, 46);

        uint8_t encrypted_pms[512];
        if (rsa_public_encrypt(cert_modulus, cert_mod_len, cert_exponent, cert_exp_len, pre_master, 48, encrypted_pms) != 0) {
            kfree(ctx); return NULL;
        }

        size_t cke_body = 2 + cert_mod_len;
        size_t cke_total = 4 + cke_body;
        uint8_t* cke = kmalloc(cke_total);
        if (!cke) { kfree(ctx); return NULL; }
        cke[0] = 16;
        cke[1] = (cke_body >> 16) & 0xFF;
        cke[2] = (cke_body >>  8) & 0xFF;
        cke[3] = cke_body & 0xFF;
        cke[4] = (cert_mod_len >> 8) & 0xFF;
        cke[5] = cert_mod_len & 0xFF;
        memcpy(cke + 6, encrypted_pms, cert_mod_len);
        send_record(ctx, TLS_HANDSHAKE, cke, cke_total);
        kfree(cke);
        
        uint8_t seed[64];
        memcpy(seed, client_random, 32);
        memcpy(seed + 32, server_random, 32);
        tls_prf_sha256(pre_master, 48, "master secret", seed, 64, master_secret, 48);
    }

    uint8_t key_block[112];
    {
        uint8_t seed[64];
        memcpy(seed, server_random, 32);
        memcpy(seed + 32, client_random, 32);
        // GCM: MAC=0, key=16, IV=4 (implicit). Total = 0+0+16+16+4+4 = 40 bytes required
        // CBC: MAC=32, key=16, IV=16 (unused). Total = 112
        tls_prf_sha256(master_secret, 48, "key expansion", seed, 64, key_block, 112);
    }

    if (ctx->is_gcm) {
        {
            char dbg[128]; int dp=0; const char* hex="0123456789ABCDEF";
            dbg[dp++]='K';dbg[dp++]='W';dbg[dp++]=':';
            for(int k=0;k<4;k++){dbg[dp++]=hex[(key_block[k]>>4)&0xF];dbg[dp++]=hex[key_block[k]&0xF];}
            dbg[dp++]=' ';dbg[dp++]='I';dbg[dp++]='V';dbg[dp++]=':';
            for(int k=32;k<36;k++){dbg[dp++]=hex[(key_block[k]>>4)&0xF];dbg[dp++]=hex[key_block[k]&0xF];}
            dbg[dp]=0; print_string(dbg); print_string("\n");
        }
        aes128_gcm_init(&ctx->client_gcm, key_block + 0);
        aes128_gcm_init(&ctx->server_gcm, key_block + 16);
        memcpy(ctx->client_iv, key_block + 32, 4);
        memcpy(ctx->server_iv, key_block + 36, 4);
    } else {
        memcpy(ctx->client_mac_key, key_block +  0, 32);
        memcpy(ctx->server_mac_key, key_block + 32, 32);
        aes128_init(&ctx->client_aes, key_block + 64);
        aes128_init(&ctx->server_aes, key_block + 80);
        memcpy(ctx->client_iv, key_block +  96, 16);
        memcpy(ctx->server_iv, key_block + 112 - 16, 16);
    }

    {
        uint8_t ccs[1] = {1};
        print_string("  TLS: Sending CCS...\n");
        send_record(ctx, TLS_CHANGE_CIPHER_SPEC, ccs, 1);
        print_string("  TLS: CCS sent\n");
    }

    ctx->tx_encrypted = 1;
    print_string("  TLS: tx_encrypted set\n");

    {
        uint8_t hs_hash[32];
        sha256(ctx->handshake_messages, ctx->handshake_len, hs_hash);
        print_string("  TLS: handshake hash computed\n");
        {
            char dbg[64]; int dp=0;
            dbg[dp++]='H';dbg[dp++]='L';dbg[dp++]='=';dbg[dp++]=' ';
            char snum[16]; itoa(ctx->handshake_len, snum); int si=0; while(snum[si]) dbg[dp++]=snum[si++];
            dbg[dp]=0; print_string(dbg); print_string("\n");
        }
        {
            char dbg[128]; int dp=0;
            dbg[dp++]='H';dbg[dp++]='S';dbg[dp++]='H';dbg[dp++]=':';
            const char* hex="0123456789ABCDEF";
            for(int k=0;k<4;k++){char h0=hex[(hs_hash[k]>>4)&0xF];char h1=hex[hs_hash[k]&0xF];dbg[dp++]=h0;dbg[dp++]=h1;}
            dbg[dp]=0; print_string(dbg); print_string("\n");
        }

        uint8_t verify_data[12];
        tls_prf_sha256(master_secret, 48, "client finished", hs_hash, 32, verify_data, 12);
        print_string("  TLS: verify_data computed\n");
        {
            char dbg[64]; int dp=0;
            dbg[dp++]='V';dbg[dp++]=':';dbg[dp++]=' ';
            for(int k=0;k<4;k++){char h[3]="00";const char* hex="0123456789ABCDEF";h[0]=hex[(verify_data[k]>>4)&0xF];h[1]=hex[verify_data[k]&0xF];dbg[dp++]=h[0];dbg[dp++]=h[1];}
            dbg[dp]=0; print_string(dbg); print_string("\n");
        }
        {
            char dbg[64]; int dp=0;
            dbg[dp++]='M';dbg[dp++]='S';dbg[dp++]=':';dbg[dp++]=' ';
            for(int k=0;k<4;k++){char h[3]="00";const char* hex="0123456789ABCDEF";h[0]=hex[(master_secret[k]>>4)&0xF];h[1]=hex[master_secret[k]&0xF];dbg[dp++]=h[0];dbg[dp++]=h[1];}
            dbg[dp]=0; print_string(dbg); print_string("\n");
        }

        uint8_t fin[16];
        fin[0] = 20;
        fin[1] = 0; fin[2] = 0; fin[3] = 12;
        memcpy(fin + 4, verify_data, 12);

        print_string("  TLS: Sending Finished...\n");
        send_record(ctx, TLS_HANDSHAKE, fin, 16);
        print_string("  TLS: Finished sent\n");
    }

    print_string("  TLS: Waiting for server CCS + Finished...\n");
    int fin_timeout = 200;
    while (fin_timeout > 0) {
        uint8_t type;
        uint8_t* rec = kmalloc(16384);
        if (!rec) { kfree(ctx); return NULL; }
        size_t rec_len;

        if (recv_record(ctx, &type, rec, &rec_len) < 0) {
            print_string("  TLS: recv_record failed in final loop\n");
            kfree(rec); kfree(ctx); return NULL;
        }

        if (type == TLS_CHANGE_CIPHER_SPEC) {
            print_string("  TLS: Got server CCS\n");
            ctx->rx_encrypted = 1;
            kfree(rec);
            continue;
        }

        if (type == TLS_ALERT) {
            print_string("  TLS: Got alert from server (level=");
            char abuf[16]; itoa(rec[0], abuf); print_string(abuf);
            print_string(", desc=");
            itoa(rec[1], abuf); print_string(abuf);
            print_string(")\n");
            kfree(rec); kfree(ctx); return NULL;
        }

        if (type == TLS_HANDSHAKE && rec_len >= 4 && rec[0] == 20) {
            print_string("  TLS: Got server Finished\n");
            kfree(rec);
            break;
        }

        {
            char dbg[64];
            extern void itoa(uint64_t n, char* s);
            dbg[0] = 'T'; dbg[1] = 'L'; dbg[2] = 'S'; dbg[3] = ':'; dbg[4] = ' ';
            dbg[5] = 'u'; dbg[6] = 'n'; dbg[7] = 'e'; dbg[8] = 'x'; dbg[9] = 'p';
            dbg[10] = ' '; dbg[11] = 't'; dbg[12] = 'y'; dbg[13] = 'p'; dbg[14] = 'e';
            dbg[15] = '='; dbg[16] = 0;
            print_string(dbg); itoa(type, dbg); print_string(dbg);
            print_string("\n");
        }
        kfree(rec);
        fin_timeout--;
    }
    if (fin_timeout <= 0) {
        print_string("  TLS: Timeout waiting for server Finished\n");
        kfree(ctx); return NULL;
    }

    return ctx;
}

int tls_send(tls_ctx_t* ctx, const void* data, size_t len) {
    if (!ctx || !ctx->tx_encrypted) return -1;
    return send_record(ctx, TLS_APPLICATION_DATA, (const uint8_t*)data, len);
}

int tls_recv(tls_ctx_t* ctx, void* buf, size_t max_len) {
    if (!ctx || !ctx->rx_encrypted) return -1;
    uint8_t type;
    size_t len;
    if (recv_record(ctx, &type, (uint8_t*)buf, &len) < 0) return -1;
    if (type == TLS_APPLICATION_DATA) return (int)len;
    return 0;
}

void tls_close(tls_ctx_t* ctx) {
    if (ctx) {
        uint8_t alert[2] = {1, 0};
        send_record(ctx, TLS_ALERT, alert, 2);
        kfree(ctx);
    }
}

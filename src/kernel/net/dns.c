#include "kernel/net/dns.h"
#include "kernel/net/udp.h"
#include "kernel/net/in.h"
#include <string.h>
#include "drivers/video/framebuffer.h"

static uint32_t dns_server = 0x0302000A; // 10.0.2.3 (QEMU user-net DNS proxy, network order)

void dns_set_server(uint32_t ip)
{
    dns_server = ip;
}

static int encode_dns_name(uint8_t* dst, const char* hostname)
{
    int written = 0;
    while (*hostname) {
        const char* dot = hostname;
        while (*dot && *dot != '.') dot++;
        int label_len = dot - hostname;
        if (label_len > 63) return -1;
        dst[written++] = (uint8_t)label_len;
        memcpy(dst + written, hostname, label_len);
        written += label_len;
        hostname = *dot ? dot + 1 : dot;
    }
    dst[written++] = 0;
    return written;
}

static int parse_dns_name(const uint8_t* msg, int msg_len, int off, char* out, int out_len)
{
    int out_pos = 0;
    int jumped = 0;
    int saved_off = 0;
    int jumps = 0;

    while (off < msg_len) {
        uint8_t len = msg[off];
        if (len == 0) {
            off++;
            break;
        }
        if ((len & 0xC0) == 0xC0) {
            if (!jumped) {
                saved_off = off + 2;
                jumped = 1;
            }
            if (++jumps > 20) return -1; // Prevent infinite loops
            if (off + 1 >= msg_len) return -1;
            off = ((len & 0x3F) << 8) | msg[off + 1];
            continue;
        }
        off++;
        if (off + len > msg_len) return -1;
        if (out_pos > 0 && out_pos < out_len) out[out_pos++] = '.';
        for (int i = 0; i < len && out_pos < out_len; i++)
            out[out_pos++] = msg[off + i];
        off += len;
    }
    if (out_pos < out_len) out[out_pos] = 0;
    
    if (jumped) return saved_off;
    return off;
}

int dns_resolve(const char* hostname, uint32_t* ip_addr)
{
    if (!hostname || !ip_addr) return -1;

    // Check if hostname is already an IPv4 address (e.g. "1.1.1.1")
    int is_ip = 1;
    uint32_t ip_val = 0;
    int octets = 0;
    int cur_val = 0;
    for (int i = 0; hostname[i]; i++) {
        if (hostname[i] >= '0' && hostname[i] <= '9') {
            cur_val = cur_val * 10 + (hostname[i] - '0');
        } else if (hostname[i] == '.') {
            ip_val = (ip_val << 8) | (cur_val & 0xFF);
            cur_val = 0;
            octets++;
        } else {
            is_ip = 0; break;
        }
    }
    if (is_ip && octets == 3) {
        ip_val = (ip_val << 8) | (cur_val & 0xFF);
        *ip_addr = htonl(ip_val);
        return 0;
    }

    uint8_t query[512];
    memset(query, 0, sizeof(query));

    struct dns_header* hdr = (struct dns_header*)query;
    static uint16_t query_id = 1;
    hdr->id      = htons(query_id++);
    hdr->flags   = htons(DNS_FLAG_RD);
    hdr->qdcount = htons(1);

    int off = sizeof(struct dns_header);
    int enc_len = encode_dns_name(query + off, hostname);
    if (enc_len < 0) {
//        print_string("  DNS: Failed to encode hostname\n");
        return -1;
    }
    off += enc_len;

    query[off++] = 0;
    query[off++] = DNS_TYPE_A;
    query[off++] = 0;
    query[off++] = DNS_CLASS_IN;

    uint16_t sport = 30000 + (query_id % 1000);
//    print_string("  DNS: Querying ");
//    print_string(hostname);
//    print_string(" -> ");
    char buf[16];
    extern void itoa(uint64_t n, char* s);
//    itoa(ntohl(dns_server), buf); print_string(buf);
//    print_string("\n");

    if (udp_output(dns_server, DNS_PORT, sport, query, off) < 0) {
//        print_string("  DNS: UDP send failed\n");
        return -1;
    }

    /* Flush any stale RX entries for our port from previous calls */
    {
        uint8_t dummy[64];
        uint16_t dummy_len = sizeof(dummy);
        uint32_t dummy_ip;
        uint16_t dummy_port;
        while (udp_recv(sport, &dummy_ip, &dummy_port, dummy, &dummy_len) == 0) {
            dummy_len = sizeof(dummy);
        }
    }

    int timeout = 100;
    int retry = 20;
    while (timeout > 0) {
        extern void net_poll();
        net_poll();

        uint8_t reply[512];
        uint16_t reply_len = sizeof(reply);
        uint32_t src_ip;
        uint16_t src_port;
        if (udp_recv(sport, &src_ip, &src_port, reply, &reply_len) == 0) {
            if (src_ip != dns_server || src_port != htons(DNS_PORT)) continue;
            if (reply_len < sizeof(struct dns_header)) {
//                print_string("  DNS: Reply too short\n");
                return -1;
            }

            struct dns_header* rh = (struct dns_header*)reply;
            uint16_t rflags = ntohs(rh->flags);
            uint16_t rcode  = rflags & DNS_FLAG_RCODE_MASK;

            if (rcode != 0) {
//                print_string("  DNS: Server error rcode=");
//                char rc[5]; itoa(rcode, rc); print_string(rc);
//                print_string("\n");
                return -1;
            }

            uint16_t ancount = ntohs(rh->ancount);
            if (ancount == 0) {
//                print_string("  DNS: No answers\n");
                return -1;
            }

            off = sizeof(struct dns_header);
            int qcount = ntohs(rh->qdcount);
            for (int i = 0; i < qcount; i++) {
                char tmp[256];
                off = parse_dns_name(reply, reply_len, off, tmp, sizeof(tmp));
                if (off < 0) return -1;
                off += 4;
            }

            for (int i = 0; i < ancount; i++) {
                char tmp[256];
                off = parse_dns_name(reply, reply_len, off, tmp, sizeof(tmp));
                if (off < 0) {
//                    print_string("  DNS: parse_dns_name failed\n");
                    break;
                }
                
                if (off + 10 > (int)reply_len) {
//                    print_string("  DNS: answer record truncated\n");
                    break;
                }
                
                uint16_t type = (reply[off] << 8) | reply[off + 1];
                off += 2; // type
                off += 2; // class
                off += 4; // ttl
                uint16_t rdlength = (reply[off] << 8) | reply[off + 1];
                off += 2; // rdlength
                
//                print_string("  DNS: record type=");
//                char tbuf[10]; itoa(type, tbuf); print_string(tbuf);
//                print_string(" rdlen=");
//                char rbuf[10]; itoa(rdlength, rbuf); print_string(rbuf);
//                print_string("\n");
                
                if (type == DNS_TYPE_A && rdlength == 4 && off + 4 <= (int)reply_len) {
                    uint32_t addr = 0;
                    addr |= (uint32_t)reply[off] << 24;
                    addr |= (uint32_t)reply[off + 1] << 16;
                    addr |= (uint32_t)reply[off + 2] << 8;
                    addr |= (uint32_t)reply[off + 3];
                    *ip_addr = htonl(addr);
//                    print_string("  DNS: Resolved to ");
//                    itoa(ntohl(*ip_addr), buf); print_string(buf);
//                    print_string("\n");
                    return 0;
                }
                
                // Skip the record data
                off += rdlength;
            }

//            print_string("  DNS: No A record found\n");
            return -1;
        }

        extern void sleep_task(uint32_t ms);
        sleep_task(50);
        timeout--;

        retry--;
        if (retry <= 0) {
            retry = 60;
            udp_output(dns_server, DNS_PORT, sport, query, off);
        }
    }

//    print_string("  DNS: Timeout\n");

    /* Hardcoded fallback for common domains */
    static const struct {
        const char* name;
        uint32_t    ip;
    } fallback[] = {
        {"google.com",       htonl(0xD83AD6CE)},  /* 216.58.214.206 */
        {"example.com",      htonl(0x5DB8D822)},  /* 93.184.216.34 */
        {"github.com",       htonl(0xC01EFF71)},  /* 192.30.255.113 */
        {"open-meteo.com",   htonl(0x681A0E6F)},  /* 104.26.14.111 */
        {"api.open-meteo.com", htonl(0x681A0E6F)}, /* 104.26.14.111 */
        {0, 0}
    };
    for (int i = 0; fallback[i].name; i++) {
        const char* a = hostname;
        const char* b = fallback[i].name;
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == 0 && *b == 0) {
            *ip_addr = fallback[i].ip;
//            print_string("  DNS: Using hardcoded IP for ");
//            print_string(hostname);
//            print_string("\n");
            return 0;
        }
    }
    return -1;
}

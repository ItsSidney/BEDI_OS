#include "kernel/net/icmp.h"
#include "kernel/net/ip.h"
#include "kernel/net/if.h"
#include "drivers/video/framebuffer.h"
#include <string.h>

/*
 * ICMP implementation for BEDI OS.
 */

uint16_t icmp_checksum(void* vdata, size_t length) {
    uint8_t* data = (uint8_t*)vdata;
    uint32_t sum = 0;

    for (; length > 1; length -= 2) {
        sum += (data[0] << 8) | data[1];
        data += 2;
    }
    if (length == 1)
        sum += (data[0] << 8);

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return htons(~sum);
}

void icmp_input(struct mbuf* m, int off) {
    struct ip* ip = (struct ip*)(m->m_data - off);
    struct icmphdr* icp = (struct icmphdr*)m->m_data;
    int icmplen = ntohs(ip->ip_len) - off;

    if (icp->icmp_type == ICMP_ECHO) {
        /* Convert Echo Request to Echo Reply */
        icp->icmp_type = ICMP_ECHOREPLY;
        icp->icmp_cksum = 0;
        icp->icmp_cksum = icmp_checksum(icp, icmplen);
        
        /* Swap IP addresses */
        struct in_addr tmp = ip->ip_src;
        ip->ip_src = ip->ip_dst;
        ip->ip_dst = tmp;
        
        /* Move m_data back to IP header for ip_output */
        m->m_data -= off;
        m->m_len += off;
        
        ip_output(m, NULL);
    } else if (icp->icmp_type == ICMP_ECHOREPLY) {
//        print_string("  Reply received from ");
        char buf[16];
        itoa(ntohl(ip->ip_src.s_addr), buf);
//        print_string(buf);
//        print_string("!\n");
        m_freem(m);
    } else {
        m_freem(m);
    }
}

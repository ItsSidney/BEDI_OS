#include "kernel/net/if.h"
#include "kernel/net/mbuf.h"
#include "kernel/net/if_arp.h"
#include "kernel/net/ethernet.h"
#include "kernel/net/in.h"
#include <string.h>
#include "drivers/video/framebuffer.h"

/*
 * ARP logic for BEDI OS.
 */

struct arp_entry {
    struct in_addr ip;
    uint8_t mac[6];
    int valid;
};

#define ARP_CACHE_SIZE 16
static struct arp_entry arp_cache[ARP_CACHE_SIZE] = {
    {{0x0202000A}, {0x52, 0x54, 0x00, 0x12, 0x34, 0x56}, 1}, /* 10.0.2.2 gateway */
    {{0x0302000A}, {0x52, 0x54, 0x00, 0x12, 0x34, 0x56}, 1}  /* 10.0.2.3 DNS proxy */
};

void arp_input(struct mbuf* m) {
    struct ether_arp* ea;
    struct ifnet* ifp = m->m_pkthdr.rcvif;
    
    if (m->m_len < sizeof(struct ether_arp)) {
        m_freem(m);
        return;
    }
    
    ea = (struct ether_arp*)m->m_data;
    
    if (ntohs(ea->ea_hdr.ar_op) == ARPOP_REPLY) {
        /* Update cache — overwrite existing entry or use empty slot */
        int idx = -1;
        for (int i = 0; i < ARP_CACHE_SIZE; i++) {
            if (arp_cache[i].valid && arp_cache[i].ip.s_addr == *(uint32_t*)ea->arp_spa) {
                idx = i; break;
            }
            if (!arp_cache[i].valid && idx < 0) idx = i;
        }
        if (idx >= 0) {
            memcpy(&arp_cache[idx].ip, ea->arp_spa, 4);
            memcpy(arp_cache[idx].mac, ea->arp_sha, 6);
            arp_cache[idx].valid = 1;
            print_string("  ARP: Learned ");
            {
                char hex[] = "0123456789ABCDEF";
                char s[3];
                for (int j = 0; j < 6; j++) {
                    s[0] = hex[(arp_cache[idx].mac[j] >> 4) & 0xF];
                    s[1] = hex[arp_cache[idx].mac[j] & 0xF];
                    s[2] = 0;
                    print_string(s);
                    if (j < 5) print_string(":");
                }
            }
            print_string(" for gateway\n");
        }
    } else if (ntohs(ea->ea_hdr.ar_op) == ARPOP_REQUEST) {
        /* Learn sender's IP-MAC from any ARP request — overwrite if existing */
        {
            int idx = -1;
            for (int i = 0; i < ARP_CACHE_SIZE; i++) {
                if (arp_cache[i].valid && arp_cache[i].ip.s_addr == *(uint32_t*)ea->arp_spa) {
                    idx = i; break;
                }
                if (!arp_cache[i].valid && idx < 0) idx = i;
            }
            if (idx >= 0) {
                memcpy(&arp_cache[idx].ip, ea->arp_spa, 4);
                memcpy(arp_cache[idx].mac, ea->arp_sha, 6);
                arp_cache[idx].valid = 1;
            }
        }
        /* Handle ARP requests for our IP */
        if (memcmp(ea->arp_tpa, &ifp->if_ip, 4) == 0) {
            struct mbuf* am = m_gethdr(MT_DATA);
            if (!am) { m_freem(m); return; }
            
            am->m_data += sizeof(struct ether_header);
            struct ether_arp* reply = (struct ether_arp*)am->m_data;
            
            reply->ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
            reply->ea_hdr.ar_pro = htons(ETHERTYPE_IP);
            reply->ea_hdr.ar_hln = 6;
            reply->ea_hdr.ar_pln = 4;
            reply->ea_hdr.ar_op = htons(ARPOP_REPLY);
            
            memcpy(reply->arp_sha, ifp->if_hwaddr, 6);
            memcpy(reply->arp_spa, &ifp->if_ip, 4);
            memcpy(reply->arp_tha, ea->arp_sha, 6);
            memcpy(reply->arp_tpa, ea->arp_spa, 4);
            
            am->m_len = sizeof(struct ether_arp);
            ether_output(ifp, am, ea->arp_sha, ETHERTYPE_ARP);
        }
    }
    
    m_freem(m);
}

static void print_hex_ip(uint32_t ip_net) {
    char hex[] = "0123456789ABCDEF";
    char s[3]; s[2] = 0;
    uint8_t* b = (uint8_t*)&ip_net;
    for (int k = 0; k < 4; k++) {
        uint8_t v = b[k];
        s[0] = hex[(v >> 4) & 0xF]; s[1] = hex[v & 0xF];
        print_string(s);
        if (k < 3) print_string(".");
    }
}

void arp_resolve(struct ifnet* ifp, struct mbuf* m, const struct in_addr* dst, uint8_t* dest_enaddr) {
    /* Check cache */
    int hit_idx = -1;
    uint8_t qemu_mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip.s_addr == dst->s_addr) {
            hit_idx = i;
            break;
        }
    }

    /* On HIT, send packet immediately */
    if (hit_idx >= 0) {
        ether_output(ifp, m, arp_cache[hit_idx].mac, ETHERTYPE_IP);
        /* If using fallback MAC, probe for real MAC */
        if (memcmp(arp_cache[hit_idx].mac, qemu_mac, 6) == 0) {
            struct mbuf* am = m_gethdr(MT_DATA);
            if (am) {
                am->m_data += sizeof(struct ether_header);
                struct ether_arp* ea = (struct ether_arp*)am->m_data;
                memset(ea, 0, sizeof(struct ether_arp));
                ea->ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
                ea->ea_hdr.ar_pro = htons(ETHERTYPE_IP);
                ea->ea_hdr.ar_hln = 6;
                ea->ea_hdr.ar_pln = 4;
                ea->ea_hdr.ar_op = htons(ARPOP_REQUEST);
                memcpy(ea->arp_sha, ifp->if_hwaddr, 6);
                memcpy(ea->arp_spa, &ifp->if_ip, 4);
                memcpy(ea->arp_tpa, dst, 4);
                am->m_len = sizeof(struct ether_arp);
                uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                ether_output(ifp, am, bcast, ETHERTYPE_ARP);
            }
        }
        return;
    }

    /* MISS: Send ARP Request */
    struct mbuf* am = m_gethdr(MT_DATA);
    if (!am) { m_freem(m); return; }
    
    am->m_data += sizeof(struct ether_header);
    struct ether_arp* ea = (struct ether_arp*)am->m_data;
    memset(ea, 0, sizeof(struct ether_arp));
    ea->ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
    ea->ea_hdr.ar_pro = htons(ETHERTYPE_IP);
    ea->ea_hdr.ar_hln = 6;
    ea->ea_hdr.ar_pln = 4;
    ea->ea_hdr.ar_op = htons(ARPOP_REQUEST);
    memcpy(ea->arp_sha, ifp->if_hwaddr, 6);
    memcpy(ea->arp_spa, &ifp->if_ip, 4);
    memcpy(ea->arp_tpa, dst, 4);
    
    am->m_len = sizeof(struct ether_arp);
    uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    ether_output(ifp, am, bcast, ETHERTYPE_ARP);
    
    m_freem(m);
}

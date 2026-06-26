#include "kernel/net/if.h"
#include "kernel/net/mbuf.h"
#include "kernel/net/if_arp.h"
#include "kernel/net/ethernet.h"
#include "kernel/net/in.h"
#include <string.h>
#include "drivers/video/framebuffer.h"
#include "kernel/lib/stdio.h"

/*
 * ARP logic for BEDI OS.
 */

struct arp_entry {
    struct in_addr ip;
    uint8_t mac[6];
    int valid;
    int is_static;
};

#define ARP_CACHE_SIZE 16
static struct arp_entry arp_cache[ARP_CACHE_SIZE] = {
    {{0x0202000A}, {0x52, 0x55, 0x0a, 0x00, 0x02, 0x02}, 1, 1}, /* 10.0.2.2 gateway / DNS proxy (same host) */
};

#define ARP_PENDING_MAX 4
static struct {
    struct in_addr dst;
    struct mbuf *m;
} arp_pending[ARP_PENDING_MAX];
static int arp_pending_count = 0;

static int arp_find_or_alloc(uint32_t ip) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip.s_addr == ip) return i;
    }
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) return i;
    }
    return -1;
}

void arp_input(struct mbuf* m) {
    struct ether_arp* ea;
    struct ifnet* ifp = m->m_pkthdr.rcvif;
    
    if (m->m_len < sizeof(struct ether_arp)) {
//        print_string("  ARP: Too short\n");
        m_freem(m);
        return;
    }
    
    ea = (struct ether_arp*)m->m_data;
    uint16_t op = ntohs(ea->ea_hdr.ar_op);
    {
        char dbg[64];
        snprintf(dbg, sizeof(dbg), "  ARP: op=%u hrd=%u pro=0x%04X hln=%u pln=%u\n",
            op, ntohs(ea->ea_hdr.ar_hrd), ntohs(ea->ea_hdr.ar_pro),
            ea->ea_hdr.ar_hln, ea->ea_hdr.ar_pln);
//        print_string(dbg);
    }
    
    if (op == ARPOP_REPLY) {
        uint32_t reply_ip = *(uint32_t*)ea->arp_spa;
        uint32_t tgt_ip = *(uint32_t*)ea->arp_tpa;
        uint8_t reply_mac[6];
        memcpy(reply_mac, ea->arp_sha, 6);
        {
            char dbg[64];
            snprintf(dbg, sizeof(dbg), "  ARP: Reply from %u.%u.%u.%u (%02x:%02x:%02x:%02x:%02x:%02x) for %u.%u.%u.%u\n",
                (reply_ip)&0xFF, (reply_ip>>8)&0xFF, (reply_ip>>16)&0xFF, (reply_ip>>24)&0xFF,
                reply_mac[0], reply_mac[1], reply_mac[2], reply_mac[3], reply_mac[4], reply_mac[5],
                (tgt_ip)&0xFF, (tgt_ip>>8)&0xFF, (tgt_ip>>16)&0xFF, (tgt_ip>>24)&0xFF);
//            print_string(dbg);
        }
        
        int idx = arp_find_or_alloc(reply_ip);
        if (idx >= 0) {
            if (!arp_cache[idx].is_static) {
                memcpy(&arp_cache[idx].ip, &reply_ip, 4);
                memcpy(arp_cache[idx].mac, reply_mac, 6);
                arp_cache[idx].valid = 1;
            }
        }
        
        for (int i = 0; i < arp_pending_count; i++) {
            if (arp_pending[i].m && arp_pending[i].dst.s_addr == reply_ip) {
                ether_output(ifp, arp_pending[i].m, reply_mac, ETHERTYPE_IP);
                arp_pending[i].m = NULL;
            }
        }
    } else if (op == ARPOP_REQUEST) {
        uint32_t req_ip = *(uint32_t*)ea->arp_spa;
        uint32_t tgt_ip = *(uint32_t*)ea->arp_tpa;
        {
            char dbg[64];
            snprintf(dbg, sizeof(dbg), "  ARP: Request from %u.%u.%u.%u for %u.%u.%u.%u (our IP: %u.%u.%u.%u)\n",
                (req_ip)&0xFF, (req_ip>>8)&0xFF, (req_ip>>16)&0xFF, (req_ip>>24)&0xFF,
                (tgt_ip)&0xFF, (tgt_ip>>8)&0xFF, (tgt_ip>>16)&0xFF, (tgt_ip>>24)&0xFF,
                (ifp->if_ip)&0xFF, (ifp->if_ip>>8)&0xFF, (ifp->if_ip>>16)&0xFF, (ifp->if_ip>>24)&0xFF);
//            print_string(dbg);
        }
        int idx = arp_find_or_alloc(req_ip);
        if (idx >= 0 && !arp_cache[idx].is_static) {
            memcpy(&arp_cache[idx].ip, &req_ip, 4);
            memcpy(arp_cache[idx].mac, ea->arp_sha, 6);
            arp_cache[idx].valid = 1;
        }
        
        if (memcmp(ea->arp_tpa, &ifp->if_ip, 4) == 0) {
//            print_string("  ARP: Sending reply\n");
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

void arp_resolve(struct ifnet* ifp, struct mbuf* m, const struct in_addr* dst, uint8_t* dest_enaddr) {
    int hit_idx = -1;
    uint8_t qemu_mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip.s_addr == dst->s_addr) {
            hit_idx = i;
            break;
        }
    }
    
    if (hit_idx >= 0) {
        ether_output(ifp, m, arp_cache[hit_idx].mac, ETHERTYPE_IP);
        if (!arp_cache[hit_idx].is_static &&
            memcmp(arp_cache[hit_idx].mac, qemu_mac, 6) == 0) {
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
    
    if (arp_pending_count < ARP_PENDING_MAX) {
        arp_pending[arp_pending_count].dst = *dst;
        arp_pending[arp_pending_count].m = m;
        arp_pending_count++;
    } else {
        m_freem(m);
    }
    
    struct mbuf* am = m_gethdr(MT_DATA);
    if (!am) return;
    
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

#include "kernel/net/mbuf.h"
#include "kernel/net/tcp.h"
#include "kernel/net/ip.h"
#include "kernel/net/in.h"
#include "kernel/net/if.h"
#include "kernel/net/socket.h"
#include "kernel/net/icmp.h"
#include "kernel/mem/kheap.h"
#include "kernel/lib/stdio.h"
#include "kernel/task/task.h"
#include <string.h>
extern void serial_puts(const char* s);

#define MAX_SOCKETS 32
static struct socket* sockets[MAX_SOCKETS];

static int tcp_send_packet(struct socket* so, uint8_t flags, const void* data, int len) {
    struct mbuf* m = m_getcl(MT_DATA);
    if (m == NULL) return -1;
    if (m->m_ext.ext_buf == NULL) { m_free(m); return -1; }

    m->m_data = (char*)m->m_ext.ext_buf + 256;
    if (len > 0 && data) {
        memcpy(m->m_data, data, len);
        m->m_len = len;
    }

    m->m_data -= sizeof(struct tcphdr);
    m->m_len += sizeof(struct tcphdr);
    struct tcphdr* th = (struct tcphdr*)m->m_data;
    memset(th, 0, sizeof(struct tcphdr));
    th->th_sport = so->so_local.sin_port;
    th->th_dport = so->so_remote.sin_port;
    th->th_off = sizeof(struct tcphdr) >> 2;
    th->th_flags = flags;
    th->th_seq = htonl(so->so_seq);
    th->th_ack = htonl(so->so_ack);
    th->th_win = htons(4096);

    m->m_data -= sizeof(struct ip);
    m->m_len += sizeof(struct ip);
    struct ip* ip = (struct ip*)m->m_data;
    memset(ip, 0, sizeof(struct ip));
    ip->ip_v = 4;
    ip->ip_hl = 5;
    ip->ip_ttl = 64;
    static uint16_t tcp_ip_id = 0;
    ip->ip_id = htons(tcp_ip_id++);
    ip->ip_p = IPPROTO_TCP;
    ip->ip_len = htons(m->m_len);
    
    struct ifnet* ifp = if_find("em0");
    ip->ip_src.s_addr = (ifp && ifp->if_ip != 0) ? ifp->if_ip : htonl(0x0A00020F);
    ip->ip_dst = so->so_remote.sin_addr;
    
    /* Force ACK field directly in mbuf to prevent corruption */
    uint32_t ack_val = htonl(so->so_ack);
    uint8_t* tcp_hdr = (uint8_t*)m->m_data + sizeof(struct ip);
    tcp_hdr[8] = (ack_val >> 24) & 0xFF;
    tcp_hdr[9] = (ack_val >> 16) & 0xFF;
    tcp_hdr[10] = (ack_val >> 8) & 0xFF;
    tcp_hdr[11] = ack_val & 0xFF;
    th->th_ack = ack_val;
    
    th->th_sum = 0;
    th->th_sum = tcp_checksum(ip, th, m, sizeof(struct ip));
    
    int _ret = ip_output(m, ifp);
    if (_ret != 0) {
//        print_string("  ERROR: ip_output failed!\n");
    }
    return _ret;
}

void tcp_input(struct mbuf* m, int off) {
    struct ip* ip = (struct ip*)(m->m_data - off);
    struct tcphdr* th = (struct tcphdr*)m->m_data;

    static int rx_pkt_count = 0;
    if ((th->th_flags & TH_RST) || (th->th_flags & TH_FIN)) {
        rx_pkt_count++;
        char buf[48];
        int n = 0;
        buf[n++] = 'T'; buf[n++] = 'C'; buf[n++] = 'P'; buf[n++] = ':'; buf[n++] = ' ';
        buf[n++] = 'r'; buf[n++] = 'x'; buf[n++] = ' ';
        if (th->th_flags & TH_RST) { buf[n++] = 'R'; buf[n++] = 'S'; buf[n++] = 'T'; }
        if (th->th_flags & TH_FIN) { buf[n++] = 'F'; buf[n++] = 'I'; buf[n++] = 'N'; }
        buf[n++] = '\n';
        buf[n] = 0;
        serial_puts(buf);
    }

    /* Find matching socket */
    struct socket* so = NULL;
    
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (sockets[i] && ntohs(sockets[i]->so_local.sin_port) == ntohs(th->th_dport)) {
            so = sockets[i];
            break;
        }
    }
    
    if (so == NULL) {
        char dbg[128];
        snprintf(dbg, sizeof(dbg),
            "  TCP: no socket for pkt#%u sport=%u dport=%u flags=0x%02X\n",
            rx_pkt_count, ntohs(th->th_sport), ntohs(th->th_dport), th->th_flags);
//        print_string(dbg);
        m_freem(m);
        return;
    }
    
    /* Simplified State Machine */
    if ((th->th_flags & TH_SYN) && (th->th_flags & TH_ACK)) {
        if (so->so_state == TCPS_SYN_SENT) {
            char buf[96];
            snprintf(buf, sizeof(buf),
                "  TCP: SYN-ACK recv seq=%u ack=%u\n",
                (unsigned)ntohl(th->th_seq), (unsigned)ntohl(th->th_ack));
            serial_puts(buf);
            so->so_ack = ntohl(th->th_seq) + 1;
            so->so_seq = ntohl(th->th_ack);
            so->so_state = TCPS_ESTABLISHED;
            tcp_send_packet(so, TH_ACK, NULL, 0);
            char buf2[96];
            snprintf(buf2, sizeof(buf2),
                "  TCP: Sent ACK seq=%u ack=%u\n",
                (unsigned)so->so_seq, (unsigned)so->so_ack);
//            serial_puts(buf2);
        } else if (so->so_state == TCPS_ESTABLISHED) {
            /* Retransmitted SYN-ACK: acknowledge it */
            tcp_send_packet(so, TH_ACK, NULL, 0);
        }
    }
    
    /* Handle data */
    int data_len = ntohs(ip->ip_len) - (ip->ip_hl << 2) - (th->th_off << 2);
    if (data_len > 0) {
        /* Update ACK */
        so->so_ack = ntohl(th->th_seq) + data_len;
        
        /* Copy to socket buffer */
        void* data = (char*)m->m_data + (th->th_off << 2);
        if (so->so_rcv_buf == NULL) {
            so->so_rcv_buf = kmalloc(65536);
            so->so_rcv_len = 0;
        }
        
        if (so->so_rcv_len + data_len <= 65536) {
            memcpy((char*)so->so_rcv_buf + so->so_rcv_len, data, data_len);
            so->so_rcv_len += data_len;
        }

        /* Send ACK for data */
        tcp_send_packet(so, TH_ACK, NULL, 0);
    }
    
    /* Handle FIN - mark socket as closed */
    if (th->th_flags & TH_FIN) {
        so->so_ack++;
        tcp_send_packet(so, TH_ACK, NULL, 0);
        so->so_closed = 1;
    }

    m_freem(m);
}

/* Socket API implementations */

int sys_socket(int domain, int type, int protocol) {
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (sockets[i] == NULL) {
            struct socket* so = kmalloc(sizeof(struct socket));
            memset(so, 0, sizeof(struct socket));
            so->so_type = type;
            so->so_rcv_buf = kmalloc(65536);
            so->so_rcv_len = 0;
            so->so_closed = 0;
            sockets[i] = so;
            return i;
        }
    }
    return -1;
}

int sys_connect(int s, const struct sockaddr* name, int namelen) {
    if (s < 0 || s >= MAX_SOCKETS || sockets[s] == NULL) {
        return -1;
    }
    struct socket* so = sockets[s];
    so->so_remote = *(const struct sockaddr_in*)name;
    so->so_state = TCPS_SYN_SENT;
    so->so_seq = 1000; /* Initial sequence number */
    so->so_ack = 0;
    so->so_local.sin_port = htons(20000 + s);

    /* Send SYN packet */
    if (tcp_send_packet(so, TH_SYN, NULL, 0) == 0) {
//        print_string("  TCP: SYN sent\n");
        so->so_seq++; /* SYN occupies 1 sequence number */
    }

    /* Wait for SYN-ACK with retransmit every 3s */
    int timeout = 500;
    int syn_retry = 8;
    while (so->so_state != TCPS_ESTABLISHED && timeout > 0) {
        extern void net_poll();
        net_poll();
        sleep_task(50);
        timeout--;
        if (timeout % 60 == 0 && syn_retry > 0) {
            syn_retry--;
            so->so_seq = 1000; /* re-use original seq on retransmit */
            tcp_send_packet(so, TH_SYN, NULL, 0);
        }
    }

    if (so->so_state == TCPS_ESTABLISHED) {
//        print_string("  TCP: Connected\n");
        return 0;
    }
//    print_string("  TCP: Connect timeout\n");
    return -1;
}

int sys_send(int s, const void* msg, int len, int flags) {
    if (s < 0 || s >= MAX_SOCKETS || sockets[s] == NULL) {
//        print_string("  TCP: sys_send invalid socket or null\n");
        return -1;
    }
    struct socket* so = sockets[s];

    /* Print first 16 bytes */
    int res = tcp_send_packet(so, TH_ACK | TH_PUSH, msg, len);
    if (res == 0) {
        so->so_seq += len;
        return len;
    }
    
    return -1;
}

int sys_recv(int s, void* buf, int len, int flags) {
    if (s < 0 || s >= MAX_SOCKETS || sockets[s] == NULL) return -1;
    struct socket* so = sockets[s];
    
    /* Wait for data with a timeout (approx 3 seconds for first data) */
    int timeout = 60; // 60 * 50ms = 3s
    while (so->so_rcv_len == 0 && timeout > 0 && !so->so_closed) {
        extern void net_poll();
        net_poll();
        sleep_task(50);
        timeout--;
    }
    
    if (so->so_rcv_len == 0) {
        return 0;
    }
    
    int to_copy = (len < so->so_rcv_len) ? len : so->so_rcv_len;
    memcpy(buf, so->so_rcv_buf, to_copy);
    
    if (to_copy < so->so_rcv_len) {
        memmove(so->so_rcv_buf, (char*)so->so_rcv_buf + to_copy, so->so_rcv_len - to_copy);
    }
    so->so_rcv_len -= to_copy;
    
    return to_copy;
}

int sys_socket_close(int s) {
    if (s < 0 || s >= MAX_SOCKETS || sockets[s] == NULL) return -1;
    
    struct socket* so = sockets[s];
    
    if (so->so_rcv_buf) kfree(so->so_rcv_buf);
    kfree(so);
    sockets[s] = NULL;
    return 0;
}

int sys_socket_closed(int s) {
    if (s < 0 || s >= MAX_SOCKETS || sockets[s] == NULL) return 1;
    return sockets[s]->so_closed;
}

int sys_ping(uint32_t ip_addr) {
    struct ifnet* ifp = if_list_head();
    while (ifp) {
        if ((ifp->if_flags & IFF_RUNNING) && ifp->if_ip != 0) break;
        ifp = ifp->if_next;
    }
    if (!ifp) return -1;

    struct mbuf* m = m_getcl(MT_DATA);
    if (!m) return -1;

    /* Reserve space for headers */
    m->m_data += 100;

    struct icmphdr* icp = (struct icmphdr*)m->m_data;
    memset(icp, 0, sizeof(struct icmphdr));
    icp->icmp_type = ICMP_ECHO;
    icp->icmp_code = 0;
    icp->icmp_id = htons(0x1234);
    icp->icmp_seq = htons(1);

    m->m_len = sizeof(struct icmphdr);
    /* Calculate ICMP checksum */
    extern uint16_t icmp_checksum(void* vdata, size_t length);
    icp->icmp_cksum = icmp_checksum(icp, m->m_len);

    /* Prepend IP header */
    m->m_data -= sizeof(struct ip);
    m->m_len += sizeof(struct ip);
    struct ip* ip = (struct ip*)m->m_data;
    memset(ip, 0, sizeof(struct ip));
    ip->ip_v = 4;
    ip->ip_hl = 5;
    ip->ip_p = IPPROTO_ICMP;
    ip->ip_len = htons(m->m_len);
    ip->ip_ttl = 64;
    ip->ip_src.s_addr = ifp->if_ip;
    ip->ip_dst.s_addr = ip_addr;

//    print_string("  Ping: Sending Echo Request to ");
    char buf[16];
    itoa(ntohl(ip_addr), buf);
//    print_string(buf);
//    print_string("\n");

    return ip_output(m, ifp);
}

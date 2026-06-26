#ifndef _KERNEL_NET_SOCKET_H
#define _KERNEL_NET_SOCKET_H

#include <stdint.h>
#include "in.h"
#include "if.h"

/*
 * Socket API definitions following FreeBSD's <sys/socket.h>
 */

struct sockaddr {
    uint8_t  sa_len;
    uint8_t  sa_family;
    char     sa_data[14];
};

#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3

struct socket {
    int      so_type;
    int      so_state; /* Uses TCPS_* states from tcp.h */
    struct   sockaddr_in so_local;
    struct   sockaddr_in so_remote;
    
    /* Buffer for received data */
    void*    so_rcv_buf;
    uint32_t so_rcv_len;
    
    /* TCP sequence numbers */
    uint32_t so_seq;
    uint32_t so_ack;
    int      so_closed; /* Set to 1 when FIN received */
};

/* State flags */
#define SS_ISCONNECTED  0x01
#define SS_ISCONNECTING 0x02

int sys_socket(int domain, int type, int protocol);
int sys_bind(int s, const struct sockaddr* name, int namelen);
int sys_connect(int s, const struct sockaddr* name, int namelen);
int sys_send(int s, const void* msg, int len, int flags);
int sys_recv(int s, void* buf, int len, int flags);
int sys_socket_close(int s);
int sys_socket_closed(int s);
int sys_ping(uint32_t ip);
int sys_dns_resolve(const char* hostname, uint32_t* ip_addr);

#endif /* _KERNEL_NET_SOCKET_H */

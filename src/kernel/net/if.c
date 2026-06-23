#include "kernel/net/if.h"
#include <string.h>

/*
 * FreeBSD-style interface management for BEDI OS.
 */

static struct ifnet* ifnet_list = NULL;

void if_attach(struct ifnet* ifp) {
    ifp->if_next = ifnet_list;
    ifnet_list = ifp;
}

void if_detach(struct ifnet* ifp) {
    struct ifnet** pp = &ifnet_list;
    while (*pp != NULL) {
        if (*pp == ifp) {
            *pp = ifp->if_next;
            return;
        }
        pp = &((*pp)->if_next);
    }
}

struct ifnet* if_find(const char* name) {
    struct ifnet* ifp = ifnet_list;
    while (ifp != NULL) {
        if (strcmp(ifp->if_xname, name) == 0) {
            return ifp;
        }
        ifp = ifp->if_next;
    }
    return NULL;
}

struct ifnet* if_list_head(void) {
    return ifnet_list;
}

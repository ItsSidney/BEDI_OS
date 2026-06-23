#ifndef VIRTIO_NET_H
#define VIRTIO_NET_H

#include "drivers/bus/pci.h"

void virtio_net_attach(pci_device_t* dev);
void virtio_net_poll(struct ifnet* ifp);

#endif

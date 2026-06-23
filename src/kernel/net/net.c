#include "kernel/net/if.h"
#include "kernel/net/in.h"
#include "drivers/bus/pci.h"
#include "drivers/video/framebuffer.h"
#include "string.h"

/*
 * Coordination for networking initialization in BEDI OS.
 */

extern void e1000_attach(pci_device_t* dev);
extern void e1000_poll(struct ifnet* ifp);

extern void loopback_init();

extern void boot_log_add(const char* tag, const char* msg, uint32_t tag_color, uint32_t msg_color);
extern void draw_boot_log(void);

void net_init() {
    extern void boot_log_add(const char* tag, const char* msg, uint32_t tag_color, uint32_t msg_color);
    extern void draw_boot_log(void);

    boot_log_add("NET", "Initializing FreeBSD-style stack...", 0x39D2C0, 0x8B949E);
    draw_boot_log();

    loopback_init();

    /* Scan PCI for supported network cards */
    int count = pci_get_device_count(); (void)count;

    for (int i = 0; i < count; i++) {
        pci_device_t* dev = pci_get_device(i);
        if (dev->class_id == 0x02) {
            if (dev->vendor_id == 0x8086 && (dev->device_id == 0x100E || dev->device_id == 0x100F || dev->device_id == 0x101A || dev->device_id == 0x1570 || dev->device_id == 0x15F3 || dev->device_id == 0x24F3)) {
                boot_log_add("NET", "Intel e1000 found, attaching...", 0x39D2C0, 0x3FB950);
                draw_boot_log();
                e1000_attach(dev);
                
                struct ifnet* ifp = if_find("em0");
                if (ifp) {
                    ifp->if_ip = htonl(0x0A00020F);
                    boot_log_add("NET", "em0 assigned IP 10.0.2.15", 0x39D2C0, 0x3FB950);
                    draw_boot_log();
                }
            } else {
                boot_log_add("NET", "Unsupported Network Controller", 0x39D2C0, 0xF0883E);
                draw_boot_log();
            }
        }
    }
}

void net_poll() {
    struct ifnet* ifp = if_list_head();
    while (ifp) {
        if (strcmp(ifp->if_xname, "em0") == 0) {
            e1000_poll(ifp);
        }
        ifp = ifp->if_next;
    }
}

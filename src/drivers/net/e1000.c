#include "kernel/net/if.h"
#include "kernel/net/mbuf.h"
#include "kernel/net/ethernet.h"
#include "drivers/bus/pci.h"
#include "kernel/mem/kheap.h"
#include "kernel/mem/vmm.h"
#include "kernel/task/task.h"
#include "drivers/video/framebuffer.h"
#include <string.h>
#include <stdint.h>

#define E1000_REG_CTRL      0x0000
#define E1000_REG_STATUS    0x0008
#define E1000_REG_EERD      0x0014
#define E1000_REG_ICR       0x00C0
#define E1000_REG_IMS       0x00D0
#define E1000_REG_RCTL      0x0100
#define E1000_REG_TCTL      0x0400
#define E1000_REG_RDBAL     0x2800
#define E1000_REG_RDBAH     0x2804
#define E1000_REG_RDLEN     0x2808
#define E1000_REG_RDH       0x2810
#define E1000_REG_RDT       0x2818
#define E1000_REG_TDBAL     0x3800
#define E1000_REG_TDBAH     0x3804
#define E1000_REG_TDLEN     0x3808
#define E1000_REG_TDH       0x3810
#define E1000_REG_TDT       0x3818
#define E1000_REG_TIPG      0x0410
#define E1000_REG_MTA       0x5200
#define E1000_REG_RAL       0x5400
#define E1000_REG_RAH       0x5404

#define RCTL_EN             (1 << 1)
#define RCTL_SBP            (1 << 2)
#define RCTL_UPE            (1 << 3)
#define RCTL_MPE            (1 << 4)
#define RCTL_LBM_NONE       (0 << 6)
#define RCTL_RDMTS_HALF     (0 << 8)
#define RCTL_BAM            (1 << 15)
#define RCTL_SZ_2048        (0 << 16)
#define RCTL_SECRC          (1 << 26)

#define TCTL_EN             (1 << 1)
#define TCTL_PSP            (1 << 3)
#define TCTL_CT             (0x10 << 4)
#define TCTL_COLD           (0x40 << 12)

#define TDESC_CMD_EOP       (1 << 0)
#define TDESC_CMD_IFCS      (1 << 1)
#define TDESC_CMD_RS        (1 << 3)
#define TDESC_STAT_DD       (1 << 0)

#define E1000_CTRL_RST      (1 << 26)
#define E1000_CTRL_SLU      (1 << 6)
#define E1000_CTRL_ASDE     (1 << 5)

#define E1000_STATUS_LU     (1 << 1)

struct e1000_rx_desc {
    uint64_t addr;
    uint16_t len;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed));

struct e1000_tx_desc {
    uint64_t addr;
    uint16_t len;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed));

#define RX_DESC_COUNT 128
#define TX_DESC_COUNT 128

struct e1000_softc {
    struct ifnet* ifp;
    pci_device_t* pci_dev;
    uint64_t      mmio_base;
    uint32_t      io_base;
    
    volatile struct e1000_rx_desc* rx_descs;
    volatile struct e1000_tx_desc* tx_descs;
    void*                 rx_buffers[RX_DESC_COUNT];
    void*                 tx_bounce_buffer;
    
    uint16_t rx_cur;
    uint16_t tx_cur;
};

extern uint64_t hhdm_offset;
extern uint64_t vmm_get_phys(uint64_t virt);

static inline void e1000_write(struct e1000_softc* sc, uint32_t reg, uint32_t val) {
    *(volatile uint32_t*)(sc->mmio_base + reg) = val;
}

static inline uint32_t e1000_read(struct e1000_softc* sc, uint32_t reg) {
    return *(volatile uint32_t*)(sc->mmio_base + reg);
}

extern void boot_log_add(const char* tag, const char* msg, uint32_t tag_color, uint32_t msg_color);
extern void draw_boot_log(void);

static int e1000_init(struct ifnet* ifp) {
    struct e1000_softc* sc = ifp->if_softc;

    boot_log_add("e1000", "Starting init...", 0xF0883E, 0x3FB950);
    draw_boot_log();

    // Disable interrupts
    e1000_write(sc, E1000_REG_IMS, 0);
    e1000_write(sc, E1000_REG_ICR, 0xFFFFFFFF);

    boot_log_add("e1000", "Reset device...", 0xF0883E, 0x3FB950);
    draw_boot_log();

    // Reset device - skip for I219/I225 as they may not support standard reset
    uint16_t dev_id = sc->pci_dev->device_id;
    if (dev_id != 0x1570 && dev_id != 0x15F3 && dev_id != 0x24F3) {
        e1000_write(sc, E1000_REG_CTRL, E1000_CTRL_RST);

        // Wait for reset to complete
        int timeout = 100000;
        while ((e1000_read(sc, E1000_REG_CTRL) & E1000_CTRL_RST) && timeout-- > 0) {
            __asm__("pause");
        }
        if (timeout <= 0) {
            boot_log_add("e1000", "Reset timeout, continuing...", 0xF0883E, 0xF0883E);
            draw_boot_log();
        } else {
            boot_log_add("e1000", "Reset done", 0xF0883E, 0x3FB950);
            draw_boot_log();
        }
    } else {
        boot_log_add("e1000", "Skipping reset for I219/I225", 0xF0883E, 0x3FB950);
        draw_boot_log();
    }

    // Re-enable PCI bus mastering after reset
    pci_enable_bus_mastering(sc->pci_dev);

    boot_log_add("e1000", "Bus mastering enabled", 0xF0883E, 0x3FB950);
    draw_boot_log();

    // Force Link Up immediately
    uint32_t ctrl = e1000_read(sc, E1000_REG_CTRL);
    e1000_write(sc, E1000_REG_CTRL, ctrl | E1000_CTRL_SLU | E1000_CTRL_ASDE);

    // Quick non-blocking link check
    // Use busy wait instead of sleep_task to avoid scheduler issues during early init
    for (volatile int i = 0; i < 5000000; i++) __asm__("pause");
    if (e1000_read(sc, E1000_REG_STATUS) & E1000_STATUS_LU) {
        boot_log_add("e1000", "Link UP", 0xF0883E, 0x3FB950);
        draw_boot_log();
    } else {
        boot_log_add("e1000", "Link unavailable (forced SLU)", 0xF0883E, 0xF0883E);
        draw_boot_log();
    }

    // Re-initialize MAC address registers
    uint32_t ral;
    uint32_t rah;
    memcpy(&ral, &ifp->if_hwaddr[0], 4);
    memcpy(&rah, &ifp->if_hwaddr[4], 2);
    e1000_write(sc, E1000_REG_RAL, ral);
    e1000_write(sc, E1000_REG_RAH, rah | 0x80000000);

    // Multi-cast table

    uint64_t rx_phys = vmm_get_phys((uint64_t)sc->rx_descs);
    e1000_write(sc, E1000_REG_RDBAL, rx_phys & 0xFFFFFFFF);
    e1000_write(sc, E1000_REG_RDBAH, rx_phys >> 32);
    e1000_write(sc, E1000_REG_RDLEN, RX_DESC_COUNT * sizeof(struct e1000_rx_desc));
    e1000_write(sc, E1000_REG_RDH, 0);
    e1000_write(sc, E1000_REG_RDT, RX_DESC_COUNT - 1);
    e1000_write(sc, E1000_REG_RCTL, RCTL_EN | RCTL_SBP | RCTL_UPE | RCTL_MPE | RCTL_BAM | RCTL_SZ_2048 | RCTL_SECRC);

    uint64_t tx_phys = vmm_get_phys((uint64_t)sc->tx_descs);

    e1000_write(sc, E1000_REG_TDBAL, tx_phys & 0xFFFFFFFF);
    e1000_write(sc, E1000_REG_TDBAH, tx_phys >> 32);
    e1000_write(sc, E1000_REG_TDLEN, TX_DESC_COUNT * sizeof(struct e1000_tx_desc));
    e1000_write(sc, E1000_REG_TDH, 0);
    e1000_write(sc, E1000_REG_TDT, 0);
    
    // TIPG: Transmit Inter-Packet Gap
    e1000_write(sc, E1000_REG_TIPG, (6 << 20) | (8 << 10) | 10);

    // TCTL: Enable | Pad Short Packets | Collision Threshold | Collision Distance
    e1000_write(sc, E1000_REG_TCTL, TCTL_EN | TCTL_PSP | (0x10 << 4) | (0x40 << 12));

    if (!(e1000_read(sc, E1000_REG_STATUS) & E1000_STATUS_LU)) {
        uint32_t ctrl2 = e1000_read(sc, E1000_REG_CTRL);
        e1000_write(sc, E1000_REG_CTRL, ctrl2 | E1000_CTRL_SLU);
    }

    ifp->if_flags |= IFF_RUNNING;
    return 0;
}

static int e1000_output(struct ifnet* ifp, struct mbuf* m) {
    struct e1000_softc* sc = ifp->if_softc;
    uint16_t cur = sc->tx_cur;
    
    /* Calculate total length of mbuf chain */
    int total_len = 0;
    struct mbuf* n;
    for (n = m; n != NULL; n = n->m_next) {
        total_len += n->m_len;
    }

    if (total_len > 2048) {
        m_freem(m);
        return -1;
    }
    m_copydata(m, 0, total_len, sc->tx_bounce_buffer);

    uint64_t virt = (uint64_t)sc->tx_bounce_buffer;
    uint64_t phys = vmm_get_phys(virt);
    
    if (phys == 0) {
        m_freem(m);
        return -1;
    }
    
    sc->tx_descs[cur].addr = phys;
    sc->tx_descs[cur].len = total_len;
    sc->tx_descs[cur].status = 0;
    sc->tx_descs[cur].cmd = TDESC_CMD_EOP | TDESC_CMD_IFCS | TDESC_CMD_RS;
    
    __asm__ volatile("" : : : "memory");

    sc->tx_cur = (cur + 1) % TX_DESC_COUNT;
    e1000_write(sc, E1000_REG_TDT, sc->tx_cur);
    
    int spin = 5000;
    int total_waits = 0;
    while (!(sc->tx_descs[cur].status & TDESC_STAT_DD)) {
        if (--spin > 0) {
            __asm__ volatile("pause" : : : "memory");
        } else {
            extern void yield(void);
            yield();
            spin = 1000;
            total_waits++;
        }
        if (total_waits > 50) {
            m_freem(m);
            return -1;
        }
    }
    
    m_freem(m);
    return 0;
}

void e1000_poll(struct ifnet* ifp) {
    struct e1000_softc* sc = ifp->if_softc;
    int max_packets = 32;

    while ((sc->rx_descs[sc->rx_cur].status & 0x1) && max_packets-- > 0) {
        uint16_t len = sc->rx_descs[sc->rx_cur].len;

        struct mbuf* m = m_getcl(MT_DATA);
        if (m) {
            memcpy(m->m_data, sc->rx_buffers[sc->rx_cur], len);
            m->m_len = len;
            m->m_pkthdr.len = len;
            m->m_pkthdr.rcvif = ifp;
            m->m_flags |= M_PKTHDR;
            ether_input(ifp, m);
        }
        
        sc->rx_descs[sc->rx_cur].status = 0;
        uint16_t old_cur = sc->rx_cur;
        sc->rx_cur = (sc->rx_cur + 1) % RX_DESC_COUNT;
        e1000_write(sc, E1000_REG_RDT, old_cur);
    }
}

void e1000_attach(pci_device_t* dev) {
    pci_enable_bus_mastering(dev);
    struct e1000_softc* sc = kmalloc(sizeof(struct e1000_softc));
    struct ifnet* ifp = kmalloc(sizeof(struct ifnet));
    memset(sc, 0, sizeof(struct e1000_softc));
    memset(ifp, 0, sizeof(struct ifnet));

    sc->pci_dev = dev;
    sc->mmio_base = dev->bar_64[0] + hhdm_offset;
    sc->rx_descs = kmalloc_aligned(RX_DESC_COUNT * sizeof(struct e1000_rx_desc), 4096);
    sc->tx_descs = kmalloc_aligned(TX_DESC_COUNT * sizeof(struct e1000_tx_desc), 4096);
    sc->tx_bounce_buffer = kmalloc_aligned(2048, 2048);
    
    for (int i = 0; i < RX_DESC_COUNT; i++) {
        sc->rx_buffers[i] = kmalloc_aligned(2048, 2048);
        sc->rx_descs[i].addr = vmm_get_phys((uint64_t)sc->rx_buffers[i]);
    }

    strcpy(ifp->if_xname, "em0");
    ifp->if_softc = sc;
    ifp->if_init = e1000_init;
    ifp->if_output = e1000_output;
    
    uint32_t ral = e1000_read(sc, E1000_REG_RAL);
    uint32_t rah = e1000_read(sc, E1000_REG_RAH);
    memcpy(&ifp->if_hwaddr[0], &ral, 4);
    memcpy(&ifp->if_hwaddr[4], &rah, 2);

    if_attach(ifp);
    e1000_init(ifp);
    boot_log_add("e1000", "em0 attached and initialized", 0xF0883E, 0x3FB950);
    draw_boot_log();
}

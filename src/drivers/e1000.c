#include "drivers/e1000.h"
#include "drivers/serial.h"
#include "arch/irq.h"
#include "arch/pic.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Static state for active E1000 adapter */
static uintptr_t          s_e1000_mmio = 0;
static uint8_t            s_mac[ETH_ALEN] = { 0 };
static net_stats_t        s_stats = { 0 };
static net_rx_callback_t  s_rx_cb = NULL;

/* Descriptor Rings (aligned and allocated from PMM frames) */
static e1000_rx_desc_t*   s_rx_descs = NULL;
static uint8_t*           s_rx_buffers[E1000_NUM_RX_DESC];
static uint16_t           s_rx_cur = 0;

static e1000_tx_desc_t*   s_tx_descs = NULL;
static uint8_t*           s_tx_buffers[E1000_NUM_TX_DESC];
static uint16_t           s_tx_cur = 0;

/*
 * e1000_write32:
 * Writes a 32-bit register in the E1000 MMIO space.
 */
static inline void e1000_write32(uint32_t reg, uint32_t val) {
    *(volatile uint32_t*)(s_e1000_mmio + reg) = val;
}

/*
 * e1000_read32:
 * Reads a 32-bit register from the E1000 MMIO space.
 */
static inline uint32_t e1000_read32(uint32_t reg) {
    return *(volatile uint32_t*)(s_e1000_mmio + reg);
}

/*
 * e1000_read_eeprom:
 * Reads a 16-bit word from the E1000 EEPROM via the EERD register.
 */
static uint16_t e1000_read_eeprom(uint8_t addr) {
    /* Bit 0: START = 1, Bits 8..15: Address */
    uint32_t req = 1 | ((uint32_t)addr << 8);
    e1000_write32(E1000_REG_EERD, req);

    uint32_t val = 0;
    /* Poll until Bit 4 (DONE) is set by hardware */
    for (int i = 0; i < 20000; i++) {
        val = e1000_read32(E1000_REG_EERD);
        if (val & (1 << 4)) {
            break;
        }
    }
    return (uint16_t)(val >> 16);
}

/*
 * e1000_read_mac:
 * Extracts the 6-byte Ethernet MAC address from EEPROM or RAL/RAH registers.
 */
static void e1000_read_mac(void) {
    uint16_t w0 = e1000_read_eeprom(0);
    uint16_t w1 = e1000_read_eeprom(1);
    uint16_t w2 = e1000_read_eeprom(2);

    /* Check if EEPROM returned a valid MAC */
    if (w0 != 0 && w0 != 0xFFFF) {
        s_mac[0] = (uint8_t)(w0 & 0xFF);
        s_mac[1] = (uint8_t)((w0 >> 8) & 0xFF);
        s_mac[2] = (uint8_t)(w1 & 0xFF);
        s_mac[3] = (uint8_t)((w1 >> 8) & 0xFF);
        s_mac[4] = (uint8_t)(w2 & 0xFF);
        s_mac[5] = (uint8_t)((w2 >> 8) & 0xFF);
    } else {
        /* Fallback: read directly from Receive Address Low / High registers */
        uint32_t ral = e1000_read32(E1000_REG_RAL);
        uint32_t rah = e1000_read32(E1000_REG_RAH);

        s_mac[0] = (uint8_t)(ral & 0xFF);
        s_mac[1] = (uint8_t)((ral >> 8) & 0xFF);
        s_mac[2] = (uint8_t)((ral >> 16) & 0xFF);
        s_mac[3] = (uint8_t)((ral >> 24) & 0xFF);
        s_mac[4] = (uint8_t)(rah & 0xFF);
        s_mac[5] = (uint8_t)((rah >> 8) & 0xFF);

        /* Ensure Receive Address Valid (AV) bit 31 is set in RAH */
        e1000_write32(E1000_REG_RAH, rah | (1 << 31));
    }
}

/*
 * e1000_poll_rx:
 * Checks descriptor ring for completed incoming packets and forwards to callback.
 */
static void e1000_poll_rx(void) {
    while (s_rx_descs[s_rx_cur].status & 0x01) { /* DD: Descriptor Done */
        uint16_t len = s_rx_descs[s_rx_cur].length;
        uint8_t* buf = s_rx_buffers[s_rx_cur];

        if (s_rx_cb && len > 0) {
            s_rx_cb(buf, len);
        }

        s_stats.packets_rx++;
        s_stats.bytes_rx += len;

        /* Reset status and advance ring */
        s_rx_descs[s_rx_cur].status = 0;
        uint16_t old_cur = s_rx_cur;
        s_rx_cur = (s_rx_cur + 1) % E1000_NUM_RX_DESC;

        /* Advance Receive Tail pointer so hardware can reuse this descriptor */
        e1000_write32(E1000_REG_RDT, old_cur);
    }
}

/*
 * e1000_irq_handler:
 * Hardware interrupt service routine for E1000 NIC.
 */
static void e1000_irq_handler(registers_t* regs) {
    (void)regs;
    /* Reading ICR acknowledges and clears pending interrupt lines on E1000 */
    uint32_t icr = e1000_read32(E1000_REG_ICR);
    if (icr == 0) {
        return; /* Spurious or shared IRQ not intended for this device */
    }

    /* Bit 2: Link Status Change (LSC) */
    if (icr & 0x04) {
        uint32_t status = e1000_read32(E1000_REG_STATUS);
        s_stats.is_link_up = (status & 0x02) != 0;
        kprintf("[E1000] Link Status Changed: %s\n", s_stats.is_link_up ? "LINK UP" : "LINK DOWN");
    }

    /* Bit 7: Receiver Timer Interrupt (packet arrived) or Bit 6: Receiver Overrun */
    if (icr & (0x80 | 0x40 | 0x10)) {
        e1000_poll_rx();
    }
}

/*
 * e1000_init:
 * Initializes Intel 82540EM Gigabit NIC.
 */
bool e1000_init(const pci_device_t* dev) {
    if (!dev || dev->bar_type[0] != PCI_BAR_TYPE_MMIO) {
        serial_writestring("[E1000] Error: Invalid device or BAR0 is not MMIO!\n");
        return false;
    }

    serial_writestring("[E1000] Initializing Intel Gigabit NIC (82540EM)...\n");

    /* Step 1: Enable PCI Bus Mastering and Memory Access */
    pci_enable_bus_master(dev);

    /* Step 2: Map 128 KB MMIO space into page directory */
    uintptr_t bar0_phys = dev->bar[0];
    s_e1000_mmio = bar0_phys;

    /* 128 KB = 32 pages of 4096 bytes */
    for (uint32_t i = 0; i < 32; i++) {
        uintptr_t p = bar0_phys + (i * PAGE_SIZE_4K);
        paging_map_page(p, p, PAGE_WRITE | PAGE_PCD);
    }

    /* Step 3: Software Reset and Link Up */
    e1000_write32(E1000_REG_CTRL, e1000_read32(E1000_REG_CTRL) | (1 << 26)); /* RST */
    for (volatile int i = 0; i < 50000; i++) { } /* Settle delay */

    /* Set SLU (Set Link Up, Bit 6) and clear Device Reset */
    e1000_write32(E1000_REG_CTRL, (1 << 6));

    /* Disable and clear all pending interrupts */
    e1000_write32(E1000_REG_IMC, 0xFFFFFFFF);
    e1000_read32(E1000_REG_ICR);

    /* Step 4: Retrieve MAC address */
    e1000_read_mac();
    kprintf("[E1000] Hardware MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
            s_mac[0], s_mac[1], s_mac[2], s_mac[3], s_mac[4], s_mac[5]);

    /* Step 5: Clear Multicast Table Array (MTA) */
    for (int i = 0; i < 128; i++) {
        e1000_write32(E1000_REG_MTA + (i * 4), 0);
    }

    /* Step 6: Allocate and Configure Receive (RX) Descriptor Ring */
    uintptr_t rx_ring_frame = pmm_alloc_frame();
    if (!rx_ring_frame) {
        serial_writestring("[E1000] Error: Failed to allocate RX ring frame!\n");
        return false;
    }
    memset((void*)rx_ring_frame, 0, PAGE_SIZE_4K);
    s_rx_descs = (e1000_rx_desc_t*)rx_ring_frame;

    /* Allocate packet buffers (2 buffers per 4 KB frame) */
    for (int i = 0; i < E1000_NUM_RX_DESC; i += 2) {
        uintptr_t buf_frame = pmm_alloc_frame();
        if (!buf_frame) {
            serial_writestring("[E1000] Error: Failed to allocate packet buffer frame!\n");
            return false;
        }
        s_rx_buffers[i] = (uint8_t*)buf_frame;
        s_rx_descs[i].buffer_addr = (uint64_t)buf_frame;
        s_rx_descs[i].status = 0;

        s_rx_buffers[i + 1] = (uint8_t*)(buf_frame + ETH_MAX_BUF);
        s_rx_descs[i + 1].buffer_addr = (uint64_t)(buf_frame + ETH_MAX_BUF);
        s_rx_descs[i + 1].status = 0;
    }
    s_rx_cur = 0;

    /* Setup RX registers */
    e1000_write32(E1000_REG_RDBAL, (uint32_t)rx_ring_frame);
    e1000_write32(E1000_REG_RDBAH, 0);
    e1000_write32(E1000_REG_RDLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    e1000_write32(E1000_REG_RDH, 0);
    e1000_write32(E1000_REG_RDT, E1000_NUM_RX_DESC - 1);

    /*
     * Configure RCTL:
     * - Bit 1: EN (Receiver Enable)
     * - Bit 2: SBP (Store Bad Packets)
     * - Bit 3: UPE (Unicast Promiscuous)
     * - Bit 4: MPE (Multicast Promiscuous)
     * - Bit 15: BAM (Broadcast Accept Mode)
     * - Bits 17:16: BSIZE = 2048 bytes (0b00)
     * - Bit 26: SECRC (Strip Ethernet CRC)
     */
    e1000_write32(E1000_REG_RCTL, (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 15) | (1 << 26));

    /* Step 7: Allocate and Configure Transmit (TX) Descriptor Ring */
    uintptr_t tx_ring_frame = pmm_alloc_frame();
    if (!tx_ring_frame) {
        serial_writestring("[E1000] Error: Failed to allocate TX ring frame!\n");
        return false;
    }
    memset((void*)tx_ring_frame, 0, PAGE_SIZE_4K);
    s_tx_descs = (e1000_tx_desc_t*)tx_ring_frame;

    for (int i = 0; i < E1000_NUM_TX_DESC; i += 2) {
        uintptr_t buf_frame = pmm_alloc_frame();
        s_tx_buffers[i] = (uint8_t*)buf_frame;
        s_tx_buffers[i + 1] = (uint8_t*)(buf_frame + ETH_MAX_BUF);
    }
    s_tx_cur = 0;

    /* Setup TX registers */
    e1000_write32(E1000_REG_TDBAL, (uint32_t)tx_ring_frame);
    e1000_write32(E1000_REG_TDBAH, 0);
    e1000_write32(E1000_REG_TDLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    e1000_write32(E1000_REG_TDH, 0);
    e1000_write32(E1000_REG_TDT, 0);

    /* Recommended Inter-Packet Gap (TIPG): IPGT=10, IPGR1=8, IPGR2=6 */
    e1000_write32(E1000_REG_TIPG, 0x0060200A);

    /*
     * Configure TCTL:
     * - Bit 1: EN (Transmitter Enable)
     * - Bit 3: PSP (Pad Short Packets to 64 bytes)
     * - Bits 11:4: CT (Collision Threshold = 15)
     * - Bits 21:12: COLD (Collision Distance = 64)
     */
    e1000_write32(E1000_REG_TCTL, (1 << 1) | (1 << 3) | (0x0F << 4) | (0x40 << 12));

    /* Step 8: Register IRQ handler & unmask PIC */
    if (dev->irq_line < 16) {
        irq_register_handler(dev->irq_line, e1000_irq_handler);
        pic_unmask_irq(dev->irq_line);
        kprintf("[E1000] Registered IRQ %u\n", dev->irq_line);
    }

    /* Enable interrupts: RXT0 (Bit 7), TXDW (Bit 0), LSC (Bit 2), RXO (Bit 6) */
    e1000_write32(E1000_REG_IMS, 0x80 | 0x01 | 0x04 | 0x40);

    /* Check link status */
    uint32_t status = e1000_read32(E1000_REG_STATUS);
    s_stats.is_link_up = (status & 0x02) != 0;
    kprintf("[E1000] Initial link state: %s (STATUS: 0x%08x)\n",
            s_stats.is_link_up ? "LINK UP (Connected)" : "LINK DOWN", status);

    return true;
}

/*
 * e1000_send_packet:
 * Sends an Ethernet packet onto the wire using the TX descriptor ring.
 */
bool e1000_send_packet(const void* data, size_t len) {
    if (!data || len == 0 || len > ETH_FRAME_LEN) {
        return false;
    }

    uint16_t cur = s_tx_cur;

    /* Copy packet data to transmit buffer */
    memcpy(s_tx_buffers[cur], data, len);

    s_tx_descs[cur].buffer_addr = (uint64_t)(uintptr_t)s_tx_buffers[cur];
    s_tx_descs[cur].length      = (uint16_t)len;
    /* Command flags: EOP (Bit 0) | IFCS (Bit 1) | RS (Bit 3) */
    s_tx_descs[cur].cmd         = (1 << 0) | (1 << 1) | (1 << 3);
    s_tx_descs[cur].status      = 0;

    /* Advance ring pointer and notify hardware */
    s_tx_cur = (s_tx_cur + 1) % E1000_NUM_TX_DESC;
    e1000_write32(E1000_REG_TDT, s_tx_cur);

    s_stats.packets_tx++;
    s_stats.bytes_tx += len;

    return true;
}

/*
 * e1000_register_rx_callback:
 * Registers packet receiver callback.
 */
void e1000_register_rx_callback(net_rx_callback_t cb) {
    s_rx_cb = cb;
}

/*
 * e1000_get_mac:
 * Returns the hardware MAC address.
 */
const uint8_t* e1000_get_mac(void) {
    return s_mac;
}

/*
 * e1000_get_stats:
 * Returns network statistics for E1000 interface.
 */
const net_stats_t* e1000_get_stats(void) {
    return &s_stats;
}

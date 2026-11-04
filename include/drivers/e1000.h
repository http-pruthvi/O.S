#ifndef DRIVERS_E1000_H
#define DRIVERS_E1000_H

#include "lib/types.h"
#include "drivers/pci.h"
#include "drivers/net.h"

/*
 * e1000.h:
 * Intel 82540EM Gigabit Ethernet NIC Driver.
 *
 * Supported PCI IDs:
 * - Vendor 0x8086 (Intel Corporation)
 * - Device 0x100E (82540EM - Standard VirtualBox / QEMU Gigabit NIC)
 * - Device 0x100F (82545EM)
 * - Device 0x1004 (82543GC)
 */

/* E1000 Register Offsets */
#define E1000_REG_CTRL      0x0000 /* Device Control */
#define E1000_REG_STATUS    0x0008 /* Device Status */
#define E1000_REG_EECD      0x0010 /* EEPROM/Flash Control */
#define E1000_REG_EERD      0x0014 /* EEPROM Read */
#define E1000_REG_CTRL_EXT  0x0018 /* Extended Device Control */
#define E1000_REG_ICR       0x00C0 /* Interrupt Cause Read */
#define E1000_REG_ITR       0x00C4 /* Interrupt Throttling Rate */
#define E1000_REG_ICS       0x00C8 /* Interrupt Cause Set */
#define E1000_REG_IMS       0x00D0 /* Interrupt Mask Set */
#define E1000_REG_IMC       0x00D8 /* Interrupt Mask Clear */
#define E1000_REG_RCTL      0x0100 /* Receive Control */
#define E1000_REG_TCTL      0x0400 /* Transmit Control */
#define E1000_REG_TIPG      0x0410 /* Transmit Inter-Packet Gap */
#define E1000_REG_RDBAL     0x2800 /* RX Descriptor Base Address Low */
#define E1000_REG_RDBAH     0x2804 /* RX Descriptor Base Address High */
#define E1000_REG_RDLEN     0x2808 /* RX Descriptor Length */
#define E1000_REG_RDH       0x2810 /* RX Descriptor Head */
#define E1000_REG_RDT       0x2818 /* RX Descriptor Tail */
#define E1000_REG_TDBAL     0x3800 /* TX Descriptor Base Address Low */
#define E1000_REG_TDBAH     0x3804 /* TX Descriptor Base Address High */
#define E1000_REG_TDLEN     0x3808 /* TX Descriptor Length */
#define E1000_REG_TDH       0x3810 /* TX Descriptor Head */
#define E1000_REG_TDT       0x3818 /* TX Descriptor Tail */
#define E1000_REG_MTA       0x5200 /* Multicast Table Array (128 dwords) */
#define E1000_REG_RAL       0x5400 /* Receive Address Low */
#define E1000_REG_RAH       0x5404 /* Receive Address High */

/* Descriptor Ring Sizes */
#define E1000_NUM_RX_DESC   32
#define E1000_NUM_TX_DESC   16

/* E1000 Receive Descriptor (Legacy format: 16 bytes) */
typedef struct {
    uint64_t buffer_addr; /* Physical address of packet buffer */
    uint16_t length;      /* Packet length received */
    uint16_t checksum;    /* Hardware packet checksum */
    uint8_t  status;      /* Status flags (bit 0 = Descriptor Done) */
    uint8_t  errors;      /* Error flags */
    uint16_t special;
} __attribute__((packed)) e1000_rx_desc_t;

/* E1000 Transmit Descriptor (Legacy format: 16 bytes) */
typedef struct {
    uint64_t buffer_addr; /* Physical address of data to transmit */
    uint16_t length;      /* Data length in bytes */
    uint8_t  cso;         /* Checksum offset */
    uint8_t  cmd;         /* Command flags (EOP, IFCS, RS) */
    uint8_t  status;      /* Status flags (bit 0 = Descriptor Done) */
    uint8_t  css;         /* Checksum start */
    uint16_t special;
} __attribute__((packed)) e1000_tx_desc_t;

/* Driver Interface */
bool                e1000_init(const pci_device_t* dev);
bool                e1000_send_packet(const void* data, size_t len);
void                e1000_register_rx_callback(net_rx_callback_t cb);
const uint8_t*      e1000_get_mac(void);
const net_stats_t*  e1000_get_stats(void);

#endif /* DRIVERS_E1000_H */

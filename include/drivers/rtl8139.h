#ifndef DRIVERS_RTL8139_H
#define DRIVERS_RTL8139_H

#include "lib/types.h"
#include "drivers/pci.h"
#include "drivers/net.h"

/*
 * rtl8139.h:
 * Realtek RTL8139 Fast Ethernet PCI NIC Driver.
 *
 * Supported PCI IDs:
 * - Vendor 0x10EC (Realtek Semiconductor Corp.)
 * - Device 0x8139 (RTL-8139/8139C/8139D)
 */

/* RTL8139 Register Offsets */
#define RTL_REG_MAC0        0x00 /* MAC Address Byte 0 */
#define RTL_REG_MAR0        0x08 /* Multicast Filter */
#define RTL_REG_TSD0        0x10 /* Transmit Status of Descriptor 0 */
#define RTL_REG_TSAD0       0x20 /* Transmit Start Address of Descriptor 0 */
#define RTL_REG_RBSTART     0x30 /* Receive Buffer Start Address */
#define RTL_REG_CMD         0x37 /* Command Register */
#define RTL_REG_CAPR        0x38 /* Current Address of Packet Read */
#define RTL_REG_CBR         0x3A /* Current Buffer Address */
#define RTL_REG_IMR         0x3C /* Interrupt Mask Register */
#define RTL_REG_ISR         0x3E /* Interrupt Status Register */
#define RTL_REG_TCR         0x40 /* Transmit Configuration Register */
#define RTL_REG_RCR         0x44 /* Receive Configuration Register */
#define RTL_REG_CONFIG1     0x52 /* Configuration Register 1 */

/* Driver Interface */
bool                rtl8139_init(const pci_device_t* dev);
bool                rtl8139_send_packet(const void* data, size_t len);
void                rtl8139_register_rx_callback(net_rx_callback_t cb);
const uint8_t*      rtl8139_get_mac(void);
const net_stats_t*  rtl8139_get_stats(void);

#endif /* DRIVERS_RTL8139_H */

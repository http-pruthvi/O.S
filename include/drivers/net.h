#ifndef DRIVERS_NET_H
#define DRIVERS_NET_H

#include "lib/types.h"

/*
 * net.h:
 * Hardware Network Interface Abstraction Layer.
 *
 * Why this is needed:
 * Decouples the physical network card driver (Intel E1000, Realtek RTL8139)
 * from the upper TCP/IP protocol stack (ARP, IPv4, ICMP, UDP, TCP).
 * Upper layers interact exclusively through uniform packet transmit and receive
 * callback hooks.
 */

#define ETH_ALEN       6     /* Hardware address length (6 bytes for Ethernet MAC) */
#define ETH_HLEN       14    /* Ethernet header length (Dest 6 + Src 6 + Type 2) */
#define ETH_FRAME_LEN  1514  /* Maximum Ethernet payload + header without FCS */
#define ETH_MAX_BUF    2048  /* Standard buffer allocation size per frame */

/* Network link and packet throughput statistics */
typedef struct {
    bool     is_link_up;
    uint32_t packets_tx;
    uint32_t packets_rx;
    uint32_t bytes_tx;
    uint32_t bytes_rx;
} net_stats_t;

/* Receiver callback function pointer signature */
typedef void (*net_rx_callback_t)(const void* data, size_t len);

/* Subsystem Initialization */
void net_driver_init(void);

/* Transmit an Ethernet frame to the physical network */
bool net_send_packet(const void* data, size_t len);

/* Register callback invoked whenever a valid Ethernet frame arrives */
void net_register_receive_callback(void (*rx_cb)(const void* data, size_t len));

/* Accessor for 6-byte hardware MAC address */
const uint8_t* net_get_mac(void);

/* Returns true if an Ethernet NIC was successfully detected and configured */
bool net_is_active(void);

/* Returns descriptive model string for active NIC */
const char* net_get_device_name(void);

/* Accessor for cumulative network interface statistics */
const net_stats_t* net_get_stats(void);

#endif /* DRIVERS_NET_H */

#ifndef NET_ETHERNET_H
#define NET_ETHERNET_H

#include "lib/types.h"
#include "net/netdev.h"

/*
 * ethernet.h:
 * IEEE 802.3 Ethernet II Frame protocol definition and handling.
 *
 * Why this is needed:
 * Ethernet II is the standard link-layer frame format for modern computer networks.
 * It encapsulates higher-level network protocols (ARP and IPv4) with 6-byte source
 * and destination MAC addresses and a 2-byte EtherType discriminator.
 */

#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_ARP  0x0806

#define ETH_MIN_LEN    60
#define ETH_MAX_LEN    1514
#define ETH_HDR_LEN    14

/* Standard 14-byte Ethernet II frame header */
typedef struct __attribute__((packed)) {
    uint8_t  dest_mac[ETH_ALEN];
    uint8_t  src_mac[ETH_ALEN];
    uint16_t ethertype; /* Network byte order (big-endian) */
} eth_header_t;

/* Global broadcast MAC address (FF:FF:FF:FF:FF:FF) */
extern const uint8_t ETH_BROADCAST_MAC[ETH_ALEN];

/* Initializes Ethernet subsystem */
void ethernet_init(void);

/* Formats a 6-byte MAC address into hexadecimal string (e.g. "08:00:27:4d:5d:23") */
void mac_to_str(const uint8_t* mac, char* buf);

/* Transmits an Ethernet II frame to target MAC with given ethertype */
int ethernet_send(const uint8_t* dest_mac, uint16_t ethertype, const void* payload, size_t len);

/* Handles an incoming Ethernet II frame and dispatches to ARP or IPv4 handler */
void ethernet_handle_packet(const void* packet, size_t len);

#endif /* NET_ETHERNET_H */

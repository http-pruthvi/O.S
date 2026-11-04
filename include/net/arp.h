#ifndef NET_ARP_H
#define NET_ARP_H

#include "lib/types.h"
#include "net/netdev.h"

/*
 * arp.h:
 * Address Resolution Protocol (ARP - RFC 826).
 *
 * Why this is needed:
 * Translates 32-bit IPv4 addresses into 48-bit physical Ethernet MAC addresses.
 * Maintains an in-memory cache of resolved IP-to-MAC mappings, responds to ARP
 * requests directed at our IP, and broadcasts ARP requests when looking up unknown peers.
 */

#define ARP_HTYPE_ETHERNET 1
#define ARP_PTYPE_IPV4     0x0800
#define ARP_HLEN_ETHERNET  6
#define ARP_PLEN_IPV4      4
#define ARP_OP_REQUEST     1
#define ARP_OP_REPLY       2

#define ARP_CACHE_CAPACITY 16

/* Standard 28-byte ARP packet for Ethernet and IPv4 */
typedef struct __attribute__((packed)) {
    uint16_t htype;             /* Hardware type (1 = Ethernet) */
    uint16_t ptype;             /* Protocol type (0x0800 = IPv4) */
    uint8_t  hlen;              /* Hardware address length (6 bytes) */
    uint8_t  plen;              /* Protocol address length (4 bytes) */
    uint16_t opcode;            /* ARP Opcode (1=Request, 2=Reply) */
    uint8_t  sender_mac[ETH_ALEN];
    uint32_t sender_ip;         /* Network byte order */
    uint8_t  target_mac[ETH_ALEN];
    uint32_t target_ip;         /* Network byte order */
} arp_packet_t;

/* Entry in kernel ARP cache table */
typedef struct {
    uint32_t ip;                /* Target IP (network byte order) */
    uint8_t  mac[ETH_ALEN];     /* Resolved MAC address */
    uint32_t timestamp;         /* System uptime tick when cached */
    bool     valid;             /* Slot occupancy flag */
} arp_entry_t;

/* Initializes ARP cache table and static mappings */
void arp_init(void);

/* Processes an incoming ARP packet (handles request and reply) */
void arp_handle_packet(const void* data, size_t len);

/* Looks up an IP in the ARP cache; broadcasts ARP request if missing */
bool arp_lookup(uint32_t ip, uint8_t* out_mac);

/* Broadcasts an ARP request for the specified IP */
void arp_request(uint32_t ip);

/* Manually registers an IP-to-MAC mapping into the ARP cache table */
void arp_insert(uint32_t ip, const uint8_t* mac);

#endif /* NET_ARP_H */

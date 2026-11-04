#ifndef NET_NETDEV_H
#define NET_NETDEV_H

#include "lib/types.h"
#include "drivers/net.h"

/*
 * netdev.h:
 * Network device abstraction and global network stack initialization.
 *
 * Why this is needed:
 * Provides a standardized hardware-independent interface for network adapters
 * (VirtualBox NAT Am79C973, RTL8139, loopback, or virtual responders). It holds
 * interface configuration (MAC, IP, Netmask, Gateway, DNS) and TX/RX counters.
 */

#define NET_IFNAME_MAX 16

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

/* Helper macros for IPv4 address manipulation in network byte order */
#define IP_ADDR(a, b, c, d) \
    ((uint32_t)(((uint8_t)(a)) | \
               (((uint8_t)(b)) << 8) | \
               (((uint8_t)(c)) << 16) | \
               (((uint8_t)(d)) << 24)))

#define IP_OCTET_1(ip) ((uint8_t)((ip) & 0xFF))
#define IP_OCTET_2(ip) ((uint8_t)(((ip) >> 8) & 0xFF))
#define IP_OCTET_3(ip) ((uint8_t)(((ip) >> 16) & 0xFF))
#define IP_OCTET_4(ip) ((uint8_t)(((ip) >> 24) & 0xFF))

/* Endianness byte-swap helpers for 16-bit and 32-bit integers */
static inline uint16_t htons(uint16_t v) {
    return (uint16_t)(((v & 0xFF) << 8) | ((v >> 8) & 0xFF));
}

static inline uint16_t ntohs(uint16_t v) {
    return htons(v);
}

static inline uint32_t htonl(uint32_t v) {
    return ((v & 0xFF) << 24) |
           ((v & 0xFF00) << 8) |
           ((v & 0xFF0000) >> 8) |
           ((v >> 24) & 0xFF);
}

static inline uint32_t ntohl(uint32_t v) {
    return htonl(v);
}

/* Network device structure */
typedef struct netdev {
    char     name[NET_IFNAME_MAX];
    uint8_t  mac[ETH_ALEN];
    uint32_t ip;          /* Network byte order */
    uint32_t netmask;     /* Network byte order */
    uint32_t gateway;     /* Network byte order */
    uint32_t dns;         /* Network byte order */
    uint32_t tx_packets;
    uint32_t rx_packets;
    uint32_t tx_bytes;
    uint32_t rx_bytes;
    bool     link_up;
    int    (*send)(const void* data, size_t len);
} netdev_t;

/* Initializes the TCP/IP network subsystem and default network interface */
void net_init(void);

/* Returns pointer to the primary default network device */
netdev_t* netdev_get_default(void);

/* Ingests a received raw Ethernet frame into the TCP/IP stack */
void net_receive_packet(const void* data, size_t len);

/* Periodic polling hook for network packet processing */
void net_poll(void);

/* Virtual NAT responder simulation hook */
void netdev_simulate_response(const void* frame, size_t len);

#endif /* NET_NETDEV_H */

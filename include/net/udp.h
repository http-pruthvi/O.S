#ifndef NET_UDP_H
#define NET_UDP_H

#include "lib/types.h"
#include "net/netdev.h"

/*
 * udp.h:
 * User Datagram Protocol (UDP - RFC 768).
 *
 * Why this is needed:
 * Connectionless, lightweight transport protocol used for low-latency queries
 * such as DNS (Domain Name System). Supports port multiplexing and dispatches
 * incoming datagrams to registered port callbacks.
 */

#define UDP_MAX_SOCKETS 16

/* Standard 8-byte UDP header */
typedef struct __attribute__((packed)) {
    uint16_t src_port;       /* Source port (network byte order) */
    uint16_t dest_port;      /* Destination port (network byte order) */
    uint16_t length;         /* UDP header + payload length in bytes */
    uint16_t checksum;       /* Optional checksum (0 if omitted) */
} udp_header_t;

/* Callback function type for registered UDP port listeners */
typedef void (*udp_handler_t)(uint32_t src_ip, uint16_t src_port, const void* data, size_t len);

/* Initializes UDP subsystem */
void udp_init(void);

/* Registers a callback listener for a specific local UDP port */
int udp_bind(uint16_t port, udp_handler_t handler);

/* Unregisters a listener from a local UDP port */
void udp_unbind(uint16_t port);

/* Sends a UDP datagram to destination IP and port */
int udp_send(uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, const void* payload, size_t len);

/* Handles an incoming UDP packet and routes it to bound port listener */
void udp_handle_packet(uint32_t src_ip, const void* data, size_t len);

#endif /* NET_UDP_H */

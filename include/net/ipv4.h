#ifndef NET_IPV4_H
#define NET_IPV4_H

#include "lib/types.h"
#include "net/netdev.h"

/*
 * ipv4.h:
 * Internet Protocol version 4 (IPv4 - RFC 791).
 *
 * Why this is needed:
 * IPv4 provides host-to-host datagram delivery across packet networks. It encapsulates
 * transport-layer protocols (ICMP, TCP, UDP), performs header checksum calculation,
 * and routes packets through the local subnet or default gateway.
 */

#define IPV4_PROTO_ICMP 1
#define IPV4_PROTO_TCP  6
#define IPV4_PROTO_UDP  17

#define IPV4_DEFAULT_TTL 64
#define IPV4_IHL_MIN     5
#define IPV4_VERSION     4

/* Standard 20-byte IPv4 packet header */
typedef struct __attribute__((packed)) {
    uint8_t  ihl_version;    /* Version (4 bits) + Internet Header Length (4 bits) */
    uint8_t  tos;            /* Type of Service / DSCP */
    uint16_t total_length;   /* Entire packet size in bytes (header + payload) */
    uint16_t id;             /* Datagram identification number */
    uint16_t flags_fragment; /* 3-bit flags + 13-bit fragment offset */
    uint8_t  ttl;            /* Time to Live */
    uint8_t  protocol;       /* Transport layer protocol (ICMP, TCP, UDP) */
    uint16_t checksum;       /* 16-bit one's complement header checksum */
    uint32_t src_ip;         /* Source IPv4 Address (network byte order) */
    uint32_t dest_ip;        /* Destination IPv4 Address (network byte order) */
} ipv4_header_t;

/* Initializes IPv4 subsystem */
void ipv4_init(void);

/* Calculates standard 16-bit one's complement Internet checksum */
uint16_t ipv4_checksum(const void* data, size_t len);

/* Sends an IPv4 packet to destination IP, routing through gateway if external */
int ipv4_send(uint32_t dest_ip, uint8_t protocol, const void* payload, size_t len);

/* Handles incoming IPv4 packet, validates header, and dispatches to protocol handler */
void ipv4_handle_packet(const void* data, size_t len);

/* Converts 32-bit IPv4 address into dotted-decimal string (e.g. "10.0.2.15") */
void ip_to_str(uint32_t ip, char* buf);

/* Parses dotted-decimal string into 32-bit IPv4 address (network byte order) */
uint32_t str_to_ip(const char* str);

#endif /* NET_IPV4_H */

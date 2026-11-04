#ifndef NET_ICMP_H
#define NET_ICMP_H

#include "lib/types.h"
#include "net/netdev.h"

/*
 * icmp.h:
 * Internet Control Message Protocol (ICMP - RFC 792).
 *
 * Why this is needed:
 * Used for network diagnostics, error reporting, and ping reachability checks.
 * Handles incoming Echo Requests by automatically responding with Echo Replies,
 * and provides icmp_ping() to test round-trip time (RTT) to remote hosts.
 */

#define ICMP_TYPE_ECHO_REPLY   0
#define ICMP_TYPE_ECHO_REQUEST 8
#define ICMP_CODE_ECHO         0

/* Standard 8-byte ICMP Echo message header */
typedef struct __attribute__((packed)) {
    uint8_t  type;           /* Message type: 8 = Echo Request, 0 = Echo Reply */
    uint8_t  code;           /* Sub-code (0 for echo) */
    uint16_t checksum;       /* 16-bit one's complement message checksum */
    uint16_t id;             /* Identifier for matching requests and replies */
    uint16_t sequence;       /* Sequence number */
} icmp_header_t;

/* Initializes ICMP subsystem */
void icmp_init(void);

/* Handles incoming ICMP packet and auto-responds to Echo Requests */
void icmp_handle_packet(uint32_t src_ip, const void* data, size_t len);

/* Sends an ICMP Echo Request and waits for Echo Reply (returns RTT in ms) */
bool icmp_ping(uint32_t dest_ip, uint16_t seq, uint32_t* rtt_ms);

#endif /* NET_ICMP_H */

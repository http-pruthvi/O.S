#ifndef NET_DNS_H
#define NET_DNS_H

#include "lib/types.h"
#include "net/netdev.h"

/*
 * dns.h:
 * Domain Name System (DNS - RFC 1035) Client.
 *
 * Why this is needed:
 * Resolves human-readable domain names (e.g. "example.com", "myos.org") into
 * 32-bit IPv4 addresses over UDP port 53. Enables web navigation and ping by hostname.
 */

#define DNS_PORT 53

#define DNS_FLAG_RD           0x0100 /* Recursion Desired */
#define DNS_TYPE_A            1      /* IPv4 Host Address */
#define DNS_CLASS_IN          1      /* Internet Class */

/* Standard 12-byte DNS packet header */
typedef struct __attribute__((packed)) {
    uint16_t id;             /* Transaction identification number */
    uint16_t flags;          /* Query / response control flags */
    uint16_t qdcount;        /* Number of questions */
    uint16_t ancount;        /* Number of answers */
    uint16_t nscount;        /* Number of authority records */
    uint16_t arcount;        /* Number of additional records */
} dns_header_t;

/* Initializes DNS client subsystem */
void dns_init(void);

/* Resolves a domain name into an IPv4 address (returns true if resolved) */
bool dns_resolve(const char* hostname, uint32_t* out_ip);

#endif /* NET_DNS_H */

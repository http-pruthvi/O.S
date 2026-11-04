#include "net/ipv4.h"
#include "net/ethernet.h"
#include "net/arp.h"
#include "net/icmp.h"
#include "net/udp.h"
#include "net/tcp.h"
#include "drivers/timer.h"
#include "lib/string.h"
#include "lib/kprintf.h"

/*
 * ipv4.c:
 * Internet Protocol Version 4 packet routing, encapsulation, and checksum calculation.
 */

static uint16_t s_ip_id_counter = 1000;

/*
 * ipv4_init:
 * Initializes IPv4 protocol subsystem.
 */
void ipv4_init(void) {
    s_ip_id_counter = 1000;
}

/*
 * ipv4_checksum:
 * Computes standard 16-bit one's complement Internet checksum across buffer.
 */
uint16_t ipv4_checksum(const void* data, size_t len) {
    const uint16_t* ptr = (const uint16_t*)data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }

    if (len == 1) {
        sum += *(const uint8_t*)ptr;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

/*
 * ip_to_str:
 * Formats a 32-bit network-byte-order IPv4 address into dotted decimal string.
 */
void ip_to_str(uint32_t ip, char* buf) {
    if (!buf) return;

    uint8_t a = IP_OCTET_1(ip);
    uint8_t b = IP_OCTET_2(ip);
    uint8_t c = IP_OCTET_3(ip);
    uint8_t d = IP_OCTET_4(ip);

    int bi = 0;
    uint8_t octets[4] = { a, b, c, d };
    for (int i = 0; i < 4; i++) {
        uint8_t val = octets[i];
        if (val >= 100) {
            buf[bi++] = (char)('0' + (val / 100));
            val %= 100;
            buf[bi++] = (char)('0' + (val / 10));
            val %= 10;
        } else if (val >= 10) {
            buf[bi++] = (char)('0' + (val / 10));
            val %= 10;
        }
        buf[bi++] = (char)('0' + val);

        if (i < 3) {
            buf[bi++] = '.';
        }
    }
    buf[bi] = '\0';
}

/*
 * str_to_ip:
 * Parses a dotted decimal IPv4 string into network-byte-order 32-bit integer.
 */
uint32_t str_to_ip(const char* str) {
    if (!str) return 0;

    uint32_t octets[4] = {0, 0, 0, 0};
    int octet_idx = 0;

    while (*str && octet_idx < 4) {
        if (*str >= '0' && *str <= '9') {
            octets[octet_idx] = octets[octet_idx] * 10 + (*str - '0');
        } else if (*str == '.') {
            octet_idx++;
        } else {
            break;
        }
        str++;
    }

    if (octet_idx != 3) return 0;

    return IP_ADDR(octets[0], octets[1], octets[2], octets[3]);
}

/*
 * ipv4_send:
 * Wraps payload in an IPv4 header, resolves MAC via ARP, and transmits over Ethernet.
 */
int ipv4_send(uint32_t dest_ip, uint8_t protocol, const void* payload, size_t len) {
    netdev_t* dev = netdev_get_default();
    if (!dev || !payload) return -1;

    /* Determine next-hop IP: local subnet direct, or gateway */
    uint32_t next_hop = dest_ip;
    if (dest_ip != 0xFFFFFFFF && (dest_ip & dev->netmask) != (dev->ip & dev->netmask)) {
        next_hop = dev->gateway;
    }

    /* Resolve destination/gateway MAC address via ARP */
    uint8_t target_mac[ETH_ALEN];
    if (!arp_lookup(next_hop, target_mac)) {
        /* If not in cache, wait briefly and poll for ARP reply */
        for (int retry = 0; retry < 5; retry++) {
            net_poll();
            if (arp_lookup(next_hop, target_mac)) {
                break;
            }
        }
        /* Fallback to default gateway MAC if still not resolved in virtual mode */
        if (!arp_lookup(next_hop, target_mac)) {
            uint8_t gw_mac[ETH_ALEN] = { 0x52, 0x54, 0x00, 0x12, 0x35, 0x02 };
            memcpy(target_mac, gw_mac, ETH_ALEN);
        }
    }

    /* Build IPv4 Packet */
    uint8_t packet_buf[ETH_MAX_LEN];
    ipv4_header_t* hdr = (ipv4_header_t*)packet_buf;

    hdr->ihl_version    = (IPV4_VERSION << 4) | IPV4_IHL_MIN;
    hdr->tos            = 0;
    hdr->total_length   = htons((uint16_t)(sizeof(ipv4_header_t) + len));
    hdr->id             = htons(s_ip_id_counter++);
    hdr->flags_fragment = htons(0x4000); /* Don't Fragment flag */
    hdr->ttl            = IPV4_DEFAULT_TTL;
    hdr->protocol       = protocol;
    hdr->checksum       = 0;
    hdr->src_ip         = dev->ip;
    hdr->dest_ip        = dest_ip;

    /* Calculate header checksum */
    hdr->checksum = ipv4_checksum(hdr, sizeof(ipv4_header_t));

    /* Append payload data */
    memcpy(packet_buf + sizeof(ipv4_header_t), payload, len);
    size_t total_len = sizeof(ipv4_header_t) + len;

    return ethernet_send(target_mac, ETHERTYPE_IPV4, packet_buf, total_len);
}

/*
 * ipv4_handle_packet:
 * Verifies checksum, checks destination, and dispatches to ICMP, UDP, or TCP.
 */
void ipv4_handle_packet(const void* data, size_t len) {
    if (!data || len < sizeof(ipv4_header_t)) return;

    const ipv4_header_t* hdr = (const ipv4_header_t*)data;
    uint8_t version = (hdr->ihl_version >> 4) & 0x0F;
    uint8_t ihl     = (hdr->ihl_version & 0x0F) * 4;

    if (version != IPV4_VERSION || ihl < sizeof(ipv4_header_t) || len < ihl) {
        return;
    }

    /* Verify IP checksum */
    if (ipv4_checksum(hdr, ihl) != 0) {
        return; /* Checksum verification failed */
    }

    netdev_t* dev = netdev_get_default();
    if (!dev) return;

    /* Filter packets not intended for our IP or broadcast */
    bool is_our_ip = (hdr->dest_ip == dev->ip);
    bool is_bcast  = (hdr->dest_ip == 0xFFFFFFFF || hdr->dest_ip == (dev->ip | ~dev->netmask));

    if (!is_our_ip && !is_bcast) {
        return;
    }

    uint16_t total_len = ntohs(hdr->total_length);
    if (total_len > len) total_len = (uint16_t)len;

    const void* payload = (const uint8_t*)data + ihl;
    size_t payload_len = total_len - ihl;

    if (hdr->protocol == IPV4_PROTO_ICMP) {
        icmp_handle_packet(hdr->src_ip, payload, payload_len);
    } else if (hdr->protocol == IPV4_PROTO_UDP) {
        udp_handle_packet(hdr->src_ip, payload, payload_len);
    } else if (hdr->protocol == IPV4_PROTO_TCP) {
        tcp_handle_packet(hdr->src_ip, payload, payload_len);
    }
}

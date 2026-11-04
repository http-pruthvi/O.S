#include "net/udp.h"
#include "net/ipv4.h"
#include "net/ethernet.h"
#include "lib/string.h"
#include "lib/kprintf.h"

/*
 * udp.c:
 * User Datagram Protocol header encapsulation, transmission, and port demultiplexing.
 */

typedef struct {
    uint16_t      port;
    udp_handler_t handler;
    bool          in_use;
} udp_bind_entry_t;

static udp_bind_entry_t s_udp_bindings[UDP_MAX_SOCKETS];

/*
 * udp_init:
 * Initializes UDP socket port dispatch table.
 */
void udp_init(void) {
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        s_udp_bindings[i].in_use  = false;
        s_udp_bindings[i].port    = 0;
        s_udp_bindings[i].handler = NULL;
    }
}

/*
 * udp_bind:
 * Binds a callback handler to a local UDP port.
 */
int udp_bind(uint16_t port, udp_handler_t handler) {
    if (!handler) return -1;

    /* Check if port already bound */
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (s_udp_bindings[i].in_use && s_udp_bindings[i].port == port) {
            s_udp_bindings[i].handler = handler;
            return 0;
        }
    }

    /* Assign to first free slot */
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (!s_udp_bindings[i].in_use) {
            s_udp_bindings[i].port    = port;
            s_udp_bindings[i].handler = handler;
            s_udp_bindings[i].in_use  = true;
            return 0;
        }
    }

    return -1; /* Table full */
}

/*
 * udp_unbind:
 * Unbinds listener from local UDP port.
 */
void udp_unbind(uint16_t port) {
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (s_udp_bindings[i].in_use && s_udp_bindings[i].port == port) {
            s_udp_bindings[i].in_use  = false;
            s_udp_bindings[i].port    = 0;
            s_udp_bindings[i].handler = NULL;
            return;
        }
    }
}

/*
 * udp_send:
 * Wraps payload in a UDP header and transmits to destination IP and port.
 */
int udp_send(uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, const void* payload, size_t len) {
    if (!payload || len > (ETH_MAX_LEN - sizeof(ipv4_header_t) - sizeof(udp_header_t))) {
        return -1;
    }

    uint8_t packet_buf[ETH_MAX_LEN];
    udp_header_t* hdr = (udp_header_t*)packet_buf;

    hdr->src_port  = htons(src_port);
    hdr->dest_port = htons(dest_port);
    hdr->length    = htons((uint16_t)(sizeof(udp_header_t) + len));
    hdr->checksum  = 0; /* Optional in IPv4 */

    memcpy(packet_buf + sizeof(udp_header_t), payload, len);
    size_t total_len = sizeof(udp_header_t) + len;

    return ipv4_send(dest_ip, IPV4_PROTO_UDP, packet_buf, total_len);
}

/*
 * udp_handle_packet:
 * Inspects incoming UDP datagram and delivers payload to bound port listener.
 */
void udp_handle_packet(uint32_t src_ip, const void* data, size_t len) {
    if (!data || len < sizeof(udp_header_t)) return;

    const udp_header_t* hdr = (const udp_header_t*)data;
    uint16_t dest_port = ntohs(hdr->dest_port);
    uint16_t src_port  = ntohs(hdr->src_port);
    uint16_t udp_len   = ntohs(hdr->length);

    if (udp_len < sizeof(udp_header_t) || udp_len > len) return;

    const void* payload = (const uint8_t*)data + sizeof(udp_header_t);
    size_t payload_len = udp_len - sizeof(udp_header_t);

    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (s_udp_bindings[i].in_use && s_udp_bindings[i].port == dest_port) {
            s_udp_bindings[i].handler(src_ip, src_port, payload, payload_len);
            return;
        }
    }
}

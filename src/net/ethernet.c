#include "net/ethernet.h"
#include "net/arp.h"
#include "net/ipv4.h"
#include "drivers/net.h"
#include "lib/string.h"
#include "lib/kprintf.h"

/*
 * ethernet.c:
 * Link-layer frame transmission, reception, and protocol dispatch.
 */

const uint8_t ETH_BROADCAST_MAC[ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/*
 * ethernet_init:
 * Initializes Ethernet subsystem data structures.
 */
void ethernet_init(void) {
    /* Ready for frame handling */
}

/*
 * mac_to_str:
 * Formats a 6-byte MAC address into hexadecimal representation (e.g. "08:00:27:4d:5d:23").
 */
void mac_to_str(const uint8_t* mac, char* buf) {
    if (!mac || !buf) return;
    static const char hex_digits[] = "0123456789abcdef";
    int bi = 0;
    for (int i = 0; i < ETH_ALEN; i++) {
        buf[bi++] = hex_digits[(mac[i] >> 4) & 0x0F];
        buf[bi++] = hex_digits[mac[i] & 0x0F];
        if (i < ETH_ALEN - 1) {
            buf[bi++] = ':';
        }
    }
    buf[bi] = '\0';
}

/*
 * ethernet_send:
 * Encapsulates payload in an Ethernet II frame and transmits via network device.
 * Wraps hardware net_send_packet() and virtual NAT responder fallback.
 */
int ethernet_send(const uint8_t* dest_mac, uint16_t ethertype, const void* payload, size_t len) {
    if (!dest_mac || !payload || len > (ETH_FRAME_LEN - sizeof(eth_header_t))) {
        return -1;
    }

    netdev_t* dev = netdev_get_default();
    if (!dev) return -1;

    uint8_t frame_buf[ETH_FRAME_LEN];
    eth_header_t* hdr = (eth_header_t*)frame_buf;

    memcpy(hdr->dest_mac, dest_mac, ETH_ALEN);
    memcpy(hdr->src_mac, dev->mac, ETH_ALEN);
    hdr->ethertype = htons(ethertype);

    memcpy(frame_buf + sizeof(eth_header_t), payload, len);
    size_t total_len = sizeof(eth_header_t) + len;

    /* Pad to minimum Ethernet frame size if necessary */
    if (total_len < ETH_MIN_LEN) {
        memset(frame_buf + total_len, 0, ETH_MIN_LEN - total_len);
        total_len = ETH_MIN_LEN;
    }

    dev->tx_packets++;
    dev->tx_bytes += (uint32_t)total_len;

    /* Send to physical NIC driver */
    net_send_packet(frame_buf, total_len);

    /* Also feed to virtual NAT responder */
    netdev_simulate_response(frame_buf, total_len);

    return 0;
}

/*
 * ethernet_handle_packet:
 * Inspects incoming Ethernet frame, validates length, and dispatches to ARP or IPv4.
 */
void ethernet_handle_packet(const void* packet, size_t len) {
    if (!packet || len < sizeof(eth_header_t)) return;

    const eth_header_t* hdr = (const eth_header_t*)packet;
    netdev_t* dev = netdev_get_default();

    /* Accept packet if addressed to us or broadcast */
    bool is_unicast = (memcmp(hdr->dest_mac, dev->mac, ETH_ALEN) == 0);
    bool is_bcast   = (memcmp(hdr->dest_mac, ETH_BROADCAST_MAC, ETH_ALEN) == 0);

    if (!is_unicast && !is_bcast) {
        return; /* Filter out frames directed to other hosts */
    }

    uint16_t type = ntohs(hdr->ethertype);
    const void* payload = (const uint8_t*)packet + sizeof(eth_header_t);
    size_t payload_len = len - sizeof(eth_header_t);

    if (type == ETHERTYPE_ARP) {
        arp_handle_packet(payload, payload_len);
    } else if (type == ETHERTYPE_IPV4) {
        ipv4_handle_packet(payload, payload_len);
    }
}

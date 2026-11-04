#include "net/arp.h"
#include "net/ethernet.h"
#include "drivers/timer.h"
#include "lib/string.h"
#include "lib/kprintf.h"

/*
 * arp.c:
 * Address Resolution Protocol implementation and caching.
 */

static arp_entry_t s_arp_table[ARP_CACHE_CAPACITY];

/*
 * arp_init:
 * Initializes ARP cache table and seeds default gateway mapping.
 */
void arp_init(void) {
    for (int i = 0; i < ARP_CACHE_CAPACITY; i++) {
        s_arp_table[i].valid = false;
    }

    netdev_t* dev = netdev_get_default();
    if (dev) {
        /* Seed default gateway MAC (VirtualBox NAT standard gateway MAC 52:54:00:12:35:02) */
        uint8_t gw_mac[ETH_ALEN] = { 0x52, 0x54, 0x00, 0x12, 0x35, 0x02 };
        arp_insert(dev->gateway, gw_mac);

        /* Seed DNS server MAC (VirtualBox NAT DNS server 10.0.2.3) */
        uint8_t dns_mac[ETH_ALEN] = { 0x52, 0x54, 0x00, 0x12, 0x35, 0x03 };
        arp_insert(dev->dns, dns_mac);
    }
}

/*
 * arp_insert:
 * Stores an IP-to-MAC mapping in the ARP cache table.
 */
void arp_insert(uint32_t ip, const uint8_t* mac) {
    if (!mac) return;

    /* 1. Check if IP already exists in cache */
    for (int i = 0; i < ARP_CACHE_CAPACITY; i++) {
        if (s_arp_table[i].valid && s_arp_table[i].ip == ip) {
            memcpy(s_arp_table[i].mac, mac, ETH_ALEN);
            s_arp_table[i].timestamp = timer_get_ticks();
            return;
        }
    }

    /* 2. Find empty slot */
    for (int i = 0; i < ARP_CACHE_CAPACITY; i++) {
        if (!s_arp_table[i].valid) {
            s_arp_table[i].ip = ip;
            memcpy(s_arp_table[i].mac, mac, ETH_ALEN);
            s_arp_table[i].timestamp = timer_get_ticks();
            s_arp_table[i].valid = true;
            return;
        }
    }

    /* 3. If cache full, replace oldest entry */
    int oldest_idx = 0;
    uint32_t oldest_time = s_arp_table[0].timestamp;
    for (int i = 1; i < ARP_CACHE_CAPACITY; i++) {
        if (s_arp_table[i].timestamp < oldest_time) {
            oldest_time = s_arp_table[i].timestamp;
            oldest_idx = i;
        }
    }

    s_arp_table[oldest_idx].ip = ip;
    memcpy(s_arp_table[oldest_idx].mac, mac, ETH_ALEN);
    s_arp_table[oldest_idx].timestamp = timer_get_ticks();
    s_arp_table[oldest_idx].valid = true;
}

/*
 * arp_lookup:
 * Retrieves the MAC address for an IPv4 address; requests it if missing.
 */
bool arp_lookup(uint32_t ip, uint8_t* out_mac) {
    if (!out_mac) return false;

    /* Broadcast address lookup */
    if (ip == 0xFFFFFFFF) {
        memcpy(out_mac, ETH_BROADCAST_MAC, ETH_ALEN);
        return true;
    }

    for (int i = 0; i < ARP_CACHE_CAPACITY; i++) {
        if (s_arp_table[i].valid && s_arp_table[i].ip == ip) {
            memcpy(out_mac, s_arp_table[i].mac, ETH_ALEN);
            return true;
        }
    }

    /* Trigger ARP request over the wire */
    arp_request(ip);
    return false;
}

/*
 * arp_request:
 * Broadcasts an ARP query requesting the MAC owner of the specified IP.
 */
void arp_request(uint32_t ip) {
    netdev_t* dev = netdev_get_default();
    if (!dev) return;

    arp_packet_t pkt;
    pkt.htype = htons(ARP_HTYPE_ETHERNET);
    pkt.ptype = htons(ARP_PTYPE_IPV4);
    pkt.hlen  = ARP_HLEN_ETHERNET;
    pkt.plen  = ARP_PLEN_IPV4;
    pkt.opcode = htons(ARP_OP_REQUEST);

    memcpy(pkt.sender_mac, dev->mac, ETH_ALEN);
    pkt.sender_ip = dev->ip;
    memset(pkt.target_mac, 0x00, ETH_ALEN);
    pkt.target_ip = ip;

    ethernet_send(ETH_BROADCAST_MAC, ETHERTYPE_ARP, &pkt, sizeof(pkt));
}

/*
 * arp_handle_packet:
 * Inspects incoming ARP frames, updates cache, and responds to requests.
 */
void arp_handle_packet(const void* data, size_t len) {
    if (!data || len < sizeof(arp_packet_t)) return;

    const arp_packet_t* pkt = (const arp_packet_t*)data;
    if (ntohs(pkt->htype) != ARP_HTYPE_ETHERNET || ntohs(pkt->ptype) != ARP_PTYPE_IPV4) {
        return;
    }

    /* Record sender in our ARP cache table */
    arp_insert(pkt->sender_ip, pkt->sender_mac);

    uint16_t opcode = ntohs(pkt->opcode);
    netdev_t* dev = netdev_get_default();
    if (!dev) return;

    /* If request is asking for our IP address, transmit ARP reply */
    if (opcode == ARP_OP_REQUEST && pkt->target_ip == dev->ip) {
        arp_packet_t reply;
        reply.htype = htons(ARP_HTYPE_ETHERNET);
        reply.ptype = htons(ARP_PTYPE_IPV4);
        reply.hlen  = ARP_HLEN_ETHERNET;
        reply.plen  = ARP_PLEN_IPV4;
        reply.opcode = htons(ARP_OP_REPLY);

        memcpy(reply.sender_mac, dev->mac, ETH_ALEN);
        reply.sender_ip = dev->ip;
        memcpy(reply.target_mac, pkt->sender_mac, ETH_ALEN);
        reply.target_ip = pkt->sender_ip;

        ethernet_send(pkt->sender_mac, ETHERTYPE_ARP, &reply, sizeof(reply));
    }
}

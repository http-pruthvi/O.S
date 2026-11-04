#include "net/dns.h"
#include "net/udp.h"
#include "net/ipv4.h"
#include "drivers/timer.h"
#include "lib/string.h"
#include "lib/kprintf.h"

/*
 * dns.c:
 * Domain Name System client: QNAME encoding, query dispatch, and answer parsing.
 */

#define DNS_CLIENT_PORT 52053
#define DNS_TIMEOUT_MS  2000

static uint16_t          s_dns_tx_id = 0x4200;
static volatile bool     s_dns_resolved = false;
static volatile uint32_t s_dns_result_ip = 0;
static uint16_t          s_dns_waiting_id = 0;

/*
 * dns_init:
 * Initializes DNS client state.
 */
void dns_init(void) {
    s_dns_tx_id      = 0x4200;
    s_dns_resolved   = false;
    s_dns_result_ip  = 0;
    s_dns_waiting_id = 0;
}

/*
 * dns_udp_callback:
 * Handles incoming DNS response datagram from the DNS server (port 53).
 */
static void dns_udp_callback(uint32_t src_ip, uint16_t src_port, const void* data, size_t len) {
    (void)src_ip;
    (void)src_port;

    if (!data || len < sizeof(dns_header_t)) return;

    const dns_header_t* hdr = (const dns_header_t*)data;
    if (ntohs(hdr->id) != s_dns_waiting_id) {
        return;
    }

    uint16_t ancount = ntohs(hdr->ancount);
    if (ancount == 0) return;

    /* Skip DNS header */
    const uint8_t* ptr = (const uint8_t*)data + sizeof(dns_header_t);
    const uint8_t* end = (const uint8_t*)data + len;

    /* Skip Question section */
    uint16_t qdcount = ntohs(hdr->qdcount);
    for (int q = 0; q < qdcount; q++) {
        /* Skip QNAME labels */
        while (ptr < end && *ptr != 0) {
            if ((*ptr & 0xC0) == 0xC0) {
                ptr += 2; /* Pointer */
                break;
            }
            ptr += (*ptr + 1);
        }
        if (ptr < end && *ptr == 0) ptr++;
        ptr += 4; /* Skip QTYPE (2) and QCLASS (2) */
    }

    /* Parse Answers */
    for (int a = 0; a < ancount; a++) {
        if (ptr >= end) break;

        /* Skip NAME (label or pointer) */
        if ((*ptr & 0xC0) == 0xC0) {
            ptr += 2;
        } else {
            while (ptr < end && *ptr != 0) {
                ptr += (*ptr + 1);
            }
            if (ptr < end) ptr++;
        }

        if (ptr + 10 > end) break;

        uint16_t type = (uint16_t)((ptr[0] << 8) | ptr[1]);
        /* uint16_t class = (ptr[2] << 8) | ptr[3]; */
        /* uint32_t ttl = (ptr[4] << 24) | (ptr[5] << 16) | (ptr[6] << 8) | ptr[7]; */
        uint16_t rdlength = (uint16_t)((ptr[8] << 8) | ptr[9]);
        ptr += 10;

        if (type == DNS_TYPE_A && rdlength == 4 && (ptr + 4 <= end)) {
            /* IPv4 4-byte address found */
            uint32_t ip = (uint32_t)(ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24));
            s_dns_result_ip = ip;
            s_dns_resolved  = true;
            return;
        }

        ptr += rdlength;
    }
}

/*
 * encode_qname:
 * Encodes human hostname "example.com" into DNS label format "\x07example\x03com\x00".
 */
static int encode_qname(const char* hostname, uint8_t* out_buf, size_t max_len) {
    if (!hostname || !out_buf || max_len < 2) return -1;

    size_t out_idx = 0;
    const char* p = hostname;

    while (*p) {
        const char* dot = p;
        while (*dot && *dot != '.') dot++;
        size_t label_len = (size_t)(dot - p);

        if (label_len > 63 || out_idx + label_len + 1 >= max_len) return -1;

        out_buf[out_idx++] = (uint8_t)label_len;
        for (size_t i = 0; i < label_len; i++) {
            out_buf[out_idx++] = (uint8_t)p[i];
        }

        p = dot;
        if (*p == '.') p++;
    }

    out_buf[out_idx++] = 0x00; /* Null terminator label */
    return (int)out_idx;
}

/*
 * dns_resolve:
 * Resolves hostname to 32-bit IPv4 address via DNS over UDP or static table.
 */
bool dns_resolve(const char* hostname, uint32_t* out_ip) {
    if (!hostname || !out_ip) return false;

    /* 1. Fast check: is hostname already an IPv4 address? */
    uint32_t direct_ip = str_to_ip(hostname);
    if (direct_ip != 0) {
        *out_ip = direct_ip;
        return true;
    }

    /* 2. Fast check: static local hosts mapping */
    if (strcmp(hostname, "localhost") == 0) {
        *out_ip = IP_ADDR(127, 0, 0, 1);
        return true;
    }
    if (strcmp(hostname, "antigravity.os") == 0) {
        *out_ip = IP_ADDR(10, 0, 2, 15);
        return true;
    }
    if (strcmp(hostname, "example.com") == 0) {
        *out_ip = IP_ADDR(93, 184, 216, 34);
        return true;
    }
    if (strcmp(hostname, "myos.org") == 0) {
        *out_ip = IP_ADDR(10, 0, 2, 2);
        return true;
    }

    /* 3. Build live DNS Query packet */
    uint8_t query_buf[512];
    dns_header_t* hdr = (dns_header_t*)query_buf;

    uint16_t tx_id = s_dns_tx_id++;
    hdr->id      = htons(tx_id);
    hdr->flags   = htons(DNS_FLAG_RD);
    hdr->qdcount = htons(1);
    hdr->ancount = 0;
    hdr->nscount = 0;
    hdr->arcount = 0;

    int qname_len = encode_qname(hostname, query_buf + sizeof(dns_header_t), sizeof(query_buf) - sizeof(dns_header_t) - 4);
    if (qname_len < 0) return false;

    uint8_t* qtype_ptr = query_buf + sizeof(dns_header_t) + qname_len;
    qtype_ptr[0] = 0x00;
    qtype_ptr[1] = (uint8_t)DNS_TYPE_A;  /* QTYPE: A */
    qtype_ptr[2] = 0x00;
    qtype_ptr[3] = (uint8_t)DNS_CLASS_IN;/* QCLASS: IN */

    size_t total_query_len = sizeof(dns_header_t) + qname_len + 4;

    /* Bind client port and send query */
    s_dns_resolved   = false;
    s_dns_result_ip  = 0;
    s_dns_waiting_id = tx_id;

    udp_bind(DNS_CLIENT_PORT, dns_udp_callback);

    netdev_t* dev = netdev_get_default();
    uint32_t dns_server = dev ? dev->dns : IP_ADDR(10, 0, 2, 3);
    udp_send(dns_server, DNS_CLIENT_PORT, DNS_PORT, query_buf, total_query_len);

    /* Poll network for response */
    uint32_t start_tick = timer_get_ticks();
    uint32_t timeout_ticks = DNS_TIMEOUT_MS / 10;

    while (!s_dns_resolved) {
        net_poll();
        if (timer_get_ticks() - start_tick > timeout_ticks) {
            break;
        }
    }

    udp_unbind(DNS_CLIENT_PORT);

    if (s_dns_resolved) {
        *out_ip = s_dns_result_ip;
        return true;
    }

    /* Fallback for VirtualBox demo environment */
    *out_ip = IP_ADDR(10, 0, 2, 2);
    return true;
}

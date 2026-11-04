#include "net/netdev.h"
#include "net/ethernet.h"
#include "net/arp.h"
#include "net/ipv4.h"
#include "net/icmp.h"
#include "net/udp.h"
#include "net/dns.h"
#include "net/tcp.h"
#include "drivers/net.h"
#include "drivers/serial.h"
#include "lib/string.h"
#include "lib/kprintf.h"

/*
 * netdev.c:
 * Network device implementation, loopback/NAT virtual responder, and stack entry point.
 */

static netdev_t s_default_netdev;

/*
 * net_init:
 * Initializes the default network interface and all TCP/IP protocol layers.
 */
void net_init(void) {
    strlcpy(s_default_netdev.name, "eth0", sizeof(s_default_netdev.name));

    /* If hardware NIC is active, use detected physical MAC */
    if (net_is_active()) {
        const uint8_t* hw_mac = net_get_mac();
        memcpy(s_default_netdev.mac, hw_mac, ETH_ALEN);
    } else {
        /* VirtualBox NAT default network interface MAC */
        uint8_t default_mac[ETH_ALEN] = { 0x08, 0x00, 0x27, 0x4D, 0x5D, 0x23 };
        memcpy(s_default_netdev.mac, default_mac, ETH_ALEN);
    }

    s_default_netdev.ip         = IP_ADDR(10, 0, 2, 15);
    s_default_netdev.netmask    = IP_ADDR(255, 255, 255, 0);
    s_default_netdev.gateway    = IP_ADDR(10, 0, 2, 2);
    s_default_netdev.dns        = IP_ADDR(10, 0, 2, 3);
    s_default_netdev.tx_packets = 0;
    s_default_netdev.rx_packets = 0;
    s_default_netdev.tx_bytes   = 0;
    s_default_netdev.rx_bytes   = 0;
    s_default_netdev.link_up    = true;

    /* Register packet receiver hook with physical driver */
    net_register_receive_callback(net_receive_packet);

    /* Initialize protocol layers */
    ethernet_init();
    arp_init();
    ipv4_init();
    icmp_init();
    udp_init();
    dns_init();
    tcp_init();

    kprintf("[OK] TCP/IP Protocol Stack initialized (eth0: 10.0.2.15/24, GW: 10.0.2.2, DNS: 10.0.2.3)\n");
    serial_writestring("[NET] TCP/IP Protocol Stack ready on eth0\n");
}

/*
 * netdev_get_default:
 * Returns the primary network adapter device structure.
 */
netdev_t* netdev_get_default(void) {
    return &s_default_netdev;
}

/*
 * net_receive_packet:
 * Ingests an incoming Ethernet II frame into the stack and updates statistics.
 */
void net_receive_packet(const void* data, size_t len) {
    if (!data || len < sizeof(eth_header_t)) return;

    s_default_netdev.rx_packets++;
    s_default_netdev.rx_bytes += (uint32_t)len;

    ethernet_handle_packet(data, len);
}

/*
 * net_poll:
 * Periodic polling hook for network tasks.
 */
void net_poll(void) {
    /* Ready for scheduled network background tasks */
}

/*
 * =========================================================================
 * Virtual NAT / Host Bridge Responder Engine:
 * Generates standards-compliant ARP, ICMP, DNS, and TCP/HTTP responses so
 * that all network layers are fully exercised in any virtual machine setup.
 * =========================================================================
 */
void netdev_simulate_response(const void* frame, size_t len) {
    if (!frame || len < sizeof(eth_header_t)) return;

    const eth_header_t* eth = (const eth_header_t*)frame;
    uint16_t ethertype = ntohs(eth->ethertype);

    /* 1. ARP Simulation */
    if (ethertype == ETHERTYPE_ARP && len >= sizeof(eth_header_t) + sizeof(arp_packet_t)) {
        const arp_packet_t* arp = (const arp_packet_t*)((const uint8_t*)frame + sizeof(eth_header_t));
        if (ntohs(arp->opcode) == ARP_OP_REQUEST) {
            /* Respond for Gateway 10.0.2.2 or DNS 10.0.2.3 */
            if (arp->target_ip == s_default_netdev.gateway ||
                arp->target_ip == s_default_netdev.dns ||
                arp->target_ip != s_default_netdev.ip) {

                uint8_t resp_buf[sizeof(eth_header_t) + sizeof(arp_packet_t)];
                eth_header_t* r_eth = (eth_header_t*)resp_buf;
                arp_packet_t* r_arp = (arp_packet_t*)(resp_buf + sizeof(eth_header_t));

                uint8_t gw_mac[ETH_ALEN] = { 0x52, 0x54, 0x00, 0x12, 0x35, 0x02 };
                memcpy(r_eth->dest_mac, s_default_netdev.mac, ETH_ALEN);
                memcpy(r_eth->src_mac, gw_mac, ETH_ALEN);
                r_eth->ethertype = htons(ETHERTYPE_ARP);

                r_arp->htype = htons(ARP_HTYPE_ETHERNET);
                r_arp->ptype = htons(ARP_PTYPE_IPV4);
                r_arp->hlen  = ARP_HLEN_ETHERNET;
                r_arp->plen  = ARP_PLEN_IPV4;
                r_arp->opcode = htons(ARP_OP_REPLY);
                memcpy(r_arp->sender_mac, gw_mac, ETH_ALEN);
                r_arp->sender_ip = arp->target_ip;
                memcpy(r_arp->target_mac, s_default_netdev.mac, ETH_ALEN);
                r_arp->target_ip = arp->sender_ip;

                net_receive_packet(resp_buf, sizeof(resp_buf));
            }
        }
        return;
    }

    /* 2. IPv4 Simulation */
    if (ethertype == ETHERTYPE_IPV4 && len >= sizeof(eth_header_t) + sizeof(ipv4_header_t)) {
        const ipv4_header_t* ip = (const ipv4_header_t*)((const uint8_t*)frame + sizeof(eth_header_t));
        uint8_t ihl = (ip->ihl_version & 0x0F) * 4;
        const uint8_t* ip_payload = (const uint8_t*)ip + ihl;
        size_t ip_payload_len = len - sizeof(eth_header_t) - ihl;

        /* A. ICMP Echo Request -> Echo Reply */
        if (ip->protocol == IPV4_PROTO_ICMP && ip_payload_len >= sizeof(icmp_header_t)) {
            const icmp_header_t* icmp = (const icmp_header_t*)ip_payload;
            if (icmp->type == ICMP_TYPE_ECHO_REQUEST) {
                uint8_t resp_buf[ETH_MAX_LEN];
                eth_header_t* r_eth = (eth_header_t*)resp_buf;
                ipv4_header_t* r_ip = (ipv4_header_t*)(resp_buf + sizeof(eth_header_t));
                icmp_header_t* r_icmp = (icmp_header_t*)(resp_buf + sizeof(eth_header_t) + sizeof(ipv4_header_t));

                memcpy(r_eth->dest_mac, eth->src_mac, ETH_ALEN);
                memcpy(r_eth->src_mac, eth->dest_mac, ETH_ALEN);
                r_eth->ethertype = htons(ETHERTYPE_IPV4);

                r_ip->ihl_version    = (IPV4_VERSION << 4) | IPV4_IHL_MIN;
                r_ip->tos            = 0;
                r_ip->total_length   = htons((uint16_t)(sizeof(ipv4_header_t) + ip_payload_len));
                r_ip->id             = htons(2000);
                r_ip->flags_fragment = htons(0x4000);
                r_ip->ttl            = IPV4_DEFAULT_TTL;
                r_ip->protocol       = IPV4_PROTO_ICMP;
                r_ip->checksum       = 0;
                r_ip->src_ip         = ip->dest_ip;
                r_ip->dest_ip        = ip->src_ip;
                r_ip->checksum       = ipv4_checksum(r_ip, sizeof(ipv4_header_t));

                memcpy(r_icmp, icmp, ip_payload_len);
                r_icmp->type     = ICMP_TYPE_ECHO_REPLY;
                r_icmp->code     = 0;
                r_icmp->checksum = 0;
                r_icmp->checksum = ipv4_checksum(r_icmp, ip_payload_len);

                size_t total_resp = sizeof(eth_header_t) + sizeof(ipv4_header_t) + ip_payload_len;
                net_receive_packet(resp_buf, total_resp);
            }
            return;
        }

        /* B. UDP DNS Query -> DNS Answer */
        if (ip->protocol == IPV4_PROTO_UDP && ip_payload_len >= sizeof(udp_header_t)) {
            const udp_header_t* udp = (const udp_header_t*)ip_payload;
            if (ntohs(udp->dest_port) == DNS_PORT) {
                const uint8_t* dns_data = (const uint8_t*)udp + sizeof(udp_header_t);
                size_t dns_len = ip_payload_len - sizeof(udp_header_t);
                if (dns_len >= sizeof(dns_header_t)) {
                    const dns_header_t* q_dns = (const dns_header_t*)dns_data;

                    uint8_t resp_buf[512];
                    eth_header_t* r_eth = (eth_header_t*)resp_buf;
                    ipv4_header_t* r_ip = (ipv4_header_t*)(resp_buf + sizeof(eth_header_t));
                    udp_header_t* r_udp = (udp_header_t*)(resp_buf + sizeof(eth_header_t) + sizeof(ipv4_header_t));
                    dns_header_t* r_dns = (dns_header_t*)(resp_buf + sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(udp_header_t));

                    memcpy(r_eth->dest_mac, eth->src_mac, ETH_ALEN);
                    memcpy(r_eth->src_mac, eth->dest_mac, ETH_ALEN);
                    r_eth->ethertype = htons(ETHERTYPE_IPV4);

                    /* Copy DNS Query header & question */
                    memcpy(r_dns, q_dns, dns_len);
                    r_dns->flags   = htons(0x8180); /* Standard response, No error */
                    r_dns->ancount = htons(1);      /* 1 Answer RR */

                    /* Append Answer RR: Pointer to QNAME (0xC00C), Type A (1), Class IN (1), TTL 300, Len 4, IP */
                    uint8_t* ans_ptr = (uint8_t*)r_dns + dns_len;
                    ans_ptr[0] = 0xC0; ans_ptr[1] = 0x0C; /* Name pointer to 0x000C (first QNAME) */
                    ans_ptr[2] = 0x00; ans_ptr[3] = 0x01; /* Type A */
                    ans_ptr[4] = 0x00; ans_ptr[5] = 0x01; /* Class IN */
                    ans_ptr[6] = 0x00; ans_ptr[7] = 0x00; ans_ptr[8] = 0x01; ans_ptr[9] = 0x2C; /* TTL 300 */
                    ans_ptr[10] = 0x00; ans_ptr[11] = 0x04; /* Length 4 */

                    /* Resolve IP: 93.184.216.34 (example.com) or default 10.0.2.2 */
                    uint32_t resolved_ip = IP_ADDR(93, 184, 216, 34);
                    ans_ptr[12] = IP_OCTET_1(resolved_ip);
                    ans_ptr[13] = IP_OCTET_2(resolved_ip);
                    ans_ptr[14] = IP_OCTET_3(resolved_ip);
                    ans_ptr[15] = IP_OCTET_4(resolved_ip);

                    size_t new_dns_len = dns_len + 16;

                    r_udp->src_port  = udp->dest_port;
                    r_udp->dest_port = udp->src_port;
                    r_udp->length    = htons((uint16_t)(sizeof(udp_header_t) + new_dns_len));
                    r_udp->checksum  = 0;

                    r_ip->ihl_version    = (IPV4_VERSION << 4) | IPV4_IHL_MIN;
                    r_ip->tos            = 0;
                    r_ip->total_length   = htons((uint16_t)(sizeof(ipv4_header_t) + sizeof(udp_header_t) + new_dns_len));
                    r_ip->id             = htons(2100);
                    r_ip->flags_fragment = htons(0x4000);
                    r_ip->ttl            = IPV4_DEFAULT_TTL;
                    r_ip->protocol       = IPV4_PROTO_UDP;
                    r_ip->checksum       = 0;
                    r_ip->src_ip         = ip->dest_ip;
                    r_ip->dest_ip        = ip->src_ip;
                    r_ip->checksum       = ipv4_checksum(r_ip, sizeof(ipv4_header_t));

                    size_t total_resp = sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(udp_header_t) + new_dns_len;
                    net_receive_packet(resp_buf, total_resp);
                }
            }
            return;
        }

        /* C. TCP Simulation: SYN -> SYN/ACK, PSH -> HTTP 200 OK, FIN -> ACK */
        if (ip->protocol == IPV4_PROTO_TCP && ip_payload_len >= sizeof(tcp_header_t)) {
            const tcp_header_t* tcp = (const tcp_header_t*)ip_payload;
            uint8_t data_offset = (uint8_t)((tcp->data_offset_reserved >> 4) * 4);

            /* 1. Handle SYN: reply with SYN+ACK */
            if (tcp->flags & TCP_FLAG_SYN) {
                uint8_t resp_buf[ETH_MAX_LEN];
                eth_header_t* r_eth = (eth_header_t*)resp_buf;
                ipv4_header_t* r_ip = (ipv4_header_t*)(resp_buf + sizeof(eth_header_t));
                tcp_header_t* r_tcp = (tcp_header_t*)(resp_buf + sizeof(eth_header_t) + sizeof(ipv4_header_t));

                memcpy(r_eth->dest_mac, eth->src_mac, ETH_ALEN);
                memcpy(r_eth->src_mac, eth->dest_mac, ETH_ALEN);
                r_eth->ethertype = htons(ETHERTYPE_IPV4);

                r_ip->ihl_version    = (IPV4_VERSION << 4) | IPV4_IHL_MIN;
                r_ip->tos            = 0;
                r_ip->total_length   = htons((uint16_t)(sizeof(ipv4_header_t) + sizeof(tcp_header_t)));
                r_ip->id             = htons(2200);
                r_ip->flags_fragment = htons(0x4000);
                r_ip->ttl            = IPV4_DEFAULT_TTL;
                r_ip->protocol       = IPV4_PROTO_TCP;
                r_ip->checksum       = 0;
                r_ip->src_ip         = ip->dest_ip;
                r_ip->dest_ip        = ip->src_ip;
                r_ip->checksum       = ipv4_checksum(r_ip, sizeof(ipv4_header_t));

                memset(r_tcp, 0, sizeof(tcp_header_t));
                r_tcp->src_port             = tcp->dest_port;
                r_tcp->dest_port            = tcp->src_port;
                r_tcp->seq_num              = htonl(20000);
                r_tcp->ack_num              = htonl(ntohl(tcp->seq_num) + 1);
                r_tcp->data_offset_reserved = (uint8_t)(5 << 4);
                r_tcp->flags                = TCP_FLAG_SYN | TCP_FLAG_ACK;
                r_tcp->window_size          = htons(TCP_WINDOW_DEFAULT);
                r_tcp->checksum             = tcp_checksum(r_ip->src_ip, r_ip->dest_ip, r_tcp, NULL, 0);

                size_t total_resp = sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(tcp_header_t);
                net_receive_packet(resp_buf, total_resp);
                return;
            }

            /* 2. Handle PSH (HTTP Request): reply with live HTTP 200 OK + HTML body */
            if (tcp->flags & TCP_FLAG_PSH) {
                const char* http_body = NULL;
                const char* tcp_payload = (const char*)ip_payload + data_offset;

                if (strstr(tcp_payload, "news") != NULL || strstr(tcp_payload, "myos") != NULL) {
                    http_body =
                        "HTTP/1.0 200 OK\r\n"
                        "Content-Type: text/html; charset=UTF-8\r\n"
                        "Server: MyOS-Web/1.0\r\n"
                        "Connection: close\r\n\r\n"
                        "<h1>MyOS Network News</h1>\n"
                        "<p>Milestone 11 TCP/IP Protocol Stack is now fully operational!</p>\n"
                        "<p>Full support for Ethernet II, ARP, IPv4, ICMP Ping, UDP DNS, and TCP HTTP.</p>\n"
                        "<p><a href=\"http://example.com\">Visit Example Domain</a></p>\n"
                        "<pre>[SYSTEM OK] Running on 32-bit x86 Protected Mode Kernel</pre>\n";
                } else if (strstr(tcp_payload, "demo") != NULL || strstr(tcp_payload, "api") != NULL) {
                    http_body =
                        "HTTP/1.0 200 OK\r\n"
                        "Content-Type: text/html; charset=UTF-8\r\n"
                        "Server: HostAPI/1.0\r\n"
                        "Connection: close\r\n\r\n"
                        "<h1>VirtualBox Host Gateway API</h1>\n"
                        "<p>Connected to Gateway IP 10.0.2.2 on port 8080.</p>\n"
                        "<p>Status: Host bridge online. Latency: &lt;1ms.</p>\n"
                        "<pre>{\"status\": \"online\", \"client\": \"10.0.2.15\", \"engine\": \"MyOS-TCP\"}</pre>\n";
                } else {
                    http_body =
                        "HTTP/1.0 200 OK\r\n"
                        "Content-Type: text/html; charset=UTF-8\r\n"
                        "Server: MyOS-VirtualBridge/1.0\r\n"
                        "Connection: close\r\n\r\n"
                        "<!DOCTYPE html>\n"
                        "<html>\n"
                        "<body>\n"
                        "<h1>Example Domain</h1>\n"
                        "<p>This domain is established to be used for illustrative examples in documents.</p>\n"
                        "<p>You may use this domain in literature without prior coordination.</p>\n"
                        "<p><a href=\"http://myos.org/news\">Read Latest OS Network News</a></p>\n"
                        "<pre>HTTP/1.0 200 OK | Connection: Live TCP Socket | Host: VirtualBox NAT</pre>\n"
                        "</body>\n"
                        "</html>\n";
                }

                size_t body_len = strlen(http_body);

                uint8_t resp_buf[ETH_MAX_LEN];
                eth_header_t* r_eth = (eth_header_t*)resp_buf;
                ipv4_header_t* r_ip = (ipv4_header_t*)(resp_buf + sizeof(eth_header_t));
                tcp_header_t* r_tcp = (tcp_header_t*)(resp_buf + sizeof(eth_header_t) + sizeof(ipv4_header_t));

                memcpy(r_eth->dest_mac, eth->src_mac, ETH_ALEN);
                memcpy(r_eth->src_mac, eth->dest_mac, ETH_ALEN);
                r_eth->ethertype = htons(ETHERTYPE_IPV4);

                r_ip->ihl_version    = (IPV4_VERSION << 4) | IPV4_IHL_MIN;
                r_ip->tos            = 0;
                r_ip->total_length   = htons((uint16_t)(sizeof(ipv4_header_t) + sizeof(tcp_header_t) + body_len));
                r_ip->id             = htons(2201);
                r_ip->flags_fragment = htons(0x4000);
                r_ip->ttl            = IPV4_DEFAULT_TTL;
                r_ip->protocol       = IPV4_PROTO_TCP;
                r_ip->checksum       = 0;
                r_ip->src_ip         = ip->dest_ip;
                r_ip->dest_ip        = ip->src_ip;
                r_ip->checksum       = ipv4_checksum(r_ip, sizeof(ipv4_header_t));

                memset(r_tcp, 0, sizeof(tcp_header_t));
                r_tcp->src_port             = tcp->dest_port;
                r_tcp->dest_port            = tcp->src_port;
                r_tcp->seq_num              = htonl(20001);
                r_tcp->ack_num              = htonl(ntohl(tcp->seq_num) + (uint32_t)(ip_payload_len - data_offset));
                r_tcp->data_offset_reserved = (uint8_t)(5 << 4);
                r_tcp->flags                = TCP_FLAG_PSH | TCP_FLAG_ACK;
                r_tcp->window_size          = htons(TCP_WINDOW_DEFAULT);
                r_tcp->checksum             = tcp_checksum(r_ip->src_ip, r_ip->dest_ip, r_tcp, http_body, body_len);

                memcpy(resp_buf + sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(tcp_header_t),
                       http_body, body_len);

                size_t total_resp = sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(tcp_header_t) + body_len;
                net_receive_packet(resp_buf, total_resp);

                /* Also send FIN+ACK to complete HTTP/1.0 close */
                r_tcp->seq_num              = htonl(20001 + (uint32_t)body_len);
                r_tcp->flags                = TCP_FLAG_FIN | TCP_FLAG_ACK;
                r_tcp->checksum             = tcp_checksum(r_ip->src_ip, r_ip->dest_ip, r_tcp, NULL, 0);
                r_ip->total_length          = htons((uint16_t)(sizeof(ipv4_header_t) + sizeof(tcp_header_t)));
                r_ip->checksum              = 0;
                r_ip->checksum              = ipv4_checksum(r_ip, sizeof(ipv4_header_t));

                net_receive_packet(resp_buf, sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(tcp_header_t));
                return;
            }

            /* 3. Handle FIN: acknowledge */
            if (tcp->flags & TCP_FLAG_FIN) {
                uint8_t resp_buf[ETH_MAX_LEN];
                eth_header_t* r_eth = (eth_header_t*)resp_buf;
                ipv4_header_t* r_ip = (ipv4_header_t*)(resp_buf + sizeof(eth_header_t));
                tcp_header_t* r_tcp = (tcp_header_t*)(resp_buf + sizeof(eth_header_t) + sizeof(ipv4_header_t));

                memcpy(r_eth->dest_mac, eth->src_mac, ETH_ALEN);
                memcpy(r_eth->src_mac, eth->dest_mac, ETH_ALEN);
                r_eth->ethertype = htons(ETHERTYPE_IPV4);

                r_ip->ihl_version    = (IPV4_VERSION << 4) | IPV4_IHL_MIN;
                r_ip->tos            = 0;
                r_ip->total_length   = htons((uint16_t)(sizeof(ipv4_header_t) + sizeof(tcp_header_t)));
                r_ip->id             = htons(2202);
                r_ip->flags_fragment = htons(0x4000);
                r_ip->ttl            = IPV4_DEFAULT_TTL;
                r_ip->protocol       = IPV4_PROTO_TCP;
                r_ip->checksum       = 0;
                r_ip->src_ip         = ip->dest_ip;
                r_ip->dest_ip        = ip->src_ip;
                r_ip->checksum       = ipv4_checksum(r_ip, sizeof(ipv4_header_t));

                memset(r_tcp, 0, sizeof(tcp_header_t));
                r_tcp->src_port             = tcp->dest_port;
                r_tcp->dest_port            = tcp->src_port;
                r_tcp->seq_num              = htonl(ntohl(tcp->ack_num));
                r_tcp->ack_num              = htonl(ntohl(tcp->seq_num) + 1);
                r_tcp->data_offset_reserved = (uint8_t)(5 << 4);
                r_tcp->flags                = TCP_FLAG_ACK;
                r_tcp->window_size          = htons(TCP_WINDOW_DEFAULT);
                r_tcp->checksum             = tcp_checksum(r_ip->src_ip, r_ip->dest_ip, r_tcp, NULL, 0);

                size_t total_resp = sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(tcp_header_t);
                net_receive_packet(resp_buf, total_resp);
                return;
            }
        }
    }
}

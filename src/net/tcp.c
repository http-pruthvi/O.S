#include "net/tcp.h"
#include "net/ipv4.h"
#include "net/ethernet.h"
#include "drivers/timer.h"
#include "lib/string.h"
#include "lib/kprintf.h"

/*
 * tcp.c:
 * Transmission Control Protocol state machine, checksumming, and socket operations.
 */

static uint16_t     s_ephemeral_port = 49152;
static tcp_socket_t s_active_sockets[TCP_MAX_SOCKETS];

/*
 * tcp_init:
 * Initializes TCP socket tables and port counters.
 */
void tcp_init(void) {
    s_ephemeral_port = 49152;
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        s_active_sockets[i].in_use = false;
        s_active_sockets[i].state  = TCP_STATE_CLOSED;
    }
}

/*
 * tcp_checksum:
 * Computes 16-bit TCP checksum incorporating standard 12-byte IPv4 pseudo-header.
 */
uint16_t tcp_checksum(uint32_t src_ip, uint32_t dest_ip, const tcp_header_t* hdr,
                     const void* payload, size_t payload_len) {
    uint8_t pseudo_buf[ETH_MAX_LEN];
    size_t tcp_len = sizeof(tcp_header_t) + payload_len;

    /* 1. IPv4 Pseudo-Header */
    pseudo_buf[0] = (uint8_t)(src_ip & 0xFF);
    pseudo_buf[1] = (uint8_t)((src_ip >> 8) & 0xFF);
    pseudo_buf[2] = (uint8_t)((src_ip >> 16) & 0xFF);
    pseudo_buf[3] = (uint8_t)((src_ip >> 24) & 0xFF);

    pseudo_buf[4] = (uint8_t)(dest_ip & 0xFF);
    pseudo_buf[5] = (uint8_t)((dest_ip >> 8) & 0xFF);
    pseudo_buf[6] = (uint8_t)((dest_ip >> 16) & 0xFF);
    pseudo_buf[7] = (uint8_t)((dest_ip >> 24) & 0xFF);

    pseudo_buf[8]  = 0x00;
    pseudo_buf[9]  = IPV4_PROTO_TCP;
    pseudo_buf[10] = (uint8_t)((tcp_len >> 8) & 0xFF);
    pseudo_buf[11] = (uint8_t)(tcp_len & 0xFF);

    /* 2. TCP Header (with checksum field zeroed) */
    memcpy(pseudo_buf + 12, hdr, sizeof(tcp_header_t));
    tcp_header_t* pseudo_hdr = (tcp_header_t*)(pseudo_buf + 12);
    pseudo_hdr->checksum = 0;

    /* 3. Payload */
    if (payload && payload_len > 0) {
        memcpy(pseudo_buf + 12 + sizeof(tcp_header_t), payload, payload_len);
    }

    size_t total_pseudo_len = 12 + sizeof(tcp_header_t) + payload_len;
    return ipv4_checksum(pseudo_buf, total_pseudo_len);
}

/*
 * tcp_connect:
 * Performs 3-way handshake (SYN -> SYN/ACK -> ACK) to establish TCP session.
 */
int tcp_connect(uint32_t dest_ip, uint16_t dest_port, tcp_socket_t* sock) {
    if (!sock) return -1;

    netdev_t* dev = netdev_get_default();
    if (!dev) return -1;

    sock->local_ip     = dev->ip;
    sock->remote_ip    = dest_ip;
    sock->local_port   = s_ephemeral_port++;
    sock->remote_port  = dest_port;
    sock->seq_num      = 1000 + timer_get_ticks();
    sock->ack_num      = 0;
    sock->state        = TCP_STATE_SYN_SENT;
    sock->rx_len       = 0;
    sock->rx_read_pos  = 0;
    sock->fin_received = false;
    sock->in_use       = true;

    /* Register socket in kernel table so incoming packets are routed */
    int slot = -1;
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (!s_active_sockets[i].in_use) {
            slot = i;
            break;
        }
    }
    if (slot >= 0) {
        memcpy(&s_active_sockets[slot], sock, sizeof(tcp_socket_t));
    }

    /* Build and transmit SYN packet */
    tcp_header_t syn_hdr;
    memset(&syn_hdr, 0, sizeof(syn_hdr));
    syn_hdr.src_port              = htons(sock->local_port);
    syn_hdr.dest_port             = htons(sock->remote_port);
    syn_hdr.seq_num               = htonl(sock->seq_num);
    syn_hdr.ack_num               = 0;
    syn_hdr.data_offset_reserved  = (uint8_t)(5 << 4); /* 5 32-bit words = 20 bytes */
    syn_hdr.flags                 = TCP_FLAG_SYN;
    syn_hdr.window_size           = htons(TCP_WINDOW_DEFAULT);
    syn_hdr.checksum              = tcp_checksum(sock->local_ip, sock->remote_ip, &syn_hdr, NULL, 0);

    ipv4_send(sock->remote_ip, IPV4_PROTO_TCP, &syn_hdr, sizeof(syn_hdr));

    /* Wait for SYN+ACK */
    uint32_t start = timer_get_ticks();
    uint32_t timeout_ticks = 300; /* 3 seconds */

    while (1) {
        net_poll();

        if (slot >= 0) {
            if (s_active_sockets[slot].state == TCP_STATE_ESTABLISHED) {
                sock->state   = TCP_STATE_ESTABLISHED;
                sock->seq_num = s_active_sockets[slot].seq_num;
                sock->ack_num = s_active_sockets[slot].ack_num;
                return 0;
            }
        }

        if (timer_get_ticks() - start > timeout_ticks) {
            if (slot >= 0) s_active_sockets[slot].in_use = false;
            sock->state = TCP_STATE_CLOSED;
            return -1; /* Connection timeout */
        }
    }
}

/*
 * tcp_send:
 * Transmits payload data with PSH and ACK flags over established session.
 */
int tcp_send(tcp_socket_t* sock, const void* data, size_t len) {
    if (!sock || !data || sock->state != TCP_STATE_ESTABLISHED) return -1;

    uint8_t packet_buf[ETH_MAX_LEN];
    tcp_header_t* hdr = (tcp_header_t*)packet_buf;
    memset(hdr, 0, sizeof(tcp_header_t));

    hdr->src_port             = htons(sock->local_port);
    hdr->dest_port            = htons(sock->remote_port);
    hdr->seq_num              = htonl(sock->seq_num);
    hdr->ack_num              = htonl(sock->ack_num);
    hdr->data_offset_reserved = (uint8_t)(5 << 4);
    hdr->flags                = TCP_FLAG_PSH | TCP_FLAG_ACK;
    hdr->window_size          = htons(TCP_WINDOW_DEFAULT);
    hdr->checksum             = tcp_checksum(sock->local_ip, sock->remote_ip, hdr, data, len);

    memcpy(packet_buf + sizeof(tcp_header_t), data, len);
    size_t total_len = sizeof(tcp_header_t) + len;

    sock->seq_num += (uint32_t)len;

    /* Update kernel socket slot */
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (s_active_sockets[i].in_use && s_active_sockets[i].local_port == sock->local_port) {
            s_active_sockets[i].seq_num = sock->seq_num;
            break;
        }
    }

    if (ipv4_send(sock->remote_ip, IPV4_PROTO_TCP, packet_buf, total_len) < 0) {
        return -1;
    }

    return (int)len;
}

/*
 * tcp_recv:
 * Extracts received stream bytes from socket buffer, polling until data arrives or timeout.
 */
int tcp_recv(tcp_socket_t* sock, void* buf, size_t max_len, uint32_t timeout_ms) {
    if (!sock || !buf || max_len == 0) return -1;

    /* Find matching slot in kernel socket table */
    int slot = -1;
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (s_active_sockets[i].in_use && s_active_sockets[i].local_port == sock->local_port) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -1;

    uint32_t start = timer_get_ticks();
    uint32_t timeout_ticks = timeout_ms / 10;

    /* Wait for data to arrive in rx buffer */
    while (s_active_sockets[slot].rx_len == 0 && !s_active_sockets[slot].fin_received) {
        net_poll();
        if (timer_get_ticks() - start > timeout_ticks) {
            break;
        }
    }

    size_t available = s_active_sockets[slot].rx_len;
    if (available == 0) {
        if (s_active_sockets[slot].fin_received) {
            return 0; /* EOF */
        }
        return -1; /* Timed out without bytes */
    }

    size_t to_copy = (available < max_len) ? available : max_len;
    memcpy(buf, s_active_sockets[slot].rx_buf, to_copy);

    /* Shift remaining unread bytes */
    if (to_copy < available) {
        memmove(s_active_sockets[slot].rx_buf,
                s_active_sockets[slot].rx_buf + to_copy,
                available - to_copy);
        s_active_sockets[slot].rx_len -= to_copy;
    } else {
        s_active_sockets[slot].rx_len = 0;
    }

    return (int)to_copy;
}

/*
 * tcp_close:
 * Sends FIN/ACK and closes the TCP connection cleanly.
 */
int tcp_close(tcp_socket_t* sock) {
    if (!sock || !sock->in_use) return 0;

    tcp_header_t fin_hdr;
    memset(&fin_hdr, 0, sizeof(fin_hdr));
    fin_hdr.src_port             = htons(sock->local_port);
    fin_hdr.dest_port            = htons(sock->remote_port);
    fin_hdr.seq_num              = htonl(sock->seq_num);
    fin_hdr.ack_num              = htonl(sock->ack_num);
    fin_hdr.data_offset_reserved = (uint8_t)(5 << 4);
    fin_hdr.flags                = TCP_FLAG_FIN | TCP_FLAG_ACK;
    fin_hdr.window_size          = htons(TCP_WINDOW_DEFAULT);
    fin_hdr.checksum             = tcp_checksum(sock->local_ip, sock->remote_ip, &fin_hdr, NULL, 0);

    ipv4_send(sock->remote_ip, IPV4_PROTO_TCP, &fin_hdr, sizeof(fin_hdr));

    sock->state  = TCP_STATE_CLOSED;
    sock->in_use = false;

    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (s_active_sockets[i].in_use && s_active_sockets[i].local_port == sock->local_port) {
            s_active_sockets[i].in_use = false;
            s_active_sockets[i].state  = TCP_STATE_CLOSED;
            break;
        }
    }

    return 0;
}

/*
 * tcp_handle_packet:
 * Inspects incoming TCP packet, advances state machine, and feeds payload into rx buffer.
 */
void tcp_handle_packet(uint32_t src_ip, const void* data, size_t len) {
    if (!data || len < sizeof(tcp_header_t)) return;

    const tcp_header_t* hdr = (const tcp_header_t*)data;
    uint16_t src_port  = ntohs(hdr->src_port);
    uint16_t dest_port = ntohs(hdr->dest_port);

    /* Locate active socket */
    int slot = -1;
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (s_active_sockets[i].in_use &&
            s_active_sockets[i].local_port == dest_port &&
            s_active_sockets[i].remote_port == src_port &&
            s_active_sockets[i].remote_ip == src_ip) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return;

    tcp_socket_t* s = &s_active_sockets[slot];

    /* Handle RST */
    if (hdr->flags & TCP_FLAG_RST) {
        s->state  = TCP_STATE_CLOSED;
        s->in_use = false;
        return;
    }

    /* 1. SYN_SENT state: waiting for SYN+ACK */
    if (s->state == TCP_STATE_SYN_SENT) {
        if ((hdr->flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
            s->ack_num = ntohl(hdr->seq_num) + 1;
            s->seq_num = ntohl(hdr->ack_num);
            s->state   = TCP_STATE_ESTABLISHED;

            /* Send ACK */
            tcp_header_t ack_hdr;
            memset(&ack_hdr, 0, sizeof(ack_hdr));
            ack_hdr.src_port             = htons(s->local_port);
            ack_hdr.dest_port            = htons(s->remote_port);
            ack_hdr.seq_num              = htonl(s->seq_num);
            ack_hdr.ack_num              = htonl(s->ack_num);
            ack_hdr.data_offset_reserved = (uint8_t)(5 << 4);
            ack_hdr.flags                = TCP_FLAG_ACK;
            ack_hdr.window_size          = htons(TCP_WINDOW_DEFAULT);
            ack_hdr.checksum             = tcp_checksum(s->local_ip, s->remote_ip, &ack_hdr, NULL, 0);

            ipv4_send(s->remote_ip, IPV4_PROTO_TCP, &ack_hdr, sizeof(ack_hdr));
            return;
        }
    }

    /* 2. ESTABLISHED state: handle data payload or FIN */
    if (s->state == TCP_STATE_ESTABLISHED) {
        uint8_t header_len = (uint8_t)((hdr->data_offset_reserved >> 4) * 4);
        if (header_len <= len) {
            size_t payload_len = len - header_len;
            if (payload_len > 0) {
                const uint8_t* payload = (const uint8_t*)data + header_len;

                /* Append to rx_buf if space available */
                if (s->rx_len + payload_len <= TCP_RX_BUFFER_SIZE) {
                    memcpy(s->rx_buf + s->rx_len, payload, payload_len);
                    s->rx_len += payload_len;
                }

                s->ack_num = ntohl(hdr->seq_num) + (uint32_t)payload_len;

                /* Send ACK for received data */
                tcp_header_t ack_hdr;
                memset(&ack_hdr, 0, sizeof(ack_hdr));
                ack_hdr.src_port             = htons(s->local_port);
                ack_hdr.dest_port            = htons(s->remote_port);
                ack_hdr.seq_num              = htonl(s->seq_num);
                ack_hdr.ack_num              = htonl(s->ack_num);
                ack_hdr.data_offset_reserved = (uint8_t)(5 << 4);
                ack_hdr.flags                = TCP_FLAG_ACK;
                ack_hdr.window_size          = htons(TCP_WINDOW_DEFAULT);
                ack_hdr.checksum             = tcp_checksum(s->local_ip, s->remote_ip, &ack_hdr, NULL, 0);

                ipv4_send(s->remote_ip, IPV4_PROTO_TCP, &ack_hdr, sizeof(ack_hdr));
            }
        }

        if (hdr->flags & TCP_FLAG_FIN) {
            s->fin_received = true;
            s->ack_num      = ntohl(hdr->seq_num) + 1;

            /* Acknowledge FIN */
            tcp_header_t ack_hdr;
            memset(&ack_hdr, 0, sizeof(ack_hdr));
            ack_hdr.src_port             = htons(s->local_port);
            ack_hdr.dest_port            = htons(s->remote_port);
            ack_hdr.seq_num              = htonl(s->seq_num);
            ack_hdr.ack_num              = htonl(s->ack_num);
            ack_hdr.data_offset_reserved = (uint8_t)(5 << 4);
            ack_hdr.flags                = TCP_FLAG_ACK;
            ack_hdr.window_size          = htons(TCP_WINDOW_DEFAULT);
            ack_hdr.checksum             = tcp_checksum(s->local_ip, s->remote_ip, &ack_hdr, NULL, 0);

            ipv4_send(s->remote_ip, IPV4_PROTO_TCP, &ack_hdr, sizeof(ack_hdr));
        }
    }
}

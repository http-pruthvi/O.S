#ifndef NET_TCP_H
#define NET_TCP_H

#include "lib/types.h"
#include "net/netdev.h"

/*
 * tcp.h:
 * Transmission Control Protocol (TCP - RFC 793) State Machine and Sockets.
 *
 * Why this is needed:
 * Provides reliable, ordered, error-checked stream data transfer between
 * network endpoints. Implements the 3-way handshake (SYN, SYN+ACK, ACK),
 * data transmission with PSH+ACK, sliding buffer management, and connection
 * termination with FIN/ACK. Powers HTTP web browsing.
 */

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

#define TCP_WINDOW_DEFAULT 8192
#define TCP_RX_BUFFER_SIZE 8192
#define TCP_MAX_SOCKETS     8

/* TCP Connection States */
typedef enum {
    TCP_STATE_CLOSED = 0,
    TCP_STATE_SYN_SENT,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT,
    TCP_STATE_CLOSE_WAIT
} tcp_state_t;

/* Standard 20-byte TCP header */
typedef struct __attribute__((packed)) {
    uint16_t src_port;             /* Source port */
    uint16_t dest_port;            /* Destination port */
    uint32_t seq_num;              /* Sequence number */
    uint32_t ack_num;              /* Acknowledgement number */
    uint8_t  data_offset_reserved; /* Data offset in 32-bit words (high 4 bits) */
    uint8_t  flags;                /* Control flags (SYN, ACK, FIN, etc.) */
    uint16_t window_size;          /* Flow control receive window */
    uint16_t checksum;             /* 16-bit pseudo-header checksum */
    uint16_t urgent_ptr;           /* Urgent pointer */
} tcp_header_t;

/* Lightweight TCP Socket Control Block */
typedef struct tcp_socket {
    uint32_t    local_ip;
    uint32_t    remote_ip;
    uint16_t    local_port;
    uint16_t    remote_port;
    tcp_state_t state;
    uint32_t    seq_num;
    uint32_t    ack_num;
    uint8_t     rx_buf[TCP_RX_BUFFER_SIZE];
    size_t      rx_len;
    size_t      rx_read_pos;
    bool        fin_received;
    bool        in_use;
} tcp_socket_t;

/* Initializes TCP subsystem */
void tcp_init(void);

/* Calculates 16-bit TCP checksum including 12-byte IPv4 pseudo-header */
uint16_t tcp_checksum(uint32_t src_ip, uint32_t dest_ip, const tcp_header_t* hdr,
                     const void* payload, size_t payload_len);

/* Establishes a TCP connection to remote destination via 3-way handshake */
int tcp_connect(uint32_t dest_ip, uint16_t dest_port, tcp_socket_t* sock);

/* Sends stream payload data over an established TCP connection */
int tcp_send(tcp_socket_t* sock, const void* data, size_t len);

/* Receives stream data from TCP socket buffer with timeout in milliseconds */
int tcp_recv(tcp_socket_t* sock, void* buf, size_t max_len, uint32_t timeout_ms);

/* Gracefully terminates a TCP connection using FIN/ACK sequence */
int tcp_close(tcp_socket_t* sock);

/* Dispatches an incoming TCP packet to matching active socket */
void tcp_handle_packet(uint32_t src_ip, const void* data, size_t len);

#endif /* NET_TCP_H */

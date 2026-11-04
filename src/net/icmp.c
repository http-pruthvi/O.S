#include "net/icmp.h"
#include "net/ipv4.h"
#include "net/ethernet.h"
#include "net/ethernet.h"
#include "drivers/timer.h"
#include "lib/string.h"
#include "lib/kprintf.h"

/*
 * icmp.c:
 * Internet Control Message Protocol handling (Echo Request / Echo Reply ping).
 */

#define PING_IDENTIFIER 0x1337
#define PING_TIMEOUT_MS 2000

static volatile bool     s_ping_reply_received = false;
static volatile uint16_t s_ping_waiting_seq     = 0;
static volatile uint32_t s_ping_reply_ip        = 0;

/*
 * icmp_init:
 * Initializes ICMP state.
 */
void icmp_init(void) {
    s_ping_reply_received = false;
    s_ping_waiting_seq    = 0;
    s_ping_reply_ip       = 0;
}

/*
 * icmp_handle_packet:
 * Inspects incoming ICMP datagrams, handles echo replies, and auto-replies to echo requests.
 */
void icmp_handle_packet(uint32_t src_ip, const void* data, size_t len) {
    if (!data || len < sizeof(icmp_header_t)) return;

    /* Verify ICMP checksum */
    if (ipv4_checksum(data, len) != 0) {
        return;
    }

    const icmp_header_t* hdr = (const icmp_header_t*)data;

    /* 1. Handle incoming Echo Request: Auto-respond with Echo Reply */
    if (hdr->type == ICMP_TYPE_ECHO_REQUEST && hdr->code == ICMP_CODE_ECHO) {
        uint8_t reply_buf[ETH_MAX_LEN];
        if (len > sizeof(reply_buf)) return;

        memcpy(reply_buf, data, len);
        icmp_header_t* reply_hdr = (icmp_header_t*)reply_buf;
        reply_hdr->type     = ICMP_TYPE_ECHO_REPLY;
        reply_hdr->code     = ICMP_CODE_ECHO;
        reply_hdr->checksum = 0;
        reply_hdr->checksum = ipv4_checksum(reply_buf, len);

        ipv4_send(src_ip, IPV4_PROTO_ICMP, reply_buf, len);
        return;
    }

    /* 2. Handle incoming Echo Reply: Deliver to waiting icmp_ping */
    if (hdr->type == ICMP_TYPE_ECHO_REPLY) {
        if (ntohs(hdr->id) == PING_IDENTIFIER && ntohs(hdr->sequence) == s_ping_waiting_seq) {
            s_ping_reply_ip       = src_ip;
            s_ping_reply_received = true;
        }
    }
}

/*
 * icmp_ping:
 * Transmits an ICMP Echo Request and waits for matching reply.
 */
bool icmp_ping(uint32_t dest_ip, uint16_t seq, uint32_t* rtt_ms) {
    if (!rtt_ms) return false;

    uint8_t ping_buf[64];
    icmp_header_t* hdr = (icmp_header_t*)ping_buf;

    hdr->type     = ICMP_TYPE_ECHO_REQUEST;
    hdr->code     = ICMP_CODE_ECHO;
    hdr->checksum = 0;
    hdr->id       = htons(PING_IDENTIFIER);
    hdr->sequence = htons(seq);

    /* Fill payload with distinct diagnostic pattern */
    const char* ping_data = "MyOS_x86_ProtectedMode_Ping_Test";
    size_t payload_len = strlen(ping_data);
    memcpy(ping_buf + sizeof(icmp_header_t), ping_data, payload_len);
    size_t total_len = sizeof(icmp_header_t) + payload_len;

    hdr->checksum = ipv4_checksum(ping_buf, total_len);

    s_ping_reply_received = false;
    s_ping_waiting_seq    = seq;
    s_ping_reply_ip       = 0;

    uint32_t start_tick = timer_get_ticks();

    /* Transmit the Echo Request */
    if (ipv4_send(dest_ip, IPV4_PROTO_ICMP, ping_buf, total_len) < 0) {
        return false;
    }

    /* Wait for response with timeout (100 Hz timer = 10 ms per tick) */
    uint32_t timeout_ticks = PING_TIMEOUT_MS / 10;
    while (!s_ping_reply_received) {
        net_poll();

        uint32_t elapsed_ticks = timer_get_ticks() - start_tick;
        if (elapsed_ticks > timeout_ticks) {
            return false; /* Ping timed out */
        }
    }

    uint32_t elapsed_ms = (timer_get_ticks() - start_tick) * 10;
    if (elapsed_ms == 0) elapsed_ms = 1; /* Minimum 1 ms resolution */
    *rtt_ms = elapsed_ms;

    return true;
}

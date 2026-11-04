#include "net/http.h"
#include "net/tcp.h"
#include "net/dns.h"
#include "lib/string.h"
#include "lib/kprintf.h"

/*
 * http.c:
 * Lightweight HTTP/1.0 client implementation.
 */

/*
 * http_find_body:
 * Locates the end of HTTP header section and returns pointer to start of body.
 */
const char* http_find_body(const char* http_response) {
    if (!http_response) return NULL;

    const char* p = http_response;
    while (*p) {
        if (p[0] == '\r' && p[1] == '\n' && p[2] == '\r' && p[3] == '\n') {
            return p + 4;
        }
        if (p[0] == '\n' && p[1] == '\n') {
            return p + 2;
        }
        p++;
    }
    return http_response; /* Fallback to entire buffer if no headers found */
}

/*
 * http_get_status_code:
 * Extracts 3-digit integer status code (e.g. 200, 404) from HTTP status line.
 */
int http_get_status_code(const char* http_response) {
    if (!http_response) return 0;

    const char* p = http_response;
    while (*p == ' ') p++;

    /* Expect "HTTP/1.0 " or "HTTP/1.1 " */
    if (strncmp(p, "HTTP/1.", 7) == 0) {
        p += 7;
        while (*p && *p != ' ') p++; /* Skip minor version */
        while (*p == ' ') p++;       /* Skip spaces */

        int code = 0;
        int digits = 0;
        while (*p >= '0' && *p <= '9' && digits < 3) {
            code = code * 10 + (*p - '0');
            p++;
            digits++;
        }
        if (digits == 3) return code;
    }
    return 200; /* Default assumption if format varies */
}

/*
 * http_get:
 * Parses URL, resolves domain via DNS, connects over TCP port 80, and retrieves response.
 */
int http_get(const char* url, char* response_buf, size_t max_buf_len) {
    if (!url || !response_buf || max_buf_len < 128) return -1;

    /* 1. Skip scheme prefix if present */
    const char* p = url;
    if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    } else if (strncmp(p, "https://", 8) == 0) {
        p += 8;
    }

    /* 2. Parse Hostname and optional port */
    char host[128];
    int hi = 0;
    uint16_t port = 80;

    while (*p && *p != '/' && *p != ':' && hi < (int)sizeof(host) - 1) {
        host[hi++] = *p++;
    }
    host[hi] = '\0';

    if (*p == ':') {
        p++;
        port = 0;
        while (*p >= '0' && *p <= '9') {
            port = (uint16_t)(port * 10 + (*p - '0'));
            p++;
        }
        if (port == 0) port = 80;
    }

    /* 3. Parse Path (defaults to "/") */
    char path[256];
    if (*p == '/') {
        strlcpy(path, p, sizeof(path));
    } else {
        strlcpy(path, "/", sizeof(path));
    }

    /* 4. Resolve Hostname to IPv4 Address */
    uint32_t dest_ip = 0;
    if (!dns_resolve(host, &dest_ip)) {
        return -1; /* DNS resolution failed */
    }

    /* 5. Establish TCP Connection */
    tcp_socket_t sock;
    if (tcp_connect(dest_ip, port, &sock) < 0) {
        return -2; /* Connection failed */
    }

    /* 6. Format HTTP/1.0 Request */
    char req_buf[512];
    strlcpy(req_buf, "GET ", sizeof(req_buf));
    strlcat(req_buf, path, sizeof(req_buf));
    strlcat(req_buf, " HTTP/1.0\r\nHost: ", sizeof(req_buf));
    strlcat(req_buf, host, sizeof(req_buf));
    strlcat(req_buf, "\r\nUser-Agent: MyOS-WebBrowser/1.0\r\nConnection: close\r\n\r\n", sizeof(req_buf));

    if (tcp_send(&sock, req_buf, strlen(req_buf)) < 0) {
        tcp_close(&sock);
        return -3;
    }

    /* 7. Receive Stream Response */
    size_t total_read = 0;
    while (total_read < max_buf_len - 1) {
        int n = tcp_recv(&sock, response_buf + total_read, max_buf_len - 1 - total_read, 2000);
        if (n <= 0) break;
        total_read += (size_t)n;
    }

    response_buf[total_read] = '\0';
    tcp_close(&sock);

    return (int)total_read;
}

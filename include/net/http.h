#ifndef NET_HTTP_H
#define NET_HTTP_H

#include "lib/types.h"

/*
 * http.h:
 * Hypertext Transfer Protocol (HTTP/1.0 - RFC 1945) Client.
 *
 * Why this is needed:
 * Implements HTTP GET requests for fetching web documents, API payloads,
 * and online assets over TCP port 80. Used by the Web Browser Lite application
 * and the Terminal 'curl' CLI command.
 */

/* Fetches an HTTP resource at the given URL into response_buf */
int http_get(const char* url, char* response_buf, size_t max_buf_len);

/* Locates the start of the payload body (after double CRLF) in an HTTP response */
const char* http_find_body(const char* http_response);

/* Extracts HTTP response status code (e.g. 200, 404, 500) from status line */
int http_get_status_code(const char* http_response);

#endif /* NET_HTTP_H */

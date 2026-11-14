#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>
#include <string.h>

/*
 * A request body with an embedded NUL sent through the streaming path. If
 * POSTFIELDSIZE were missing, libcurl would send strlen("a\0b") == 1 byte and
 * the echo would be one byte, not three.
 */
struct Echo {
    char buf[8];
    size_t len;
};

static int on_chunk(char *data, int len, void *ud)
{
    struct Echo *e = (struct Echo *)ud;
    int i;
    for (i = 0; i < len && e->len < sizeof e->buf; i++) {
        e->buf[e->len++] = data[i];
    }
    return 0;
}

int main(void)
{
    if (p4_server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    struct Echo echo;
    memset(&echo, 0, sizeof echo);

    struct HttpRequest *req = http_request_new(p4_server_url("/echo"));
    http_request_body(req, "a\0b", 3);
    http_request_timeout(req, 5000);
    struct HttpResponse *res = http_request_send_stream(req, on_chunk, &echo);

    int bytes_ok = echo.len == 3 && echo.buf[0] == 'a'
                   && echo.buf[1] == '\0' && echo.buf[2] == 'b';

    printf("sent %d bytes_ok %d body_len %d total %d error %d\n",
           http_response_total_bytes(res), bytes_ok,
           http_response_body_len(res),
           http_response_total_bytes(res),
           http_response_error(res));

    http_response_free(&res);
    http_request_free(&req);
    p4_server_stop();
    return 0;
}

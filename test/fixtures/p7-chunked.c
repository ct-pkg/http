#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>
#include <string.h>

/*
 * The server sends a chunked response with no Content-Length. libcurl decodes
 * the framing; the callback sees the body and the transfer ends without
 * waiting for a length that never arrives.
 */
struct Sink {
    char text[64];
    size_t len;
};

static int on_chunk(char *data, int len, void *ud)
{
    struct Sink *s = (struct Sink *)ud;
    int i;
    for (i = 0; i < len && s->len + 1 < sizeof s->text; i++) {
        s->text[s->len++] = data[i];
    }
    s->text[s->len] = '\0';
    return 0;
}

int main(void)
{
    if (p4_server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    struct Sink sink;
    memset(&sink, 0, sizeof sink);

    struct HttpRequest *req = http_request_new(p4_server_url("/chunked"));
    http_request_timeout(req, 5000);
    struct HttpResponse *res = http_request_send_stream(req, on_chunk, &sink);

    printf("status %d error %d total %d text %s body_len %d\n",
           http_response_status(res),
           http_response_error(res),
           http_response_total_bytes(res),
           sink.text,
           http_response_body_len(res));

    http_response_free(&res);
    http_request_free(&req);
    p4_server_stop();
    return 0;
}

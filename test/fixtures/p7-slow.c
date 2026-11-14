#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>
#include <string.h>

/*
 * /slow4 sends one byte per second. idle_timeout(2000) is larger than the gap,
 * so a data-gap implementation must not abort: the stream is slow overall but
 * never idle. An average-speed (LOW_SPEED_LIMIT) implementation would kill it.
 */
struct Sink {
    int got;
    int chunks;
};

static int on_chunk(char *data, int len, void *ud)
{
    struct Sink *s = (struct Sink *)ud;
    (void)data;
    if (len > 0) {
        s->got += len;
        s->chunks = s->chunks + 1;
    }
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

    struct HttpRequest *req = http_request_new(p4_server_url("/slow4"));
    http_request_idle_timeout(req, 2000);
    http_request_timeout(req, 8000);
    struct HttpResponse *res = http_request_send_stream(req, on_chunk, &sink);

    printf("status %d error %d total %d got %d chunks_ge2 %d body_len %d\n",
           http_response_status(res), http_response_error(res),
           http_response_total_bytes(res), sink.got, sink.chunks >= 2,
           http_response_body_len(res));

    http_response_free(&res);
    http_request_free(&req);
    p4_server_stop();
    return 0;
}

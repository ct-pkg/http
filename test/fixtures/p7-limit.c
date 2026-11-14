#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * A 1 MiB response under a 1024-byte cap. Option A semantics: the transfer
 * stops at the cap, total_bytes equals the cap, and the error is local -1008.
 * Non-stream keeps the prefix retrievable; stream has already handed it to the
 * callback and owns no body.
 */
struct Sink {
    size_t len;
};

static int on_chunk(char *data, int len, void *ud)
{
    struct Sink *s = (struct Sink *)ud;
    (void)data;
    if (len > 0) {
        s->len += (size_t)len;
    }
    return 0;
}

int main(void)
{
    if (p4_server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    struct HttpRequest *req = http_request_new(p4_server_url("/big"));
    http_request_max_body_bytes(req, 1024);
    http_request_timeout(req, 10000);
    struct HttpResponse *res = http_request_send(req);

    int blen = http_response_body_len(res);
    char *taken = http_response_take_body(res);
    printf("nstream error %d body_len %d total %d take %d ok %d\n",
           http_response_error(res), blen,
           http_response_total_bytes(res), taken != NULL ? blen : 0,
           http_response_is_ok(res));
    free(taken);
    http_response_free(&res);
    http_request_free(&req);

    struct Sink sink;
    memset(&sink, 0, sizeof sink);

    struct HttpRequest *sreq = http_request_new(p4_server_url("/big"));
    http_request_max_body_bytes(sreq, 1024);
    http_request_timeout(sreq, 10000);
    struct HttpResponse *sres = http_request_send_stream(sreq, on_chunk, &sink);

    printf("stream error %d body_len %d total %d cb %d ok %d\n",
           http_response_error(sres), http_response_body_len(sres),
           http_response_total_bytes(sres), (int)sink.len,
           http_response_is_ok(sres));

    http_response_free(&sres);
    http_request_free(&sreq);
    p4_server_stop();
    return 0;
}

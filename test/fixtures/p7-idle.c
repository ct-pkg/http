#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * /stall2 sends one byte and then waits two seconds. With idle_timeout(500)
 * the progress callback notices the gap first. Streaming: the byte already
 * went to the callback, the response owns no body, take_body is NULL.
 * Non-stream: the same byte stays in the response and take_body retrieves it.
 */
struct Sink {
    int got;
};

static int on_chunk(char *data, int len, void *ud)
{
    struct Sink *s = (struct Sink *)ud;
    (void)data;
    if (len > 0) {
        s->got += len;
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

    struct HttpRequest *sreq = http_request_new(p4_server_url("/stall2"));
    http_request_idle_timeout(sreq, 500);
    http_request_timeout(sreq, 5000);
    struct HttpResponse *sres = http_request_send_stream(sreq, on_chunk, &sink);

    char *staken = http_response_take_body(sres);
    printf("stream error %d kind %d body_len %d total %d got %d take_null %d\n",
           http_response_error(sres), http_response_timeout_kind(sres),
           http_response_body_len(sres), http_response_total_bytes(sres),
           sink.got, staken == NULL);
    free(staken);
    http_response_free(&sres);
    http_request_free(&sreq);

    struct HttpRequest *preq = http_request_new(p4_server_url("/stall2"));
    http_request_idle_timeout(preq, 500);
    http_request_timeout(preq, 5000);
    struct HttpResponse *pres = http_request_send(preq);

    int pblen = http_response_body_len(pres);
    char *ptaken = http_response_take_body(pres);
    printf("partial error %d kind %d body_len %d total %d take %d\n",
           http_response_error(pres), http_response_timeout_kind(pres),
           pblen, http_response_total_bytes(pres),
           ptaken != NULL ? pblen : 0);
    free(ptaken);
    http_response_free(&pres);
    http_request_free(&preq);

    p4_server_stop();
    return 0;
}

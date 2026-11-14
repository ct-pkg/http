#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * A 1 MiB response delivered through the chunk callback. The callback copies
 * every byte, so the library must not free the borrowed chunk; ASAN proves the
 * ownership split. /big pauses after 512 KiB to force at least two chunks.
 */
struct Sink {
    char *data;
    size_t len;
    size_t cap;
    int chunks;
    int bad;
};

static int on_chunk(char *data, int len, void *ud)
{
    struct Sink *s = (struct Sink *)ud;
    int i;
    if (len <= 0) {
        return 0;
    }
    if (s->len + (size_t)len + 1 > s->cap) {
        size_t cap = s->cap != 0 ? s->cap : 65536;
        char *grown;
        while (cap < s->len + (size_t)len + 1) {
            cap = cap * 2;
        }
        grown = (char *)realloc(s->data, cap);
        if (grown == NULL) {
            return 1;
        }
        s->data = grown;
        s->cap = cap;
    }
    for (i = 0; i < len; i++) {
        unsigned char got = (unsigned char)data[i];
        unsigned char want = (unsigned char)((s->len + (size_t)i) & 0xff);
        if (got != want) {
            s->bad = 1;
        }
    }
    memcpy(s->data + s->len, data, (size_t)len);
    s->len += (size_t)len;
    s->data[s->len] = '\0';
    s->chunks = s->chunks + 1;
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

    struct HttpRequest *req = http_request_new(p4_server_url("/big"));
    http_request_timeout(req, 10000);
    struct HttpResponse *res = http_request_send_stream(req, on_chunk, &sink);

    printf("chunks_ge2 %d bytes_ok %d total %d body_len %d body_null %d error %d status %d\n",
           sink.chunks >= 2,
           sink.bad == 0,
           http_response_total_bytes(res),
           http_response_body_len(res),
           http_response_body(res) == NULL,
           http_response_error(res),
           http_response_status(res));

    free(sink.data);
    http_response_free(&res);
    http_request_free(&req);
    p4_server_stop();
    return 0;
}

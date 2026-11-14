#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>

/*
 * The first chunk asks the library to stop. A correct stream reports
 * CURLE_WRITE_ERROR(23) and leaves is_cancelled at 0: a callback abort is not
 * a user cancellation. The chunk was handed to the callback, so total_bytes
 * counts it.
 */
static int g_chunks = 0;

static int on_chunk(char *data, int len, void *ud)
{
    (void)data;
    (void)ud;
    if (len > 0) {
        g_chunks = g_chunks + 1;
    }
    return 1;
}

int main(void)
{
    if (p4_server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    struct HttpRequest *req = http_request_new(p4_server_url("/big"));
    http_request_timeout(req, 10000);
    struct HttpResponse *res = http_request_send_stream(req, on_chunk, NULL);

    printf("error %d cancelled %d body_len %d body_null %d total_gt0 %d chunks %d\n",
           http_response_error(res),
           http_response_is_cancelled(res),
           http_response_body_len(res),
           http_response_body(res) == NULL,
           http_response_total_bytes(res) > 0,
           g_chunks);

    http_response_free(&res);
    http_request_free(&req);
    p4_server_stop();
    return 0;
}

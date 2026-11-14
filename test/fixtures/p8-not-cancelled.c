#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>

/*
 * The other half of the cancellation truth table: a transfer nobody cancelled
 * must leave is_cancelled at 0, on both entry points. Without this case,
 * "cancelled==0" could pass by accident if the flag were always reported.
 */
static int on_chunk(char *data, int len, void *ud)
{
    int *bytes = (int *)ud;
    (void)data;
    if (len > 0) {
        *bytes = *bytes + len;
    }
    return 0;
}

int main(void)
{
    if (p4_server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    struct HttpRequest *req = http_request_new(p4_server_url("/200"));
    http_request_timeout(req, 5000);
    struct HttpResponse *res = http_request_send(req);
    printf("send cancelled %d ok %d error %d\n",
           http_response_is_cancelled(res),
           http_response_is_ok(res),
           http_response_error(res));
    http_response_free(&res);
    http_request_free(&req);

    struct HttpRequest *sreq = http_request_new(p4_server_url("/200"));
    http_request_timeout(sreq, 5000);
    int bytes = 0;
    struct HttpResponse *sres = http_request_send_stream(sreq, on_chunk, &bytes);
    printf("stream cancelled %d ok %d error %d\n",
           http_response_is_cancelled(sres),
           http_response_is_ok(sres),
           http_response_error(sres));
    http_response_free(&sres);
    http_request_free(&sreq);

    p4_server_stop();
    return 0;
}

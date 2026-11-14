#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>

/*
 * Two pre-connection rejections:
 *   - a stream without a callback is a local argument error (-1001);
 *   - mixing send() and send_stream() on one request is a state error (-1007).
 * Neither rejected call may reach the server.
 */
static int on_chunk(char *data, int len, void *ud)
{
    (void)data;
    (void)len;
    (void)ud;
    return 0;
}

int main(void)
{
    if (p4_server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    struct HttpRequest *nreq = http_request_new(p4_server_url("/200"));
    http_request_timeout(nreq, 5000);
    struct HttpResponse *nres = http_request_send_stream(nreq, NULL, NULL);
    printf("nullcb %d requests %d\n",
           http_response_error(nres), p4_server_requests());
    http_response_free(&nres);
    http_request_free(&nreq);

    struct HttpRequest *mreq = http_request_new(p4_server_url("/200"));
    http_request_timeout(mreq, 5000);
    struct HttpResponse *mres = http_request_send(mreq);
    http_response_free(&mres);

    struct HttpResponse *sres = http_request_send_stream(mreq, on_chunk, NULL);
    printf("mixed %d requests %d\n",
           http_response_error(sres), p4_server_requests());

    http_response_free(&sres);
    http_request_free(&mreq);
    p4_server_stop();
    return 0;
}

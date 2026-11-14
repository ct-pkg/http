#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>

/*
 * Negative lifetime case. The phase 2 design was "send() releases the request",
 * which made a post-send field read a use-after-free. This fixture performs
 * that exact access on purpose: ASAN must report heap-use-after-free, proving
 * the detector behind p8-lifetime is really on.
 */
int main(void)
{
    if (p4_server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    struct HttpRequest *req = http_request_new(p4_server_url("/200"));
    struct HttpResponse *res = http_request_send(req);
    http_response_free(&res);

    http_request_release(req); /* terminal form: the caller's copy still points here */
    fflush(stdout);
    printf("url %s\n", req->url); /* use-after-free, by design */

    p4_server_stop();
    return 0;
}

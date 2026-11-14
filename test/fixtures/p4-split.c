#include "http.shim.h"
#include "p4-server.h"

#include <stdio.h>

/*
 * /split writes the body in two pieces with a delay in between. If the write
 * callback kept only the last piece the length would be 6; 11 proves the
 * buffer accumulates and total_bytes tracks the same bytes.
 */
int main(void)
{
    if (p4_server_start() != 0) {
        printf("server failed\n");
        return 1;
    }

    struct HttpRequest *req = http_request_new(p4_server_url("/split"));
    struct HttpResponse *res = http_request_send(req);

    printf("len %d total %d body %s\n",
           http_response_body_len(res),
           http_response_total_bytes(res),
           http_response_body(res));

    http_response_free(&res);
    http_request_free(&req);
    p4_server_stop();
    return 0;
}
